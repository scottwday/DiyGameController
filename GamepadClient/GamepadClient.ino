/****************************************************************
 * DIY Gamepad Code
 ****************************************************************/

void setup() {
  // Setup the LED on the Arduino board so we can make it blink!
  pinMode(LED_BUILTIN, OUTPUT);

  // We will send out the "serial"ized buttons over the Serial port
  // to the game console or computer
  Serial.begin(2400);

  // These pins map to the buttons:
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);
  pinMode(7, INPUT_PULLUP);
  pinMode(8, INPUT_PULLUP);
  pinMode(9, INPUT_PULLUP);
}

bool button_pressed(int button_number)
{
  return digitalRead(button_number) == 0;
}

void loop() {

  // Add up all the buttons into a single number
  byte number_to_transmit = 0;
  if (button_pressed(2)) number_to_transmit += 0b00000001;
  if (button_pressed(3)) number_to_transmit += 0b00000010;
  if (button_pressed(4)) number_to_transmit += 0b00000100;
  if (button_pressed(5)) number_to_transmit += 0b00001000;
  if (button_pressed(6)) number_to_transmit += 0b00010000;
  if (button_pressed(7)) number_to_transmit += 0b00100000;
  if (button_pressed(8)) number_to_transmit += 0b01000000;
  if (button_pressed(9)) number_to_transmit += 0b10000000;

  // Send the number representing the buttons that are pressed
  Serial.write(number_to_transmit);
  
  // Turn on the LED if any button is pressed
  digitalWrite(LED_BUILTIN, number_to_transmit > 0);

  delay(10);
}
