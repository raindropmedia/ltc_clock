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
const int ltcPin = 0;
const int ltcInvPin = 1;
const int syncPin = 4;
const int syncInterval = 30;
elapsedMillis lastSync;
elapsedMillis lastDisplayUpdate;
bool initsync;
float infps;
byte mode;

#include <TimeLib.h>
#include <TM1637Display.h>

#define CLK 19
#define DIO 18

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
AudioConnection          patchCord2(usb1, 0, ltc1, 0);

#else
struct ltcframe_t {
  uint64_t data;
  uint16_t sync;
};
#endif

IntervalTimer ltcTimer;
IntervalTimer fpsTimer;

time_t timedestination;

TM1637Display display(CLK, DIO);

volatile int clkCnt = 0;
ltcframe_t ltc;
ltcframe_t ltccount;

float ltcTimer_freq;

uint8_t displayBlank[] = { 0x00, 0x00, 0x00, 0x00 };
uint8_t displayFull[] = { 0xff, 0xff, 0xff, 0xff };

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
    digitalWriteFast(ltcInvPin, !state);
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
  //data = 0;
  //inc frame-number:
  //TODO: realize "drop frame numbering" (when D-Flag is set)
  t++;
  if (t >= (int) fps) t = 0;
  //set frame number:
  //t10 = t / 10;
  //data |= (t10 & 0x03) << 8; // Seten 10er Frame
  //data |= ((t - t10 * 10)) << 0; // Seten 1er Frame
  ltc1.setframe(&ltctemp, t);

  //set time:
  t = second();
  //t10 = t / 10;
  //data |= (t10 & 0x07) << 24; //Seconds tens
  //data |= ((t - t10 * 10) & 0x0f) << 16;
  ltc1.setsecond(&ltctemp, t);

  t = minute();
  //t10 = t / 10;
  //data |= (uint64_t)(t10 & 0x07) << 40; //minute tens
  //data |= (uint64_t)((t - t10 * 10) & 0x0f) << 32;
  ltc1.setminute(&ltctemp, t);

  t = hour();
  //t10 = t / 10;
  //data |= (uint64_t)(t10 & 0x03) << 56; //hour tens
  //data |= (uint64_t)((t - t10 * 10) & 0x0f) << 48;
  ltc1.sethour(&ltctemp, t);

  //set parity:
  int parity = (!getParity(ltctemp.data)) & 0x01;
  if ((int) fps == 25) {
    ltctemp.data |= (uint64_t) parity << 59;
  } else {
    ltctemp.data |= (uint64_t) parity << 27;
  }

  ltc = ltctemp;
  //Serial.printf("Out1: %02d:%02d:%02d.%02d\n", ltc1.hour(&ltc), ltc1.minute(&ltc), ltc1.second(&ltc), ltc1.frame(&ltc));
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
      calcTime(&ltc);
      Serial.printf("Out: %02d:%02d:%02d\n", hour(), minute(), second());
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

  Teensy3Clock.set(0 * 3600 + 0 * 60 + 0 );
  //setTime(10 * 3600 + 0 * 60 + 0 );

  timedestination=(12 * 3600 + 12 * 60 + 12 );
  
  setTime(0);
  setSyncProvider(getTeensy3Time);
  setSyncInterval(syncInterval);
  pinMode(syncPin, OUTPUT);
  pinMode(ltcPin, OUTPUT);
  pinMode(ltcInvPin, OUTPUT);
  Serial.begin(115200);
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
}

void displayTime(ltcframe_t *ltc) {
  uint8_t data[] = { 0xff, 0xff, 0xff, 0xff };
  //Serial.printf("Display: %02d\n", ltc1.hour(ltc));
  data[0] = display.encodeDigit(((int) (ltc->data >> 40) & 0x07));
  data[1] = display.encodeDigit(((int) (ltc->data >> 32) & 0x0f));
  data[2] = display.encodeDigit(((int) (ltc->data >> 24) & 0x07));
  data[3] = display.encodeDigit(((int) (ltc->data >> 16) & 0x0f));
  display.setSegments(data);
}

void calcTime(ltcframe_t *ltc) {
  time_t temptime;
  int t;
  mode=1;
  switch (mode) {
  case 1:
    temptime = timedestination-now(); //Countdown auf Zeit
    break;
  case 2:
    temptime = now()-timedestination; //Countdown auf Zeit
    break;
  default:
    ltc1.setsecond(ltc, second());
    ltc1.setminute(ltc, minute());
    ltc1.sethour(ltc, hour());
    break; // Wird nicht benötigt, wenn Statement(s) vorhanden sind
  }
  
  Serial.printf("TimeIST : %02d:%02d:%02d\n", hour(), minute(), second());
  Serial.printf("TimeSOLL: %02d:%02d:%02d\n", hour(timedestination), minute(timedestination), second(timedestination));
  Serial.printf("TimeDIFF: %02d:%02d:%02d\n", hour(temptime), minute(temptime), second(temptime));
}
