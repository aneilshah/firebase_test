#pragma once

// external variables
extern int ADC1_COUNT;
//extern float ADC1_VOLT; 
extern String CONN_STATUS;
extern int FB_ERR;
extern int LOOP_COUNT;
extern int LOOP_TIME;
extern const char* VERSION;
extern int WIFI_ERR;

#define VERBOSE 1
#define LOG_FIREBASE 1

#define OFF 0
#define ON 1
#define FALSE 0
#define TRUE 1

#define LOOPS_PER_SEC 10

// OLED MAIN MENU MODE
#define MAIN_TIMEOUT_SEC 30.0

// A2D Constants
#define FURN_ON_COUNTS  500  // 0.4V
#define FURN_OFF_COUNTS 100  // 0.1V
