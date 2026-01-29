#pragma once
#include <Arduino.h>

void initDisplay();
void displayText();
void displayPopupScreen(String text, String details);
void updatePopupScreen();
void oledMain(float timeout_sec);
void oledMinimized();
void oledOff();

#define OLED_OFF 0
#define OLED_MAIN 1
#define OLED_POPUP 2
#define OLED_MINIMIZED 3

#define OLED_MODE_NO_TIMEOUT 0