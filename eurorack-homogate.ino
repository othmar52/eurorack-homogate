 

/**
 * https://github.com/othmar52/eurorack-homogate
 * arduino based eurorack module to add or remove single gate pulses
 * very useful in combination with the TuringMachine (https://github.com/TomWhitwell/TuringMachine)
 * this module gives us the possibility to nudge a locked sequence left and right on the timeline
 * by adding or supressing sigle gate pulses
 * further it is possible to de-/increase the gate length with a potentiometer (6 milliseconds - incoming gate interval)
 * further it is possible to add a time offset to the outgoing gate (0 milliseconds - incoming gate interval)
 *
 * thanks to https://github.com/TomWhitwell/TuringMachine/issues/166
 * thanks to https://fuzzysynth.blogspot.com/2016/07/interfacing-modular-and-microcontrollers.html
 * thanks to https://github.com/joeSeggiola/arduino-eurorack-projects/blob/master/clock-divider/clock-divider.ino
 *
 * TODO: fast gate length change bug
 * on very fast decreasing gate length from max to min we get some unwanted shift
 */

// you can have as many instances as you have pins available
// you need at least 8 arduino pins per instance (incl. 2 analogue and 1 that supports interrupts)
#define NUM_INSTANCES 2

uint8_t debounceButtonMilliSeconds = 10;
uint16_t potentiometerMaxValue = 1023;
uint8_t minGateLengthMilliSeconds = 6;

typedef struct {
  uint8_t gatePin;
  uint8_t ledPin;
  bool isOpen = false;
  unsigned long lastOpen = 0;
  long interval= 0;
  long length = minGateLengthMilliSeconds; // make sure first gate is sent out without knowing interval or length of incoming gate
  long offset = 0;
  long cyclesHigh = 0; 
  long cyclesLow = 0; 
} gateParams;

typedef struct {
  uint8_t nudgeAheadPin;
  uint8_t nudgeBehindPin;
  uint8_t potLengthPin;
  bool invertPotLength = true; // on DIY PCB +/- may be reversed for easier layout
  uint8_t potOffsetPin;
  bool invertPotOffset = false; // on DIY PCB +/- may be reversed for easier layout
  uint8_t resetPin;
  gateParams in;
  gateParams out;
  int16_t cyclesDiff = 0; // TODO: check if we really need this. plan is to implement an auto correction for 8 bars
  unsigned long openedFor = 0;
  bool currentlySupressing = false;
  unsigned long currentlyAdding = 0;
  bool reopenAfterAdd = false;
  bool nudgeAheadState = LOW;
  bool nudgeBehindState = LOW;

  unsigned int addGatesQue = 0;
  unsigned int supressGatesQue = 0;
  unsigned long lastQueAdd = 0; // for debouncing buttons

} ioInstance;

typedef struct {
  ioInstance io[NUM_INSTANCES];
} homogate;

struct potValues100 {
  uint8_t length;
  uint8_t offset;
};

homogate hg;

/**
 * last argument "resetPin" for a feature that is not implemented yet
 */
ioInstance addGateBenderInstance(
  const uint8_t gateInPin,
  const uint8_t gateOutPin,
  const uint8_t ledInPin,
  const uint8_t ledOutPin,
  const uint8_t nudgeAheadPin,
  const uint8_t nudgeBehindPin,
  const uint8_t potLengthPin,
  const uint8_t potOffsetPin,
  const uint8_t resetPin) {

  ioInstance io = addGateBenderInstance(
    gateInPin,
    gateOutPin,
    ledInPin,
    ledOutPin,
    nudgeAheadPin,
    nudgeBehindPin,
    potLengthPin,
    potOffsetPin
  );
  io.resetPin = resetPin;
  pinMode(resetPin, INPUT_PULLUP);
  return io;
}

