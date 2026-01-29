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

private:
  void writeLog(const String& url, bool writeDailyData);

  FirebaseClient& fb;
  int writesToday = 0;
};
