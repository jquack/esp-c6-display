#pragma once

#include <Arduino.h>

struct ApiResult {
  bool ok;        // HTTP request succeeded
  bool hasValue;  // response was a number (not "null")
  int  value;     // valid only if hasValue
};

class ApiClient {
public:
  explicit ApiClient(const char* url);
  ApiResult fetch();

private:
  const char* _url;
};
