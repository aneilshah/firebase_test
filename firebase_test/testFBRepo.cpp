#include "TestFBRepo.h"

// Project includes
#include "global.h"
#include "ntp.h"
#include "oledFunc.h"
#include "eventQueue.h"
#include "diagCounters.h"
#include <Firebase_ESP_Client.h>  // FirebaseJson

// -----------------------------------------------------------------------------
// Paths / limits
// -----------------------------------------------------------------------------
#define FB_PATH "/firebase_test"
#define MAX_DAILY_FB_WRITES 2000

// Event Queue Object
static EventQueue g_eventQ;

// Tuning
static const uint8_t  MAX_RETRIES = 5;

// Convert JSON -> string, enqueue.
// Returns true if queued, false if payload too large.
static bool queueEventJson(EventQueue& q, const String& path, FirebaseJson& json, unsigned long loopCount) {
  String payload;
  json.toString(payload, true); // pretty=true OK; set false to reduce size if needed

  if (!q.enqueue(path, payload, loopCount)) {
    Serial.printf("[EventQ] DROP: too large pathLen=%u/%u payloadLen=%u/%u\n",
                  (unsigned)path.length(), (unsigned)PATH_SIZE,
                  (unsigned)payload.length(), (unsigned)DATA_SIZE);
    return false;
  }

  Serial.printf("[EventQ] enqueued size=%u\n", (unsigned)q.size());
  return true;
}

// Backoff schedule in seconds by retry count.
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

// Flush ONE item max per call.
static bool flushEventQueueTask(EventQueue& q, FirebaseClient& fb) {
  const EventQueue::Item* it = q.peekOldest();
  if (!it) return false;

  // Age check (in seconds, based on loops)
  const unsigned long nowLoops = LOOP_COUNT;
  const unsigned long ageLoops = nowLoops - it->createdLoop;
  const unsigned long ageSec   = (LOOPS_PER_SEC > 0) ? (ageLoops / LOOPS_PER_SEC) : 0;

  const unsigned long needSec = retryDelaySec(it->retries);

  // If not old enough to retry yet, do nothing.
  if (ageSec < needSec) return false;

  // Too many retries and reached final window: drop it.
  if (it->retries >= MAX_RETRIES && ageSec >= retryDelaySec(MAX_RETRIES)) {
    Serial.printf("[EventQ] DROP oldest: retries=%u ageSec=%lu path=%s\n",
                  (unsigned)it->retries, ageSec, it->path);
    q.dropOldest();
    return true;
  }

  if (!fb.ready()) {
    Serial.printf("[EventQ] FB not ready; bump retry. retries=%u ageSec=%lu\n",
                  (unsigned)it->retries, ageSec);
    q.incOldestRetries();
    return true;
  }

  // Convert payload string -> FirebaseJson (temporary)
  FirebaseJson json;
  json.setJsonData(String(it->data));

  const bool ok = fb.writeJSON(String(it->path), json);

  if (ok) {
    Serial.printf("[EventQ] SENT ok; pop. ageSec=%lu path=%s\n", ageSec, it->path);
    q.dropOldest();
    diagInc(FB_QUEUE_FLUSH_OK);
  } else {
    Serial.printf("[EventQ] SEND fail; inc retry. ageSec=%lu retries=%u err=%s path=%s\n",
                  ageSec, (unsigned)it->retries, fb.lastError().c_str(), it->path);
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

void TestFBRepo::queueJsonOrDrop(const String& url, FirebaseJson& json) {
  // Keep enqueue cheap; if it doesn't fit, drop and log.
  queueEventJson(g_eventQ, url, json, (unsigned long)LOOP_COUNT);
  diagInc(FB_QUEUE_ENQ);
}

void TestFBRepo::tickEventQueue() {
  // If there’s anything queued, attempt at most one send per tick.
  flushEventQueueTask(g_eventQ, fb);
}

void TestFBRepo::writeLog(const String& url, bool writeDailyData) {
  // If FB not ready, queue the log instead of dropping it.
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

  if (!firebaseOK()) {
    Serial.println("FB not ready; queueing log write");
    queueJsonOrDrop(url, json);
    return;
  }

  if (writesToday >= MAX_DAILY_FB_WRITES) {
    Serial.println("ERROR: FB NOT WRITTEN - MAX WRITES EXCEEDED");
    // You can choose to queue anyway. For now, don't, to preserve your safety limit.
    return;
  }

  if (!fb.writeJSON(url, json)) {
    Serial.println("FB writeJSON failed; queueing. err=" + fb.lastError());
    queueJsonOrDrop(url, json);
    return;
  }

  writesToday++;
}

void TestFBRepo::writeFirebaseData(bool writeDailyData) {
  if (!firebaseOK()) {
    Serial.println("Firebase Not Connected");
    oledMain(MAIN_TIMEOUT_SEC);
    // DON'T return; let writeLog() queue
  }

  displayPopupScreen("FIREBASE...", "Writing Data to Firebase");
  oledMain(MAIN_TIMEOUT_SEC);

  Serial.println();
  Serial.println("*** Writing Firebase Test Data to Firebase");
  const unsigned long sendDataPrevMillis = millis();

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

  // Write into your test tree (as your code intended elsewhere)
  const String PATH = String(FB_PATH) + "/" + year;

  const String URL1 = PATH + "/last_event/";
  const String URL2 = PATH + "/detail_logs/" + timestamp + "/";
  const String URL3 = PATH + "/daily_logs/"  + timestamp + "/";

  writeLog(URL1, false);
  if (writeDailyData) writeLog(URL3, true);
  else                writeLog(URL2, false);

  Serial.printf("Time to Write: %lu ms\n",
                (unsigned long)(millis() - sendDataPrevMillis));

  oledMain(MAIN_TIMEOUT_SEC);
}

void TestFBRepo::writeEventData(const String& eventText) {
  // Year gating consistent with writeFirebaseData()
  String year = "0";
  if (validClock()) year = getYearStr();

  const int yearInt = year.toInt();
  if (yearInt < 2026) {
    Serial.println("Skipping Firebase event write: Year < 2026 (" + year + ")");
    return;
  }

  // Use the same test tree + year as writeFirebaseData()
  const String PATH = String(FB_PATH) + "/" + year;

  // Unique key (keeps events from overwriting)
  const String timestampKey = getTimelog();

  const String URL = PATH + "/events/" + timestampKey + "/";

  FirebaseJson json;
  json.set("day", getDayStr());
  json.set("month", getMonthStr());
  json.set("year", getYearStr());
  json.set("hour", getHourStr());
  json.set("min", getMinStr());
  json.set("sec", getSecStr());
  json.set("event", eventText);
  json.set("timestamp", timestampKey);

  // If FB not ready OR write fails, queue it.
  if (!firebaseOK()) {
    Serial.println("FB not ready (writeEvent); queueing event");
    queueJsonOrDrop(URL, json);
    return;
  }

  if (!fb.writeJSON(URL, json)) {
    Serial.println("FB writeEvent failed; queueing. err=" + fb.lastError());
    queueJsonOrDrop(URL, json);
    return;
  }
}
