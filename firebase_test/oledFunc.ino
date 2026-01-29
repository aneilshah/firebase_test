#include <Arduino.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"

#include "global.h"
#include "firebase.h"
#include "ntp.h"

// Display (same as pump)
SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// State (same as pump)
unsigned int OLED_MODE;

String PopupText = "";
String PopupDetails = "";
unsigned int PopupTimer = 0;

unsigned int MainTimer = 0;
unsigned int MainTimeout = OLED_MODE_NO_TIMEOUT;

#define MIN_MODE_CYCLE_TIME (5 * LOOPS_PER_SEC)

// Optional power control (remove if Vext not defined on Arduino hardware)
void VextON(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}
void VextOFF(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, HIGH);
}

void initDisplay() {
  display.init();
  OLED_MODE = OLED_OFF;

  // If Arduino hardware doesn't have Vext, remove these lines.
  VextON();
}

void displayPopupScreen(String text, String details) {
  PopupText = text;
  PopupDetails = details;
  PopupTimer = 0;
  OLED_MODE = OLED_POPUP;
  updatePopupScreen();
}

void updatePopupScreen() {
  if (OLED_MODE != OLED_POPUP) return;

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, PopupText);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 16, PopupDetails);
  display.display();

  PopupTimer++;
}

void oledMain(float timeout_sec) {
  OLED_MODE = OLED_MAIN;
  MainTimer = int(timeout_sec * LOOPS_PER_SEC);
  MainTimeout = int(timeout_sec * LOOPS_PER_SEC);
}

void oledMinimized() {
  OLED_MODE = OLED_MINIMIZED;
}

void oledOff() {
  OLED_MODE = OLED_OFF;
}

void displayText() {
  static unsigned int cnt = 0;
  static unsigned int horOffset = 0;
  static unsigned int vertOffset = 0;

  horOffset = (cnt / (60 * LOOPS_PER_SEC)) % 10;

  String fbStatus = firebaseOK() ? "OK" : "OFF";

  if (OLED_MODE == OLED_MAIN) {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);

    // Main Display
    display.drawString(horOffset, 0, "SHAHMAN Firebase " + String(VERSION));
    display.drawString(horOffset, 10, "WIFI: " + String(CONN_STATUS) + "  FB: " + fbStatus);
    display.drawString(horOffset, 20, "Firebase" + String(" [") + String("") + String("]  "));
    display.display();

    // Pump-style timeout -> minimized
    if (MainTimer > 0) MainTimer--;
    if (MainTimeout != OLED_MODE_NO_TIMEOUT && MainTimer == 0) {
      OLED_MODE = OLED_MINIMIZED;
    }
  }

  else if (OLED_MODE == OLED_MINIMIZED) {
    const unsigned int MODE_COUNT = 5;
    unsigned int mode = (cnt / MIN_MODE_CYCLE_TIME) % MODE_COUNT;

    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);

    if (mode == 0) display.drawString(horOffset, vertOffset, "FIREBASE APP " + String(VERSION));
    else if (mode == 1) display.drawString(horOffset, vertOffset, "FB: " + String("TBD"));
    else if (mode == 2) display.drawString(horOffset, vertOffset, "CLOCK: " + getClock());
    else if (mode == 3) display.drawString(horOffset, vertOffset, "DATE: " + getDate());
    else if (mode == 4) display.drawString(horOffset, vertOffset, "Wifi: " + String(CONN_STATUS) + " FB: " + fbStatus);
    else display.drawString(horOffset, vertOffset, "Press Button");

    if (cnt % (120 * LOOPS_PER_SEC) == 0) vertOffset = random(0, 50);

    display.display();
  }

  else if (OLED_MODE == OLED_POPUP) {
    updatePopupScreen();
  }

  cnt++;
}
