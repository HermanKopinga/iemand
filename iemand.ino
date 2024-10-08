// iemand by herman@kopinga.nl
// 2024-07-01:
// ESP32 based simple e-paper display with an rtc and a button
// mostly sleeps, activates on button press to update screen to current date&time
//
// Sleep features learned from: https://randomnerdtutorials.com/esp32-external-wake-up-deep-sleep/

#include <WiFi.h>  // we need wifi to get internet access
#include <time.h>  // for time() ctime()

#include <GxEPD2_BW.h>
#include "DFRobot_SD3031.h"

// Password method from Arduino forums (https://forum.arduino.cc/t/avoid-clear-text-wifi-passwords-in-github/1040822/2)
#include <credentials.h>        // if you have an external file with your credentials you can use it - remove before upload
#ifndef STASSID                 // either use an external .h file containing STASSID and STAPSK - or \
                                // add defines to your boards - or
#define STASSID "your-ssid"     // ... modify these line to your SSID
#define STAPSK "your-password"  // ... and set your WIFI password
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

#define GxEPD2_DISPLAY_CLASS GxEPD2_BW
#define GxEPD2_DRIVER_CLASS GxEPD2_290_T94_V2

#define MAX_DISPLAY_BUFFER_SIZE 65536ul
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=5*/ D8, /*DC=*/D7, /*RST=*/D6, /*BUSY=*/D5));  // Iemand wiring to esp32

#undef MAX_DISPLAY_BUFFER_SIZE
#undef MAX_HEIGHT

#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

#define buttonPin D3
#define ledPin D4

// Clock stuff
DFRobot_SD3031 rtc;
#define MY_NTP_SERVER "nl.pool.ntp.org"
#define MY_TZ "CET-1CEST,M3.5.0,M10.5.0/3"
/* Globals */
time_t now;  // this are the seconds since Epoch (1970) - UTC
tm tm;       // the structure tm holds time information in a more convenient way *

const char* const dayNames[] = { "zondag", "maandag", "dinsdag", "woensdag", "donderdag", "vrijdag", "zaterdag" };
const char* const monthNames[] = { "januari", "februari", "maart", "april", "mei", "juni", "juli", "augustus", "september", "oktober", "november", "december" };

/*
 How I would like this to work. (august 2024)
 - Wake
 VALIDATE VOLTAGE!
 - if voltage ok: continue
 - if voltage reasonable: show warning.
 - if voltage semi-low: don't work and print on screen.
 - if voltage very low: DONT WORK
 Time check
 - if fresh boot notify on screen that time might be unreliable and/or wifi connection is requested.
 Button process
 - if button pressed: 
   - Determine if RTC time is acceptable
   - Update display
 Try time update.
 - if last time RTC was synced or RTC unacceptable
   - if months ago, try sync through wifi
   - if year ago, add small notification to screen.
SLEEP
*/

#define wakeDefault 0
#define wakeButton 1
#define wakeTimer 2

int batteryvalue = 0;

RTC_DATA_ATTR int bootCount = 0;
int wakereason = wakeDefault;  // 0 = default, 1 = from button, 2 is from timer

/*
  Method to print and save the reason by which ESP32 woke from sleep
*/
void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Wakeup caused by external signal using RTC_IO");
      wakereason = wakeButton;
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      wakereason = wakeButton;
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wakeup caused by timer");
      wakereason = wakeTimer;
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP: Serial.println("Wakeup caused by ULP program"); break;
    default: Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 0);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  Serial.begin(115200);

  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
  print_wakeup_reason();

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_26, 0);  //1 = High, 0 = Low

  // Measure battery value.
  // ToDo: rough calibrate
  batteryvalue = analogRead(A0);
  Serial.print("Value of A0: ");
  Serial.println(batteryvalue);

  if (wakereason == wakeButton) {
    // Check if time in RTC is different than epoch.
    // Accept that time (for now) as true, so no wifi needed.
    // Wait for the chip to be initialized completely, and then exit
    while (rtc.begin() != 0) {
      Serial.println("Failed to init chip, please check if the chip connection is fine. ");
      delay(1000);
    }
    rtc.setHourSystem(rtc.e24hours);  //Set display format

    sTimeData_t sTime;
    sTime = rtc.getRTCTime();
    if (sTime.year > 1980) {
      // Time is "ok".
      Serial.print("Time is later than 1980: ");
      Serial.println(sTime.year);
    } else {
      // Time is outdated.
      // Set it through wifi.
    }


    digitalWrite(ledPin, 1);
    updateDisplay();
    printTime();
    digitalWrite(ledPin, 0);
  }
  //updateTimeThroughWifi();
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}

