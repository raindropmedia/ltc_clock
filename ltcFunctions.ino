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

  if (reverseltc) {
    if (bitcount < 16) {
      bitval = (int) (ltct.sync >> bitcount) & 0x01;
    } else {
      bitval = (int) (ltct.data >> (bitcount - 16)) & 0x01; 
    }
  } else {
    if (bitcount < 64) {
      bitval = (int) (ltct.data >> bitcount) & 0x01;
    } else {
      bitval = (int) (ltct.sync >> (bitcount - 64)) & 0x01; //backward
    }
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
          t = storage.fps;
        }
        if (now() >= timedestination - storage.flash) {
          displayBlink = true;
        }
        if (now() >= timedestination) {
          counterrun = false;
          t = 0;
          btn[6].ledstate = 0;
          btn[7].ledstate = 0;
          btn[8].ledstate = 2;
          displayBlink = false;
          Serial.println("Countdown erreicht!");
          break;
        }
        timediff = timedestination - now(); //Countdown auf def. Zeit
        //Serial.printf("Start: %d Diff:%d Dest:%d Now:%d\n", timestart, timediff, timedestination, now());
        t--;
      }
      break;
    case 2:
      if (counterrun) {
        if (lastsecond != second()) {
          lastsecond = second();
          t = storage.fps;
        }
        timediff =  timedestination - now(); //Countdown auf 0
        if (timediff < storage.flash) {
          displayBlink = true;
        }
        if (timediff < 0) {
          timediff = 0;
          t = 0;
          counterrun = false;
          btn[6].ledstate = 0;
          btn[7].ledstate = 0;
          btn[8].ledstate = 2;
          displayBlink = false;
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
        if (now() >= timedestination - storage.flash) {
          displayBlink = true;
        }
        if (now() >= timedestination) {
          counterrun = false;
          timediff++;
          btn[6].ledstate = 0;
          btn[7].ledstate = 0;
          btn[8].ledstate = 2;
          displayBlink = false;
          Serial.println("Countup erreicht!");
          break;
        }
        timediff = now() - timestart; //Countup auf def. Zeit
        //Serial.printf("Start: %d Diff:%d Dest:%d Now:%d\n", timestart, timediff, timedestination, now());
      }
      if (t >= (int) storage.fps) t = 0;
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
          btn[6].ledstate = 0;
          btn[7].ledstate = 0;
          btn[8].ledstate = 2;
          displayBlink = false;
          Serial.println("Countup 99 erreicht!");
          break;
        }
        timediff = now() - timestart; //Countup von 0
      }
      if (t >= (int) storage.fps) t = 0;
      break;
    default:
      t++;
      if (lastsecond != second()) {
        lastsecond = second();
        t = 0;
      }
      timediff = (hour() * 3600 + minute() * 60 + second() );
      if (t >= (int) storage.fps) t = 0;
      break; // Wird nicht benötigt, wenn Statement(s) vorhanden sind
  }
  ltc1.setframe(&ltctemp, t);
  ltc1.setsecond(&ltctemp, second(timediff));
  ltc1.setminute(&ltctemp, minute(timediff));
  ltc1.sethour(&ltctemp, hour(timediff));


  //set parity:
  int parity = (!getParity(ltctemp.data)) & 0x01;
  if ((int) storage.fps == 25) {
    ltctemp.data |= (uint64_t) parity << 59;
  } else {
    ltctemp.data |= (uint64_t) parity << 27;
  }

  ltc = ltctemp;
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
