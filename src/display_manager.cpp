#include "display_manager.h"

#include <Arduino_GFX_Library.h>
#include <math.h>

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

// HSV (h:0..359, s:0..255, v:0..255) → RGB565
static uint16_t hsv565(int h, uint8_t s, uint8_t v) {
  h = ((h % 360) + 360) % 360;
  uint8_t region    = h / 60;
  uint8_t remainder = (h - region * 60) * 255 / 60;
  uint16_t p = (v * (255 - s)) / 255;
  uint16_t q = (v * (255 - (s * remainder) / 255)) / 255;
  uint16_t t = (v * (255 - (s * (255 - remainder)) / 255)) / 255;
  uint8_t r, g, b;
  switch (region) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
  }
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void DisplayManager::tickCryingAirplane(uint32_t frame) {
  // Cessna 182 side view, facing right.
  // De-flickered design:
  //   - Static plane drawn ONCE per color cycle (every COLOR_CYCLE_FRAMES).
  //   - Per-frame updates: only the prop disc and the tear column.
  //   - No bob (bob would require full per-frame redraw).

  const int mainH  = SCR_H - MAIN_Y;
  const int cx     = SCR_W / 2;

  // Reserve a strip at the top of the main area for the "no data" label.
  const int labelH    = 30;
  const int planeAreaY = MAIN_Y + labelH;
  const int planeAreaH = SCR_H - planeAreaY;
  const int cy         = planeAreaY + planeAreaH / 2 + 4;

  // Slow color cycle: change hue once per cycle (~6s at 10fps).
  constexpr uint32_t COLOR_CYCLE_FRAMES = 60;
  const uint32_t cycleIdx = frame / COLOR_CYCLE_FRAMES;
  const bool colorChanged = (cycleIdx != _lastColorCycle);
  const bool fullRedraw   = !_animationInitialized || colorChanged;
  _animationInitialized = true;
  _lastColorCycle       = cycleIdx;

  const int hue = (int)(cycleIdx * 30) % 360;
  uint16_t fuselageC = hsv565(hue,             230, 240);
  uint16_t wingC     = hsv565(hue + 120,       230, 240);
  uint16_t tailC     = hsv565(hue +  60,       230, 240);
  uint16_t accentC   = hsv565(hue + 200,       255, 255);
  uint16_t propC     = hsv565(hue + 300,       200, 255);
  const uint16_t windowC = 0x18C3;             // dark glass
  const uint16_t strutC  = 0xCE59;             // light grey
  const uint16_t tireC   = 0x0000;             // black

  if (fullRedraw) {
    _tft->fillRect(0, MAIN_Y, SCR_W, mainH, BG_COLOR);

    // ----- "no data" label at top of main area -----
    const char* label = "no data";
    const int textSize = 3;                   // 18x24 px chars
    const int textW = (int)strlen(label) * 6 * textSize;
    const int textH = 8 * textSize;
    const int textX = (SCR_W - textW) / 2;
    const int textY = MAIN_Y + (labelH - textH) / 2;
    _tft->setTextSize(textSize);
    _tft->setTextColor(accentC, BG_COLOR);
    _tft->setCursor(textX, textY);
    _tft->print(label);
  }

  // Eye position (used by both static face draw and per-frame tear stream).
  const int eyeX = cx + 15;
  const int eyeY = cy - 6;

  if (fullRedraw) {
  // ----- Tail (back / left) -----
  // Vertical fin
  _tft->fillTriangle(cx - 95, cy -  6,
                     cx - 75, cy - 38,
                     cx - 65, cy -  6,
                     tailC);
  // Horizontal stabilizer (slim)
  _tft->fillRoundRect(cx - 100, cy - 12, 32, 6, 2, tailC);

  // ----- Fuselage -----
  // Tail boom (slim left half)
  _tft->fillRoundRect(cx - 85, cy - 6, 70, 16, 6, fuselageC);
  // Cabin (taller, where pilot sits)
  _tft->fillRoundRect(cx - 25, cy - 14, 75, 28, 10, fuselageC);
  // Engine cowling (right end)
  _tft->fillRoundRect(cx + 45, cy - 12, 35, 26, 8, fuselageC);

  // ----- High wing (above fuselage, full span) -----
  _tft->fillRoundRect(cx - 105, cy - 26, 200, 9, 3, wingC);
  // Pitot/wing tip hints
  _tft->drawPixel(cx + 94,  cy - 22, accentC);
  _tft->drawPixel(cx - 104, cy - 22, accentC);

  // Wing strut (diagonal V from wing bottom to fuselage bottom)
  for (int dx = -1; dx <= 1; dx++) {
    _tft->drawLine(cx + 12 + dx, cy - 17,
                   cx -  8 + dx, cy + 12, strutC);
  }

  // ----- Cockpit window (where the eye is) -----
  // Slanted trapezoid following Cessna's windshield rake
  _tft->fillTriangle(cx - 18, cy - 13, cx + 30, cy - 13, cx + 36, cy - 4, windowC);
  _tft->fillRect    (cx - 18, cy - 13, 54, 10, windowC);
  // window frame highlight
  _tft->drawLine(cx + 30, cy - 13, cx + 36, cy - 4, accentC);

  // ----- Sad pilot eye inside cockpit -----
  _tft->fillCircle(eyeX, eyeY, 6, 0xFFFF);
  _tft->fillCircle(eyeX - 2, eyeY + 2, 3, 0x0000);  // pupil drooped (sad)
  _tft->drawPixel(eyeX - 3, eyeY, 0xFFFF);          // catchlight

  // Sad eyebrow (red, slanted)
  _tft->drawLine(eyeX - 8, eyeY - 8, eyeX + 6, eyeY - 5, RED_COLOR);
  _tft->drawLine(eyeX - 8, eyeY - 9, eyeX + 6, eyeY - 6, RED_COLOR);

  // Frown on the cowling/cheek below window
  int mx = eyeX - 4;
  int my = cy + 6;
  _tft->drawLine(mx,     my + 2, mx + 5,  my,     RED_COLOR);
  _tft->drawLine(mx + 5, my,     mx + 12, my,     RED_COLOR);
  _tft->drawLine(mx + 12,my,     mx + 17, my + 2, RED_COLOR);

  // ----- Tricycle landing gear (static) -----
  {
    int gearTopY  = cy + 14;
    int wheelY    = cy + 36;
    int mainX = cx + 5;
    for (int dx = -1; dx <= 0; dx++) {
      _tft->drawLine(mainX + dx, gearTopY, mainX + dx, wheelY - 4, strutC);
    }
    _tft->fillCircle(mainX, wheelY, 5, tireC);
    _tft->drawCircle(mainX, wheelY, 5, accentC);
    int noseX = cx + 56;
    for (int dx = -1; dx <= 0; dx++) {
      _tft->drawLine(noseX + dx, gearTopY, noseX + dx, wheelY - 4, strutC);
    }
    _tft->fillCircle(noseX, wheelY, 5, tireC);
    _tft->drawCircle(noseX, wheelY, 5, accentC);
  }
  }  // end if (fullRedraw)

  // ----- Propeller spinning on the nose (per-frame) -----
  int propX = cx + 82;
  int propY = cy + 1;
  // Clear prop disc — looks like motion-blurred prop background.
  _tft->fillCircle(propX, propY, 19, BG_COLOR);
  _tft->fillCircle(propX, propY, 3, accentC);       // hub
  // Two-blade prop, rotating through 4 phases
  switch ((int)(frame % 4)) {
    case 0:
      _tft->drawLine(propX, propY - 18, propX, propY + 18, propC);
      _tft->drawLine(propX + 1, propY - 18, propX + 1, propY + 18, propC);
      break;
    case 1:
      _tft->drawLine(propX - 13, propY - 13, propX + 13, propY + 13, propC);
      _tft->drawLine(propX - 12, propY - 14, propX + 14, propY + 12, propC);
      break;
    case 2:
      _tft->drawLine(propX - 18, propY, propX + 18, propY, propC);
      _tft->drawLine(propX - 18, propY + 1, propX + 18, propY + 1, propC);
      break;
    case 3:
      _tft->drawLine(propX - 13, propY + 13, propX + 13, propY - 13, propC);
      _tft->drawLine(propX - 14, propY + 12, propX + 12, propY - 14, propC);
      break;
  }

  // ----- Tears falling from cockpit (per-frame) -----
  const uint16_t tearC    = 0x5DFF;
  const uint16_t tearHigh = 0x07FF;
  const int tearOriginX = eyeX - 3;
  const int tearOriginY = cy + 12;
  // Clear the vertical strip the tears occupy so old tear pixels disappear.
  _tft->fillRect(tearOriginX - 6, tearOriginY, 14, SCR_H - tearOriginY, BG_COLOR);
  for (int i = 0; i < 5; i++) {
    int span  = 80;
    int phase = (frame * 4 + i * 16) % span;
    int tx    = tearOriginX + ((i & 1) ? 3 : -3);
    int ty    = tearOriginY + phase * 2;
    if (ty > SCR_H - 5) continue;
    _tft->fillCircle(tx, ty,     2, tearC);
    _tft->fillCircle(tx, ty - 3, 1, tearC);
    _tft->drawPixel(tx - 1, ty - 1, tearHigh);
  }
}

int DisplayManager::rssiToBars(int rssi) {
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}
