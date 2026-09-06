#include "history/player_history.h"

#include <string.h>
#include <stdio.h>

#include "nvs.h"
#include "log.h"

static const char * TAG = "HIST";
static const char * NVS_NAMESPACE = "epaper";
// NVS keys are per slot: "ph<slot>_head", "ph<slot>_count", "ph<slot>_buf"
static void makeKey(char * buf, size_t buf_size, int slot, const char * suffix) {
    snprintf(buf, buf_size, "ph%d_%s", slot, suffix);
}

bool PlayerHistory::load(int slot) {
    m_slot = slot;

    char key_head[16];
    char key_count[16];
    char key_buf[16];
    makeKey(key_head, sizeof(key_head), m_slot, "head");
    makeKey(key_count, sizeof(key_count), m_slot, "count");
    makeKey(key_buf, sizeof(key_buf), m_slot, "buf");

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        lprintf(TAG, "Slot %d: no saved history", m_slot);
        return false;
    }
    if (err != ESP_OK) {
        eprintf(TAG, "nvs_open failed: %d", err);
        return false;
    }

    uint32_t saved_head = 0;
    uint32_t saved_count = 0;
    err = nvs_get_u32(handle, key_head, &saved_head);
    if (err != ESP_OK) {
        lprintf(TAG, "Slot %d: %s not found, starting fresh", m_slot, key_head);
        nvs_close(handle);
        return false;
    }
    err = nvs_get_u32(handle, key_count, &saved_count);
    if (err != ESP_OK) {
        lprintf(TAG, "Slot %d: %s not found, starting fresh", m_slot, key_count);
        nvs_close(handle);
        return false;
    }

    size_t buf_size = sizeof(m_buf);
    err = nvs_get_blob(handle, key_buf, m_buf, &buf_size);
    nvs_close(handle);

    if (err != ESP_OK) {
        eprintf(TAG, "nvs_get_blob failed: %d", err);
        return false;
    }

    if (saved_head >= (uint32_t)MAX_SAMPLES || saved_count > (uint32_t)MAX_SAMPLES) {
        eprintf(TAG, "Slot %d: corrupt NVS history, resetting", m_slot);
        memset(m_buf, 0, sizeof(m_buf));
        m_head = 0;
        m_count = 0;
        return false;
    }

    m_head = (int)saved_head;
    m_count = (int)saved_count;
    lprintf(TAG, "Slot %d: loaded %d history samples", m_slot, m_count);
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
    char key_head[16];
    char key_count[16];
    char key_buf[16];
    makeKey(key_head, sizeof(key_head), m_slot, "head");
    makeKey(key_count, sizeof(key_count), m_slot, "count");
    makeKey(key_buf, sizeof(key_buf), m_slot, "buf");

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        eprintf(TAG, "nvs_open failed: %d", err);
        return false;
    }

    err = nvs_set_u32(handle, key_head, (uint32_t)m_head);
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, key_count, (uint32_t)m_count);
    }
    if (err == ESP_OK) {
        err = nvs_set_blob(handle, key_buf, m_buf, sizeof(m_buf));
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
