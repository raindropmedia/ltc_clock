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
float fps = 25;
const int ltcPin = 2;
const int syncPin = 33;
const int syncInterval = 30;
elapsedMillis lastSync;
elapsedMillis lastDisplayUpdate;
bool initsync, counterrun, counterpaused;
bool colonVisible = false;    
float infps;
byte countermode, lastsecond;
const int clockPin = 3;
const int upPin = 4;
const int downPin = 5;
const int hourPin = 6;
const int minutePin = 7;
const int secondPin = 8;
const int startPin = 9;
const int pausePin = 10;
const int stopPin = 11;

const int clockLedPin = 12;
const int upLedPin = 13;
const int downLedPin = 15;

const int startLedPin = 16;
const int pauseLedPin = 17;
const int stopLedPin = 23;

const int hourLedPin = 20;
const int minuteLedPin = 21;
const int secondLedPin = 22;

//struct OUT from slave to master
typedef struct {
  byte key;
  byte led;
  char name[50];
}
B_t;
B_t btn[9];

#include <TimeLib.h>
#include <TM1637Display.h>
#include "OneButton.h"

#define SEG_A   0b00000001
#define SEG_B   0b00000010
#define SEG_C   0b00000100
#define SEG_D   0b00001000
#define SEG_E   0b00010000
#define SEG_F   0b00100000
#define SEG_G   0b01000000
#define SEG_DP  0b10000000

OneButton clockbutton(clockPin);  // 10 ms debounce
OneButton upbutton(upPin);  // 10 ms debounce
OneButton downbutton(downPin);  // 10 ms debounce
OneButton hourbutton(hourPin);  // 10 ms debounce
OneButton minutebutton(minutePin);  // 10 ms debounce
OneButton secondbutton(secondPin);  // 10 ms debounce
OneButton startbutton(startPin);  // 10 ms debounce
OneButton pausebutton(pausePin);  // 10 ms debounce
OneButton stopbutton(stopPin);  // 10 ms debounce

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

void genLtc() {
  static int state = 0;

  if (clkCnt++ >= 2 * 80) {
    //ltcTimer.end();
    clkCnt = 0;
    return;
  }

  int bitval;
  int bitcount = clkCnt / 2;

  ltcframe_t ltct;
  ltct = ltc;

  if (bitcount < 64) {
    bitval = (int) (ltct.data >> bitcount) & 0x01;
  } else {
    bitval = (int) (ltct.sync >> (bitcount - 64)) & 0x01; //backward
  }

  if ( (bitval == 1) || ( (bitval == 0) && (clkCnt & 0x01) ) == 0) {
    state = !state; // toggle state
    digitalWriteFast(ltcPin, state);
  }

}

//https://www.geeksforgeeks.org/program-to-find-parity/
inline
int getParity(uint64_t n)
{
  int parity = 0;
  while (n)
  {
    parity = !parity;
    n     = n & (n - 1);
  }
  return parity & 0x01;
}


/*

   Gets called by rising edge of syncPin
*/
void startLtc() {


  ltcTimer.begin(genLtc, ltcTimer_freq);
  ltcTimer.priority(0);

  int t, t10;
  ltcframe_t ltctemp = ltc;

  clkCnt = 0;

  //get frame number:
  t = ((ltctemp.data >> 8) & 0x03) * 10 + (ltctemp.data & 0x0f);
  //Serial.printf("t start: %d\n", t);

  //zero out ltc data, leave  d+c flags and user-bits untouched:
  ltctemp.data &= 0xf0f0f0f0f0f0fcf0ULL;

  switch (countermode) {
    case 1:
      if (counterrun) {
        if (lastsecond != second()) {
          lastsecond = second();
          t = fps;
        }
        if (now() > timedestination) {
          counterrun = false;
          t = 0;
          Serial.println("Countdown erreicht!");
          break;
        }
        timediff = timedestination - now(); //Countdown auf def. Zeit
        t--;
      }
      break;
    case 2:
      if (counterrun) {
        if (lastsecond != second()) {
          lastsecond = second();
          t = fps;
        }
        timediff = timestart - (now() - timedestination); //Countdown auf 0
        if (timediff < 0) {
          timediff = 0;
          t = 0;
          counterrun = false;
          Serial.println("Countdown auf 0 erreicht!");
          break;
        }
        //Serial.printf("Start: %d Diff:%d Dest:%d Now:%d\n", timestart, timediff, timedestination, now());
        t--;
      }
      break;
    case 3:
      if (counterrun) {
        t++;
        if (lastsecond != second()) {
          lastsecond = second();
          t = 0;
        }
        if (now() > timedestination) {
          counterrun = false;
          timediff++;
          Serial.println("Countup erreicht!");
          break;
        }
        timediff = now() - timestart; //Countup auf def. Zeit
      }
      if (t >= (int) fps) t = 0;
      break;
    case 4:
      if (counterrun) {
        t++;
        if (lastsecond != second()) {
          lastsecond = second();
          t = 0;
        }
        if (timediff >= (99 * 3600 + 59 * 60 + 59)) {
          counterrun = false;
          t = 0;
          Serial.println("Countup 99 erreicht!");
          break;
        }
        timediff = now() - timestart; //Countup von 0
      }
      if (t >= (int) fps) t = 0;
      break;
    default:
      t++;
      if (lastsecond != second()) {
        lastsecond = second();
        t = 0;
      }
      timediff = (hour() * 3600 + minute() * 60 + second() );
      if (t >= (int) fps) t = 0;
      break; // Wird nicht benötigt, wenn Statement(s) vorhanden sind
  }
  ltc1.setframe(&ltctemp, t);
  ltc1.setsecond(&ltctemp, second(timediff));
  ltc1.setminute(&ltctemp, minute(timediff));
  ltc1.sethour(&ltctemp, hour(timediff));


  //set parity:
  int parity = (!getParity(ltctemp.data)) & 0x01;
  if ((int) fps == 25) {
    ltctemp.data |= (uint64_t) parity << 59;
  } else {
    ltctemp.data |= (uint64_t) parity << 27;
  }

  ltc = ltctemp;
  //Serial.printf("Out1: %02d:%02d:%02d.%02d\n", ltc1.hour(&ltc), ltc1.minute(&ltc), ltc1.second(&ltc), ltc1.frame(&ltc));
  //Serial.printf("OutLTC  : %02d:%02d:%02d.%02d\n", ltc1.hour(&ltc), ltc1.minute(&ltc), ltc1.second(&ltc), ltc1.frame(&ltc));
}

