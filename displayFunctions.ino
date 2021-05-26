void updateDisplay(ltcframe_t *ltc) {
  if (lastDisplayUpdate > 250) {      // "sincePrint" auto-increases
    lastDisplayUpdate = 0;
    uint8_t data[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

    if (showSetup) {
      if (setupmenu == 0) {
        memcpy(data, displayMenuBuzzer, sizeof(displayMenuBuzzer));
        if (storage.buzzerActive) {
          //On
          data[4] = 0x3f;
          data[5] = 0x54;
        } else {
          //Off
          data[3] = 0x3f;
          data[4] = 0x71;
          data[5] = 0x71;
        }

      } else if (setupmenu == 1) {
        memcpy(data, displayMenuBright, sizeof(displayMenuBright));
        data[5] = display.encodeDigit((int) (storage.brightness));
      }

    } else if (showOutput) {
      data[0] = display.encodeDigit(((int) (ltc->data >> 56) & 0x07));
      data[1] = display.encodeDigit(((int) (ltc->data >> 48) & 0x0f));
      data[2] = display.encodeDigit(((int) (ltc->data >> 40) & 0x07));
      data[3] = display.encodeDigit(((int) (ltc->data >> 32) & 0x0f));
      data[4] = display.encodeDigit(((int) (ltc->data >> 24) & 0x07));
      data[5] = display.encodeDigit(((int) (ltc->data >> 16) & 0x0f));
    } else {
      byte tsec = second(countertime[countermode]);
      byte tmin = minute(countertime[countermode]);
      byte thour = hour(countertime[countermode]);

      data[0] = display.encodeDigit((int) ((thour / 10) % 10));
      data[1] = display.encodeDigit((int) (thour % 10));
      data[2] = display.encodeDigit((int) ((tmin / 10) % 10));
      data[3] = display.encodeDigit((int) (tmin % 10));
      data[4] = display.encodeDigit((int) ((tsec / 10) % 10));
      data[5] = display.encodeDigit((int) (tsec % 10));
      //Serial.printf("Countertime  : %02d:%02d:%02d\n", thour, tmin, tsec);
    }


    if (externsync) {
      display.setColon(1);
    } else {
      if (lastColonBlink > 500) {
        display.setColon(!display.getColon());
        lastColonBlink = 0;
      }
    }
    display.setSegments(data);
    if (lastDebugOutput > 1000) {
      lastDebugOutput = 0;
      Serial.printf("OutLTC  : %02d:%02d:%02d.%02d\n", ltc1.hour(ltc), ltc1.minute(ltc), ltc1.second(ltc), ltc1.frame(ltc));
    }
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
    if (displayBlink == true) {
      //Serial.print("Brightness: ");
      //Serial.println(display.getBrightness() - 8);
      if (storage.buzzerActive) {
        digitalWrite(buzzerPin, !digitalRead(buzzerPin));
      }
      if ((display.getBrightness() - 8) > 0) {
        display.setBrightness(0);
      } else {
        display.setBrightness(storage.brightness);
      }
    } else {
      display.setBrightness(storage.brightness);
      digitalWrite(buzzerPin, LOW);
    }
    lastBtnLedBlink = 0;
  }
}
