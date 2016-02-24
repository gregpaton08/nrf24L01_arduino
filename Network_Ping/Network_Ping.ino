/*
 Copyright (C) 2011 James Coliz, Jr. <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

/**
 * Example: Network topology, and pinging across a tree/mesh network
 *
 * Using this sketch, each node will send a ping to every other node in the network every few seconds. 
 * The RF24Network library will route the message across the mesh to the correct node.
 *
 * This sketch is greatly complicated by the fact that at startup time, each
 * node (including the base) has no clue what nodes are alive.  So,
 * each node builds an array of nodes it has heard about.  The base
 * periodically sends out its whole known list of nodes to everyone.
 *
 * To see the underlying frames being relayed, compile RF24Network with
 * #define SERIAL_DEBUG.
 *
 * Update: The logical node address of each node is set below, and are grouped in twos for demonstration.
 * Number 0 is the master node. Numbers 1-2 represent the 2nd layer in the tree (02,05).
 * Number 3 (012) is the first child of number 1 (02). Number 4 (015) is the first child of number 2.
 * Below that are children 5 (022) and 6 (025), and so on as shown below 
 * The tree below represents the possible network topology with the addresses defined lower down
 *
 *     Addresses/Topology                            Node Numbers  (To simplify address assignment in this demonstration)
 *             00                  - Master Node         ( 0 )
 *           02  05                - 1st Level children ( 1,2 )
 *    32 22 12    15 25 35 45    - 2nd Level children (7,5,3-4,6,8)
 *
 * eg:
 * For node 4 (Address 015) to contact node 1 (address 02), it will send through node 2 (address 05) which relays the payload
 * through the master (00), which sends it through to node 1 (02). This seems complicated, however, node 4 (015) can be a very
 * long way away from node 1 (02), with node 2 (05) bridging the gap between it and the master node.
 *
 * To use the sketch, upload it to two or more units and set the NODE_ADDRESS below. If configuring only a few
 * units, set the addresses to 0,1,3,5... to configure all nodes as children to each other. If using many nodes,
 * it is easiest just to increment the NODE_ADDRESS by 1 as the sketch is uploaded to each device.
 */

#include <avr/pgmspace.h>
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include "printf.h"

/***********************************************************************
************* Set the Node Address *************************************
/***********************************************************************/

// These are the Octal addresses that will be assigned
const uint16_t node_address_set[10] = { 00, 02, 05, 012, 015, 022, 025, 032, 035, 045 };
 
// 0 = Master
// 1-2 (02,05)   = Children of Master(00)
// 3,5 (012,022) = Children of (02)
// 4,6 (015,025) = Children of (05)
// 7   (032)     = Child of (02)
// 8,9 (035,045) = Children of (05)

uint8_t NODE_ADDRESS = 0;  // Use numbers 0 through to select an address from the array

/***********************************************************************/
/***********************************************************************/


// CE & CS pins to use (Using 7,8 on Uno,Nano)
RF24 radio(9, 10);
RF24Network network(radio); 

// Our node address
uint16_t this_node;

// Delay manager to send pings regularly (in ms).
const unsigned long interval = 1000;
unsigned long last_time_sent;

// Array of nodes we are aware of
const short max_active_nodes = 10;
uint16_t active_nodes[max_active_nodes];
short num_active_nodes = 0;
short next_ping_node_index = 0;

// Prototypes for functions to send & handle messages
bool send_T(uint16_t node);
bool send_N(uint16_t node);

void handle(RF24NetworkHeader& header);
void handle_T(RF24NetworkHeader& header);
void handle_N(RF24NetworkHeader& header);
void add_node(uint16_t node);


void setup() {
  Serial.begin(115200);
  printf_begin();
  printf_P(PSTR("\n\rRF24Network/examples/meshping/\n\r"));

  // Which node are we?
  this_node = node_address_set[NODE_ADDRESS];

  // Bring up the RF network
  SPI.begin();   
  radio.begin();
  radio.setPALevel(RF24_PA_HIGH);
  network.begin(100, this_node);

  radio.printDetails();
}