ioInstance addGateBenderInstance(
  const uint8_t gateInPin,
  const uint8_t gateOutPin,
  const uint8_t ledInPin,
  const uint8_t ledOutPin,
  const uint8_t nudgeAheadPin,
  const uint8_t nudgeBehindPin,
  const uint8_t potLengthPin,
  const uint8_t potOffsetPin) {

  ioInstance io;
  gateParams in;
  gateParams out;
  io.in = in;
  io.in.gatePin = gateInPin;
  io.in.ledPin = ledInPin;
  io.out = out;
  io.out.gatePin = gateOutPin;
  io.out.ledPin = ledOutPin;

  io.nudgeAheadPin = nudgeAheadPin;
  io.nudgeBehindPin = nudgeBehindPin;
  io.potLengthPin = potLengthPin;
  io.potOffsetPin = potOffsetPin;

  pinMode(gateInPin, INPUT);
  pinMode(gateOutPin, OUTPUT);
  pinMode(nudgeAheadPin, INPUT_PULLUP);
  pinMode(nudgeBehindPin, INPUT_PULLUP);
  pinMode(ledInPin, OUTPUT);
  pinMode(ledOutPin, OUTPUT);
  pinMode(potLengthPin, INPUT);
  pinMode(potOffsetPin, INPUT);
  return io;
}

void setup() {
  //Serial.begin(115200);
  //Serial.println("hello. i am homogate");
  hg.io[0] = addGateBenderInstance(
    2,  // gateInPin
    4,  // gateOutPin
    A5, // ledInPin
    A6, // ledOutPin
    6,  // nudgeAheadPin
    5,  // nudgeBehindPin
    A4, // potLengthPin
    A7  // potOffsetPin
  );

  // arguments seems to be impossible for interrupts!?
  // so we have to attach those for each instance
  attachInterrupt(
    digitalPinToInterrupt(hg.io[0].in.gatePin),
    handleIncomingGateChange1,
    CHANGE
  );

  hg.io[1] = addGateBenderInstance(
    3,  // gateInPin
    12,  // gateOutPin
    A0, // ledInPin
    A1, // ledOutPin
    10,  // nudgeAheadPin
    11,  // nudgeBehindPin
    A2, // potLengthPin
    A3  // potOffsetPin
  );

  // arguments seems to be impossible for interrupts!?
  // so we have to attach those for each instance
  attachInterrupt(
    digitalPinToInterrupt(hg.io[1].in.gatePin),
    handleIncomingGateChange2,
    CHANGE
  );
}

void loop() {
  loopPushButtons();
  loopPots();
  checkGateOutHigh();
  checkGateOutLow();
}

/**
 * send gate out high to TS jack with led indication
 */
void gateOutHigh(uint8_t i) {
  if (hg.io[i].out.isOpen == true) {
    return;
  }
  hg.io[i].out.isOpen = true;
  hg.io[i].out.cyclesHigh++;
  hg.io[i].out.lastOpen = millis();
  digitalWrite(hg.io[i].out.gatePin, LOW);   // inverted
  digitalWrite(hg.io[i].out.ledPin, HIGH);
}

/**
 * send gate out low to TS jack with led indication
 */
void gateOutLow(uint8_t i) {
  if (hg.io[i].out.isOpen == false) {
    return;
  }
  hg.io[i].out.isOpen = false;
  hg.io[i].out.cyclesLow++;
  digitalWrite(hg.io[i].out.gatePin, HIGH);   // inverted
  digitalWrite(hg.io[i].out.ledPin, LOW);

  /*
  if (hg.io[i].in.cyclesLow % 4 == 0) {
    Serial.print("chI: ");
    Serial.print(hg.io[i].in.cyclesHigh);
    Serial.print(" clI: ");
    Serial.println(hg.io[i].in.cyclesLow);
    Serial.print("chO: ");
    Serial.print(hg.io[i].out.cyclesHigh);
    Serial.print(" clO: ");
    Serial.println(hg.io[i].out.cyclesLow);
  }
  */
}


/**
 * check if the outgoing gate state is same as incoming gate state
 * but avoid to send a new gate in case we have to supress the current gate pulse
 */
void checkGateOutHigh() {
  for (uint8_t i=0; i<NUM_INSTANCES; i++) {
    if (hg.io[i].out.isOpen == true) {
      // already high - no need for further checks
      continue;  
    }

    if (hg.io[i].currentlySupressing == true) {
      continue;  
    }
  
    if (hg.io[i].supressGatesQue > 0 && hg.io[i].in.isOpen == true) {
      hg.io[i].supressGatesQue--;
      //Serial.println("supressing gate");
      hg.io[i].currentlySupressing = true;
      continue;
    }
  
    if (hg.io[i].addGatesQue > 0) {
      handleAddGate(i);
      continue;
    }

    if (millis() >= hg.io[i].in.lastOpen + hg.io[i].out.offset && hg.io[i].openedFor != hg.io[i].in.lastOpen) {
      hg.io[i].openedFor = hg.io[i].in.lastOpen;
      gateOutHigh(i);
    }
  }
}

