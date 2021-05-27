/*
  Linear Timecode generation and decoding combined

  Please modify as needed

  (c) Frank B, 2020
  (pls keep copyright-info)

  To enable decoding:
  #define USE_LTC_INPUT
  and connect pin1 to pinA2 (=pin 16)

  Licence: MIT
*/

#define USE_LTC_INPUT //comment-out for generation only
#define DEFAULT_DISPLAYBRIGHTNESS 7
#define DEFAULT_BUZZERACTIVE 1
#define DEFAULT_FPS 25
#define DEFAULT_FLASH 10
#define CONFIG_VERSION "v2"
// Tell it where to store your config data in EEPROM
#define CONFIG_START 32

//float fps = 25;
const int ltcPin = 2;
const int syncPin = 33;
const int syncInterval = 30;
elapsedMillis lastSync, lastDisplayUpdate, lastBtnLedUpdate, lastBtnLedBlink, lastColonBlink, lastDebugOutput;
bool initsync, counterrun, counterpaused, externsync, displayBlink, reverseltc;
bool colonVisible = false;
bool reversecountmode;
bool showOutput, showSetup;
float infps;
byte countermode, lastsecond, setupmenu;

const int clockLedPin = 12;
const int upLedPin = 13;
const int downLedPin = 15;

const int startLedPin = 16;
const int pauseLedPin = 17;
const int stopLedPin = 23;

const int hourLedPin = 20;
const int minuteLedPin = 21;
const int secondLedPin = 22;

const int buzzerPin = 11;

enum BtnName {
  BTNCLOCK = 15,
  BTNUP = 7,
  BTNDOWN = 11,
  BTNHOUR = 5,
  BTNMINUTE = 9,
  BTNSECOND = 1,
  BTNSTART = 4,
  BTNPAUSE = 14,
  BTNSTOP = 10,
  MENUNEXT = 1,
  MENUUP = 2,
  MENUDOWN = 3
};

//struct OUT from slave to master
typedef struct {
  byte key;
  byte ledpin;
  byte ledstate;
  char name[50];
}
B_t;
B_t btn[9];

//struct OUT from slave to master
struct StoreStruct {
  char version[4];
  bool buzzerActive;
  byte brightness;
  float fps;
  byte flash;
} storage = {
  CONFIG_VERSION,
  DEFAULT_BUZZERACTIVE,
  DEFAULT_DISPLAYBRIGHTNESS,
  DEFAULT_FPS,
  DEFAULT_FLASH
};
#include <Time.h>
#include <TimeLib.h>
#include <TM1637Display.h>
#include "OneButton.h"
#include <EEPROM.h>

#define SEG_A   0b00000001
#define SEG_B   0b00000010
#define SEG_C   0b00000100
#define SEG_D   0b00001000
#define SEG_E   0b00010000
#define SEG_F   0b00100000
#define SEG_G   0b01000000
#define SEG_DP  0b10000000

#define CLK 19
#define DIO 18
#define NUMBEROFDIGITS 6

#ifdef USE_LTC_INPUT
/*
   Connect ltc
*/
#include <Audio.h>
#include <analyze_ltc.h>
AudioInputUSB            usb1;           //xy=386,450
AudioInputAnalog         adc1;
AudioAnalyzeLTC          ltc1;
//AudioConnection          patchCord2(adc1, 0, ltc1, 0);
AudioConnection          patchCord2(adc1, ltc1);

#else
struct ltcframe_t {
  uint64_t data;
  uint16_t sync;
};
#endif

IntervalTimer ltcTimer;
IntervalTimer fpsTimer;

time_t timedestination;
time_t timediff;
time_t timestart;
time_t timepaused;
time_t countertime[5];

TM1637Display display(CLK, DIO, 2, NUMBEROFDIGITS);

volatile int clkCnt = 0;
ltcframe_t ltc;
ltcframe_t ltccount;

float ltcTimer_freq;

