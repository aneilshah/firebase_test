#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>


// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
#define EST (-5*3600)
#define LEAP_YEAR(Y)     ( (Y>0) && !(Y%4) && ( (Y%100) || !(Y%400) ) )

unsigned int Month = 0;
unsigned int Day = 0;
unsigned int Year = 0;

void setupNTP() {
  timeClient.begin();
  timeClient.setTimeOffset(EST);
  timeClient.setUpdateInterval(3600 * 1000);  // Every Hour
}

void updateNTP() {
  // Note by default this only triggers every 60 seconds
  if (timeClient.update()) {
    Serial.println("Updating NTP Clock");
    updateDate();
  }
}

String getClock() {
  return timeClient.getFormattedTime();
}

void updateDate() {
  unsigned long rawTime = timeClient.getEpochTime() / 86400L;  // in days
  unsigned long days = 0, year = 1970;
  uint8_t month;
  static const uint8_t monthDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};

  while((days += (LEAP_YEAR(year) ? 366 : 365)) <= rawTime)
    year++;
  rawTime -= days - (LEAP_YEAR(year) ? 366 : 365); // now it is days in this year, starting at 0
  days=0;
  for (month=0; month<12; month++) {
    uint8_t monthLength;
    if (month == 1) { // february
      monthLength = LEAP_YEAR(year) ? 29 : 28;
    } else {
      monthLength = monthDays[month];
    }
    if (rawTime < monthLength) break;
    rawTime -= monthLength;
  }
  Month = month + 1;
  Day = rawTime + 1;
  Year = year;

  //String monthStr = ++month < 10 ? "0" + String(month) : String(month); // jan is month 1  
  //String dayStr = ++rawTime < 10 ? "0" + String(rawTime) : String(rawTime); // day of month  
  //return String(year) + "-" + monthStr + "-" + dayStr + "T" + getClock();
}

String getDate() {
  // Format 2024.02.15
  char ts[50];
  sprintf(ts, "%s.%s.%s", getYearStr(), getMonthStr(), getDayStr());
  return String(ts);

  return "TBD";
}

String getTimestamp() {
  // Format 2024.02.15T03:42:17
  char ts[50];
  sprintf(ts, "%s.%s.%sT%s:%s:%s", getYearStr(), getMonthStr(), getDayStr(), 
            getHourStr(), getMinStr(), getSecStr());
  return String(ts);

  return "TBD";
}

String getTimelog() {
  // Format 2024_02_15T03_42_17
  char ts[50];
  sprintf(ts, "%s_%s_%sT%s_%s_%s", getYearStr(), getMonthStr(), getDayStr(), 
            getHourStr(), getMinStr(), getSecStr());
  return String(ts);
}

String getHourStr() {
  int hr = timeClient.getHours(); 
  return (hr < 10) ? "0" + String(hr) : String(hr); 
}

String getMinStr() {
  int min = timeClient.getMinutes(); 
  return (min < 10) ? "0" + String(min) : String(min); 
}

String getSecStr() {
  int sec = timeClient.getSeconds(); 
  return (sec < 10) ? "0" + String(sec) : String(sec); 
}

String getYearStr() {
  return String(Year);
}

String getMonthStr() {
  return (Month < 10) ? "0" + String(Month) : String(Month);
}

String getDayStr() {
  return (Day < 10) ? "0" + String(Day) : String(Day);
}

unsigned int getHourInt() {return timeClient.getHours();}
unsigned int getMinInt() {return timeClient.getMinutes();}
unsigned int getSecInt() {return timeClient.getSeconds();}
unsigned int getDayInt() {return Day;}
unsigned int getMonthInt() {return Month;}
unsigned int getYearInt() {return Year;}

bool validClock() {
  time_t now = time(nullptr);
  return (now > 1767225600 && now < 2556144000); // 2026 - 2050
}