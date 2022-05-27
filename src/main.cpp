#if __cplusplus > 199711L
#define register
#endif

#include "Arduino.h"
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <FastLED.h>
#include <AccelStepper.h>

enum REMOTE : uint64_t
{
  up = 0xFF18E7,
  down = 0xFF4AB5,
  left = 0xFF10EF,
  right = 0xFF5AA5,
  ok = 0xFF38C7,
  star = 0xFF6897,
  hash = 0xFFB04F,
  _0 = 0xFF9867,
  _1 = 0xFFA25D,
  _2 = 0xFF629D,
  _3 = 0xFFE21D,
  _4 = 0xFF22DD,
  _5 = 0xFF02FD,
  _6 = 0xFFC23D,
  _7 = 0xFFE01F,
  _8 = 0xFFA857,
  _9 = 0xFF906F
};

enum RUN_STATE
{
  HOMING,
  STANDBY,
  BET,
  WIN
};

RUN_STATE runState = RUN_STATE::HOMING;

// Pins

const uint16_t endstopPin = 5;
const uint16_t stepDirPin = 4;
const uint16_t stepMovePin = 0;

const uint16_t leftLightsPin = 12;
const uint16_t rightLightsPin = 14;

const uint16_t boxLightsPin = 13;
const uint16_t irRecvPin = 3;

// IR vars
IRrecv irrecv(irRecvPin);
decode_results irResults;

// Button var. Shares an input pin with the IR receiver.
int irRecvLowCount = 0;

// Stepper
AccelStepper stepper(AccelStepper::DRIVER, stepDirPin, stepMovePin);

// Lights. Third one is in the box.
CRGB ledStrips[3][300];
int ledStripLengths[] = {300, 300, 20};

void rotate_init(CRGB leds[], int len, CRGB color)
{
  for (int i = 0; i < len - 1; i += 3)
  {
    leds[i] = color;
  }
}

void rotate_run(CRGB leds[], int len)
{
  CRGB first = leds[0];

  for (int i = 0; i < len - 1; i++)
  {
    leds[i] = leds[i + 1];
  }

  leds[len - 1] = first;
}

void setRunState(RUN_STATE newState)
{
  if (runState == newState)
    return;

  FastLED.clear();

  CRGB color;

  switch (newState)
  {
  case RUN_STATE::STANDBY:
    color = CRGB::Red;
    stepper.moveTo(-10000);
    break;
  case RUN_STATE::BET:
    color = CRGB::Yellow;
    stepper.moveTo(0);
    break;
  case RUN_STATE::WIN:
    color = CRGB::Green;
    break;
  }

  for (int s = 0; s < 3; s++)
  {
    rotate_init(ledStrips[s], ledStripLengths[s], color);
  }

  runState = newState;
}

int tick = 0;
int ledAnimateSpeed = 5000;

void animateLEDs()
{
  tick++;
  if (tick < ledAnimateSpeed)
    return;

  // Each strip
  for (int s = 0; s < 3; s++)
  {
    rotate_run(ledStrips[s], ledStripLengths[s]);
  }

  // Write led data.
  FastLED.show();

  tick = 0;
}

void setup()
{

  // Wait for serial to be establised.
  Serial.begin(115200);
  while (!Serial)
    delay(50);

  Serial.println();
  Serial.println("Boot...");
  Serial.println();
  Serial.println();

  // Listen for IR messages
  pinMode(irRecvPin, INPUT_PULLUP);
  irrecv.enableIRIn();

  // Configure LEDs.
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 5000);
  FastLED.addLeds<NEOPIXEL, leftLightsPin>(ledStrips[0], ledStripLengths[0]);
  FastLED.addLeds<NEOPIXEL, rightLightsPin>(ledStrips[1], ledStripLengths[1]);
  FastLED.addLeds<NEOPIXEL, boxLightsPin>(ledStrips[2], ledStripLengths[2]);
  FastLED.clear(true);

  // Endstop is normally closed, and tied to ground.
  pinMode(endstopPin, INPUT_PULLUP);

  // Stepper homes on power.
  stepper.setMaxSpeed(1000.0);
  stepper.setAcceleration(200.0);
  stepper.move(15000);

  Serial.print("Homing...");
  while (digitalRead(endstopPin) == LOW)
  {
    stepper.run();
    yield();
  }

  Serial.println("done.");

  // Increase move speed after home.
  stepper.setCurrentPosition(100);
  stepper.setMaxSpeed(10000.0);
  stepper.setAcceleration(1000.0);

  // Start state machine.
  setRunState(RUN_STATE::STANDBY);
}

void loop()
{

  // Count cycles while the button is down.
  if (digitalRead(irRecvPin) == LOW)
  {
    irRecvLowCount++;
  }
  else
  {
    // On LOW to HIGH, if enough cycles have been LOW, it's likely a button press, not an IR message.
    if (irRecvLowCount > 3000)
    {
      Serial.println("BET BUTTON PRESSED");
      setRunState(RUN_STATE::BET);
    }

    irRecvLowCount = 0;
  }

  if (irrecv.decode(&irResults))
  {
    switch (irResults.value)
    {
    case REMOTE::up:
      Serial.println("UP REMOTE BUTTON PRESSED");
      setRunState(RUN_STATE::STANDBY);
      break;
    case REMOTE::down:
      Serial.println("DOWN REMOTE BUTTON PRESSED");
      setRunState(RUN_STATE::BET);
      break;
    case REMOTE::ok:
      Serial.println("OK REMOTE BUTTON PRESSED");
      setRunState(RUN_STATE::WIN);
      break;
    }

    irrecv.resume(); // Await the next value
  }

  // Process stepper moves.
  stepper.run();

  animateLEDs();
}
