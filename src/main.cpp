#include <Arduino.h>
#include <limits.h>

#include "wifi_config.h"
#include "display_manager.h"
#include "wifi_manager.h"
#include "api_client.h"

static constexpr const char* API_URL = "https://phtgc.nl/api/current?simple=1";
// Poll fast (2s) when there is a value to track; back off (10s) while null so
// the airplane animation isn't interrupted by the blocking HTTPS call.
static constexpr unsigned long API_INTERVAL_VALUE_MS = 2000;
static constexpr unsigned long API_INTERVAL_NULL_MS  = 10000;
static constexpr unsigned long ANIM_FRAME_MS         = 110;  // ~9 fps

static DisplayManager display;
static WifiManager    wifi;
static ApiClient      api(API_URL);

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[boot] esp32-c6-lcd-1.3");

  display.begin();
  display.drawTopBar("", 0, false);
  // Start in animation state until we get a real number.

  wifi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
  wifi.loop();

  // --- Top bar: redraw only when wifi state changes meaningfully ---
  static bool   lastConnected = false;
  static int    lastRssi      = 999;
  static String lastSsid;

  bool   conn = wifi.isConnected();
  int    rssi = wifi.getRSSI();
  String ssid = wifi.getSSID();

  if (conn != lastConnected || ssid != lastSsid || abs(rssi - lastRssi) > 3) {
    display.drawTopBar(ssid, rssi, conn);
    lastConnected = conn;
    lastRssi      = rssi;
    lastSsid      = ssid;
  }

  // --- API: poll every 2s when connected ---
  static unsigned long lastFetchMs = 0;
  static int           lastShown   = INT_MIN;
  static bool          animating   = true;  // start animating until first valid value

  unsigned long now = millis();
  unsigned long pollInterval = animating ? API_INTERVAL_NULL_MS : API_INTERVAL_VALUE_MS;
  if (conn && now - lastFetchMs >= pollInterval) {
    lastFetchMs = now;
    ApiResult r = api.fetch();
    if (r.ok && r.hasValue) {
      if (r.value != lastShown || animating) {
        display.drawBigValue(r.value);
        lastShown = r.value;
        animating = false;
      }
    } else {
      // null or fetch failure → keep crying. Do NOT touch animFrame so the
      // existing animation continues seamlessly instead of restarting.
      animating = true;
    }
  }

  // --- Animation tick ---
  static unsigned long lastFrameMs = 0;
  static uint32_t      animFrame   = 0;
  if (animating && now - lastFrameMs >= ANIM_FRAME_MS) {
    lastFrameMs = now;
    display.tickCryingAirplane(animFrame++);
  }

  delay(15);
}