void initLtcData()
{
  ltc.sync = 0xBFFC;
  ltc.data = (uint64_t) 0; //<- place your initial data here
}

void genFpsSync() {
  static int state = 0;
  state = !state; // toggle state
  digitalWriteFast(syncPin, state);
}

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
          ltc.data = ltcframe.data;
          ltc1.setframe(&ltc, fps);
          Serial.println("Sync mit Input LTC");
          Serial.printf("SyncTime: %02d:%02d:%02d.%02d - Fps:%f\n", ltc1.hour(&ltcframe), ltc1.minute(&ltcframe), ltc1.second(&ltcframe), ltc1.frame(&ltcframe), infps + 1);
          Serial.printf("SyncedTime: %02d:%02d:%02d.%02d\n", ltc1.hour(&ltc), ltc1.minute(&ltc), ltc1.second(&ltc), ltc1.frame(&ltc));
          lastSync = 0;
          initsync = true;
          infps = 0;
        }
      }
      Serial.printf("In: %02d:%02d:%02d.%02d\n", ltc1.hour(&ltcframe), ltc1.minute(&ltcframe), ltc1.second(&ltcframe), ltc1.frame(&ltcframe));
    }
    if (lastDisplayUpdate > 1000) {      // "sincePrint" auto-increases
      lastDisplayUpdate = 0;
      displayTime(&ltc);
      Serial.printf("OutLTC  : %02d:%02d:%02d.%02d\n", ltc1.hour(&ltc), ltc1.minute(&ltc), ltc1.second(&ltc), ltc1.frame(&ltc));
      //Serial.printf("timeDIFF: %02d:%02d:%02d\n", hour(timediff), minute(timediff), second(timediff));
    }
    /*clockbutton.tick();
    upbutton.tick();
    downbutton.tick();
    hourbutton.tick();
    minutebutton.tick();
    secondbutton.tick();
    startbutton.tick();
    pausebutton.tick();
    stopbutton.tick();*/
    
    byte keycode = display.getKeyCode();
    if(keycode!=0){
      Serial.printf("Keycode  : %d\n", keycode);
      for(byte i=0;i<9;i++){
        if(keycode==btn[i].key){
          digitalWrite(btn[i].led,HIGH);
        }else{
          digitalWrite(btn[i].led,LOW);
        }
      }
    }
  }
}

