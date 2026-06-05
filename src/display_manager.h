#pragma once

#include <Arduino.h>

class Arduino_DataBus;
class Arduino_GFX;

class DisplayManager {
public:
  DisplayManager();
  void begin();
  void drawTopBar(const String& ssid, int rssi, bool connected);
  void drawBigValue(int value);
  void tickCryingAirplane(uint32_t frame);

  static int rssiToBars(int rssi);

private:
  Arduino_DataBus* _bus;
  Arduino_GFX*     _tft;

  // Animation state — used to skip full-frame clears between color cycles.
  bool     _animationInitialized = false;
  uint32_t _lastColorCycle       = 0;

  void drawSignalIcon(int x, int y, int bars);
};
