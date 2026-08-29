# CLAUDE.md

This file provides guidance to Claude Code when working with code in this repository.

## Project Overview

ESP-IDF C++ firmware for the CrowPanel ESP32-S3 4.2" e-ink display (SSD1683, 400×300 monochrome). Fetches live Roblox game stats from two API endpoints and renders a dashboard on the e-paper display using LVGL.

## Build & Flash

ESP-IDF v6.1 is installed with the ESP-IDF Installation Manager (eim, https://docs.espressif.com/projects/idf-im-ui/) under `~/.espressif/`. `tools/idf_env.sh` activates it (and pins the version tag); never source `~/esp/esp-idf/export.sh`.

```bash
. tools/idf_env.sh
idf.py build
idf.py -p /dev/cu.wchusbserial1420 flash
idf.py -p /dev/cu.wchusbserial1420 monitor
```

WiFi credentials are set via `idf.py menuconfig` → "Epaper App Config". Run once, then rebuild.

No tests or linter are configured.

## Hardware

**Board:** CrowPanel ESP32-S3-WROOM-1-N8R8 (8MB flash, 8MB PSRAM)  
**Display:** SSD1683, 400×300, 1-bit monochrome, bit-bang SPI

| Signal | GPIO |
|--------|------|
| SCK    | 12   |
| MOSI   | 11   |
| RST    | 47   |
| DC     | 46   |
| CS     | 45   |
| BUSY   | 48   |
| Display power enable | 7 |
| System power enable  | 41 |

**Buttons** (all active-LOW, 10K pull-up):

| Button | GPIO | Action |
|--------|------|--------|
| HOME   | 2    | Force immediate fetch (also deep-sleep wakeup) |
| EXIT   | 1    | Enter deep sleep |
| UP     | 6    | Increase poll interval |
| DOWN   | 4    | Decrease poll interval |
| OK     | 5    | Force full e-paper refresh (clears ghosting) |

**Serial port:** `/dev/cu.wchusbserial1420` (CH34x USB-UART bridge)

## Architecture

### Source Layout

```
main/
  main.cc              # app_main → 16KB FreeRTOS task → App::init/poll loop
  Kconfig.projbuild    # WiFi SSID/password via menuconfig

src/
  app.h / app.cc       # Top-level App class; owns all subsystems; poll loop
  log.h                # dprintf/lprintf/eprintf macros wrapping ESP_LOG*
  display/
    epaper.h/cc        # SSD1683 low-level driver (GPIO bit-bang SPI)
    lvgl_display.h/cc  # LVGL init; RGB565→1-bit flush callback; PSRAM draw buf
  network/
    wifi.h/cc          # WiFi station; semaphore-based ready signal
    http_poller.h/cc   # FreeRTOS task; two HTTP GETs per cycle; SNTP init
  input/
    buttons.h/cc       # GPIO ISR with 50ms debounce; posts to queue
  screens/
    player_count.h/cc  # LVGL screen layout and all setter methods
  fonts/               # lv_font_conv-generated C files (checked in)
    lv_font_press_start_2p_80.c
    lv_font_builder_sans_semibold_16.c
    lv_font_builder_sans_semibold_20.c
    lv_font_builder_sans_semibold_28.c
```

### Key Design Decisions

**LVGL + e-paper:** LVGL renders in 16-bit RGB565 into a 240KB PSRAM buffer. The flush callback thresholds each pixel at `>= 0x8000` (white) or below (black) and packs the result into a 15KB 1-bit buffer, which is then written to the SSD1683. Using LVGL's I1 display format instead would require `LV_COLOR_DEPTH=1` globally, which LVGL v9 does not support — so the RGB565+conversion approach is correct and intentional. `lv_tick_set_cb` uses `esp_timer_get_time()`. The flush callback blocks until the e-paper refresh completes (~1s fast, ~4s full).

**E-paper refresh modes:** First display after boot uses `fullRefresh()` (~4s) to clear any ghost image. All subsequent updates use `fastRefresh()` (~1s). Pressing OK triggers `requestFullRefresh()` to force the next update to be a full refresh.

**HTTP polling:** `HttpPoller` runs in a dedicated FreeRTOS task. On startup it waits for WiFi (up to 60s), then syncs time via SNTP (`pool.ntp.org`). Each poll cycle does two sequential GETs:
1. `https://games.roblox.com/v1/games?universeIds=9786190497` → name, playing, visits, updated
2. `https://games.roblox.com/v1/games/votes?universeIds=9786190497` → upVotes, downVotes

JSON is parsed with `strstr`/`strtol` (no JSON library). `HttpResult` is a POD struct passed via `xQueueOverwrite`.

**Fonts:** Generated with `npx lv_font_conv` from source font files in `~/fonts/` and `~/Roblox-Builder-Fonts/`. The generated `.c` files are checked into `src/fonts/`. To regenerate:

```bash
npx lv_font_conv --font ~/fonts/PressStart2P-Regular.ttf \
  --size 80 --bpp 1 --format lvgl --no-compress -r 0x20-0x7E \
  -o src/fonts/lv_font_press_start_2p_80.c --force-fast-kern-format

npx lv_font_conv --font ~/Roblox-Builder-Fonts/fonts/BuilderSans/BuilderSans-SemiBold-600.otf \
  --size 28 --bpp 1 --format lvgl --no-compress -r 0x20-0x7E \
  -o src/fonts/lv_font_builder_sans_semibold_28.c --force-fast-kern-format

# Repeat for sizes 20 and 16
```

**Timezone:** Set to `PST8PDT,M3.2.0,M11.1.0` (US Pacific with DST) via `setenv`/`tzset` in `App::init()`. Fetch timestamps display as PDT.

**TLS:** Uses `esp_crt_bundle_attach` with `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y`. `games.roblox.com` sends its full certificate chain so no intermediate certs need to be embedded.

### Screen Layout (400×300)

```
y=0
  [Game name — BuilderSans SemiBold 28px, centered]          y=10
──────────────────────────────────────────────────────────── y=54
                                                             
              [Player count — PressStart2P 80px]             y=92
              [players online — BuilderSans 16px]             y=182
                                                             
──────────────────────────────────────────────────────────── y=210
  [X,XXX,XXX visits]              [X,XXX likes (NN%)]        y=225
──────────────────────────────────────────────────────────── y=252
  [Game Updated: Mmm DD YYYY]     [As of HH:MM PDT]          y=267
y=300
```

Error icon `[!]` (26×26, top-right corner) is hidden when all is well, shown on WiFi or HTTP failure.

## Coding Style

Follows conventions from `~/cic`:
- `#pragma once` headers
- Classes: `PascalCase`; methods: `camelCase`; members: `m_snake_case`
- No exceptions; `bool` return for errors; log via `lprintf`/`eprintf`/`dprintf`
- No alignment padding — one space before `=` in assignments; do not pad to align right-hand sides
- Delete copy/assign in all classes
- `.cc` extension for implementation files
- `using namespace std` in `.cc` files only
- Never put more than one statement on one line
- Always put braces around bodies of `if`, `else`, `for`, and `while` statements
- Use `PRId32`/`PRIu32` in printf format strings for `int32_t`/`uint32_t` values
