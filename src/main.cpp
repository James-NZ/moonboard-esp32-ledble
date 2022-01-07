#include "BLESerial.h"
#include <Arduino.h>
#include <NeoPixelBus.h>

// constants
const int c_boardStandard = 0;
const int c_boardMini = 1;

// custom settings
int board = c_boardStandard;   // Define the board type : mini or standard (to be changed depending of board type used)
const int c_ledOffset = 1;     // Use every "c_ledOffset" LED on the string
const uint8_t c_pixelPin = 2;  // Use pin D2 of Arduino Nano 33 BLE (to be changed depending of your pin number used)
const bool c_checkLeds = true; // Test the led sysem at boot if true

// variables used inside project
int ledsByBoard[] = {200, 150};                                       // LEDs: usually 150 for MoonBoard Mini, 200 for a standard MoonBoard
int rowsByBoard[] = {18, 12};                                         // Rows: usually 12 for MoonBoard Mini, 18 for a standard MoonBoard
BLESerial bleSerial;                                                  // BLE serial emulation
String bleMessage = "";                                               // BLE buffer message
bool bleMessageStarted = false;                                       // Start indicator of problem message
bool bleMessageEnded = false;                                         // End indicator of problem message
uint16_t leds = ledsByBoard[board];                                   // Number of LEDs in the LED strip (usually 150 for MoonBoard Mini, 200 for a standard MoonBoard)
NeoPixelBus<NeoRgbFeature, Neo800KbpsMethod> strip(leds, c_pixelPin); // Pixel object to interact withs LEDs

// colors definitions
RgbColor red(255, 0, 0);      // Red color
RgbColor green(0, 255, 0);    // Green color
RgbColor blue(0, 0, 255);     // Blue color
RgbColor violet(255, 0, 255); // Violet color
RgbColor black(0);            // Black color

/**
 * @brief Light the LEDs for a given hold
 *
 * @param holdType Hold type (S,P,E)
 * @param holdPosition Position of the mathcing LED
 * @param ledAboveHoldEnabled Enable the LED above the hold if possible
 */
void lightHold(char holdType, int holdPosition, bool ledAboveHoldEnabled)
{
  Serial.print("Light hold: ");
  Serial.print(holdType);
  Serial.print(", ");
  Serial.print(holdPosition);

  String colorLabel = "BLACK";
  RgbColor colorRgb = black;
  
  switch (holdType)
  {
  case 'S':
    colorLabel = "GREEN";
    colorRgb = green;
    break;
  case 'P':
    colorLabel = "BLUE";
    colorRgb = blue;
    break;
  case 'E':
    colorLabel = "RED";
    colorRgb = red;
    break;
  }
  Serial.print(" color = ");
  Serial.print(colorLabel);

  // Ligth Hold
  strip.SetPixelColor(holdPosition * c_ledOffset, colorRgb);

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
      Serial.print(", led position above : ");
      Serial.print(ledAboveHoldPosition);

      // Light LED above hold
      strip.SetPixelColor(ledAboveHoldPosition * c_ledOffset, violet);
    }
  }

  Serial.println();
}

/**
 * @brief Process the BLE message to light the matching LEDs
 *
 */
void processBleMesage()
{
  /*
   * Example off received BLE messages:
   *    "~D*l#S69,S4,P82,P8,P57,P49,P28,E54#"
   *    "l#S69,S4,P93,P81,P49,P28,P10,E54#"
   *
   * first message part (separator = '#')
   *    - "~D*1" : light 2 LEDs, the selected hold and the LED above it
   *    - "1" : light the the selected hold
   *
   * second message part (separator = '#') is the problem string separated by ','
   *    - format: "S12,P34, ... ,E56"
   *    - where S = starting hold, P = intermediate hold, E = ending hold
   *    - where the following numbers are the LED position in the string
   */

  Serial.println("--------------------");
  Serial.print("Message: ");
  Serial.println(bleMessage);
  Serial.println();

  int indexHashtag1 = 0;
  int indexHashtag2 = 0;
  bool ledAboveHoldEnabled = false;

  // explode the message with char '#'
  while ((indexHashtag2 = bleMessage.indexOf('#', indexHashtag1)) != -1)
  {
    String splitMessage = bleMessage.substring(indexHashtag1, indexHashtag2);
    indexHashtag1 = indexHashtag2 + 1;

    if (splitMessage[0] == 'l') // process conf part of the ble message
    {
      ledAboveHoldEnabled = false;
    }
    else if (splitMessage[0] == '~') // process conf part of the ble message
    {
      ledAboveHoldEnabled = true;
    }
    else // process the problem part of the ble message
    {
      int indexComma1 = 0;
      int indexComma2 = 0;
      while (indexComma2 != -1)
      {
        indexComma2 = splitMessage.indexOf(',', indexComma1);
        String holdMessage = splitMessage.substring(indexComma1, indexComma2);
        indexComma1 = indexComma2 + 1;

        char holdType = holdMessage[0];                         // holdType is the first char of the string
        int holdPosition = holdMessage.substring(1).toInt();    // holdPosition start at second char of the string
        lightHold(holdType, holdPosition, ledAboveHoldEnabled); // light the hold on the board
      }
    }
  }
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
 * @brief Check LEDs by cycling through the colors red, green, blue, violet and then turning the LEDs off again
 * 
 */
void checkLeds()
{
  if (c_checkLeds)
  {
    strip.SetPixelColor(0, red);
    for (int i = 0; i < leds; i++)
    {
      strip.ShiftRight(1 * c_ledOffset);
      strip.Show();
      delay(10);
    }
    strip.SetPixelColor(0, green);
    for (int i = 0; i < leds; i++)
    {
      strip.ShiftRight(1 * c_ledOffset);
      strip.Show();
      delay(10);
    }
    strip.SetPixelColor(0, blue);
    for (int i = 0; i < leds; i++)
    {
      strip.ShiftRight(1 * c_ledOffset);
      strip.Show();
      delay(10);
    }
    strip.SetPixelColor(0, violet);
    for (int i = 0; i < leds; i++)
    {
      strip.ShiftRight(1);
      strip.Show();
      delay(10);
    }
    resetLeds();
  }
}

/**
 * @brief Initialization
 *
 */
void setup()
{
  Serial.begin(9600);
  char bleName[] = "MoonBoard A";
  bleSerial.begin(bleName);

  strip.Begin();
  strip.Show();

  checkLeds();

  Serial.println("-----------------");
  Serial.println("Waiting for the mobile app to connect ...");
  Serial.println("-----------------");
}

/**
 * @brief Infinite loop processed by the chip
 *
 */
void loop()
{
  if (bleSerial.connected()) // do something only if BLE connected
  {
    while (bleSerial.available())
    {
      // read first char
      char c = bleSerial.read();

      // message state
      if (c == '#' && !bleMessageStarted) // check start delimiter
        bleMessageStarted = true;
      else if (c == '#' && bleMessageStarted) // check end delimiter
        bleMessageEnded = true;

      // construct ble message
      bleMessage.concat(c);

      // process message if end detected
      if (bleMessageEnded)
      {
        resetLeds();
        processBleMesage();

        // reset data
        bleMessageEnded = false;
        bleMessageStarted = false;
        bleMessage = "";
      }
    }
  }
}
