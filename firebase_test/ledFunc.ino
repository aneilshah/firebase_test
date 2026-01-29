int LEDState = LED_OFF;
int LEDVal = LED_OFF;

void toggleLED() {
    if (LEDState == LED_OFF) {LEDState = LED_ON;}
    else LEDState = LED_ON;
    setLED();
}

void ledOn() {
    LEDState = LED_ON;
    LEDVal = LED_ON;
    setLED();
}

void ledOff() {
    LEDState = OFF;
    LEDVal = LED_OFF;
    setLED();
}

void ledBlinkSlow() {
    LEDState = LED_BLINK_SLOW;
}

void ledBlinkFast() {
    LEDState = LED_BLINK_FAST;
}

void processLED() {
    static int loopCount;
    if (LEDState == LED_BLINK_FAST) LEDVal = (loopCount % 6 < 3);  // 3 loops ON - 3 loops OFF
    else if (LEDState == LED_BLINK_SLOW) LEDVal = (loopCount % 10 < 5); // 5 loops ON - 5 loops OFF
    setLED();
    loopCount++;
}

void setLED() {
    digitalWrite(LED_PIN, LEDVal); // LED
}
