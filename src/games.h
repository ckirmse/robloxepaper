#pragma once

#include <stdint.h>

// The single list of Roblox universes tracked by this device.
// Everything else (HTTP URLs, result arrays, history slots) derives from it.
static constexpr int64_t GAME_UNIVERSE_IDS[] = {
    9786190497,
    10498907331,
};

static constexpr int GAME_COUNT = sizeof(GAME_UNIVERSE_IDS) / sizeof(GAME_UNIVERSE_IDS[0]);

// How long each game is shown before the display rotates to the next one
static constexpr int GAME_ROTATE_SEC = 30;
