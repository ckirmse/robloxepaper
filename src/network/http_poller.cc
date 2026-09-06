#include "network/http_poller.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <inttypes.h>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"

#include "log.h"

static const char * TAG = "HTTP";

static constexpr const char * GAMES_URL_BASE = "https://games.roblox.com/v1/games?universeIds=";
static constexpr const char * VOTES_URL_BASE = "https://games.roblox.com/v1/games/votes?universeIds=";

static constexpr int HTTP_TIMEOUT_MS = 15000;

struct HttpEventUserData {
    std::vector<char> body;
};

static esp_err_t httpEventHandler(esp_http_client_event_t * evt) {
    auto * ud = static_cast<HttpEventUserData *>(evt->user_data);
    // Accumulate all data regardless of chunked encoding (responses are small)
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        ud->body.insert(ud->body.end(),
                        static_cast<char *>(evt->data),
                        static_cast<char *>(evt->data) + evt->data_len);
    }
    return ESP_OK;
}

// Perform a single HTTP GET and return the response body. Returns false on failure.
static bool doGet(const char * url, std::vector<char> & out_body) {
    HttpEventUserData ud;

    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = HTTP_TIMEOUT_MS;
    config.event_handler = httpEventHandler;
    config.user_data = &ud;
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        eprintf(TAG, "esp_http_client_init failed for %s", url);
        return false;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        eprintf(TAG, "HTTP GET failed (%s): %s", url, esp_err_to_name(err));
        return false;
    }
    if (status != 200) {
        eprintf(TAG, "HTTP status %d for %s", status, url);
        return false;
    }

    ud.body.push_back('\0');
    out_body = std::move(ud.body);
    return true;
}

// Extract integer after key (e.g. "\"playing\":") using strstr + strtol.
static bool extractInt(const char * body, const char * key, int & out) {
    const char * p = strstr(body, key);
    if (!p) {
        return false;
    }
    p += strlen(key);
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    char * end = nullptr;
    long val = strtol(p, &end, 10);
    if (end == p) {
        return false;
    }
    out = (int32_t)val;
    return true;
}

// Extract quoted string value after key (e.g. "\"name\":\"").
// Copies into buf, stripping non-printable ASCII, '[', and ']', then trims spaces.
static bool extractString(const char * body, const char * key, char * buf, size_t buf_size) {
    const char * p = strstr(body, key);
    if (!p) {
        return false;
    }
    p += strlen(key);
    // skip opening quote
    if (*p != '"') {
        return false;
    }
    p++;

    size_t out_i = 0;
    while (*p && *p != '"' && out_i < buf_size - 1) {
        uint8_t c = (uint8_t)*p;
        if (c >= 0x20 && c <= 0x7E && c != '[' && c != ']') {
            buf[out_i++] = (char)c;
        }
        p++;
    }
    buf[out_i] = '\0';

    // trim leading spaces
    char * start = buf;
    while (*start == ' ') {
        start++;
    }
    if (start != buf) {
        memmove(buf, start, strlen(start) + 1);
    }

    // trim trailing spaces
    int len = (int)strlen(buf);
    while (len > 0 && buf[len - 1] == ' ') {
        buf[--len] = '\0';
    }

    return true;
}

// Locate the JSON object in a "data":[...] array whose top-level "id" equals
// universe_id, and copy it (NUL-terminated) into out_obj so the strstr-based
// extractors can operate on just that object. The object ends at the next
// "},{" separator or the end of the body. Returns false if not found.
static bool findGameObject(const char * body, int64_t universe_id, std::vector<char> & out_obj) {
    char key[32];
    snprintf(key, sizeof(key), "\"id\":%" PRId64, universe_id);

    const char * p = body;
    while (true) {
        p = strstr(p, key);
        if (!p) {
            return false;
        }
        // Make sure the id is not merely a prefix of a longer number
        const char * after = p + strlen(key);
        if (!isdigit((unsigned char)*after)) {
            break;
        }
        p = after;
    }

    const char * end = strstr(p, "},{");
    size_t len;
    if (end) {
        len = (size_t)(end - p) + 1;  // include the closing brace
    } else {
        len = strlen(p);
    }

    out_obj.assign(p, p + len);
    out_obj.push_back('\0');
    return true;
}

void httpPollerTask(void * arg) {
    static_cast<HttpPoller *>(arg)->run();
}


void HttpPoller::init(Wifi & wifi, QueueHandle_t result_queue) {
    m_wifi = &wifi;
    m_result_queue = result_queue;
    m_wake_sem = xSemaphoreCreateBinary();
    buildUrls();
}

void HttpPoller::buildUrls() {
    char ids[GAME_COUNT * 21 + 1] = {};
    size_t pos = 0;
    for (int i = 0; i < GAME_COUNT; i++) {
        const char * sep = (i == 0) ? "" : ",";
        pos += (size_t)snprintf(ids + pos, sizeof(ids) - pos, "%s%" PRId64, sep, GAME_UNIVERSE_IDS[i]);
    }
    snprintf(m_games_url, sizeof(m_games_url), "%s%s", GAMES_URL_BASE, ids);
    snprintf(m_votes_url, sizeof(m_votes_url), "%s%s", VOTES_URL_BASE, ids);
    lprintf(TAG, "Games URL: %s", m_games_url);
}

