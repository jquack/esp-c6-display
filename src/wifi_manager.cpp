#include "wifi_manager.h"

#include <WiFi.h>

static constexpr unsigned long RECONNECT_INTERVAL_MS = 5000;

WifiManager::WifiManager()
    : _ssid(nullptr), _password(nullptr),
      _lastReconnectAttemptMs(0), _lastConnected(false) {}

void WifiManager::begin(const char* ssid, const char* password) {
  _ssid = ssid;
  _password = password;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(_ssid, _password);

  Serial.print("[wifi] connecting to ");
  Serial.println(_ssid);
}

void WifiManager::loop() {
  bool conn = isConnected();

  if (conn != _lastConnected) {
    if (conn) {
      Serial.print("[wifi] connected, IP=");
      Serial.print(WiFi.localIP());
      Serial.print(" RSSI=");
      Serial.println(WiFi.RSSI());
    } else {
      Serial.println("[wifi] disconnected");
    }
    _lastConnected = conn;
  }

  if (!conn) {
    unsigned long now = millis();
    if (now - _lastReconnectAttemptMs > RECONNECT_INTERVAL_MS) {
      _lastReconnectAttemptMs = now;
      Serial.println("[wifi] retry...");
      WiFi.disconnect();
      WiFi.begin(_ssid, _password);
    }
  }
}

bool WifiManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

String WifiManager::getSSID() const {
  if (isConnected()) return WiFi.SSID();
  return String(_ssid ? _ssid : "");
}

int WifiManager::getRSSI() const {
  return isConnected() ? WiFi.RSSI() : 0;
}
