#include <Arduino.h>
#include "NimBLEDevice.h"
#include "BleGamepad.h"
#include "SoftwareSerialRx.h"

using namespace std;

#define RX_BAUD 2400
#define FREQ_TIMER 16000000
#define SERIAL_TIMER NRF_TIMER2

SoftwareSerialRx swSerial0(0, RX_BAUD, FREQ_TIMER, []()
                           { return SERIAL_TIMER->CC[0]; });
SoftwareSerialRx swSerial1(1, RX_BAUD, FREQ_TIMER, []()
                           { return SERIAL_TIMER->CC[1]; });

BleGamepadConfiguration gamepadConfig = BleGamepadConfiguration();
BleGamepad bleGamepad("Serial Gamepad Bridge", "Scott Day", 100);
byte hwButtonsState = 0;
byte swButtonsState = 0;
uint32_t timeout = 0;

extern "C"
{
  void irq_rx0()
  {
    // Forward edge interrupt to SwSerial
    swSerial0.onEdge();
  }

  void irq_rx1()
  {
    // Forward edge interrupt to SwSerial
    swSerial1.onEdge();
  }
}

/// @brief Arduino setup
void setup()
{
  Serial.begin(1000000);

  // Configure & Start HW timer, this is used to time the SW UART
  SERIAL_TIMER->MODE = TIMER_MODE_MODE_Timer;
  SERIAL_TIMER->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
  SERIAL_TIMER->PRESCALER = 0;
  SERIAL_TIMER->TASKS_START = 1;

  // Map pin change events to timer capture tasks (ch0->cc1, ch1->cc1)
  // This will save the exact timer value on pin change because sw interrupt timing is unreliable
  // due to the radio interrupts taking priority.
  NRF_PPI->CH[0].EEP = (uint32_t)&NRF_GPIOTE->EVENTS_IN[0];
  NRF_PPI->CH[0].TEP = (uint32_t)&SERIAL_TIMER->TASKS_CAPTURE[0];
  NRF_PPI->CH[1].EEP = (uint32_t)&NRF_GPIOTE->EVENTS_IN[1];
  NRF_PPI->CH[1].TEP = (uint32_t)&SERIAL_TIMER->TASKS_CAPTURE[1];

  // This will attach interrupts to rx pins
  swSerial0.begin(&irq_rx0);
  swSerial1.begin(&irq_rx1);

  // Enable PPI link from pin interrupt to timer capture
  NRF_PPI->CHENSET = (1 << 1) | (1 << 0);

  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  pinMode(PIN_LED3, OUTPUT);
  pinMode(PIN_LED4, OUTPUT);

  digitalWrite(PIN_LED1, 1);
  digitalWrite(PIN_LED2, 1);
  digitalWrite(PIN_LED3, 1);
  digitalWrite(PIN_LED4, 1);

  pinMode(PIN_BUTTON1, INPUT_PULLUP);
  pinMode(PIN_BUTTON2, INPUT_PULLUP);
  pinMode(PIN_BUTTON3, INPUT_PULLUP);
  pinMode(PIN_BUTTON4, INPUT_PULLUP);

  delay(100);
  Serial.println("Starting Gamepad Service...");

  String name = "Gamepad Bridge ";
  name += String(NRF_FICR->DEVICEADDR[0] & 0xFFFF, HEX);
  bleGamepad.deviceName = name.c_str();

  gamepadConfig.setAxesMin(0);
  gamepadConfig.setAxesMax(2);

  bleGamepad.begin(&gamepadConfig);
  bleGamepad.taskServer(&bleGamepad);
  Serial.println("Started Gamepad Service");
}

/// @brief Poll for new bytes and set gamepad state
void handleSerialInput()
{
  byte serialData;
  if (swSerial0.read(&serialData))
  {
    byte curSwButtons = serialData;
    digitalWrite(PIN_LED2, curSwButtons == 0);

    // Timeout is roughly in milliseconds
    // This will turn off LED when it reaches 0
    timeout = 200;

    byte changed = curSwButtons ^ swButtonsState;
    if (changed)
    {
      // Handle "thumb stick" on bits 0..3
      // 0: left, 1: center, 2: right
      int16_t x = 1 + ((curSwButtons & (1 << 3)) ? 1 : 0) - ((curSwButtons & (1 << 0)) ? 1 : 0);
      int16_t y = 1 + ((curSwButtons & (1 << 2)) ? 1 : 0) - ((curSwButtons & (1 << 1)) ? 1 : 0);
      bleGamepad.setLeftThumb(x, y);

      // Handle buttons on bits 4..7, map to buttons 1..4
      for (int i = 4; i < 8; i++)
      {
        if (changed & (1 << i))
        {
          if (curSwButtons & (1 << i))
          {
            Serial.print("B");
            Serial.println(i);
            bleGamepad.press(i + 1 - 4);
          }
          else
          {
            Serial.print("b");
            Serial.println(i);
            bleGamepad.release(i + 1 - 4);
          }
        }
      }
    }

    swButtonsState = serialData;
  }
}

/// @brief Sample the buttons on the nRF52-DK board, map to buttons 4..7
void handleDevBoardButtons()
{
  // Update gamepad state if buttons have changed
  byte curHwButtons = ~((digitalRead(PIN_BUTTON1) << 0) | (digitalRead(PIN_BUTTON2) << 1) | (digitalRead(PIN_BUTTON3) << 2) | (digitalRead(PIN_BUTTON4) << 3)) & 0x0F;
  byte changedButtons = curHwButtons ^ hwButtonsState;
  if (changedButtons)
  {
    for (int i = 0; i < 4; i++)
    {
      if (!(changedButtons & (1 << i)))
        continue;

      if (curHwButtons & 1 << i)
      {
        bleGamepad.press(i + 1 + 4);
        Serial.print("S");
        Serial.println(i);
      }
      else
      {
        bleGamepad.release(i + 1 + 4);
        Serial.print("s");
        Serial.println(i);
      }
    }
    hwButtonsState = curHwButtons;
  }
}

/// @brief Arduino main loop
void loop()
{
  if (bleGamepad.isConnected())
  {
    // Indicate we're connected
    digitalWrite(PIN_LED3, 0);

    handleDevBoardButtons();
    handleSerialInput();

    // Turn on the LED while we are receiving serial bytes
    if (timeout > 0)
      timeout--;
    digitalWrite(PIN_LED1, timeout == 0);

    // Delay is important to service FreeRTOS
    delay(1);
  }
  else
  {
    digitalWrite(PIN_LED3, 1);
  }
}