void handleAddGate(uint8_t i) {
  if (hg.io[i].currentlyAdding == 0 && hg.io[i].out.isOpen == true) {
    //Serial.println("closing open gate for adding another one");
    gateOutLow(i);
    hg.io[i].reopenAfterAdd = true;
    return;
  }
  if (hg.io[i].currentlyAdding == 0 && hg.io[i].out.isOpen == false) {
    //Serial.println("adding gate");
    gateOutHigh(i);
    hg.io[i].currentlyAdding = millis();
    return;
  }
  // make sure the added gate has a duration of minGateLengthMilliSeconds
  if (hg.io[i].currentlyAdding > 0 && millis() >= hg.io[i].currentlyAdding + minGateLengthMilliSeconds) {
    //Serial.println("closing added gate");
    gateOutLow(i);
    hg.io[i].currentlyAdding = 0;
    hg.io[i].addGatesQue--;
    if (hg.io[i].reopenAfterAdd == true) {
      //Serial.println("reopening gate");
      gateOutHigh(i);
      hg.io[i].reopenAfterAdd = false;
    }
  }
}

/*
 * check if the current millisecond requires out gate set to low
 * but take the increase gate length pot value into account
 */
void checkGateOutLow() {
  for (uint8_t i=0; i<NUM_INSTANCES; i++) {
    if (hg.io[i].out.isOpen == false) {
      // already low - no need for further checks
      //Serial.println("check low fut");
      continue;  
    }
  
    if (hg.io[i].addGatesQue > 0) {
      handleAddGate(i);
    }
    
    //Serial.println("check low 1");

    if (millis() >= hg.io[i].in.lastOpen + hg.io[i].out.offset + hg.io[i].out.length) {
      gateOutLow(i);
    }
    /*
    Serial.print("check low");
    Serial.print(hg.io[i].in.lastOpen);
    Serial.print(" x ");
    Serial.print(hg.io[i].out.offset);
    Serial.print(" x ");
    Serial.println(millis());
    */
  }
}

void handleIncomingGateChange1() {
  handleIncomingGateChange(0);
}

void handleIncomingGateChange2() {
  handleIncomingGateChange(1);
}

/*
 * incoming gate has changed its state
 */
void handleIncomingGateChange(uint8_t i) {
  if (digitalRead(hg.io[i].in.gatePin) == HIGH) {
    handleIncomingGateChangeToLow(i);
    return;
  }
  handleIncomingGateChangeToHigh(i);
}

/*
 * incoming gate has changed its state to HIGH
 * set a few variables and make sure the LED is on
 */
void handleIncomingGateChangeToHigh(uint8_t i) {
  hg.io[i].currentlySupressing = false;
  hg.io[i].in.cyclesHigh++;
  hg.io[i].in.interval = millis() - hg.io[i].in.lastOpen;
  // it does not make sense that gate length greater than interval
  if (hg.io[i].in.interval < minGateLengthMilliSeconds) {
    hg.io[i].in.interval = minGateLengthMilliSeconds;
  }
  hg.io[i].in.lastOpen = millis();
  digitalWrite(hg.io[i].in.ledPin, HIGH);
  hg.io[i].in.isOpen = true;
  //Serial.println("handleIncomingGateChangeToHigh");

  /*
  Serial.print(" IV: ");
  Serial.print(incomingGateInterval[i]);
  Serial.print(" IL: ");
  Serial.print(incomingGateLength[i]);
  Serial.print(" OL: ");
  Serial.print(outgoingGateLength[i]);
  Serial.print(" OO: ");
  Serial.println(currentOffsetMilliseconds[i]);
  */
  
}

/*
 * incoming gate has changed its state to LOW
 * set a few variables and make sure the LED is off
 */
void handleIncomingGateChangeToLow(uint8_t i) {
  hg.io[i].in.cyclesLow++;
  hg.io[i].in.length = millis() - hg.io[i].in.lastOpen;

  // it does not make sense that gate length greater than interval
  if (hg.io[i].in.length > hg.io[i].in.interval) {
    hg.io[i].in.length = minGateLengthMilliSeconds;
  }
  digitalWrite(hg.io[i].in.ledPin, LOW);
  hg.io[i].in.isOpen = false;

  /*
  Serial.print("--------- ");
  Serial.print(i);
  Serial.println(" ---------");
  Serial.print(" IV: ");
  Serial.print(hg.io[i].in.interval);
  Serial.print(" IL: ");
  Serial.println(hg.io[i].in.length);
  Serial.print(" OL: ");
  Serial.print(hg.io[i].out.length);
  Serial.print(" OO: ");
  Serial.println(hg.io[i].out.offset);
  */
}

