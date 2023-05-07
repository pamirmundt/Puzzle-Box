/*
 * Puzzle Box
 * - 4 x RFID Placement
 * - 8 x Toggle Switch (in series) Correct Order
 * - 1 x Tangram (6 magnetic pieces) Placement
 */

// Provides debugging information over serial connection if defined
#define DEBUG

#include <MFRC522.h>
#include <SPI.h>

// RFID - Number of sensors
const byte numReaders = 4;
// RFID - SPI Slave Select Pins
const byte ssPins[] = { 2, 3, 4, 5 };
// RFID - SPI Reset Pin
const byte resetPin = 8;
// RFID - Selenoid Lock Pin
const byte lockPin_rfid = A0;
// Toggle Switches - Selenoid Lock Pin
const byte lockPin_switches = A1;
// Tangram (Magnetic Sensors) - Selenoid Lock Pin
const byte lockPin_tangram = A2;
// The sequence of NFC tag IDs required to solve the puzzle
const String correctIDs[4][10] = {
  { "33292116", "43244216", "735fa498", "73c41e16", "239c9214", "83f35816", "c3bf401b", "339ef6a1", "73765916", "93421f16" },  // Star
  { "f3b74916", "93ff4116", "a3b34116", "83839514", "3518714", "53695a16", "23714a16", "f3e21c16", "f3735716", "93e0ca1c" },   // Square
  { "734b2a1b", "c3b14616", "33a5016", "e32e5216", "a3a23d16", "a31c3016", "93c12a16", "83dd5516", "735f3916", "d363c16" },    // Hexagon
  { "a38f3b16", "3fc1716", "63d02598", "f39e4616", "5344c61c", "132f5016", "93a11a16", "63fd2f1b", "83af5616", "739f1816" }    // Triangle
};

// Initialise an array of MFRC522 instances representing each reader
MFRC522 mfrc522[numReaders];
// The tag IDs currently detected by each reader
String currentIDs[numReaders];

// Toggle Switches Read Pin
const byte switchesPin = 7;
// Tangram (Magnetic Sensors) Read Pin
const byte tangramPin = 6;

// Is Tangram Puzzle Solved?
boolean tangramSolved = false;
// Is Switches Puzzle Solved?
boolean switchesSolved = false;
// Is RFID Puzzle Solved?
boolean rfidSolved = false;

void setup() {
#ifdef DEBUG
  Serial.begin(9600);
  Serial.println(F("Start!"));
#endif

  // Set the lock pins as output and low
  pinMode(lockPin_rfid, OUTPUT);
  pinMode(lockPin_switches, OUTPUT);
  pinMode(lockPin_tangram, OUTPUT);
  digitalWrite(lockPin_rfid, LOW);
  digitalWrite(lockPin_switches, LOW);
  digitalWrite(lockPin_tangram, LOW);

  // Read pins
  pinMode(tangramPin, INPUT);
  pinMode(switchesPin, INPUT);

  // We set each reader's select pin as HIGH (i.e. disabled), so
  // that they don't cause interference on the SPI bus when
  // first initialised
  for (uint8_t i = 0; i < numReaders; i++) {
    pinMode(ssPins[i], OUTPUT);
    digitalWrite(ssPins[i], HIGH);
  }

  // SPI init
  SPI.begin();

  for (uint8_t i = 0; i < numReaders; i++) {
    // Initialise the reader
    // Note that SPI pins on the reader must always be connected to certain
    // Arduino pins (on an Uno, MOSI=> pin11, MISO=> pin12, SCK=>pin13)
    // The Slave Select (SS) pin and reset pin can be assigned to any pin
    mfrc522[i].PCD_Init(ssPins[i], resetPin);

    // Set the gain to max - not sure this makes any difference...
    mfrc522[i].PCD_SetAntennaGain(MFRC522::PCD_RxGain::RxGain_min);

#ifdef DEBUG
    // Dump some debug information to the serial monitor
    Serial.print(F("Reader #"));
    Serial.print(i);
    Serial.print(F(" initialised on pin "));
    Serial.print(String(ssPins[i]));
    Serial.print(F(". Antenna strength: "));
    Serial.print(mfrc522[i].PCD_GetAntennaGain());
    Serial.print(F(". Version : "));
    mfrc522[i].PCD_DumpVersionToSerial();
#endif

    // Slight delay before activating next reader
    delay(100);
  }

#ifdef DEBUG
  Serial.println(F("--- END SETUP ---"));
#endif
}

