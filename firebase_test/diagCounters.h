#pragma once
#include <Arduino.h>

// Keep the enum names EXACTLY as your code references
enum DiagErr : uint8_t {
  WIFI_DOWN = 0,
  WIFI_RSSI_ZERO,
  NTP_INVALID,

  FB_NOT_READY,

  FB_HTTP_OK,
  FB_HTTP_NEG4,
  FB_HTTP_NEG6,
  FB_OTHER,

  FB_QUEUE_ENQ,
  FB_QUEUE_FLUSH_OK,

  DIAG_ERR_COUNT
};

const char* diagName(DiagErr e);

struct DiagCounters {
  uint32_t c[DIAG_ERR_COUNT] = {0};
  DiagErr last = FB_HTTP_OK;
};

// Global counters
extern DiagCounters g_diag;

// Increment helper
inline void diagInc(DiagErr e) {
  g_diag.c[e]++;
  g_diag.last = e;
}
