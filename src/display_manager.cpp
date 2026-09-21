#include "display_manager.h"
#include "plane_gif_data.h"

#include <Arduino_GFX_Library.h>
#include <string.h>

// ST7789 1.3" 240x240 — Waveshare ESP32-C6-LCD-1.3
// Pins inferred from datasheet (these are NOT broken out, used internally).
#define LCD_MOSI 6
#define LCD_SCLK 7
#define LCD_CS   14
#define LCD_DC   15
#define LCD_RST  21
#define LCD_BL   22

// Orientation: 1 = rotated 90° CW (right-bound). If text is upside-down or
// mirrored, swap to 3.
#define LCD_ROTATION 1

#define BG_COLOR         0x0000  // black
#define PINK_COLOR       0xFA9C  // #fc51e0
#define RED_COLOR        0xF800
#define GREEN_COLOR      0x07E0
#define LABEL_COLOR      0xFFFF  // white
#define DISCONNECT_COLOR 0xF800  // red
#define DIVIDER_COLOR    0x4208  // dim gray
#define SIGNAL_ON_COLOR  0x07E0  // green
#define SIGNAL_OFF_COLOR 0x4208  // dim gray

// Altitude zones (ft)
static uint16_t colorForValue(int v) {
  if (v < 3500)  return RED_COLOR;
  if (v <= 5500) return GREEN_COLOR;
  return PINK_COLOR;
}

// 240x240 layout, post-rotation
static constexpr int SCR_W      = 240;
static constexpr int SCR_H      = 240;
static constexpr int TOP_H      = 32;        // top status bar
static constexpr int MAIN_Y     = TOP_H + 4; // value area starts here
static constexpr int MAIN_H     = SCR_H - MAIN_Y;

DisplayManager::DisplayManager() : _bus(nullptr), _tft(nullptr) {}

void DisplayManager::begin() {
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  _bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, GFX_NOT_DEFINED);
  _tft = new Arduino_ST7789(_bus, LCD_RST, LCD_ROTATION, true /*IPS*/, 240, 240, 0, 0, 0, 0);

  _tft->begin();
  _tft->fillScreen(BG_COLOR);
  _tft->drawFastHLine(0, TOP_H, SCR_W, DIVIDER_COLOR);
}

void DisplayManager::drawTopBar(const String& ssid, int rssi, bool connected) {
  // Clear top bar area
  _tft->fillRect(0, 0, SCR_W, TOP_H, BG_COLOR);
  _tft->drawFastHLine(0, TOP_H, SCR_W, DIVIDER_COLOR);

  _tft->setTextSize(2);  // 12x16 px chars

  if (!connected) {
    _tft->setTextColor(DISCONNECT_COLOR, BG_COLOR);
    _tft->setCursor(6, 8);
    _tft->print("Connecting...");
    return;
  }

  // Signal icon on the right
  const int iconW = 26;
  const int iconX = SCR_W - iconW - 6;
  drawSignalIcon(iconX, 7, rssiToBars(rssi));

  // SSID on the left, truncated to fit
  _tft->setTextColor(PINK_COLOR, BG_COLOR);
  const int ssidMaxW = iconX - 12;          // pixel budget for SSID
  const int charW = 12;                     // at text size 2
  int maxChars = ssidMaxW / charW;
  String shown = ssid;
  if ((int)shown.length() > maxChars && maxChars > 1) {
    shown = shown.substring(0, maxChars - 1) + "~";
  }
  _tft->setCursor(6, 8);
  _tft->print(shown);
}