/*
 * check if push button states has changed and add changes to a que
 */
void loopPushButtons() {
  bool newButtonState;
  for (uint8_t i=0; i<NUM_INSTANCES; i++) {
    newButtonState = digitalRead(hg.io[i].nudgeAheadPin);
    if (newButtonState != hg.io[i].nudgeAheadState) {
      hg.io[i].nudgeAheadState = newButtonState;
      if (newButtonState == HIGH && millis() > hg.io[i].lastQueAdd + debounceButtonMilliSeconds) {
        hg.io[i].lastQueAdd = millis();
        hg.io[i].addGatesQue++;
        hg.io[i].cyclesDiff++;
        //Serial.println("got button +");
      }
    }
    newButtonState = digitalRead(hg.io[i].nudgeBehindPin);
    if (newButtonState != hg.io[i].nudgeBehindState) {
      hg.io[i].nudgeBehindState = newButtonState;
      if (newButtonState == HIGH && millis() > hg.io[i].lastQueAdd + debounceButtonMilliSeconds) {
        hg.io[i].lastQueAdd = millis();
        hg.io[i].supressGatesQue++;
        hg.io[i].cyclesDiff--;
        //Serial.println("got button -");
      }
    }
  }
}

/**
 * this function reads the potentiometers for gate length and gate offset
 * and produces resonable millisecond values for the output gate
 */
void loopPots() {
  struct potValues100 potValues;
  for (uint8_t i=0; i<NUM_INSTANCES; i++) {
    potValues = getPotValues(i);
    hg.io[i].out.offset = map(potValues.offset, 0, 99, 0, hg.io[i].in.interval - minGateLengthMilliSeconds);
    if (hg.io[i].out.offset < 0) {
      hg.io[i].out.offset = 0;
    }
    hg.io[i].out.length = map(potValues.length, 0, 99, minGateLengthMilliSeconds, hg.io[i].in.interval);
    if (hg.io[i].out.length < minGateLengthMilliSeconds) {
      hg.io[i].out.length = minGateLengthMilliSeconds;
    }
    if (hg.io[i].out.length > hg.io[i].in.interval - hg.io[i].out.offset - minGateLengthMilliSeconds) {
      hg.io[i].out.length = hg.io[i].in.interval - hg.io[i].out.offset - minGateLengthMilliSeconds;
      if (hg.io[i].out.length < minGateLengthMilliSeconds) {
        hg.io[i].out.length = minGateLengthMilliSeconds;
      }
    }   
  }
}

/**
 * in case we have maximum gate length (~ incoming gate interval) AND maximum offset (~ incoming gate interval)
 * we have to give something priority
 * 
 * use case 1:
 *   (lenght pot % + offset pot %) < 100: no need for special treatment
 *   
 * use case 2:
 *   (lenght pot % + offset pot %) > 100: we priorize offset and reduce the length
 * 
 */
struct potValues100 getPotValues(uint8_t i) {
  struct potValues100 potValues;

  uint16_t potMin;
  uint16_t potMax;
  if (hg.io[i].invertPotLength == false) {
    potMin = 0;
    potMax = potentiometerMaxValue;
  } else {
    potMin = potentiometerMaxValue;
    potMax = 0;
  }
  potValues.length = map(analogRead(hg.io[i].potLengthPin), potMin, potMax, 0, 99);

  if (hg.io[i].invertPotOffset == false) {
    potMin = 0;
    potMax = potentiometerMaxValue;
  } else {
    potMin = potentiometerMaxValue;
    potMax = 0;
  }
  potValues.offset = map(analogRead(hg.io[i].potOffsetPin), potMin, potMax, 0, 99);

  uint8_t potValueSum = potValues.length + potValues.offset;
  if (potValueSum < 99) {
    return potValues;
    
  }
  if (potValues.offset == 99) {
    potValues.offset = 98;
  }
  potValues.length = 99 - potValues.offset;
  return potValues;
}
