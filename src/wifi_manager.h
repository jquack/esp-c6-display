#pragma once

#include <Arduino.h>

class WifiManager {
public:
  WifiManager();
  void   begin(const char* ssid, const char* password);
  void   loop();
  bool   isConnected() const;
  String getSSID() const;
  int    getRSSI() const;

private:
  const char* _ssid;
  const char* _password;
  unsigned long _lastReconnectAttemptMs;
  bool _lastConnected;
};
