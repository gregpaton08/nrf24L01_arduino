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
#include "RF24.h"

/****************** User Config ***************************/
/***      Set this radio as radio number 0 or 1         ***/
bool radioNumber = 0;

// Set up the nRF24L01 radio on SPI bus
// First argument is for the chip enable pin
// Second argument is for the chip select pin
RF24 radio(9,10);

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


void setup() {

  Serial.begin(115200);
  printf_begin();
  Serial.println(F("RF24/examples/GettingStarted_CallResponse"));
  Serial.println(F("*** PRESS 'T' to begin transmitting to the other node"));
 
  // Setup and configure radio
  radio.begin();
//  while (!radio.begin()) {
//    Serial.println(F("Failed to start the radio..."));
//    delay(1000);
//  }

  // Allow optional ack payloads
  radio.enableAckPayload();

  // Ack payloads are dynamic payloads
  radio.enableDynamicPayloads();
  
//  if (radioNumber) {
//    // Both radios listen on the same pipes by default, but opposite addresses
//    radio.openWritingPipe(addresses[1]);
//    // Open a reading pipe on address 0, pipe 1
//    radio.openReadingPipe(1, addresses[0]);
//  }
//  else {
    radio.openWritingPipe(addresses[0]);
    radio.openReadingPipe(1, addresses[1]);
//  }

  // Make the radio start listening
  radio.startListening();

  // Pre-load an ack-paylod into the FIFO buffer for pipe 1
  radio.writeAckPayload(1, &counter, 1);
  radio.printDetails();
}

void pingOut() {
  byte gotByte;                                           // Initialize a variable for the incoming response
  
  radio.stopListening();                                  // First, stop listening so we can talk.      
  Serial.print(F("Now sending "));                         // Use a simple byte counter as payload
  Serial.println(counter);

  // Get the current time in microseconds
  unsigned long time = micros();  
  
  // Send the counter variable to the other radio 
  if (radio.write(&counter, 1)) {
    // If nothing in the buffer, we got an ack but it is blank
    if (!radio.available()) {
        Serial.print(F("Got blank response. round-trip delay: "));
        Serial.print(micros() - time);
        Serial.println(F(" microseconds"));     
    }
    else {      
      // If an ack with payload was received
      while (radio.available()) {
        // Read it, and display the response time
        radio.read( &gotByte, 1);

        // Get the time to compute the round trip delay
        unsigned long timer = micros();
        
        Serial.print(F("Got response "));
        Serial.print(gotByte);
        Serial.print(F(" round-trip delay: "));
        Serial.print(timer-time);
        Serial.println(F(" microseconds"));
        counter++;                                  // Increment the counter variable
      }
    }
  
  }
  // If no ack response, sending failed
  else {
    Serial.println(F("Sending failed."));
  }
}

void pongBack() {
  byte pipeNo, gotByte[32];                          // Declare variables for the pipe and the byte received
  memset(gotByte, 0, 32);
  // Read all available payloads
  while (radio.available(&pipeNo)) {
    radio.read(&gotByte, 32);                   
                                                 // Since this is a call-response. Respond directly with an ack payload.
    //gotByte += 1;                                // Ack payloads are much more efficient than switching to transmit mode to respond to a call
    radio.writeAckPayload(pipeNo, &gotByte, 1);  // This can be commented out to send empty payloads.
    Serial.print(F("Loaded next response "));
    int i = 0;
    for (; i < 32; ++i) {
      Serial.print(gotByte[i]);
    }
    Serial.println("");
  }
}

void checkForRoleChange() {
  if (Serial.available()) {
    char c = toupper(Serial.read());
    if (c == 'T' && role == role_pong_back) {      
      Serial.println(F("*** CHANGING TO TRANSMIT ROLE -- PRESS 'R' TO SWITCH BACK"));
      role = role_ping_out;  // Become the primary transmitter (ping out)
      counter = 1;
    }
    else if (c == 'R' && role == role_ping_out) {
      Serial.println(F("*** CHANGING TO RECEIVE ROLE -- PRESS 'T' TO SWITCH BACK"));      
      role = role_pong_back; // Become the primary receiver (pong back)
      radio.startListening();
      counter = 1;
      radio.writeAckPayload(1, &counter, 1);
    }
  }
}

void loop(void) {

  // ping out role
  if (role == role_ping_out) {
    pingOut();
    // Try again in 1 second
    delay(1000);  
  }

  // pong back role
  if ( role == role_pong_back ) {
    pongBack();
  }



/****************** Change Roles via Serial Commands ***************************/
  checkForRoleChange();
}
