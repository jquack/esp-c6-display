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
  void tickNoDataAnimation(uint32_t frame);

  static int rssiToBars(int rssi);

private:
  Arduino_DataBus* _bus;
  Arduino_GFX*     _tft;

  // No-data animation state: base frame is drawn once, then only the
  // propeller patch is redrawn when it changes.
  bool _animationInitialized = false;
  int  _lastPatch            = -1;

  void drawSignalIcon(int x, int y, int bars);
};