void updateTimeThroughWifi() {
    // Wifi
  // start network
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
    // Todo: also function without wifi.
    if (millis() > 4000) {
      Serial.print("mo wifi, mo problems");
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");

    // Time
    configTime(0, 0, MY_NTP_SERVER);  // 0, 0 because we will use TZ in the next line
    setenv("TZ", MY_TZ, 1);           // Set environment variable with your time zone
    tzset();

    // It seems that now takes time to 'automatically' get set.
    // 1000000 seconds is arbritrary "large" number. Should/could be better.
    // Maybe there is a check function to know the time was updated?
    while (now < 1000000) {
      time(&now);
    }

    localtime_r(&now, &tm);                                                                       // update the structure tm with the current time
    printTime();
    //rtc.setTime(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);  //Initialize time in RTC
    printTime();
  }
}

void printTime() {
  // Print 2 lines, first time from time.h 'native' thingie.
  time(&now);              // read the current time
  localtime_r(&now, &tm);  // update the structure tm with the current time
  Serial.print("NOW as time.h: year:");
  Serial.print(tm.tm_year + 1900);  // years since 1900
  Serial.print(" month:");
  Serial.print(tm.tm_mon + 1);  // January = 0 (!)
  Serial.print("\tday:");
  Serial.print(tm.tm_mday);  // day of month
  Serial.print("\thour:");
  Serial.print(tm.tm_hour);  // hours since midnight 0-23
  Serial.print("\tmin:");
  Serial.print(tm.tm_min);  // minutes after the hour 0-59
  Serial.print("\tsec:");
  Serial.print(tm.tm_sec);  // seconds after the minute 0-61*
  Serial.print("\twday");
  Serial.print(tm.tm_wday);  // days since Sunday 0-6
  if (tm.tm_isdst == 1)      // Daylight Saving Time flag
    Serial.print("\tDST");
  else
    Serial.print("\tstandard");
  Serial.println();
  
  // Now from rtc
  sTimeData_t sTime;
  sTime = rtc.getRTCTime();
  Serial.print("RTC as sTimeData: ");
  Serial.print(sTime.year, DEC);//year
  Serial.print('/');
  Serial.print(sTime.month, DEC);//month
  Serial.print('/');
  Serial.print(sTime.day, DEC);//day
  Serial.print(" (");
  Serial.print(sTime.week);//week
  Serial.print(") ");
  Serial.print(sTime.hour, DEC);//hour
  Serial.print(':');
  Serial.print(sTime.minute, DEC);//minute
  Serial.print(':');
  Serial.print(sTime.second, DEC);//second
  Serial.println(' ');
  
  // Now from rtc in a tm struct
  tm = rtc.getRTCTimeastm();
  Serial.print("RTC as time.h: year:");
  Serial.print(tm.tm_year);  // years since 1900
  Serial.print(" month:");
  Serial.print(tm.tm_mon + 1);  // January = 0 (!)
  Serial.print("\tday:");
  Serial.print(tm.tm_mday);  // day of month
  Serial.print("\thour:");
  Serial.print(tm.tm_hour);  // hours since midnight 0-23
  Serial.print("\tmin:");
  Serial.print(tm.tm_min);  // minutes after the hour 0-59
  Serial.print("\tsec:");
  Serial.print(tm.tm_sec);  // seconds after the minute 0-61*
  Serial.print("\twday");
  Serial.print(tm.tm_wday);  // days since Sunday 0-6
  if (tm.tm_isdst == 1)      // Daylight Saving Time flag
    Serial.print("\tDST");
  else
    Serial.print("\tstandard");
  Serial.println();
}

void loop() {
  // This space is intentionally left blank :)
}

void updateDisplay() {
  tm = rtc.getRTCTimeastm();

  display.init(115200, true, 2, false);  // for Waveshare boards with "clever" reset circuit, 2ms reset pulse
  display.setRotation(3);

  Serial.println("Update display.");
  printTime();

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold18pt7b);
    display.setCursor(60, 40);
    display.println(dayNames[tm.tm_wday]);
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(80, 70);
    display.print(tm.tm_mday);
    display.print(" ");
    display.println(monthNames[tm.tm_mon]);
    display.setCursor(80, 90);
    display.print(tm.tm_hour);  //hour
    display.print(" : ");
    // To avoid formatting voodoo on arduino.
    if (tm.tm_hour < 10) display.print("0");
    display.print(tm.tm_min);
    
    //display.print(" : ");
    //display.print(tm.tm_sec);
  } while (display.nextPage());
}
