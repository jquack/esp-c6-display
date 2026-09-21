# esp-c6-display

Firmware for the Waveshare **ESP32-C6-LCD-1.3** (ST7789V2, 240×240, WiFi 6).
Shows `Hello <3` and the connected WiFi SSID with a signal-strength icon.

## Architecture

| Module | Responsibility |
|---|---|
| `src/wifi_manager.{h,cpp}` | WiFi lifecycle, reconnect, RSSI. Knows nothing about the display. |
| `src/display_manager.{h,cpp}` | ST7789 driver + rendering. Takes plain values, knows nothing about WiFi. |
| `src/plane_gif_data.{h,cpp}` | Generated: `assets/plane.gif` as palette-indexed frames in flash. Do not edit by hand. |
| `tools/gif2header.py` | Regenerates `plane_gif_data.*` from a gif (needs Pillow). |
| `src/main.cpp` | Glue: instantiates both, pushes state changes to the display. |
| `include/data_source.h` | Abstract `IDataSource` + `DisplayPayload` — extensibility hook for a future API client. |

## First-time setup

```bash
# 1. Fill in WiFi password (file is gitignored)
$EDITOR include/wifi_config.h     # set WIFI_PASSWORD

# 2. Build
~/.platformio/penv/bin/pio run

# 3. Flash (board connected via USB-C)
~/.platformio/penv/bin/pio run -t upload

# 4. Watch serial
~/.platformio/penv/bin/pio device monitor
```

Add `~/.platformio/penv/bin` to your shell PATH if you want a plain `pio` command.

## Expected display

- Within ~1s of boot: `Hello <3` shows on screen.
- While joining: `Connecting...` underneath, in red.
- After join: `connected to:` + SSID + a 4-bar signal icon (green = active, gray = inactive).
- Disconnect → label flips back to `Connecting...`, reconnect logic retries every 5s.
- While the API returns `null` (no altitude): `no data` + the animated PH-TGC gif.
- Once the API returns a number: big altitude value in feet, colour by zone.

## No-data animation (the gif)

`assets/plane.gif` (240×180, 6 steps, 130 ms each) is baked into flash as an
8-bit palette image by `tools/gif2header.py`. The script stores the first frame
in full, then only the rectangle that changes between frames (the propeller,
88×79 px) once per *unique* frame — this gif has 3 unique frames, so the whole
thing costs ~64 KB of flash and each tick redraws ~7k pixels, not the full
43k. No RAM is used: `drawIndexedBitmap` reads straight from memory-mapped flash.

To swap the gif, keep it ≤240 px wide, ≤204 px tall and ≤256 colours, then:

```bash
pip install pillow
python3 tools/gif2header.py assets/plane.gif src/plane_gif_data PLANE
```

The firmware uses `huge_app.csv` (3 MB app, no OTA) because the WiFi+TLS build
alone is ~1.2 MB, which nearly fills the 1.25 MB app slot in `default.csv`.

## LCD pin map (internal, on the board — for reference)

```
MOSI = GPIO6   SCK = GPIO7   CS = GPIO14
DC   = GPIO15  RST = GPIO21  BL = GPIO22
```

These pins are NOT broken out on the header — they're hard-wired to the ST7789.
Available GPIOs on the header: 1, 2, 3, 12, 13, 16, 17, 20, 23.

## Extending with an API later

Implement `IDataSource` (see `include/data_source.h`):

```cpp
class WeatherDataSource : public IDataSource {
  DisplayPayload fetch() override {
    // HTTP GET, parse JSON, return payload
  }
};
```

The display manager can grow a generic `render(const DisplayPayload&)` method to consume any source — WiFi status, weather, sensor readings, whatever.

## Tools

- PlatformIO Core 6.x (`~/.platformio/penv/bin/pio`)
- `lib_deps`: `moononournation/GFX Library for Arduino`
