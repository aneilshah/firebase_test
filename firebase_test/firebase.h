#pragma once
#include <Arduino.h>

void setupFirebase(bool showDisplay = true);
void writeFirebaseData();
void writeEventData(String eventText);
int firebaseOK();
