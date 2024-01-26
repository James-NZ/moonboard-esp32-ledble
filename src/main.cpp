#include "BLESerial.h"
#include <Arduino.h>
#include <NeoPixelBus.h>

struct HoldReloc
{
  int origPos;
  int relocPos;
};

// constants
const int BOARD_STANDARD = 0;
const int BOARD_MINI = 1;
const int BRIGHTNESS = 200;                   // 0-255

// custom settings
int board = BOARD_STANDARD;                   // Define the board type : mini or standard (to be changed depending of board type used)
const int LED_COL_SKIP = 0;                   // Skip "LED_COL_SKIP" LEDs at the end of each column
const int LED_OFFSET = 1;                     // Light every "LED_OFFSET" LED of the LEDs strip
const uint8_t PIXEL_PIN = 2;                  // Use pin D2 of Arduino Nano 33 BLE (to be changed depending of your pin number used)
const bool CHECK_LEDS_AT_BOOT = true;         // Test the led sysem at boot if true
const HoldReloc HOLD_RELOCATE[] = { {0, 0} }; // Relocate a hold to a different position

// functions
int positionToLED(int position);

// variables used inside project
int rowsByBoard[] = {18, 12};                                                       // Rows: usually 12 for MoonBoard Mini, 18 for a standard MoonBoard
String namesByBoard[] = {"Moonboard Standard", "Moonboard Mini"};                   // Names of moonboards
BLESerial bleSerial;                                                                // BLE serial emulation
String bleMessage = "";                                                             // BLE buffer message
bool bleMessageStarted = false;                                                     // Start indicator of problem message
bool bleMessageEnded = false;                                                       // End indicator of problem message
String confMessage = "";                                                            // BLE buffer conf message
bool confMessageStarted = false;                                                    // Start indicator of conf message
bool confMessageEnded = false;                                                      // End indicator of conf message
bool ledAboveHoldEnabled = false;                                                   // Enable the LED above the hold if possible
int leds = 11 * rowsByBoard[board];                                                 // Number of LEDs on the board (usually 132 for mini, 198 for standard)
NeoPixelBus<NeoRgbFeature, Neo800KbpsMethod> strip(positionToLED(leds), PIXEL_PIN); // Pixel object to interact withs LEDs

// colors definitions
RgbColor red = RgbColor(255, 0, 0).Dim(BRIGHTNESS);
RgbColor green = RgbColor(0, 255, 0).Dim(BRIGHTNESS);
RgbColor blue = RgbColor(0, 0, 255).Dim(BRIGHTNESS);
RgbColor cyan = RgbColor(0, 128, 128).Dim(BRIGHTNESS);
RgbColor magenta = RgbColor(128, 0, 128).Dim(BRIGHTNESS);
RgbColor yellow = RgbColor(128, 128, 0).Dim(BRIGHTNESS);
RgbColor pink = RgbColor(120, 50, 85).Dim(BRIGHTNESS);
RgbColor purple = RgbColor(105, 0, 150).Dim(BRIGHTNESS);
RgbColor black = RgbColor(0);
RgbColor white = RgbColor(BRIGHTNESS);

/**
 * @brief Return the LED strip index for a position taking into account LED_COL_SKIP and LED_OFFSET
 *
 */
int positionToLED(int position)
{
  return (position * LED_OFFSET) + (floor(position / rowsByBoard[board]) * LED_COL_SKIP);
}
  
/**
 * @brief Return the string coordinates for a position as "X12" where X is the column letter and 12 the row number
 *
 */
