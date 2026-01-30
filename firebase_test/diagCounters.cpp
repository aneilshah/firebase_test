#include "diagCounters.h"

DiagCounters g_diag;

const char* diagName(DiagErr e) {
  switch (e) {
    case WIFI_DOWN:        return "WIFI_DOWN";
    case WIFI_RSSI_ZERO:   return "RSSI_ZERO";
    case NTP_INVALID:      return "NTP_BAD";
    case FB_NOT_READY:     return "FB_RDY";
    case FB_HTTP_NEG4:     return "HTTP_-4";
    case FB_HTTP_NEG6:     return "HTTP_-6";
    case FB_HTTP_OK:       return "HTTP_200";
    case FB_QUEUE_ENQ:     return "Q_ENQ";
    case FB_QUEUE_FLUSH_OK:return "Q_OK";
    default:               return "?";
  }
}
