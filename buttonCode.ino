byte lastkeycode;
elapsedMillis sinceLastKeyPress, sinceLastLongKeyPress;
int counter;

void buttonTick() {
  byte keycode = display.getKeyCode();
  if (lastkeycode != keycode) {
    if (keycode == 0) {
      if (sinceLastKeyPress < 1000) {
        buttonClick(lastkeycode);
      } else if (sinceLastKeyPress >= 1000) {
        buttonLongClick(lastkeycode);
      }
    } else {
      sinceLastKeyPress = 0;
    }
    lastkeycode = keycode;
    counter = 0;
  } else if (lastkeycode == keycode && keycode != 0) {
    if (sinceLastKeyPress > 1000) {
      if (sinceLastLongKeyPress > 100) {
        buttonLongPress(lastkeycode);
        counter++;
        sinceLastLongKeyPress = 0;
      }
    }
  }
}

void buttonLongPress(byte keycode) {
  /*Serial.print("LongPress: ");
    Serial.print(keycode);
    Serial.print(" -Counter: ");
    Serial.println(counter);*/
  switch (keycode) {
    case BTNHOUR:
      setuptime(0);
      break;
    case BTNMINUTE:
      setuptime(1);
      break;
    case BTNSECOND:
      setuptime(2);
      break;
  }
}

void buttonClick(byte keycode) {
  //Serial.print("Click: ");
  //Serial.println(keycode);
  switch (keycode) {
    case BTNCLOCK:
      if (countermode == 5) {
        clocksetup(MENUNEXT);
      } else {
        setmode(0);
      }
      break;
    case BTNUP:
      if (countermode == 5) {
        clocksetup(MENUUP);
      } else {
        setmode(4);
      }
      break;
    case BTNDOWN:
      if (countermode == 5) {
        clocksetup(MENUDOWN);
      } else {
        setmode(2);
      }
      break;
    case BTNHOUR:
      setuptime(0);
      break;
    case BTNMINUTE:
      setuptime(1);
      break;
    case BTNSECOND:
      setuptime(2);
      break;

    case BTNSTART:
      controlcounter(1);
      break;
    case BTNPAUSE:
      controlcounter(2);
      break;
    case BTNSTOP:
      if (reversecountmode) {
        reversecountmode = !reversecountmode;
        btn[8].ledstate = 0;
      } else {
        controlcounter(3);
      }
      break;
  }
}

void buttonLongClick(byte keycode) {
  //Serial.print("Long Click: ");
  //Serial.println(keycode);
  switch (keycode) {
    case BTNCLOCK:
      if (countermode == 5) {
        saveConfig();
        setfpstimer();
        setmode(0);
      } else {
        setmode(5);
      }
      break;
    case BTNUP:
      setmode(3);
      break;
    case BTNDOWN:
      setmode(1);
      break;
    case BTNSTOP:
      reversecountmode = !reversecountmode;
      if (reversecountmode) {
        btn[8].ledstate = 2;
      } else {
        btn[8].ledstate = 0;
      }
      break;
  }
}


/*hourbutton.attachClick(clocksetup);
  hourbutton.attachLongPressStart(clocksetup);
  hourbutton.attachLongPressStop(clocksetup);*/