void loop() {
  // Pump the network regularly
  network.update();

  // Is there anything ready for us?
  while (network.available()) {                      

    // If so, take a look at it
    RF24NetworkHeader header;
    network.peek(header);

    handle(header);
  }

  // Send a ping to the next node every 'interval' ms
  unsigned long now = millis();
  if (now - last_time_sent >= interval) {
    last_time_sent = now;

    // Who should we send to? By default, send to base
    uint16_t to = 00;
    
    // Or if we have active nodes send to the next active node
    if (num_active_nodes) {
      to = active_nodes[next_ping_node_index++];
  
      // Check for roll over
      if (next_ping_node_index > num_active_nodes) {
        // Next time start at the beginning
        next_ping_node_index = 0;
        // This time, send to node 00.
        to = 00;
      }
    }

    // Normal nodes send a 'T' ping
    if (this_node != to) {
      bool ok;
      if (this_node > 00 || to == 00) {
        ok = send_T(to);   
      }
      // Base node sends the current active nodes out
      else {                                                
        ok = send_N(to);
      }
  
      // Notify us of the result
      if (ok) {
        printf_P(PSTR("%lu: APP Send ok\n\r"), millis());
      }
      else {
        printf_P(PSTR("%lu: APP Send failed\n\r"), millis());
          
        // Try sending at a different time next time
        last_time_sent -= 100;                            
      }
    }
  }
}

/**
 * Send a 'T' message, the current time
 */
bool send_T(uint16_t node)
{
  RF24NetworkHeader header(node, 'T');
  
  // The 'T' message that we send is just a ulong, containing the time
  unsigned long message = millis();
  printf_P(PSTR("---------------------------------\n\r"));
  printf_P(PSTR("%lu: APP Sending %lu to 0%o...\n\r"), millis(), message, node);
  return network.write(header, &message, sizeof(unsigned long));
}

/**
 * Send an 'N' message, the active node list
 */
bool send_N(uint16_t to)
{
  RF24NetworkHeader header(/*to node*/ to, /*type*/ 'N' /*Time*/);
  
  printf_P(PSTR("---------------------------------\n\r"));
  printf_P(PSTR("%lu: APP Sending active nodes to 0%o...\n\r"), millis(), to);
  return network.write(header, active_nodes, sizeof(active_nodes));
}

/**
 * Handle a message
 */
void handle(RF24NetworkHeader& header) {
  // Dispatch the message to the correct handler.
  switch (header.type) {
    case 'T':
      handle_T(header);
      break;
    case 'N':
      handle_N(header);
      break;
    default:
      printf_P(PSTR("*** WARNING *** Unknown message type %c\n\r"), header.type);
      network.read(header, 0, 0);
      break;
  };
}

/**
 * Handle a 'T' message
 * Add the node to the list of active nodes
 */
void handle_T(RF24NetworkHeader& header) {

  // The 'T' message is just a ulong, containing the time
  unsigned long message;                                                                      
  network.read(header, &message, sizeof(unsigned long));
  printf_P(PSTR("%lu: APP Received %lu from 0%o\n\r"), millis(), message, header.from_node);


  // If this message is from ourselves or the base, don't bother adding it to the active nodes.
  if ( header.from_node != this_node || header.from_node > 00 )                                
    add_node(header.from_node);
}

/**
 * Handle an 'N' message, the active node list
 */
void handle_N(RF24NetworkHeader& header) {
  static uint16_t incoming_nodes[max_active_nodes];

  network.read(header, &incoming_nodes, sizeof(incoming_nodes));
  printf_P(PSTR("%lu: APP Received nodes from 0%o\n\r"), millis(), header.from_node);

  int i = 0;
  while (i < max_active_nodes && incoming_nodes[i] > 00)
    add_node(incoming_nodes[i++]);
}

/**
 * Add a particular node to the current list of active nodes
 */
void add_node(uint16_t node) {

  // Do we already know about this node?
  short i = num_active_nodes;
  while (i--) {
    if (active_nodes[i] == node)
        break;
  }

  // If not, add it to the table
  if (i == -1 && num_active_nodes < max_active_nodes) {
      active_nodes[num_active_nodes++] = node; 
      printf_P(PSTR("%lu: APP Added 0%o to list of active nodes.\n\r"),millis(),node);
  }
}


