void updateDisplay(ltcframe_t *ltc) {
  if (lastDisplayUpdate > 500) {      // "sincePrint" auto-increases
    lastDisplayUpdate = 0;
    uint8_t data[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    //Serial.printf("Display: %02d\n", ltc1.hour(ltc));
    data[0] = display.encodeDigit(((int) (ltc->data >> 56) & 0x07));
    data[1] = display.encodeDigit(((int) (ltc->data >> 48) & 0x0f));
    data[2] = display.encodeDigit(((int) (ltc->data >> 40) & 0x07));
    data[3] = display.encodeDigit(((int) (ltc->data >> 32) & 0x0f));
    data[4] = display.encodeDigit(((int) (ltc->data >> 24) & 0x07));
    data[5] = display.encodeDigit(((int) (ltc->data >> 16) & 0x0f));
    if(externsync){
      display.setColon(1);
    }else{
      display.setColon(!display.getColon());
    }
    display.setSegments(data);
    //Serial.printf("OutLTC  : %02d:%02d:%02d.%02d\n", ltc1.hour(ltc), ltc1.minute(ltc), ltc1.second(ltc), ltc1.frame(ltc));
  }
}

void updateBtnLed() {
  if (lastBtnLedUpdate > 100) {
    for (byte i = 0; i < 9; i++) {
      if (btn[i].ledstate == 0 || btn[i].ledstate == 1) {
        digitalWrite(btn[i].ledpin, btn[i].ledstate);
      }
    }
    lastBtnLedUpdate = 0;
  }
  if (lastBtnLedBlink > 500) {
    for (byte i = 0; i < 9; i++) {
      if (btn[i].ledstate == 2) {
        digitalWrite(btn[i].ledpin, !digitalRead(btn[i].ledpin));
      }
    }
    if(displayBlink==true){
      Serial.print("Brightness: ");
      Serial.println(display.getBrightness()-8);
      if((display.getBrightness()-8)>0){
        display.setBrightness(0);
      }else{
        display.setBrightness(displayBrightness);
      }
    }else{
      display.setBrightness(displayBrightness);
    }
    lastBtnLedBlink = 0;
  }
}
