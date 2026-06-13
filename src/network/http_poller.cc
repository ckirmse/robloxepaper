#include "network/http_poller.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_netif_sntp.h"

#include "log.h"

static const char * TAG = "HTTP";

static constexpr const char * GAMES_URL =
    "https://games.roblox.com/v1/games?universeIds=9786190497";
static constexpr const char * VOTES_URL =
    "https://games.roblox.com/v1/games/votes?universeIds=9786190497";

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
    if (!p) return false;
    p += strlen(key);
    while (*p == ' ' || *p == '\t') p++;
    char * end = nullptr;
    long val = strtol(p, &end, 10);
    if (end == p) return false;
    out = (int32_t)val;
    return true;
}

// Extract quoted string value after key (e.g. "\"name\":\"").
// Copies into buf, stripping non-printable ASCII, '[', and ']', then trims spaces.
static bool extractString(const char * body, const char * key, char * buf, size_t buf_size) {
    const char * p = strstr(body, key);
    if (!p) return false;
    p += strlen(key);
    // skip opening quote
    if (*p != '"') return false;
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
    while (*start == ' ') start++;
    if (start != buf) memmove(buf, start, strlen(start) + 1);

    // trim trailing spaces
    int len = (int)strlen(buf);
    while (len > 0 && buf[len - 1] == ' ') buf[--len] = '\0';

    return true;
}

void httpPollerTask(void * arg) {
    static_cast<HttpPoller *>(arg)->run();
}


void HttpPoller::init(Wifi & wifi, QueueHandle_t result_queue) {
    m_wifi = &wifi;
    m_result_queue = result_queue;
}

void HttpPoller::start() {
    xTaskCreate(httpPollerTask, "http_poll", 8 * 1024, this, 2, nullptr);
}

void HttpPoller::triggerNow() {
    m_force_fetch = true;
}

void HttpPoller::setPollIntervalSec(int seconds) {
    m_poll_interval_sec = seconds;
}

void HttpPoller::run() {
    lprintf(TAG, "HTTP poller task started, waiting for WiFi...");

    if (!m_wifi->waitForConnection(60000)) {
        eprintf(TAG, "WiFi timeout — HTTP poller exiting");
        vTaskDelete(nullptr);
        return;
    }

    lprintf(TAG, "WiFi connected, syncing time via SNTP...");
    esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&sntp_cfg);
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK) {
        lprintf(TAG, "SNTP sync timed out — times will show 00:00");
    } else {
        lprintf(TAG, "SNTP synced");
    }

    while (true) {
        HttpResult result = {};

        result.success = fetchGames(result);
        result.votes_success = fetchVotes(result);

        xQueueOverwrite(m_result_queue, &result);
        m_force_fetch = false;

        int waited_ms = 0;
        while (waited_ms < m_poll_interval_sec * 1000) {
            vTaskDelay(pdMS_TO_TICKS(100));
            waited_ms += 100;
            if (m_force_fetch) break;
        }
    }
}

bool HttpPoller::fetchGames(HttpResult & result) {
    std::vector<char> body;
    if (!doGet(GAMES_URL, body)) return false;

    const char * data = body.data();

    bool ok = true;
    ok &= extractInt(data, "\"playing\":", result.player_count);
    ok &= extractInt(data, "\"visits\":", result.visits);
    extractString(data, "\"name\":", result.game_name, sizeof(result.game_name));

    // Extract date from "updated":"YYYY-MM-DDT..."
    const char * upd = strstr(data, "\"updated\":\"");
    if (upd) {
        upd += strlen("\"updated\":\"");
        strncpy(result.game_updated, upd, 10);
        result.game_updated[10] = '\0';
    }

    if (ok) {
        lprintf(TAG, "Games: %d players, %d visits, name=\"%s\"",
                result.player_count, result.visits, result.game_name);
    }
    return ok;
}

bool HttpPoller::fetchVotes(HttpResult & result) {
    std::vector<char> body;
    if (!doGet(VOTES_URL, body)) return false;

    const char * data = body.data();
    bool ok = true;
    ok &= extractInt(data, "\"upVotes\":", result.up_votes);
    ok &= extractInt(data, "\"downVotes\":", result.down_votes);

    if (ok) {
        lprintf(TAG, "Votes: %d up, %d down", result.up_votes, result.down_votes);
    } else {
        eprintf(TAG, "Votes parse failed (body: %.60s)", body.data());
    }
    return ok;
}
