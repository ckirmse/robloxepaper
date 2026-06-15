#include "history/player_history.h"

#include <string.h>

#include "nvs.h"
#include "log.h"

static const char * TAG = "HIST";
static const char * NVS_NAMESPACE = "epaper";
static const char * NVS_KEY_HEAD = "ph_head";
static const char * NVS_KEY_COUNT = "ph_count";
static const char * NVS_KEY_BUF = "ph_buf";

bool PlayerHistory::load() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        lprintf(TAG, "No saved history");
        return false;
    }
    if (err != ESP_OK) {
        eprintf(TAG, "nvs_open failed: %d", err);
        return false;
    }

    uint32_t saved_head = 0;
    uint32_t saved_count = 0;
    err = nvs_get_u32(handle, NVS_KEY_HEAD, &saved_head);
    if (err != ESP_OK) {
        lprintf(TAG, "ph_head not found, starting fresh");
        nvs_close(handle);
        return false;
    }
    err = nvs_get_u32(handle, NVS_KEY_COUNT, &saved_count);
    if (err != ESP_OK) {
        lprintf(TAG, "ph_count not found, starting fresh");
        nvs_close(handle);
        return false;
    }

    size_t buf_size = sizeof(m_buf);
    err = nvs_get_blob(handle, NVS_KEY_BUF, m_buf, &buf_size);
    nvs_close(handle);

    if (err != ESP_OK) {
        eprintf(TAG, "nvs_get_blob failed: %d", err);
        return false;
    }

    if (saved_head >= (uint32_t)MAX_SAMPLES || saved_count > (uint32_t)MAX_SAMPLES) {
        eprintf(TAG, "Corrupt NVS history, resetting");
        memset(m_buf, 0, sizeof(m_buf));
        m_head = 0;
        m_count = 0;
        return false;
    }

    m_head = (int)saved_head;
    m_count = (int)saved_count;
    lprintf(TAG, "Loaded %d history samples", m_count);
    return true;
}

bool PlayerHistory::push(int32_t count, int32_t timestamp) {
    m_buf[m_head] = {count, timestamp};
    m_head = (m_head + 1) % MAX_SAMPLES;
    if (m_count < MAX_SAMPLES) {
        m_count++;
    }
    return saveToNvs();
}

const HistorySample & PlayerHistory::at(int index) const {
    int physical = (m_head - m_count + index + MAX_SAMPLES) % MAX_SAMPLES;
    return m_buf[physical];
}

int32_t PlayerHistory::peak() const {
    if (m_count == 0) {
        return 0;
    }
    int32_t max_count = 0;
    for (int i = 0; i < m_count; i++) {
        if (at(i).count > max_count) {
            max_count = at(i).count;
        }
    }
    return max_count;
}

bool PlayerHistory::saveToNvs() const {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        eprintf(TAG, "nvs_open failed: %d", err);
        return false;
    }

    err = nvs_set_u32(handle, NVS_KEY_HEAD, (uint32_t)m_head);
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, NVS_KEY_COUNT, (uint32_t)m_count);
    }
    if (err == ESP_OK) {
        err = nvs_set_blob(handle, NVS_KEY_BUF, m_buf, sizeof(m_buf));
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        eprintf(TAG, "NVS save failed: %d", err);
        return false;
    }
    return true;
}
