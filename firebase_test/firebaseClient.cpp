#include "FirebaseClient.h"
#include "global.h"
#include "diagCounters.h"
#include <WiFi.h>
#include <esp_heap_caps.h>

FirebaseClient::FirebaseClient() {}

static void dumpNetAndMem(const char* where, unsigned long sinceLastOk, unsigned long dt) {
  if (LOG_FIREBASE) Serial.printf("[%s] ms=%lu since=%lu dt=%lu WiFi=%d RSSI=%d IP=%s heap=%u minheap=%u psram=%u\n",
    where, (unsigned long)millis(), sinceLastOk, dt,
    (int)WiFi.status(), (int)WiFi.RSSI(),
    WiFi.localIP().toString().c_str(),
    (unsigned)ESP.getFreeHeap(),
    (unsigned)ESP.getMinFreeHeap(),
    (unsigned)ESP.getFreePsram()
  );

  if (LOG_FIREBASE) Serial.printf("[wifi] BSSID=%s CH=%d\n",
    WiFi.BSSIDstr().c_str(),
    WiFi.channel()
  );
}

bool FirebaseClient::begin(const char* apiKey, const char* dbUrl) {
  lastErr = "";

  if (!initialized) {
    fbdo.setResponseSize(2048);
    initialized = true;
  }

  config = FirebaseConfig();
  auth = FirebaseAuth();

  config.api_key = apiKey;
  config.database_url = dbUrl;

  config.timeout.serverResponse = 30 * 1000;
  config.timeout.wifiReconnect  = 10 * 1000;

  if (!Firebase.signUp(&config, &auth, "", "")) {
    // message is MB_String in Firebase_ESP_Client
    lastErr = config.signer.signupError.message.c_str();
    return false;
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  const unsigned long t0 = millis();
  while (!Firebase.ready() && millis() - t0 < 2000) delay(50);

  if (!Firebase.ready()) {
    // errorReason() returns Arduino String in most builds; if not, .c_str() still works
    lastErr = fbdo.errorReason();
    lastHttp = fbdo.httpCode();
    if (lastErr.length() == 0) lastErr = "Firebase not ready (token/DNS/TLS)";
  }

  return Firebase.ready();
}

bool FirebaseClient::ready() const { return Firebase.ready(); }
String FirebaseClient::lastError() const { return lastErr; }

// Build "<base><field>" into a reusable String buffer
const String& FirebaseClient::mkKey(const String& base, const char* field) {
  const size_t need = base.length() + strlen(field);
  _keyBuf.reserve(need + 8);   // Heltec core: don't touch capacity(); reserve is fine

  _keyBuf = base;
  _keyBuf += field;
  return _keyBuf;
}

bool FirebaseClient::writeInt(const String& path, int v) {
  const bool ok = Firebase.RTDB.setInt(&fbdo, path, v);
  if (!ok) lastErr = fbdo.errorReason();
  return ok;
}

bool FirebaseClient::writeFloat(const String& path, float v) {
  const bool ok = Firebase.RTDB.setFloat(&fbdo, path, v);
  if (!ok) lastErr = fbdo.errorReason();
  return ok;
}

bool FirebaseClient::writeString(const String& path, const String& v) {
  const bool ok = Firebase.RTDB.setString(&fbdo, path, v);
  if (!ok) lastErr = fbdo.errorReason();
  return ok;
}

// New overloads: base + field
bool FirebaseClient::writeInt(const String& base, const char* field, int v) {
  return writeInt(mkKey(base, field), v);
}

bool FirebaseClient::writeFloat(const String& base, const char* field, float v) {
  return writeFloat(mkKey(base, field), v);
}

bool FirebaseClient::writeString(const String& base, const char* field, const String& v) {
  return writeString(mkKey(base, field), v);
}

bool FirebaseClient::writeJSON(const String& path, FirebaseJson& json) {
  String out = "";
  // json.toString(out, true);

  static unsigned long lastOkMs = 0;
  unsigned long sinceLastOk = lastOkMs ? (millis() - lastOkMs) : 0;
  if (LOG_FIREBASE) Serial.printf("FB start: sinceLastOk=%lums\n", sinceLastOk);

  unsigned long t0 = millis();
  bool ok = Firebase.RTDB.setJSON(&fbdo, path, &json);
  unsigned long dt = millis() - t0;

  // Link health counters (your existing logic)
  if (WiFi.status() != WL_CONNECTED) {
    diagInc(WIFI_DOWN);
    lastErr = "wifi not connected";
    lastHttp = -100;
    return false;
  }

  if (WiFi.RSSI() == 0) {
    diagInc(WIFI_RSSI_ZERO);
  }

  if (ok) {
    lastHttp = fbdo.httpCode();      // NEW: cache http code
    diagInc(FB_HTTP_OK);             // NEW: count success

    if (dt > 5000) {
      if (LOG_FIREBASE) dumpNetAndMem("FB SLOW OK", sinceLastOk, dt);
    }

    if (LOG_FIREBASE) Serial.printf(
      "FB OK dt=%lums since=%lums WiFi=%d RSSI=%d http=%d\n",
      dt, sinceLastOk, (int)WiFi.status(), (int)WiFi.RSSI(), lastHttp
    );

    lastOkMs = millis();
    return true;
  }

  // ELSE ERROR
  lastErr = fbdo.errorReason();
  int code = fbdo.httpCode();
  lastHttp = code;                   // NEW: cache http code

  // NEW: classify failure codes
  if (code == -4)      diagInc(FB_HTTP_NEG4);
  else if (code == -6) diagInc(FB_HTTP_NEG6);
  else                 diagInc(FB_OTHER);   // if you keep a generic bucket

  if (LOG_FIREBASE) Serial.printf("FB writeJSON FAIL (%lums) http=%d err=%s\n",
                                  dt, code, lastErr.c_str());
  if (LOG_FIREBASE) dumpNetAndMem("FB FAIL", sinceLastOk, dt);

  // Transport failure: force reconnect + retry once
  if (code == -1) {
    Firebase.reconnectWiFi(true);
    delay(1200);

    t0 = millis();
    ok = Firebase.RTDB.setJSON(&fbdo, path, &json);
    dt = millis() - t0;

    if (ok) {
      lastHttp = fbdo.httpCode();    // NEW
      diagInc(FB_HTTP_OK);           // NEW (counts success on retry too)

      if (LOG_FIREBASE) Serial.printf(
        "FB Retry OK dt=%lums since=%lums WiFi=%d RSSI=%d http=%d\n",
        dt, sinceLastOk, (int)WiFi.status(), (int)WiFi.RSSI(), lastHttp
      );
      lastOkMs = millis();
      return true;
    }

    lastErr = fbdo.errorReason();
    lastHttp = fbdo.httpCode();      // NEW

    // NEW: classify retry failure too
    if (lastHttp == -4)      diagInc(FB_HTTP_NEG4);
    else if (lastHttp == -6) diagInc(FB_HTTP_NEG6);
    else                     diagInc(FB_OTHER);

    if (LOG_FIREBASE) Serial.printf("FB writeJSON RETRY FAIL (%lums) http=%d err=%s\n",
                                    dt, lastHttp, lastErr.c_str());
    if (LOG_FIREBASE) dumpNetAndMem("FB Retry FAIL", sinceLastOk, dt);
  }

  return false;
}


// Optional convenience exposure
const String& FirebaseClient::mkKeyPublic(const String& base, const char* field) {
  return mkKey(base, field);
}


