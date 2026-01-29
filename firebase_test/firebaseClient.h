#pragma once
#include <Arduino.h>
#include <Firebase_ESP_Client.h>

class FirebaseClient {
public:
  FirebaseClient();

  bool begin(const char* apiKey, const char* dbUrl);
  bool ready() const;
  String lastError() const;

  // Existing APIs (kept)
  bool writeInt(const String& path, int v);
  bool writeFloat(const String& path, float v);
  bool writeString(const String& path, const String& v);

  // Existing overloads (kept)
  bool writeInt(const String& base, const char* field, int v);
  bool writeFloat(const String& base, const char* field, float v);
  bool writeString(const String& base, const char* field, const String& v);

  // NEW: JSON node update (single request for many fields)
  bool writeJSON(const String& path, FirebaseJson& json);

  // Optional convenience: build "path + field" once
  const String& mkKeyPublic(const String& base, const char* field);

private:
  const String& mkKey(const String& base, const char* field);

  FirebaseData fbdo;
  FirebaseAuth auth;
  FirebaseConfig config;
  bool initialized = false;
  String lastErr;
  String _keyBuf;
};
