#include "TestFBRepo.h"

// Project includes
#include "global.h"
#include "ntp.h"
#include "oledFunc.h"
#include "eventQueue.h"
#include <Firebase_ESP_Client.h>  // FirebaseJson

// -----------------------------------------------------------------------------
// Paths / limits
// -----------------------------------------------------------------------------
#define FB_PATH "aneil"
#define FB_TEST_PATH "aneil/test_server"
#define MAX_DAILY_FB_WRITES 16


// Event Queue Object
static EventQueue g_eventQ;

// Tuning
static const uint8_t  MAX_RETRIES = 5;

// Use this function to add event data to Event Queue
// Convert JSON -> string, enqueue.
// Returns true if queued, false if payload too large.
bool queueEventJson(EventQueue& q, FirebaseJson& json, unsigned long loopCount) {
  String payload;
  json.toString(payload, true); // pretty=true is ok, but you can set false to reduce size
  // json.toString(payload, false); // smaller

  if (!q.enqueue(payload, loopCount)) {
    Serial.printf("[EventQ] DROP: payload too large (%u >= %u)\n",
                  (unsigned)payload.length(), (unsigned)DATA_SIZE);
    return false;
  }

  Serial.printf("[EventQ] enqueued size=%u\n", (unsigned)q.size());
  return true;
}

// Backoff schedule in seconds by retry count.
// Called from a task every ~10 seconds, so it's OK if thresholds aren't exact.
static unsigned long retryDelaySec(uint8_t retries) {
  switch (retries) {
    case 0: return 10;       // try after 10 sec
    case 1: return 30;       // 30 sec
    case 2: return 60;       // 60 sec
    case 3: return 120;      // 2 min
    case 4: return 600;      // 10 min
    default: return 3000;    // 50 min (>=5)
  }
}

// Run this every 10 seconds to check Event Queue and resend if needed
// Flush ONE item max per call (keeps it light).
// - fb: your FirebaseClient
// - urlBase: where you want to write the event JSON (full path)
// Returns true if it attempted a write (success or fail), false if it did nothing.

// NOTE QUEUE NEEDS TIMESTAMP KEY, AND double check the base URL

bool flushEventQueueTask(EventQueue& q, FirebaseClient& fb, const String& urlBase) {
  const EventQueue::Item* it = q.peekOldest();
  if (!it) return false;

  // Age check (in seconds, based on loops)
  const unsigned long nowLoops = LOOP_COUNT;
  const unsigned long ageLoops = nowLoops - it->createdLoop;
  const unsigned long ageSec   = (LOOPS_PER_SEC > 0) ? (ageLoops / LOOPS_PER_SEC) : 0;

  const unsigned long needSec = retryDelaySec(it->retries);

  // If not old enough to retry yet, do nothing.
  if (ageSec < needSec) {
    return false;
  }

  // If we've retried too many times AND it's reached the final window, drop it.
  if (it->retries >= MAX_RETRIES && ageSec >= retryDelaySec(MAX_RETRIES)) {
    Serial.printf("[EventQ] DROP oldest: retries=%u ageSec=%lu\n",
                  (unsigned)it->retries, ageSec);
    q.dropOldest();
    return true;
  }

  if (!fb.ready()) {
    // Not connected; bump retry counter so it backs off.
    Serial.printf("[EventQ] FB not ready; bump retry. retries=%u ageSec=%lu\n",
                  (unsigned)it->retries, ageSec);
    q.incOldestRetries();
    return true;
  }

  // Convert payload string -> FirebaseJson (temporary, short-lived)
  FirebaseJson json;
  json.setJsonData(String(it->data));

  // Write attempt
  // If you want each event to be unique, you should include a unique child key in urlBase
  // e.g. urlBase + getTimestamp() + "/" or push ID, etc.
  const bool ok = fb.writeJSON(urlBase, json);

  if (ok) {
    Serial.printf("[EventQ] SENT ok; pop. ageSec=%lu\n", ageSec);
    q.dropOldest();
  } else {
    Serial.printf("[EventQ] SEND fail; inc retry. ageSec=%lu retries=%u err=%s\n",
                  ageSec, (unsigned)it->retries, fb.lastError().c_str());
    q.incOldestRetries();
  }

  return true;
}



