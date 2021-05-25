byte lastkeycode;
elapsedMillis sinceLastKeyPress;

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
  }
}

void buttonClick(byte keycode) {
  Serial.print("Click: ");
  Serial.println(keycode);
  switch (keycode) {
    case BTNCLOCK:
      setmode(0);
      break;
    case BTNUP:
      setmode(4);
      break;
    case BTNDOWN:
      setmode(2);
      break;

    case BTNSTART:
      controlcounter(1);
      break;
    case BTNPAUSE:
      controlcounter(2);
      break;
    case BTNSTOP:
      controlcounter(3);
      break;
  }
}

void buttonLongClick(byte keycode) {
  Serial.print("Long Click: ");
  Serial.println(keycode);
  switch (keycode) {
    case BTNCLOCK:
      setmode(5);
      break;
    case BTNUP:
      setmode(3);
      break;
    case BTNDOWN:
      setmode(1);
      break;
  }
}


/*hourbutton.attachClick(clocksetup);
  hourbutton.attachLongPressStart(clocksetup);
  hourbutton.attachLongPressStop(clocksetup);*/