void loop() {
  // Assume that the puzzle has been solved
  boolean puzzleSolved = true;

  // Assume that the tags have not changed since last reading
  boolean changedValue = false;

  // Loop through each reader
  for (uint8_t i = 0; i < numReaders; i++) {
    // Initialise the sensor
    mfrc522[i].PCD_Init();

    // String to hold the ID detected by each sensor
    String readRFID = "";

    // If the sensor detects a tag and is able to read it
    if (mfrc522[i].PICC_IsNewCardPresent() && mfrc522[i].PICC_ReadCardSerial()) {
      // Extract the ID from the tag
      readRFID = dump_byte_array(mfrc522[i].uid.uidByte, mfrc522[i].uid.size);
    }

    // If the current reading is different from the last known reading
    if (readRFID != currentIDs[i]) {
      // Set the flag to show that the puzzle state has changed
      changedValue = true;
      // Update the stored value for this sensor
      currentIDs[i] = readRFID;
    }

    // If the reading fails to match the correct ID for this sensor
    // if(currentIDs[i] != correctIDs[i]) {
    // The puzzle has not been solved
    //  puzzleSolved = false;
    //}

    bool found = false;
    for (int j = 0; j < 10; j++) {
      if (currentIDs[i] == correctIDs[i][j]) {
        found = true;
        break;
      }
    }
    if (!found) {
      puzzleSolved = false;
    }

    // Halt PICC
    mfrc522[i].PICC_HaltA();
    // Stop encryption on PCD
    mfrc522[i].PCD_StopCrypto1();
  }

#ifdef DEBUG
  // If the changedValue flag has been set, at least one sensor has changed
  if (changedValue) {
    // Dump to serial the current state of all sensors
    for (uint8_t i = 0; i < numReaders; i++) {
      Serial.print(F("Reader #"));
      Serial.print(String(i));
      Serial.print(F(" on Pin #"));
      Serial.print(String((ssPins[i])));
      Serial.print(F(" detected tag: "));
      Serial.println(currentIDs[i]);
    }
    Serial.println(F("---"));
  }
#endif

  // If the puzzleSolved flag is set, all sensors detected the correct ID
  if (puzzleSolved && !rfidSolved) {
    rfidSolved = true;
    onSolve();
  }

  // Add a short delay before next polling sensors
  // delay(100);

  // Read Switches
  uint8_t switchesRead = digitalRead(switchesPin);
  // If Switches are in correct order
  if (switchesRead && !switchesSolved) {
    digitalWrite(lockPin_switches, HIGH);
    delay(100);
    digitalWrite(lockPin_switches, LOW);

    switchesSolved = true;
  }

  // Read Tangram
  uint8_t tangramRead = digitalRead(tangramPin);
  // If all Tangram pieces are placed
  if (tangramRead && !tangramSolved) {
    digitalWrite(lockPin_tangram, HIGH);
    delay(100);
    digitalWrite(lockPin_tangram, LOW);

    tangramSolved = true;
  }
}

void onSolve() {
#ifdef DEBUG
  // Print debugging message
  Serial.println(F("Puzzle Solved!"));
#endif

  // Release the lock
  digitalWrite(lockPin_rfid, HIGH);
  delay(100);
  digitalWrite(lockPin_rfid, LOW);
}

String dump_byte_array(byte *buffer, byte bufferSize) {
  String read_rfid = "";
  for (byte i = 0; i < bufferSize; i++) {
    read_rfid = read_rfid + String(buffer[i], HEX);
  }
  return read_rfid;
}
