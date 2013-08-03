#include <JeeLib.h>
#include <util/parity.h>
#include <avr/sleep.h>
#include <Arduino.h>
#include <avr/eeprom.h>

//#define DEBUG

unsigned char pingPayload[] = "L1   ";
//                               ^^------- recipient, always spaces for broadcast
//                                 ^------ 5 = request our color

#define BAND RF12_868MHZ // wireless frequency band
#define GROUP 5     // wireless net group
#define NODEID 30   // node id on wireless to which this sketch responds
#define LAMP_ID '2'

#define PIN_R 3
#define PIN_G 5
#define PIN_B 6

void setRGB(byte r, byte g, byte b) {
#ifdef DEBUG
    Serial.print("Setting ");
    Serial.print(r);
    Serial.print(", ");
    Serial.print(g);
    Serial.print(", ");
    Serial.println(b);
#endif
    OCR2B = r;
    OCR0B = g;
    OCR0A = b;
}

void loadRGB() {
    byte version = eeprom_read_byte((byte *) 0);
    byte r = eeprom_read_byte((byte *) 1);
    byte g = eeprom_read_byte((byte *) 2);
    byte b = eeprom_read_byte((byte *) 3);
    byte checksum = eeprom_read_byte((byte *) 4);

    if (version != 1) {
#ifdef DEBUG
        Serial.print("Invalid EEPROM data version: ");
        Serial.println(version);
#endif
        return;
    }

    if (((r ^ b) ^ g) != checksum) {
#ifdef DEBUG
        Serial.print("Invalid EEPROM checksum: ");
        Serial.print(checksum);
        Serial.print(", was expecting ");
        Serial.println(r ^ b ^ g);
#endif
        return;
    }

    setRGB(r, g, b);
}

void saveRGB() {
    byte r = OCR2B;
    byte g = OCR0B;
    byte b = OCR0A;
    byte checksum = r ^ g ^ b;
#ifdef DEBUG
    Serial.print("Saving ");
    Serial.print(r);
    Serial.print(", ");
    Serial.print(g);
    Serial.print(", ");
    Serial.print(b);
    Serial.print(", ");
    Serial.println(checksum);
#endif
    eeprom_write_byte((byte*) 0, 1);
    eeprom_write_byte((byte*) 1, r);
    eeprom_write_byte((byte*) 2, g);
    eeprom_write_byte((byte*) 3, b);
    eeprom_write_byte((byte*) 4, checksum);
}

void setup() {
#ifdef DEBUG
    Serial.begin(57600);
    Serial.println("LampRGB");
#endif
    rf12_initialize(NODEID, BAND, GROUP);
    /*
     pingPayload[1] = LAMP_ID;
     pingPayload[4] = 5; // Ping
     while (!rf12_canSend()) {
     rf12_recvDone();
     }
     rf12_sendStart(0, pingPayload, sizeof pingPayload);
     rf12_sendWait(0);
     */
    pinMode(PIN_R, OUTPUT);
    pinMode(PIN_G, OUTPUT);
    pinMode(PIN_B, OUTPUT);
    analogWrite(PIN_R, 1);
    analogWrite(PIN_G, 1);
    analogWrite(PIN_B, 1);
    loadRGB();
}

MilliTimer needSave;
void loop() {

    if (needSave.poll()) {
        saveRGB();
    }

    if (rf12_recvDone()) {
#ifdef DEBUG
        Serial.print("> ");
        Serial.println(rf12_len);
#endif
        if (rf12_crc == 0 && rf12_len >= 7 &&
            rf12_data[2] == 'L' &&
            rf12_data[3] == LAMP_ID) {
            setRGB(rf12_data[4], rf12_data[5], rf12_data[6]);
            needSave.set(1000);
        }
    }
}

int main(void) {

    init();
    setup();

    while (true) {
        loop();
    }
}