void setup() {
  delay(2000);
#ifdef USE_LTC_INPUT
  AudioMemory(4);
#endif
  display.setBrightness(15);
  // All segments on
  display.setSegments(displayFull);

  Teensy3Clock.set(1 * 3600 + 33 * 60 + 0 );
  //setTime(10 * 3600 + 0 * 60 + 0 );

  timedestination = (0 * 3600 + 10 * 60 + 30 );

  setTime(0);
  setSyncProvider(getTeensy3Time);
  setSyncInterval(syncInterval);
  pinMode(syncPin, OUTPUT);
  pinMode(ltcPin, OUTPUT);

  btn[0].key=15;
  btn[1].key=7;
  btn[2].key=11;
  btn[3].key=5;
  btn[4].key=9;
  btn[5].key=1;
  btn[6].key=4;
  btn[7].key=14;
  btn[8].key=10;

  btn[0].led=clockLedPin;
  btn[1].led=upLedPin;
  btn[2].led=downLedPin;
  btn[3].led=hourLedPin;
  btn[4].led=minuteLedPin;
  btn[5].led=secondLedPin;
  btn[6].led=startLedPin;
  btn[7].led=pauseLedPin;
  btn[8].led=stopLedPin;

  for(byte i=0;i<9;i++){
    pinMode(btn[i].led, OUTPUT);
  }

  clockbutton.attachClick(setmode, 0);

  upbutton.attachClick(setmode, 1);
  upbutton.attachLongPressStart(setmode, 2);

  downbutton.attachClick(setmode, 3);
  downbutton.attachLongPressStart(setmode, 4);

  hourbutton.attachClick(clocksetup);
  hourbutton.attachLongPressStart(clocksetup);
  hourbutton.attachLongPressStop(clocksetup);
  hourbutton.attachDuringLongPress(clocksetup);

  minutebutton.attachClick(clocksetup);
  minutebutton.attachLongPressStart(clocksetup);
  minutebutton.attachLongPressStop(clocksetup);
  minutebutton.attachDuringLongPress(clocksetup);

  secondbutton.attachClick(clocksetup);
  secondbutton.attachLongPressStart(clocksetup);
  secondbutton.attachLongPressStop(clocksetup);
  secondbutton.attachDuringLongPress(clocksetup);

  startbutton.attachClick(controlcounter, 1);
  pausebutton.attachClick(controlcounter, 2);
  stopbutton.attachClick(controlcounter, 3);

  Serial.begin(115200);
  delay(500);
  Serial.println("Start LTC Counter");
  ltcTimer_freq = (1.0f / (2 * 80 * (fps ))) * 1000000.0f - 0.125f;// -0.125: make it a tiny bit faster than needed to allow syncing
  Serial.printf("ltcTimer_freq: %f\n", ltcTimer_freq);

  initLtcData();
  //set frame number to last frame to force roll-over with new second()
  ltc.data &= 0xf0f0f0f0f0f0fcf0ULL; //delete all dynamic data in frame
  int t, t10;
  t = fps; //t=30;
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
  int secs = second();
  while (secs == second()) {
    ;
  }

  //start timer and pin-interrupt:
  fpsTimer.begin(&genFpsSync, (1.0f / (2 * fps)) * 1000000.0f);
  fpsTimer.priority(0);
  attachInterrupt(digitalPinToInterrupt(syncPin), &startLtc, RISING);
  NVIC_SET_PRIORITY(87, 0); //set GPIO-INT-Priority for Pin 4 / PortA
  display.clear();

  for(byte i=0;i<9;i++){
    digitalWrite(btn[i].led,HIGH);
    delay(100);
  }
  delay(500);
  for(byte i=0;i<9;i++){
    digitalWrite(btn[i].led,LOW);
    delay(100);
  }
}

void displayTime(ltcframe_t *ltc) {
  uint8_t data[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  //Serial.printf("Display: %02d\n", ltc1.hour(ltc));
  data[0] = display.encodeDigit(((int) (ltc->data >> 56) & 0x07));
  data[1] = display.encodeDigit(((int) (ltc->data >> 48) & 0x0f));
  data[2] = display.encodeDigit(((int) (ltc->data >> 40) & 0x07));
  data[3] = display.encodeDigit(((int) (ltc->data >> 32) & 0x0f));
  data[4] = display.encodeDigit(((int) (ltc->data >> 24) & 0x07));
  data[5] = display.encodeDigit(((int) (ltc->data >> 16) & 0x0f));
  display.setColon(!display.getColon());
  display.setSegments(data);
}

void setmode(byte mode) {
  countermode = mode;
  switch (countermode) {
    case 0:
      Serial.println("Clockmode");
      break;
    case 1:
      Serial.println("Countdown auf def. Zeit");
      break;
    case 2:
      Serial.println("Countdown auf 0");
      break;
    case 3:
      Serial.println("Countup auf def. Zeit");
      break;
    case 4:
      Serial.println("Countup auf 99");
      break;
  }
}

void controlcounter(byte counterrunmode) {
  Serial.printf("Runmode-In: %d\n", counterrunmode);
  switch (counterrunmode) {
    case 1:
      if (counterpaused) {
        counterpaused = false;
        timestart = now() - timepaused;
        if (countermode == 2) {
          timedestination = timedestination + timestart;
        }
      } else {
        timestart = now();
      }
      lastsecond = second();
      counterrun = true;
      Serial.println("START");
      break;
    case 2:
      if (!counterpaused) {
        counterpaused = true;
        timepaused = now();
      }
      //timestart = now();
      //lastsecond = second();
      counterrun = false;
      Serial.println("PAUSE");
      break;
    case 3:
      //timestart = now();
      //lastsecond = second();
      counterrun = false;
      Serial.println("STOP");
      break;
  }
  Serial.printf("Runmode: %d -Paused:%d -Timestart:%d\n", counterrun, counterpaused, timestart);
}
void clocksetup() {

}