String positionToCoordinates(int position)
{
  String res = "";
  int rows = rowsByBoard[board];
  char columns[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K'};
  int column = (position / rows) + 1;
  int row = 0;
  res.concat(columns[column - 1]);

  if (column % 2 == 0) // even column
    row = rows - (position % rows);
  else if (column % 2 == 1) // odd column
    row = (position % rows) + 1;

  res.concat(row);
  return res;
}

/**
 * @brief Return the relocated position for any hold in the HOLD_RELOCATE table
 *
 */
int relocateHold(int position)
{
  int relocPos = position;
  for (int i = 0; i < sizeof(HOLD_RELOCATE)/sizeof(HoldReloc); i++)
  {
    if (HOLD_RELOCATE[i].origPos == position)
    {
      relocPos = HOLD_RELOCATE[i].relocPos;
      break;
    }
  }
  return relocPos;
}

/**
 * @brief Light the LEDs for a given hold
 *
 * @param holdType Hold type (F, F, L, M, P, R, S)
 * @param holdPosition Position of the mathcing LED
 */
void lightHold(char holdType, int holdPosition)
{
  Serial.print("Light hold: ");
  Serial.print(holdType);
  Serial.print(", ");
  Serial.print(holdPosition);

  int relocPosition = relocateHold(holdPosition);
  if (holdPosition != relocPosition)
  {
    holdPosition = relocPosition;
    Serial.print(" relocated to: ");
    Serial.print(holdPosition);
  }

  String colorLabel = "BLACK";
  RgbColor colorRgb = black;

  switch (holdType)
  {
  case 'E':
    colorLabel = "RED";
    colorRgb = red;
    break;
  case 'F':
    colorLabel = "CYAN";
    colorRgb = cyan;
    break;
  case 'L':
    colorLabel = "PURPLE";
    colorRgb = purple;
    break;
  case 'M':
    colorLabel = "PINK";
    colorRgb = pink;
    break;
  case 'P':
    colorLabel = "BLUE";
    colorRgb = blue;
    break;
  case 'R':
    colorLabel = "BLUE";
    colorRgb = blue;
    break;
  case 'S':
    colorLabel = "GREEN";
    colorRgb = green;
    break;
  }
  Serial.print(" color = ");
  Serial.print(colorLabel);
  Serial.print(", coordinates: ");
  Serial.print(positionToCoordinates(holdPosition));

  // Ligth Hold
  strip.SetPixelColor(positionToLED(holdPosition), colorRgb);

  // Find the LED position above the hold
  if (ledAboveHoldEnabled)
  {
    int ledAboveHoldPosition = holdPosition;
    int gapLedAbove = 0;
    int rows = rowsByBoard[board];
    int cell = holdPosition + 1;
    int column = (cell / rows) + 1;

    if ((cell % rows == 0) || ((cell - 1) % rows == 0)) // start or end of the column
      gapLedAbove = 0;
    else if (column % 2 == 0) // even column
      gapLedAbove = -1;
    else if (column % 2 == 1) // odd column
      gapLedAbove = 1;
    else
      gapLedAbove = 9;

    if (gapLedAbove != 0 && gapLedAbove != 9)
    {
      ledAboveHoldPosition = holdPosition + gapLedAbove;
      Serial.print(", led position above: ");
      Serial.print(ledAboveHoldPosition);

      // Light LED above hold
      strip.SetPixelColor(positionToLED(ledAboveHoldPosition), yellow);
    }
  }

  Serial.println();
  strip.Show();
}

/**
 * @brief Turn off all LEDs
 *
 */
void resetLeds()
{
  strip.ClearTo(black);
  strip.Show();
}

/**
 * @brief Check LEDs by cycling through the colors red, green, blue and then turning the LEDs off again
 *
 */
void checkLeds()
{
  if (CHECK_LEDS_AT_BOOT)
  {
    RgbColor colors[] = {red, green, blue};
    int blinkDelay = 1000;
    
    // blink each color
    for (int indexColor = 0; indexColor < sizeof(colors)/sizeof(RgbColor); indexColor++)
    {
      if (indexColor != 0)
        delay(blinkDelay);
      for (int indexLed = 0; indexLed < leds; indexLed++)
        strip.SetPixelColor(positionToLED(indexLed), colors[indexColor]);
      strip.Show();
      delay(blinkDelay);
      resetLeds();
    }
  }
}

/*
 * Example of received BLE messages:
 *    "~Z*"
 *    "~D*l#S69,S4,P82,P8,P57,P49,P28,E54#"
 *    "l#S69,S4,P93,P81,P49,P28,P10,E54#"
 *    "~D*l#S103,E161,L115,R134,F150,M133#""
 *
 * first message part (separator = '#')
 *    - "~D*1" : light 2 LEDs, the selected hold and the LED above it
 *    - "1" : light the the selected hold
 *
 * second message part (separator = '#') is the problem string separated by ','
 *    - format: "S12,P34, ... ,E56"
 *    - where S = starting hold, P = intermediate hold, E = ending hold
 *    - L = left, R = right, M = match, F = foot
 *    - where the following numbers are the LED position on the strip
 */

/**
 * @brief Process the configuration message
 *
 */
void processConfMessage()
{
  Serial.println("-----------------");
  Serial.print("Configuration message: ");
  Serial.println(confMessage);

  if (confMessage.indexOf("~D*") != -1)
  {
    Serial.println("Display an additional led above each hold");
    ledAboveHoldEnabled = true;
  }

  if (confMessage.indexOf("~Z*") != -1)
  {
    Serial.println("Reset leds");
    resetLeds();
  }
}

/**
 * @brief Process the BLE message to light the matching LEDs
 *
 */
void processBleMessage()
{
  Serial.println("-----------------");
  Serial.print("Problem message: ");
  Serial.println(bleMessage);

  int indexComma1 = 0;
  int indexComma2 = 0;
  while (indexComma2 != -1)
  {
    indexComma2 = bleMessage.indexOf(',', indexComma1);
    String holdMessage = bleMessage.substring(indexComma1, indexComma2);
    indexComma1 = indexComma2 + 1;

    char holdType = holdMessage[0];                      // holdType is the first char of the string
    int holdPosition = holdMessage.substring(1).toInt(); // holdPosition start at second char of the string
    lightHold(holdType, holdPosition);                   // light the hold on the board
  }
  ledAboveHoldEnabled = false;
}

/**
 * @brief Initialization
 *
 */
void setup()
{
  Serial.begin(115200);
  char bleName[] = "MoonBoard A";
  bleSerial.begin(bleName);

  strip.Begin();
  strip.Show();

  checkLeds();

  Serial.println("-----------------");
  Serial.print("Initialization completed for ");
  Serial.println(namesByBoard[board]);
  Serial.println("Waiting for the mobile app to connect ...");
  Serial.println("-----------------");
}

/**
 * @brief Infinite loop processed by the chip
 *
 */
void loop()
{
  if (bleSerial.connected())
  {
    while (bleSerial.available())
    {
      char c = bleSerial.read();

      switch (c)
      {
      case '~':
        confMessageStarted = true;
        break;
      case '*':
        confMessageEnded = true;
        break;
      case '#':
        if (!bleMessageStarted)
        {
          bleMessageStarted = true;
        }
        else
        {
          bleMessageEnded = true;
        }
        break;
      default:
        break;
      }

      if (confMessageStarted)
      {
        confMessage.concat(c);
      }
      if (confMessageEnded)
      {
        processConfMessage();
        confMessage = "";
        confMessageStarted = false;
        confMessageEnded = false;
      }

      if (bleMessageStarted && (c != '#'))
      {
        bleMessage.concat(c);
      }
      if (bleMessageEnded)
      {
        resetLeds();
        processBleMessage();
        bleMessage = "";
        bleMessageStarted = false;
        bleMessageEnded = false;
      }
    }
  }
}
