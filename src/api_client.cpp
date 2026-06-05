#include "api_client.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

ApiClient::ApiClient(const char* url) : _url(url) {}

ApiResult ApiClient::fetch() {
  ApiResult r{false, false, 0};

  WiFiClientSecure client;
  client.setInsecure();  // skip cert validation; fine for this endpoint

  HTTPClient http;
  http.setTimeout(3000);
  if (!http.begin(client, _url)) {
    Serial.println("[api] begin failed");
    return r;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[api] HTTP %d\n", code);
    http.end();
    return r;
  }

  String body = http.getString();
  http.end();
  body.trim();
  Serial.printf("[api] body='%s'\n", body.c_str());

  r.ok = true;

  if (body.length() == 0 || body.equalsIgnoreCase("null")) {
    return r;
  }

  // Tolerate quoted numbers like "123"
  if (body.startsWith("\"") && body.endsWith("\"") && body.length() >= 2) {
    body = body.substring(1, body.length() - 1);
  }

  // toInt() returns 0 on a non-numeric string; guard against silently treating
  // garbage as 0 by requiring at least one digit.
  bool hasDigit = false;
  for (size_t i = 0; i < body.length(); i++) {
    if (isdigit(body[i])) { hasDigit = true; break; }
  }
  if (!hasDigit) return r;

  r.hasValue = true;
  r.value = body.toInt();
  return r;
}
