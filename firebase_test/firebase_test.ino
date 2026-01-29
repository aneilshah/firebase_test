// Included Library Files
#include <Arduino.h>
#include <WiFi.h>

// My Files
#include "global.h"
#include "ledFunc.h"
#include "oledFunc.h"
#include "serverFunc.h"
#include "wifiFunctions.h"
#include "firebase.h"
#include "ntp.h"

// Global Constants
#define LOG_TIME 0  // serial log for execution times

// Configurations
#define FB_WRITE_TIME_MIN 480

const char* VERSION = "V0.01";

// PIN MAPPINGS
#define  ADC1_PIN 35
#define  BTN_PIN 0
#define  DAC_PIN 26
#define  D1_PIN 0
#define  ISR_PIN 35
#define  LED_PIN 25 
#define  TS1_PIN 14  // Touch Switch

// System States
String CONN_STATUS = "OFF";
int LOOP_COUNT = 0;
int LOOP_TIME = 1000;
int ISR_CNT = 0;
unsigned int DELAY = 96;  // avg code run time is 8ms

// Error Counts
int WIFI_ERR = 0;
int FB_ERR = 0;

// Hardware States
int ADC1_COUNT = 0;
float ADC1_VOLT = 0; 
int BTN_VAL = 0;
int D1_VAL = 2;
int TS1_VAL = 0;


void IRAM_ATTR isrFunction() {
    ISR_CNT++;
}

void setup()
{
    Serial.begin(115200);

    // Init ADC
    adcAttachPin(ADC1_PIN);
    //analogSetClockDiv(255); // 1338mS

    // Init LED
    pinMode(LED_PIN, OUTPUT); // Set GPIO25 as digital output pin

    // Init ISR
    //pinMode(ISR_PIN, INPUT_PULLUP);
    //attachInterrupt(ISR_PIN, isrFunction, RISING);

    // Init OLED
    initDisplay();

    // Set WiFi to station mode and disconnect from an AP if it was previously connected
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(500);

    Serial.println();
    Serial.println();
    Serial.println("### STARTING FIREBASE MONITOR: " + String(VERSION) + " ###");
    Serial.println("Setup Complete");
    connectFirebaseWifi();

    // NTP Time Server
    setupNTP();

  // Firebase
  if (waitForWifiStable()) {
    Serial.println("Post-WiFi settle...");
    ledOn();
    delay(2000);
    ledOff();
    setupFirebase();
  } else {
    Serial.println("Deferring Firebase init to 1-minute loop");
  }

    // Test Functions
}

void loop()
{
    static unsigned long loopCount = 0;

    webServer();   // SERVICE HTTP AS FAST AS POSSIBLE

    loop100ms();                            // Run the 100ms loop
    if (loopCount%10 == 0) loop1Sec();      // Run the 1 sec loop
    if (loopCount%100 == 0) loop10Sec();    // Run the 10 sec loop 
    if (loopCount%600 == 0) loop1Min();     // Run the 1 minute loop 
    if (loopCount%36000 == 0) loop1Hour();  // Run the 1 hour loop 
    if (loopCount%864000 == 0) loop1Day();  // Run the 1 day loop 

    // Complete the loop
    loopCount++;
    LOOP_COUNT++;

    delay(DELAY);  // avg code run time is 8ms
}

void loop100ms() {

    // Read Inputs
    readDigitalButton(); 
    readADC();

    // Run Main Logic


    // Update Outputs
    displayText();
    updatePopupScreen();
    processLED();

    // process Events
    processLoopCheck();
}

void loop1Sec() {
    static long count;

    // process 1 second tasks  
    readDigital();
    readTouchSwitch();
    writeDAC();
    if (wifiOK()) updateNTP();
    
    // update loop counter
    count++;
}

void loop10Sec() {
    ensureServerStarted();

    // Check Event Queue
    //flushEventQueueTask(g_eventQ, fb, eventsUrlBase);

    static unsigned long startTime = millis();
}

void loop1Min() {
    // reconnect if needed
    if (!firebaseOK()) {setupFirebase(); FB_ERR++;}
    if (!wifiOK()) {connectFirebaseWifi(); WIFI_ERR++;}
}


void loop1Hour() {
    Serial.println();
    Serial.println("*** RUNNING 1 HOUR LOOP");

}

void loop1Day() {
    Serial.println();
    Serial.println("*** RUNNING 1 DAY LOOP");

}

void writeDAC() {
    dacWrite(DAC_PIN, LOOP_COUNT%250);
}

void readADC() {
    long startTime=micros();

    ADC1_COUNT = analogRead(ADC1_PIN);

    ADC1_VOLT = 3.3 * (ADC1_COUNT / 4095.0);
    TLog("ADC read time: ", startTime);
    TLog("ADC: " + String(ADC1_COUNT), startTime);
}

void readDigital() {
    long startTime=micros();
    D1_VAL = digitalRead(D1_PIN);
    TLog("Digital read time: ", startTime);
}

void readDigitalButton() {
    const int oldBtn = BTN_VAL;
    BTN_VAL = digitalRead(BTN_PIN);
    if (oldBtn == 1 && BTN_VAL == 0) processButton();
}

void readTouchSwitch() {
    long startTime=micros();
    TS1_VAL = touchRead(TS1_PIN);
    TLog("TS read time: ", startTime);
}

void processButton()
{
  displayPopupScreen("BUTTON PRESSED","PRG Button");
  delay(1000);
  oledMain(MAIN_TIMEOUT_SEC);
}

void processLoopCheck() {
   static unsigned long loopCount = 0;
   static unsigned long startTime = 0;
   if (loopCount % 999 == 0) {
      LOOP_TIME = int((millis() - startTime) / 100);
      Serial.println();
      Serial.println("*** Total time for 1000 loops:" + String(LOOP_TIME));
      startTime = millis();
   }
   loopCount++;
}


void Log(String text) {
  Serial.println(text);
}

void VLog(String text) {
  if (VERBOSE) Serial.println(text);
}

void TLog(String text, long startTime) {
  if (LOG_TIME) Serial.println(text + String(micros()-startTime));
}


// Version History
// 0.1 Initial Release