TestFBRepo::TestFBRepo(FirebaseClient& client) : fb(client) {}

int TestFBRepo::firebaseOK() {
  return fb.ready();
}

void TestFBRepo::clearDailyFBWriteCount() {
  writesToday = 0;
}

void TestFBRepo::writeLog(const String& url, bool writeDailyData) {
  if (!firebaseOK()) {
    Serial.println("NOT UPDATING FIREBASE - NO CONNECTION");
    return;
  }
  if (writesToday >= MAX_DAILY_FB_WRITES) {
    Serial.println("ERROR: FB NOT WRITTEN - MAX WRITES EXCEEDED");
    return;
  }

  FirebaseJson json;

  // Always written
  json.set("timestamp", getTimestamp());
  json.set("firebase_cycle_count", (int)(0));
  json.set("loop_count", (int)LOOP_COUNT);

  json.set("monitor_time_min", (float)(LOOP_COUNT / 60.0 / LOOPS_PER_SEC));
  json.set("monitor_time_day", (float)(LOOP_COUNT / 3600.0 / 24.0 / LOOPS_PER_SEC));

  json.set("loop_time", (int)LOOP_TIME);
  json.set("wifi_err", (int)WIFI_ERR);
  json.set("firebase_err", (int)FB_ERR);

  if (!fb.writeJSON(url, json)) {
    Serial.println("FB writeJSON failed: " + fb.lastError());
    return;
  }

  writesToday++;
}

void TestFBRepo::writeFirebaseData(bool writeDailyData) {
  if (!firebaseOK()) {
    Serial.println("Firebase Not Connected");
    oledMain(MAIN_TIMEOUT_SEC);
    return;
  }

  displayPopupScreen("FIREBASE...", "Writing Data to Firebase");
  oledMain(MAIN_TIMEOUT_SEC);

  Serial.println();
  Serial.println("*** Writing Firebase Test Data to Firebase");
  const unsigned long sendDataPrevMillis = millis();

  String PATH = String(FB_PATH);
  const String timestamp = getTimelog();

  String year = "0";
  if (validClock()) {
    year = getYearStr();
    Serial.println("Firebase Year: " + year);
  }

  // match pump behavior
  const int yearInt = year.toInt();
  if (yearInt < 2026) {
    Serial.println("Skipping Firebase write: Year < 2026 (" + year + ")");
    oledMain(MAIN_TIMEOUT_SEC);
    return;
  }

  PATH = String(FB_TEST_PATH) + "/" + year;

  const String URL1 = "/firebase/" + PATH + "/last_event/";
  const String URL2 = "/firebase/" + PATH + "/detail_logs/" + timestamp + "/";
  const String URL3 = "/firebase/" + PATH + "/daily_logs/" + timestamp + "/";

  writeLog(URL1, false);
  if (writeDailyData) writeLog(URL3, true);
  else                writeLog(URL2, false);

  Serial.printf("Time to Write: %lu ms\n",
                (unsigned long)(millis() - sendDataPrevMillis));

  oledMain(MAIN_TIMEOUT_SEC);
}

void TestFBRepo::writeEventData(const String& eventText) {
  if (!firebaseOK()) {
    Serial.println("Firebase Not Connected (writeEvent)");
    return;
  }

  // Year gating consistent with writeFirebaseData()
  String year = "0";
  if (validClock()) {
    year = getYearStr();
  }

  const int yearInt = year.toInt();
  if (yearInt < 2026) {
    Serial.println("Skipping Firebase event write: Year < 2026 (" + year + ")");
    return;
  }

  String PATH = String(FB_PATH);
  String(FB_TEST_PATH) + "/" + year;

  // Use the same timestamp key strategy as your logs
  const String timestampKey = getTimelog();

  const String URL = "/firebase/" + PATH + "/events/" + timestampKey + "/";

  FirebaseJson json;

  // Nested data object
  json.set("day", getDayStr());
  json.set("month", getMonthStr());
  json.set("year", getYearStr());
  json.set("hour", getHourStr());
  json.set("min", getMinStr());
  json.set("sec", getSecStr());
  json.set("event", eventText);
  json.set("timestamp", timestampKey);

  if (!fb.writeJSON(URL, json)) {
    Serial.println("FB writeEvent failed: " + fb.lastError());
    return;
  }
}
