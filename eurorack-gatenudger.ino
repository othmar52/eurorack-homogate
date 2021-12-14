 

/**
 * https://github.com/othmar52/eurorack-gatenudger
 * arduino based eurorack module to add or remove single gate pulses
 * very useful in combination with the TuringMachine (https://github.com/TomWhitwell/TuringMachine)
 * this module gives us the possibility to nudge a locked sequence left and right on the timeline
 * by adding or supressing sigle gate pulses
 * further it is possible to increase the gate length with a potentiometer
 *
 * thanks to https://github.com/TomWhitwell/TuringMachine/issues/166
 * thanks to https://fuzzysynth.blogspot.com/2016/07/interfacing-modular-and-microcontrollers.html
 * thanks to https://github.com/joeSeggiola/arduino-eurorack-projects/blob/master/clock-divider/clock-divider.ino
 *
 * TODO: fast gate length change bug
 * on very fast decreasing gate length from max to min we get some unwanted shift
 */

const int CLOCK_INPUT_PIN = 2;  // Input signal pin, must be usable for interrupts
const int GATE_OUTPUT_PIN = 3;  // Gate output

const int NUDGE_AHEAD_PIN = 4;  // pushbutton
const int NUDGE_BEHIND_PIN = 7; // pushbutton

const int LED_GATE_IN_PIN = 8;  // led indicating incoming clock pulses
const int LED_GATE_OUT_PIN = 9; // led indicating outgoing clock pulses

const int GATE_LENGTH_POT_PIN = A3; // potentiometer to increase gate length
const int potentiometerMaxValue = 680; // maximum retrieved value of analogRead() with the 10K trimpot i am using. shouldn't this be 1024?

unsigned long currentMilliSecond = 0;

bool incomingGateState = LOW;
unsigned long incomingGateLastOpen = 0;
unsigned long incomingGateInterval = 0;
unsigned long incomingGateLength = 0;

bool outgoingGateState = LOW;
unsigned long outgoingGateLastOpen = 0;

bool nudgeAheadButtonState = LOW;
bool nudgeBehindButtonState = LOW;

bool nowSupressingGate = false;

unsigned int addGatesQue = 0;
unsigned int supressGatesQue = 0;

const int minimumGapLength = 2; // milliseconds between LOW and HIGH; so maximum gate length will be gate interval minus this value [milliseconds]

void setup()
{
  // Interrupts
  pinMode(CLOCK_INPUT_PIN, INPUT);
  attachInterrupt(
    digitalPinToInterrupt(CLOCK_INPUT_PIN),
    handleIncomingGateChange,
    CHANGE
  );

  pinMode(NUDGE_AHEAD_PIN, INPUT_PULLUP);
  pinMode(NUDGE_BEHIND_PIN, INPUT_PULLUP);

  pinMode(GATE_OUTPUT_PIN, OUTPUT);
  pinMode(LED_GATE_IN_PIN, OUTPUT);
  pinMode(LED_GATE_OUT_PIN, OUTPUT);
  // Serial.begin(115200);
}

void loop()
{
  currentMilliSecond = millis();
  loopPushButtons();
  checkGateOutLow();
  checkGateOutHigh();
}

/**
 * send gate out high to TRS jack with led indication
 */
void gateOutHigh() {
  if (outgoingGateState == HIGH) {
    return;
  }
  outgoingGateState = HIGH;
  outgoingGateLastOpen = currentMilliSecond;
  digitalWrite(GATE_OUTPUT_PIN, LOW);   // inverted
  digitalWrite(LED_GATE_OUT_PIN, HIGH);
}

/**
 * send gate out low to TRS jack with led indication
 */
void gateOutLow() {
  if (outgoingGateState == LOW) {
    return;
  }
  outgoingGateState = LOW;
  digitalWrite(GATE_OUTPUT_PIN, HIGH); // inverted
  digitalWrite(LED_GATE_OUT_PIN, LOW);
}

/**
 * check if the outgoing gate state is same as incoming gate state
 * but avoid to send a new gate in case we have to supress the current gate pulse
 */
void checkGateOutHigh() {
  if (outgoingGateState == HIGH) {
    // already high - no need for further checks
    return;  
  }

  if (supressGatesQue > 0) {
    nowSupressingGate = true;
    supressGatesQue--;
    return;
  }

  if (addGatesQue > 0) {
    gateOutHigh();
    gateOutLow();
    addGatesQue--;
  }

  if (incomingGateState == HIGH) {
    gateOutHigh();
  }
}

/*
 * check if the current millisecond requires out gate set to low
 * but take the increase gate length pot value into account
 */
void checkGateOutLow() {
  if (outgoingGateState == LOW) {
    // already low - no need for further checks
    return;  
  }

  // check if we have an increased gate length
  int valueGateLengthPot = analogRead(GATE_LENGTH_POT_PIN);

  if (valueGateLengthPot < 2) {
    // potentiometer is fully turned counter clockwise
    // no gate length modification - just pass through the incoming gate pulses
    if (incomingGateState == LOW) {
      gateOutLow();
    }
    return;
  }

  // we do have an increased gate length due to potentiometer position
  int modifiedGateLength = calculateModifiedGateLengthMilliSeconds(
    valueGateLengthPot
  );

  // check if it's time to close the open gate
  if (currentMilliSecond < outgoingGateLastOpen + modifiedGateLength) {
    return;
  }
  gateOutLow();
}

/*
 * the current gate length is longer than incoming gate length
 * with the potentiometer fully turned clockwise the gate length will
 * be as long as the incoming gate interval minus 2 milliseconds
 */
int calculateModifiedGateLengthMilliSeconds(int potentiometerValue) {
  int requestedGateLength = map(
    potentiometerValue,                    // value
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
  return requestedGateLength;
}

/*
 * incoming gate has changed its state
 */
void handleIncomingGateChange() {
  if (digitalRead(CLOCK_INPUT_PIN) == HIGH) {
    handleIncomingGateChangeToLow();
    return;
  }
  handleIncomingGateChangeToHigh();
}

/*
 * incoming gate has changed its state to HIGH
 * set a few variables and make sure the LED is on
 */
void handleIncomingGateChangeToHigh() {
  incomingGateInterval = currentMilliSecond - incomingGateLastOpen;
  incomingGateLastOpen = currentMilliSecond;
  digitalWrite(LED_GATE_IN_PIN, HIGH);
  incomingGateState = HIGH;
}

/*
 * incoming gate has changed its state to LOW
 * set a few variables and make sure the LED is off
 */
void handleIncomingGateChangeToLow() {
  incomingGateLength = currentMilliSecond - incomingGateLastOpen;
  digitalWrite(LED_GATE_IN_PIN, LOW);
  incomingGateState = LOW;
  nowSupressingGate = false;
}

/*
 * check if push button states has changed and add changes to a que
 */
void loopPushButtons() {
  if (digitalRead(NUDGE_AHEAD_PIN) != nudgeAheadButtonState) {
    nudgeAheadButtonState = digitalRead(NUDGE_AHEAD_PIN);
    if (nudgeAheadButtonState == HIGH) {
      // shouldn't it be adding instead of supressing? anyway, it works...
      supressGatesQue++;
    }
  }
  if (digitalRead(NUDGE_BEHIND_PIN) != nudgeBehindButtonState) {
    nudgeBehindButtonState = digitalRead(NUDGE_BEHIND_PIN);
    if (nudgeBehindButtonState == HIGH) {
      // shouldn't it be supressing instead of adding? anyway, it works...
      addGatesQue++;
    }
  }
}