uint8_t displayBlank[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t displayFull[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

time_t getTeensy3Time()
{
  return Teensy3Clock.get();
}

void loadConfig() {
  // To make sure there are settings, and they are YOURS!
  // If nothing is found it will use the default settings.
  if (EEPROM.read(CONFIG_START + 0) == CONFIG_VERSION[0] &&
      EEPROM.read(CONFIG_START + 1) == CONFIG_VERSION[1] &&
      EEPROM.read(CONFIG_START + 2) == CONFIG_VERSION[2])
    EEPROM.get( CONFIG_START, storage );
}

void saveConfig() {
  EEPROM.put(CONFIG_START, storage);
}

//#########################################
//############## SETUP BEGIN ##############
//#########################################

void setup() {
  delay(2000);
  loadConfig();
#ifdef USE_LTC_INPUT
  AudioMemory(4);
#endif
  display.setBrightness(storage.brightness);
  // All segments on
  display.setSegments(displayFull);

  //Teensy3Clock.set(1 * 3600 + 33 * 60 + 0 );
  //setTime(10 * 3600 + 0 * 60 + 0 );

  //timedestination = (0 * 3600 + 10 * 60 + 30 );

  setTime(0);
  setSyncProvider(getTeensy3Time);
  setSyncInterval(syncInterval);
  pinMode(syncPin, OUTPUT);
  pinMode(ltcPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  btn[0].key = BTNCLOCK;
  btn[1].key = BTNUP;
  btn[2].key = BTNDOWN;
  btn[3].key = BTNHOUR;
  btn[4].key = BTNMINUTE;
  btn[5].key = BTNSECOND;
  btn[6].key = BTNSTART;
  btn[7].key = BTNPAUSE;
  btn[8].key = BTNSTOP;

  btn[0].ledpin = clockLedPin;
  btn[1].ledpin = upLedPin;
  btn[2].ledpin = downLedPin;
  btn[3].ledpin = hourLedPin;
  btn[4].ledpin = minuteLedPin;
  btn[5].ledpin = secondLedPin;
  btn[6].ledpin = startLedPin;
  btn[7].ledpin = pauseLedPin;
  btn[8].ledpin = stopLedPin;

  for (byte i = 0; i < 9; i++) {
    pinMode(btn[i].ledpin, OUTPUT);
  }

  Serial.begin(115200);
  delay(500);
  Serial.println("Start LTC Counter");
  
  initLtcData();
  //set frame number to last frame to force roll-over with new second()
  ltc.data &= 0xf0f0f0f0f0f0fcf0ULL; //delete all dynamic data in frame
  int t, t10;
  t = storage.fps; //t=30;
  Serial.printf("t: %d\n", t);
  t10 = t / 10; //t10=3
  Serial.printf("t10: %d\n", t10);
  ltc.data |= (t10 & 0x03) << 8;
  Serial.printf("t10 & 0x03: %d\n", (t10 & 0x03));
  Serial.printf("In: %02d:%02d:%02d.%02d\n", ltc1.hour(&ltc), ltc1.minute(&ltc), ltc1.second(&ltc), ltc1.frame(&ltc));
  ltc.data |= ((t - t10 * 10));
  Serial.printf("t - t10 * 10: %d\n", (t - t10 * 10));
  Serial.printf("In: %02d:%02d:%02d.%02d\n", ltc1.hour(&ltc), ltc1.minute(&ltc), ltc1.second(&ltc), ltc1.frame(&ltc));
  //now wait for seconds-change
  
  setfpstimer();
  
  display.clear();
  for (byte i = 0; i < 9; i++) {
    digitalWrite(btn[i].ledpin, HIGH);
    delay(100);
  }
  digitalWrite(buzzerPin, HIGH);
  delay(500);
  digitalWrite(buzzerPin, LOW);
  for (byte i = 0; i < 9; i++) {
    digitalWrite(btn[i].ledpin, LOW);
    delay(100);
  }
  setmode(0);
}

void setfpstimer(){
  ltcTimer_freq = (1.0f / (2 * 80 * (storage.fps ))) * 1000000.0f - 0.125f;// -0.125: make it a tiny bit faster than needed to allow syncing
  Serial.printf("ltcTimer_freq: %f\n", ltcTimer_freq);
  int secs = second();
  while (secs == second()) {
    ;
  }
  //start timer and pin-interrupt:
  fpsTimer.begin(&genFpsSync, (1.0f / (2 * storage.fps)) * 1000000.0f);
  fpsTimer.priority(0);
  attachInterrupt(digitalPinToInterrupt(syncPin), &startLtc, RISING);
  NVIC_SET_PRIORITY(87, 0); //set GPIO-INT-Priority for Pin 4 / PortA
}

//########################################
//############## LOOP BEGIN ##############
//########################################

void loop() {

  while (1) {
    if (ltc1.available()) {
      ltcframe_t ltcframe = ltc1.read();
      if ((lastSync > (syncInterval * 1000)) || (initsync == false)) { // "sincePrint" auto-increases
        if (infps < ltc1.frame(&ltcframe) || (infps > 30)) {
          infps = ltc1.frame(&ltcframe);
        }
        if (ltc1.frame(&ltcframe) == 0) {
          Teensy3Clock.set(ltc1.hour(&ltcframe) * 3600 + ltc1.minute(&ltcframe) * 60 + ltc1.second(&ltcframe));
          setTime(ltc1.hour(&ltcframe) * 3600 + ltc1.minute(&ltcframe) * 60 + ltc1.second(&ltcframe));
          if (countermode == 0) {
            ltc.data = ltcframe.data;
          }
          ltc1.setframe(&ltc, storage.fps);
          Serial.println("Sync mit Input LTC");
          Serial.printf("SyncTime: %02d:%02d:%02d.%02d - Fps:%f\n", ltc1.hour(&ltcframe), ltc1.minute(&ltcframe), ltc1.second(&ltcframe), ltc1.frame(&ltcframe), infps + 1);
          Serial.printf("SyncedTime: %02d:%02d:%02d.%02d\n", ltc1.hour(&ltc), ltc1.minute(&ltc), ltc1.second(&ltc), ltc1.frame(&ltc));
          lastSync = 0;
          initsync = true;
          infps = 0;
          externsync = true;
        }
      }
      externsync = false;
      Serial.printf("In: %02d:%02d:%02d.%02d\n", ltc1.hour(&ltcframe), ltc1.minute(&ltcframe), ltc1.second(&ltcframe), ltc1.frame(&ltcframe));
    }

    updateDisplay(&ltc);
    buttonTick();
    updateBtnLed();
  }
}

void setuptime(byte btnnr) {
  int updateval = 1;
  if (reversecountmode) {
    updateval = -1;
  }
  if (countermode < 4) {
    switch (btnnr) {
      case 0:
        countertime[countermode] = countertime[countermode] + (updateval * 3600);
        break;
      case 1:
        countertime[countermode] = countertime[countermode] + (updateval * 60);
        break;
      case 2:
        countertime[countermode] = countertime[countermode] + (updateval);
        break;
    }
  }
  //Serial.print("countertime: ");
  //Serial.println(countertime[countermode]);
}

void setmode(byte mode) {
  countermode = mode;
  btn[0].ledstate = 0;
  btn[1].ledstate = 0;
  btn[2].ledstate = 0;
  btn[6].ledstate = 0;
  btn[7].ledstate = 0;
  btn[8].ledstate = 1;
  counterpaused = false;
  counterrun = false;
  displayBlink = false;
  showSetup = false;
  reverseltc=false;
  switch (countermode) {
    case 0:
      Serial.println("Clockmode");
      btn[0].ledstate = 1;
      btn[8].ledstate = 0;
      showOutput = true;
      //displayBlink=true;
      break;
    case 1:
      Serial.println("Countdown auf def. Zeit");
      btn[2].ledstate = 2;
      showOutput = false;
      reverseltc=true;
      break;
    case 2:
      Serial.println("Countdown auf 0");
      btn[2].ledstate = 1;
      showOutput = false;
      reverseltc=true;
      break;
    case 3:
      Serial.println("Countup auf def. Zeit");
      btn[1].ledstate = 2;
      showOutput = false;
      break;
    case 4:
      Serial.println("Countup auf 99");
      btn[1].ledstate = 1;
      showOutput = false;
      break;
    case 5:
      Serial.println("Setup");
      btn[0].ledstate = 2;
      showSetup = true;
      break;
  }
}

void controlcounter(byte counterrunmode) {
  Serial.printf("Runmode-In: %d\n", counterrunmode);
  if (countermode != 0 && countermode != 5) {
    switch (counterrunmode) {
      case 1:
        if (counterrun == false) {
          btn[6].ledstate = 1;
          btn[7].ledstate = 0;
          btn[8].ledstate = 0;
          showOutput = true;
          if (counterpaused) {
            counterpaused = false;
            timestart = timestart + (now() - timepaused);
            timedestination = timedestination + (now() - timepaused);
          } else {
            timestart = now();
            if (countermode == 1) {
              tmElements_t tm;
              breakTime(timestart, tm);
              tm.Hour = hour(countertime[countermode]);
              tm.Minute = minute(countertime[countermode]);
              tm.Second = second(countertime[countermode]);
              timedestination = makeTime(tm);
              //breakTime(timestart, temptm);
              //temptm->tm_year = (int)year(timestart);
              //timedestination
            } else {
              timedestination = timestart + countertime[countermode];
            }
          }
          lastsecond = second();
          counterrun = true;
          Serial.println("START");
          Serial.printf("StartTime  : %d\n", timestart);
        }
        break;
      case 2:
        if (!counterpaused && counterrun == true) {
          btn[7].ledstate = 1;
          btn[6].ledstate = 2;
          btn[8].ledstate = 0;
          showOutput = true;
          counterpaused = true;
          timepaused = now();
          counterrun = false;
        }
        //timestart = now();
        //lastsecond = second();
        Serial.println("PAUSE");
        Serial.printf("PauseTime  : %d\n", timepaused);
        break;
      case 3:
        btn[6].ledstate = 0;
        btn[7].ledstate = 0;
        btn[8].ledstate = 1;
        //timestart = now();
        //lastsecond = second();
        showOutput = false;
        counterrun = false;
        counterpaused = false;
        displayBlink = false;
        timedestination = countertime[countermode];
        Serial.println("STOP");
        break;
    }
    Serial.printf("Countermode: %d -Runmode: %s -Paused:%s -Timestart:%d -Timedestination:%d -Timepaused:%d -NOW:%d\n", countermode, counterrun ? "true" : "false", counterpaused ? "true" : "false", timestart, timedestination, timepaused, now());
  }
}
void clocksetup(byte menuctrl) {
  switch (menuctrl) {
    case MENUNEXT:
      setupmenu++;
      if (setupmenu >= 4) {
        setupmenu = 0;
      }
      break;
    case MENUUP:
      if (setupmenu == 0) {
        storage.buzzerActive = !storage.buzzerActive;
      } else if (setupmenu == 1) {
        if (storage.brightness < 7) {
          storage.brightness++;
        }
      }else if (setupmenu == 2) {
        if (storage.fps == 25) {
          storage.fps=30;
        }
      }else if (setupmenu == 3) {
        if (storage.flash < 10) {
          storage.flash++;
        }
      }
      break;
    case MENUDOWN:
      if (setupmenu == 0) {
        storage.buzzerActive = !storage.buzzerActive;
      } else if (setupmenu == 1) {
        if (storage.brightness > 0) {
          storage.brightness--;
        }
      }else if (setupmenu == 2) {
        if (storage.fps == 30) {
          storage.fps=25;
        }
      }
      else if (setupmenu == 3) {
        if (storage.flash > 1) {
          storage.flash--;
        }
      }
      break;
  }
}
