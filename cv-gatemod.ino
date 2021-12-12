 

// arduino based eurorack module to add or remove single gate pulses
// very useful in combination with the TuringMachine (https://github.com/TomWhitwell/TuringMachine)
// this module gives us the possibility to nudge a locked sequence left and right on the timeline
// by adding or supressing sigle gate pulses
// further it is possible to increase the gate length with a potentiometer

// thanks to https://github.com/TomWhitwell/TuringMachine/issues/166
// thanks to https://fuzzysynth.blogspot.com/2016/07/interfacing-modular-and-microcontrollers.html
// thanks to https://github.com/joeSeggiola/arduino-eurorack-projects/blob/master/clock-divider/clock-divider.ino

// TODO: documentation & circuit schematics
// transistor S8050  https://www.el-component.com/images/bipolar-transistor/s8050-pinout.jpg

// blue led circuit 3V3:
//   short leg (-) -> GND
//   long leg (+) ->  resistor 4.7 K  -> PIN 8 + 9


// add supress active indicator resistor 530K
// add/supress gate indicator resistor 4.7K

// ===========================================================================


const int CLOCK_INPUT_PIN = 2;  // Input signal pin, must be usable for interrupts
const int GATE_OUTPUT_PIN = 3;  // Gate output

const int NOTCH_AHEAD_PIN = 4;  // pushbutton
const int NOTCH_BEHIND_PIN = 7; // pushbutton

const int LED_GATE_IN_PIN = 8;  // led indicating incoming clock pulses
const int LED_GATE_OUT_PIN = 9; // led indicating outgoing clock pulses

const int GATE_LENGTH_POT_PIN = A3; // potentiometer to increase gate length
const int potentiometerMaxValue = 680; // maximum retrieved value of analogRead() with a 10K trimpot

// ===========================================================================


unsigned long currentMilliSecond = 0;

bool incomingGateState = LOW;
unsigned long incomingGateLastOpen = 0;
unsigned long incomingGateInterval = 0;
unsigned long incomingGateLength = 0;

bool outgoingGateState = LOW;
unsigned long outgoingGateLastOpen = 0;

bool notchAheadButtonState = LOW;
bool notchBehindButtonState = LOW;

bool nowSupressingGate = false;

unsigned int addGatesQueue = 0;
unsigned int supressGatesQueue = 0;

const int minimumGapLength = 2; // milliseconds between LOW and HIGH; so maximum gate length will be gate interval minus this value [ms]

void gateOutHigh() {
  if (outgoingGateState == HIGH) {
    return;
  }
  outgoingGateState = HIGH;
  outgoingGateLastOpen = currentMilliSecond;
  digitalWrite(GATE_OUTPUT_PIN, LOW);   // inverted
  digitalWrite(LED_GATE_OUT_PIN, HIGH);
  
}

void gateOutLow() {
  if (outgoingGateState == LOW) {
    return;
  }
  outgoingGateState = LOW;
  digitalWrite(GATE_OUTPUT_PIN, HIGH); // inverted
  digitalWrite(LED_GATE_OUT_PIN, LOW);
}

void checkGateOutHigh() {
  if (outgoingGateState == HIGH) {
    return;  
  }
  if (supressGatesQueue > 0) {
    nowSupressingGate = true;
    supressGatesQueue--;
  }
  
  if (nowSupressingGate == true) {
    return;
  }
  
  if (addGatesQueue > 0) {
    gateOutHigh();
    gateOutLow();
    addGatesQueue--;
  }

  if (outgoingGateState == LOW && incomingGateState != outgoingGateState) {    
    gateOutHigh();
  }
}

void loopPushButtons() {
  if (digitalRead(NOTCH_AHEAD_PIN) != notchAheadButtonState) {
    notchAheadButtonState = digitalRead(NOTCH_AHEAD_PIN);
    if (notchAheadButtonState == HIGH) {
      // shouldn't it be the other way round? anyway...
      supressGatesQueue++;
    }
  }
  if (digitalRead(NOTCH_BEHIND_PIN) != notchBehindButtonState) {
    notchBehindButtonState = digitalRead(NOTCH_BEHIND_PIN);
    if (notchBehindButtonState == HIGH) {
      // shouldn't it be the other way round? anyway...
      addGatesQueue++;
    }
    //Serial.print("notch behind "); Serial.println(notchBehindButtonState);
  }
}


void checkGateOutLow() {
  if (outgoingGateState == LOW) {
    return;  
  }

  // check if we have to modify gate length
  int valueGateLengthPot = analogRead(GATE_LENGTH_POT_PIN);

  if (valueGateLengthPot < 2) {
    // no gate length modification - just copy the incoming gates
    if (incomingGateState != outgoingGateState) {
      gateOutLow();
    }
    return;
  }

  // we do have a gate length modification
  // the current gate length is bigger than incoming gate length
  int requestedGateLength = map(
    valueGateLengthPot,                    // value
    0,                                     // fromLow
    potentiometerMaxValue,                 // fromHigh
    incomingGateLength,                    // toLow
    incomingGateInterval-minimumGapLength  // toHigh
  );

  // just to make sure we have no quirks
  if (requestedGateLength < incomingGateLength) {
    requestedGateLength = incomingGateLength;
  }
  if (requestedGateLength < minimumGapLength) {
    requestedGateLength = minimumGapLength;
  }
  if (currentMilliSecond >= outgoingGateLastOpen + requestedGateLength) {
    gateOutLow();
  }
  
  
}

void handleIncomingGateChangeToHigh() {
  incomingGateInterval = currentMilliSecond - incomingGateLastOpen;
  incomingGateLastOpen = currentMilliSecond;
  digitalWrite(LED_GATE_IN_PIN, HIGH);
  incomingGateState = HIGH;
}

void handleIncomingGateChangeToLow() {
  incomingGateLength = currentMilliSecond - incomingGateLastOpen;
  digitalWrite(LED_GATE_IN_PIN, LOW);
  incomingGateState = LOW;
  nowSupressingGate = false;
}

void setup()
{
  Serial.begin(115200);
  setupCvClocksAndLeds();
}

void loop()
{
  currentMilliSecond = millis();
  checkGateOutLow();
  checkGateOutHigh();
  loopPushButtons();
}

void setupCvClocksAndLeds() {
  // Interrupts
  pinMode(CLOCK_INPUT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(CLOCK_INPUT_PIN), isrClock, CHANGE);
  
  pinMode(NOTCH_AHEAD_PIN, INPUT_PULLUP);
  pinMode(NOTCH_BEHIND_PIN, INPUT_PULLUP);

  pinMode(GATE_OUTPUT_PIN, OUTPUT);
  pinMode(LED_GATE_IN_PIN, OUTPUT);
  pinMode(LED_GATE_OUT_PIN, OUTPUT);
}


void isrClock() {
  if (digitalRead(CLOCK_INPUT_PIN) == HIGH) {
    handleIncomingGateChangeToLow();
    return;
  }
  handleIncomingGateChangeToHigh();
}
