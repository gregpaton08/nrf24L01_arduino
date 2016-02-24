/*
   Dec 2014 - TMRh20 - Updated
   Derived from examples by J. Coliz <maniacbug@ymail.com>
*/
/**
 * Example for efficient call-response using ack-payloads 
 * 
 * This example continues to make use of all the normal functionality of the radios including 
 * the auto-ack and auto-retry features, but allows ack-payloads to be written optionlly as well. 
 * This allows very fast call-response communication, with the responding radio never having to 
 * switch out of Primary Receiver mode to send back a payload, but having the option to switch to 
 * primary transmitter if wanting to initiate communication instead of respond to a commmunication. 
 */
 
#include <SPI.h>
#include <printf.h>
#include <RF24.h>
#include <RF24Network.h>

/****************** User Config ***************************/
/***      Set this radio as radio number 0 or 1         ***/
bool radioNumber = 0;

// Set up the nRF24L01 radio on SPI bus
// First argument is for the chip enable pin
// Second argument is for the chip select pin
RF24 radio(9, 10);
RF24Network network(radio); 

// Radio pipe addresses for the 2 nodes to communicate.
byte addresses[][6] = { "1Node", "2Node" };

// The various roles supported by this sketch
typedef enum {
  role_ping_out = 1,
  role_pong_back
}
role_e;

// The debug-friendly names of those roles
const char* role_friendly_name[] = { "invalid", "Ping out", "Pong back"};

// The current role. Default to receive
role_e role = role_pong_back;

// A single byte to keep track of the data being sent back and forth
byte counter = 1;

volatile boolean sendPing = false;

void buttonPress() {
  sendPing = true;
}


void setup() {

  Serial.begin(115200);
  printf_begin();
  Serial.println(F("RF24/examples/GettingStarted_CallResponse"));
  Serial.println(F("*** PRESS 'T' to begin transmitting to the other node"));
 

  // Attach interrupt to pin 2
  attachInterrupt(digitalPinToInterrupt(2), buttonPress, RISING);

  // Bring up the RF network
  SPI.begin();   
  radio.begin();
  radio.setPALevel(RF24_PA_HIGH);
  network.begin(90, 01);

  radio.printDetails();
}

void pingOut() {
  uint16_t node = 00;
  
  RF24NetworkHeader header(node, 'T');
  
  // The 'T' message that we send is just a ulong, containing the time
  unsigned long message = millis();
  printf_P(PSTR("---------------------------------\n\r"));
  printf_P(PSTR("%lu: APP Sending %lu to 0%o...\n\r"), millis(), message, node);
  network.write(header, &message, sizeof(unsigned long));
}

void loop(void) {

  // ping out role
  if (sendPing) {
    Serial.println("Button pressed...");
    pingOut();
    delay(500);
    sendPing = false;
  }

  delay(1); 
}
