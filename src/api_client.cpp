#include "api_client.h"

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <string.h>

ApiClient::ApiClient(const char* url) : _url(url) {}

ApiResult ApiClient::fetch() {
  ApiResult r{false, false, 0};

  // Pick the transport from the URL scheme: TLS (no cert validation) for
  // https://, plain TCP for http:// so a local fake server works too.
  const bool useTls = strncmp(_url, "https://", 8) == 0;
  WiFiClient       plain;
  WiFiClientSecure tls;
  if (useTls) tls.setInsecure();  // skip cert validation; fine for this endpoint
  WiFiClient& client = useTls ? static_cast<WiFiClient&>(tls) : plain;

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
