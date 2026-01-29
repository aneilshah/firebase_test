// firebase.ino  (WRAPPER / INTEGRATION LAYER)

#include <Arduino.h>
#include <WiFi.h>

// Local project headers (keep these so your existing logic compiles)
#include "global.h"
#include "oledFunc.h"
#include "ntp.h"
#include "secrets.h"

// Public API header (what the rest of your project includes)
#include "firebase.h"

// Decoupled layers
#include "FirebaseClient.h"
#include "TestFBRepo.h"

// -----------------------------------------------------------------------------
// Instances
// -----------------------------------------------------------------------------
static FirebaseClient fbClient;
static TestFBRepo testRepo(fbClient);

// -----------------------------------------------------------------------------
// Public API (must match test_firebase/firebase.h)
// -----------------------------------------------------------------------------
int firebaseOK() {
  return testRepo.firebaseOK();
}

void writeFirebaseData() {
  testRepo.writeFirebaseData(false);
}

void writeEventData(String eventText) {
  testRepo.writeEventData(eventText);
}

// -----------------------------------------------------------------------------
// setupFirebase(showDisplay)
// -----------------------------------------------------------------------------
void setupFirebase(bool showDisplay) {
  static bool inProgress = false;
  if (inProgress) return;
  inProgress = true;

  auto cleanup = [&]() {
    if (showDisplay) oledMain(MAIN_TIMEOUT_SEC);
    inProgress = false;
  };

  Serial.println("Setting up Firebase (Firebase Test)");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("FB init aborted: WiFi not connected");
    cleanup();
    return;
  }

  // Require valid time for TLS
  if (!validClock()) {
    Serial.println("FB init aborted: system time not valid");
    cleanup();
    return;
  }

  if (showDisplay) {
    displayPopupScreen("FIREBASE...", "Connecting to Firebase");
    updatePopupScreen();
  }

  // Heap check for SSL reliability
  uint32_t heap = ESP.getFreeHeap();
  Serial.printf("Free heap: %u\n", heap);
  if (heap < 80000) {
    Serial.println("FB init aborted: heap too low for SSL");
    cleanup();
    return;
  }

  yield();
  delay(10);

  bool ok = fbClient.begin(FIREBASE_API_KEY, FIREBASE_DB_URL);
  if (!ok) {
    Serial.printf("FB begin failed: %s\n", fbClient.lastError().c_str());
  }

  cleanup();
}