void DisplayManager::drawBigValue(int value) {
  _tft->fillRect(0, MAIN_Y, SCR_W, MAIN_H, BG_COLOR);
  _animationInitialized = false;  // next animation must do a full redraw

  String num = String(value);
  const int numLen = num.length();

  // Small "feet" label on second row.
  const String feetStr = "feet";
  const int feetSize  = 2;                              // 12 x 16 px chars
  const int feetH     = 8 * feetSize;
  const int feetW     = (int)feetStr.length() * 6 * feetSize;

  // Fit the number as large as possible given the remaining vertical room.
  const int paddingX = 12;
  const int gap      = 10;
  const int availW   = SCR_W  - paddingX;
  const int availH   = MAIN_H - feetH - gap - 12;       // 12 = breathing room
  int sizeByW = availW / (numLen * 6);
  int sizeByH = availH / 8;
  int numSize = min(sizeByW, sizeByH);
  if (numSize < 2)  numSize = 2;
  if (numSize > 10) numSize = 10;

  const int numW = numLen * 6 * numSize;
  const int numH = 8 * numSize;

  // Stack number + gap + feet, centered vertically in main area.
  const int totalH = numH + gap + feetH;
  const int blockY = MAIN_Y + (MAIN_H - totalH) / 2;

  const uint16_t color = colorForValue(value);

  _tft->setTextSize(numSize);
  _tft->setTextColor(color, BG_COLOR);
  _tft->setCursor((SCR_W - numW) / 2, blockY);
  _tft->print(num);

  // "feet" right-aligned with a small right margin.
  _tft->setTextSize(feetSize);
  _tft->setCursor(SCR_W - feetW - 8, blockY + numH + gap);
  _tft->print(feetStr);
}

void DisplayManager::drawSignalIcon(int x, int y, int bars) {
  // 4 vertical bars of increasing height. Footprint ~26x18.
  const int barW   = 4;
  const int barGap = 2;
  const int baseY  = y + 18;

  for (int i = 0; i < 4; i++) {
    int h = 4 + i * 4;
    int bx = x + i * (barW + barGap);
    int by = baseY - h;
    uint16_t c = (i < bars) ? SIGNAL_ON_COLOR : SIGNAL_OFF_COLOR;
    _tft->fillRect(bx, by, barW, h, c);
  }
}

// "No data" state: the PH-TGC gif (assets/plane.gif), baked into flash by
// tools/gif2header.py as an 8-bit palette image. The full frame is drawn once;
// after that each tick only re-draws the propeller rectangle from a pre-cut
// patch, so there is no flicker and a tick costs ~7k pixels of SPI traffic
// instead of ~43k.
void DisplayManager::tickNoDataAnimation(uint32_t frame) {
  const int gifX = (SCR_W - PLANE_W) / 2;
  const int gifY = SCR_H - PLANE_H;  // bottom-aligned; label strip above
  uint16_t* palette = const_cast<uint16_t*>(PLANE_PALETTE);

  if (!_animationInitialized) {
    _animationInitialized = true;
    _tft->fillRect(0, MAIN_Y, SCR_W, MAIN_H, BG_COLOR);

    // "no data" label centred in the strip between the top bar and the gif.
    const char* label = "no data";
    const int textSize = 2;  // 12x16 px chars
    const int textW = (int)strlen(label) * 6 * textSize;
    const int textH = 8 * textSize;
    _tft->setTextSize(textSize);
    _tft->setTextColor(PINK_COLOR, BG_COLOR);
    _tft->setCursor((SCR_W - textW) / 2, MAIN_Y + (gifY - MAIN_Y - textH) / 2);
    _tft->print(label);

    _tft->drawIndexedBitmap(gifX, gifY, const_cast<uint8_t*>(PLANE_BASE),
                            palette, PLANE_W, PLANE_H);
    _lastPatch = 0;  // the base frame already contains patch 0
  }

  const int patch = PLANE_SEQ[frame % PLANE_SEQ_LEN];
  if (patch == _lastPatch) return;
  _lastPatch = patch;
  _tft->drawIndexedBitmap(gifX + PLANE_PATCH_X, gifY + PLANE_PATCH_Y,
                          const_cast<uint8_t*>(PLANE_PATCH[patch]), palette,
                          PLANE_PATCH_W, PLANE_PATCH_H);
}

int DisplayManager::rssiToBars(int rssi) {
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}
