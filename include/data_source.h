#pragma once

#include <Arduino.h>

struct DisplayPayload {
  String title;
  String line1;
  String line2;
  int    signalBars;  // 0..4, or -1 to hide
};

class IDataSource {
public:
  virtual ~IDataSource() = default;
  virtual DisplayPayload fetch() = 0;
};
