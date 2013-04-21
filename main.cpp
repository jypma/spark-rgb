#include <JeeLib.h>
#include <util/parity.h>
#include <avr/sleep.h>
#include <Arduino.h>

#define DEBUG

#define BAND RF12_868MHZ // wireless frequency band
#define GROUP 5     // wireless net group
#define NODEID 30   // node id on wireless to which this sketch responds

#define LAMP_ID '2'

#define PIN_R 3
#define PIN_G 5
#define PIN_B 6


void setRGB(byte r, byte g, byte b) {
	OCR2B = r;
	OCR0A = g;
	OCR0B = b;
}

void setup () {
#ifdef DEBUG
    Serial.begin(57600);
    Serial.println("LampRGB");
#endif
    rf12_initialize(NODEID, BAND, GROUP);

    pinMode(PIN_R, OUTPUT);
    pinMode(PIN_G, OUTPUT);
    pinMode(PIN_B, OUTPUT);
    analogWrite(PIN_R, 1);
    analogWrite(PIN_G, 1);
    analogWrite(PIN_B, 1);
    setRGB(1,2,3);
}


void loop() {
	MilliTimer timer;

	if (timer.poll(100) && rf12_recvDone()
	                        && rf12_crc == 0
	                        && rf12_len >= 7
	                        && rf12_data[2] == 'R'
	                        && rf12_data[3] == LAMP_ID) {
	        setRGB (rf12_data[4], rf12_data[5], rf12_data[6]);
	    }
}

int main(void) {

  init();
  setup();

  while(true) {
    loop();
  }
}
