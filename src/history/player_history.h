#pragma once

#include <stdint.h>

struct HistorySample {
    int32_t count;
    int32_t timestamp;
};

class PlayerHistory {
public:
    static const int MAX_SAMPLES = 60;

    PlayerHistory() = default;
    ~PlayerHistory() = default;

    PlayerHistory(const PlayerHistory &) = delete;
    PlayerHistory & operator=(const PlayerHistory &) = delete;

    // Load ring buffer from NVS. Returns false if no saved data (first boot) or on error.
    bool load();

    // Append a sample and persist to NVS. Returns false on NVS write error.
    bool push(int32_t count, int32_t timestamp);

    // Number of valid samples (0 to MAX_SAMPLES).
    int count() const { return m_count; }

    // Access samples in chronological order: index 0 = oldest, count()-1 = newest.
    const HistorySample & at(int index) const;

    // Peak player count across all valid samples, or 0 if empty.
    int32_t peak() const;

private:
    HistorySample m_buf[MAX_SAMPLES] = {};
    int m_head = 0;
    int m_count = 0;

    bool saveToNvs() const;
};
