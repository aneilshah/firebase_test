void toggleLED();
void ledOn();
void ledOff();
void processLED();
void setLED();
void ledBlinkSlow();
void ledBlinkFast();

extern int LEDState;

#define LED_OFF 0
#define LED_ON 1
#define LED_BLINK_SLOW 2
#define LED_BLINK_FAST 3
