#include <JeeLib.h>
#include <util/parity.h>
#include <avr/sleep.h>
#include <Arduino.h>
#include <avr/eeprom.h>

#define DEBUG

unsigned char pingPayload[] = "L1   ";
//                               ^^------- recipient, always spaces for broadcast
//                                 ^------ 5 = request our color
unsigned char colorPayload[] = "L1      ";
//                                ^^------- recipient, always spaces for broadcast
//                                  ^------ 6 = we changed color
//                                   ^^^--- new RGB values

#define BAND RF12_868MHZ // wireless frequency band
#define GROUP 5     // wireless net group
#define NODEID 30   // node id on wireless to which this sketch responds


const int BUTTON_DELAY = 250;
const unsigned long AUTO_OFF_DELAY = 1200000; // 20 minutes

#include "kitchen.h"

byte lamp_r = 255, lamp_g = 255, lamp_b = 255;

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
#ifdef RGB
    byte version = eeprom_read_byte((byte *) 0);
    lamp_r = eeprom_read_byte((byte *) 1);
    lamp_g = eeprom_read_byte((byte *) 2);
    lamp_b = eeprom_read_byte((byte *) 3);
    byte checksum = eeprom_read_byte((byte *) 4);

    if (version != 1) {
#ifdef DEBUG
        Serial.print("Invalid EEPROM data version: ");
        Serial.println(version);
#endif
        return;
    }

    if (((lamp_r ^ lamp_b) ^ lamp_g) != checksum) {
#ifdef DEBUG
        Serial.print("Invalid EEPROM checksum: ");
        Serial.print(checksum);
        Serial.print(", was expecting ");
        Serial.println(r ^ b ^ g);
#endif
        return;
    }
#else
    lamp_r = 255;
    lamp_g = 255;
    lamp_b = 255;
#endif
    setRGB(lamp_r, lamp_g, lamp_b);
}

void saveRGB() {
#ifdef RGB
    byte checksum = lamp_r ^ lamp_g ^ lamp_b;
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
#endif
}

MilliTimer needSave;
unsigned long autoOff;
unsigned long lastButtonCheck = 0;
bool buttonDown = false;
bool lampOn = true;
bool dimUp = false;
const int dimValues[] = {1,
        2,
        3,
        4,
        6,
        8,
        11,
        16,
        22,
        31,
        44,
        62,
        87,
        123,
        173,
        255,
        255,
        255,
        255};
const int DIM_VALUES = 19;
int dimValue = DIM_VALUES - 1;
bool toggleOnRelease = false;

void setup() {
    lastButtonCheck = 0;
    buttonDown = false;
    autoOff = millis() + AUTO_OFF_DELAY;
#ifdef DEBUG
    Serial.begin(57600);
    Serial.println("LampRGB");
#endif
    rf12_initialize(NODEID, BAND, GROUP);
    pingPayload[1] = LAMP_ID;
    pingPayload[4] = 5; // Ping
    colorPayload[1] = LAMP_ID;
    colorPayload[4] = 6; // Set color
#ifdef BUTTON
    pinMode(3, INPUT);
    digitalWrite(3, 1);
#endif
    pinMode(PIN_R, OUTPUT);
    pinMode(PIN_G, OUTPUT);
    pinMode(PIN_B, OUTPUT);
    analogWrite(PIN_R, 1);
    analogWrite(PIN_G, 1);
    analogWrite(PIN_B, 1);
    loadRGB();
}

void sendColor() {
     while (!rf12_canSend()) {
         rf12_recvDone();
     }
     rf12_sendStart(0, colorPayload, sizeof colorPayload);
     rf12_sendWait(0);
}

void turnOff() {
    analogWrite(PIN_R, 0);
    analogWrite(PIN_G, 0);
    analogWrite(PIN_B, 0);
    autoOff = 0;
    lampOn = false;
    colorPayload[5] = 0;
    colorPayload[6] = 0;
    colorPayload[7] = 0;
    sendColor();
}

void turnOn() {
    analogWrite(PIN_R, 1);
    analogWrite(PIN_G, 1);
    analogWrite(PIN_B, 1);
    setRGB (lamp_r, lamp_g, lamp_b);
    autoOff = millis() + AUTO_OFF_DELAY;
    lampOn = true;
    colorPayload[5] = lamp_r;
    colorPayload[6] = lamp_g;
    colorPayload[7] = lamp_b;
    sendColor();
}

void buttonPressed() {
#ifdef DEBUG
    Serial.println("Button pressed!");
    Serial.println(buttonDown);
#endif
    if (buttonDown) {
        toggleOnRelease = false;
        // Dim
        if (dimValue >= DIM_VALUES - 1) {
            dimUp = false;
        }
        if (dimValue <= 0) {
            dimUp = true;
        }
        if (dimUp) {
            dimValue += 1;
        } else {
            dimValue -= 1;
        }
        lamp_r = lamp_g = lamp_b = dimValues[dimValue];
        turnOn();
    } else {
        toggleOnRelease = true;
    }
}

void buttonReleased() {
#ifdef DEBUG
    Serial.println("Button released!");
    Serial.println(toggleOnRelease);
#endif
    if (toggleOnRelease) {
        // Toggle
        if (lampOn) {
            turnOff();
        } else {
            turnOn();
        }
    } else {
        dimUp = !dimUp;
    }
}

void loop() {

#ifdef BUTTON
    if (digitalRead(3) == 0) {
        if (lastButtonCheck < (millis() - BUTTON_DELAY)) {
            buttonPressed();
            buttonDown = true;
            lastButtonCheck = millis();
        }
    } else {
        if (buttonDown && lastButtonCheck < (millis() - BUTTON_DELAY)) {
            buttonReleased();
            buttonDown = false;
        }
    }
#endif

    if (autoOff != 0 && millis() > autoOff) {
        turnOff();
    }

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
            if (rf12_data[4] != 0 && rf12_data[5] != 0 && rf12_data[6] != 0) {
                lamp_r = rf12_data[4];
                lamp_g = rf12_data[5];
                lamp_b = rf12_data[6];
                turnOn();
                needSave.set(1000);
            } else {
                turnOff();
            }
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
