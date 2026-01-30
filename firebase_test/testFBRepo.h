#pragma once
#include <Arduino.h>
#include "FirebaseClient.h"

class TestFBRepo {
public:
  TestFBRepo(FirebaseClient& c);

  void writeFirebaseData(bool writeDailyData);
  void writeEventData(const String& eventText);
  void clearDailyFBWriteCount();
  int firebaseOK();

  // Call this from loop10Sec(): flushes at most ONE queued item per call.
  void tickEventQueue();

private:
  void writeLog(const String& url, bool writeDailyData);
  void queueJsonOrDrop(const String& url, class FirebaseJson& json);

  FirebaseClient& fb;
  int writesToday = 0;
};