void HttpPoller::start() {
    xTaskCreate(httpPollerTask, "http_poll", 8 * 1024, this, 2, nullptr);
}

void HttpPoller::triggerNow() {
    m_force_fetch = true;
    xSemaphoreGive(m_wake_sem);
}

void HttpPoller::setPollIntervalSec(int seconds) {
    m_poll_interval_sec = seconds;
    // Wake the poller so the wait loop re-evaluates against the new interval
    xSemaphoreGive(m_wake_sem);
}

void HttpPoller::run() {
    lprintf(TAG, "HTTP poller task started, waiting for WiFi...");

    while (!m_wifi->waitForConnection(60000)) {
        eprintf(TAG, "WiFi not connected after 60s, still waiting...");
    }

    lprintf(TAG, "WiFi connected, syncing time via SNTP...");
    esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&sntp_cfg);
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) == ESP_OK) {
        m_time_synced = true;
        lprintf(TAG, "SNTP synced");
    } else {
        lprintf(TAG, "SNTP sync timed out — will retry each poll cycle");
    }

    while (true) {
        if (!m_time_synced && esp_netif_sntp_sync_wait(pdMS_TO_TICKS(2000)) == ESP_OK) {
            m_time_synced = true;
            lprintf(TAG, "SNTP synced");
        }

        HttpResult result = {};

        bool games_ok = fetchGames(result);
        bool votes_ok = fetchVotes(result);
        result.success = games_ok && votes_ok;

        xQueueOverwrite(m_result_queue, &result);
        m_force_fetch = false;
        xSemaphoreTake(m_wake_sem, 0);  // clear a wake that arrived mid-fetch

        // Sleep until the poll interval elapses, waking early on triggerNow()
        // or an interval change
        int64_t wait_start_us = esp_timer_get_time();
        while (!m_force_fetch) {
            int64_t elapsed_ms = (esp_timer_get_time() - wait_start_us) / 1000;
            int64_t remaining_ms = (int64_t)m_poll_interval_sec * 1000 - elapsed_ms;
            if (remaining_ms <= 0) {
                break;
            }
            xSemaphoreTake(m_wake_sem, pdMS_TO_TICKS(remaining_ms));
        }
    }
}

bool HttpPoller::fetchGames(HttpResult & result) {
    std::vector<char> body;
    if (!doGet(m_games_url, body)) {
        return false;
    }

    bool all_ok = true;
    std::vector<char> obj;
    for (int i = 0; i < GAME_COUNT; i++) {
        GameStats & g = result.games[i];
        if (!findGameObject(body.data(), GAME_UNIVERSE_IDS[i], obj)) {
            eprintf(TAG, "Games: universe %" PRId64 " not found in response", GAME_UNIVERSE_IDS[i]);
            all_ok = false;
            continue;
        }
        const char * data = obj.data();

        bool ok = true;
        ok &= extractInt(data, "\"playing\":", g.player_count);
        ok &= extractInt(data, "\"visits\":", g.visits);
        extractString(data, "\"name\":", g.game_name, sizeof(g.game_name));

        // Extract date from "updated":"YYYY-MM-DDT..."
        const char * upd = strstr(data, "\"updated\":\"");
        if (upd) {
            upd += strlen("\"updated\":\"");
            strncpy(g.game_updated, upd, 10);
            g.game_updated[10] = '\0';
        }

        g.success = ok;
        if (ok) {
            lprintf(TAG, "Games[%" PRId64 "]: %d players, %d visits, name=\"%s\"",
                    GAME_UNIVERSE_IDS[i], g.player_count, g.visits, g.game_name);
        } else {
            eprintf(TAG, "Games[%" PRId64 "]: parse failed", GAME_UNIVERSE_IDS[i]);
            all_ok = false;
        }
    }
    return all_ok;
}

bool HttpPoller::fetchVotes(HttpResult & result) {
    std::vector<char> body;
    if (!doGet(m_votes_url, body)) {
        return false;
    }

    bool all_ok = true;
    std::vector<char> obj;
    for (int i = 0; i < GAME_COUNT; i++) {
        GameStats & g = result.games[i];
        if (!findGameObject(body.data(), GAME_UNIVERSE_IDS[i], obj)) {
            eprintf(TAG, "Votes: universe %" PRId64 " not found in response", GAME_UNIVERSE_IDS[i]);
            all_ok = false;
            continue;
        }
        const char * data = obj.data();

        bool ok = true;
        ok &= extractInt(data, "\"upVotes\":", g.up_votes);
        ok &= extractInt(data, "\"downVotes\":", g.down_votes);

        g.votes_success = ok;
        if (ok) {
            lprintf(TAG, "Votes[%" PRId64 "]: %d up, %d down", GAME_UNIVERSE_IDS[i], g.up_votes, g.down_votes);
        } else {
            eprintf(TAG, "Votes[%" PRId64 "]: parse failed (obj: %.60s)", GAME_UNIVERSE_IDS[i], data);
            all_ok = false;
        }
    }
    return all_ok;
}
