/***************************************************************************//**
 * @file app_process.c
 * @brief app_process.c
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include "rail.h"
#include "sl_iostream.h"
#include <stdint.h>

#include "nvm3_default.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------
static bool decodePacket(size_t received_bytes);
static void parsePacket(void);

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
static volatile int packet_received = 0;
static uint8_t packet_buffer[86];
static uint8_t decoded_buffer[7];

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
/******************************************************************************
 * Application state machine, called infinitely
 *****************************************************************************/
void app_process_action(RAIL_Handle_t rail_handle)
{
  (void) rail_handle;

  ///////////////////////////////////////////////////////////////////////////
  // Put your application code here!                                       //
  // This is called infinitely.                                            //
  // Do not call blocking functions from here!                             //
  ///////////////////////////////////////////////////////////////////////////

  if(packet_received == 1) {
    RAIL_RxPacketInfo_t packetinfo;
    RAIL_RxPacketHandle_t handle = RAIL_GetRxPacketInfo(
        rail_handle, RAIL_RX_PACKET_HANDLE_OLDEST_COMPLETE, &packetinfo);

    // Reset the flag before emptying the packet queue to avoid dropping frames
    packet_received = 0;

    while(handle != RAIL_RX_PACKET_HANDLE_INVALID &&
          handle != RAIL_RX_PACKET_HANDLE_OLDEST_COMPLETE) {
      // Parse the packet content
      if(packetinfo.packetBytes <= sizeof(packet_buffer)) {
        RAIL_CopyRxPacket(packet_buffer, &packetinfo);
        if(decodePacket(packetinfo.packetBytes)) {
          parsePacket();
        }
      } else {
        printf("OVF!\n");
      }

      // Release packet
      RAIL_ReleaseRxPacket(rail_handle, handle);

      // Get the next packet handle
      handle = RAIL_GetRxPacketInfo(rail_handle,
                                    RAIL_RX_PACKET_HANDLE_OLDEST_COMPLETE,
                                    &packetinfo);
    }
  }
}

/******************************************************************************
 * RAIL callback, called if a RAIL event occurs
 *****************************************************************************/
void sl_rail_util_on_event(RAIL_Handle_t rail_handle, RAIL_Events_t events)
{
  (void) rail_handle;
  (void) events;

  ///////////////////////////////////////////////////////////////////////////
  // Put your RAIL event handling here!                                    //
  // This is called from ISR context.                                      //
  // Do not call blocking functions from here!                             //
  ///////////////////////////////////////////////////////////////////////////
  if(events & RAIL_EVENT_RX_PACKET_RECEIVED) {
      // Place a hold on this packet. We'll retrieve it from the main loop.
      RAIL_HoldRxPacket(rail_handle);
      packet_received = 1;
  }
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------

#define GET_BIT_AT(buf, pos) (((((buf)[(pos)/8] >> (7-((pos)%8))) & 1) == 1) ? 1 : 0)
#define SET_BIT_AT(buf, pos) (buf)[(pos)/8] |= (1 << (7-(pos%8)))
#define CLR_BIT_AT(buf, pos) (buf)[(pos)/8] &= ~(1 << (7-(pos%8)))

static size_t bits_until_next_edge(uint8_t* buf, size_t start_pos)
{
  unsigned int start_state = GET_BIT_AT(buf, start_pos);
  size_t cursor = start_pos + 1;
  while(GET_BIT_AT(buf, cursor) == start_state) {
      cursor++;
  }

  return cursor - start_pos;
}

static bool decodePacket(size_t received_bytes)
{
  /*
  // Debug: print raw received bits
  printf("Packet received: b'[");
  for(size_t i = 0; i < received_bytes; i++) {
    uint8_t str[9];
    for(size_t j = 0; j < 8; j++) {
      if((packet_buffer[i] >> (7-j)) & 1) {
        str[j] = '1';
      } else {
        str[j] = '0';
      }
    }
    str[8] = 0;
    printf("%s ", str);
  }
  printf("]\n");
  */

  // The packet data received begins at a pretty specific location due to how
  // the receiver is limited in setting preamble / syncword.
  //
  // +-----+     +-----+     +---------+  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // + hw1 +-----+ hw2 +-----+ swsync  +--+-+-+-+-+-+-+-+-+-+-+-+-+-+-+---------
  //          ^
  //      buffer start
  //
  // Received buffer is oversampled with 4x due to the pretty ridiculous
  // tolerance on the sent data.
  //
  // This means we need to stretch/shorten as needed in order to decode the
  // actual packet bits.

  memset(decoded_buffer, 0, sizeof(decoded_buffer));

  size_t bit_index = 0;
  size_t decoded_bit_index = 0;
  size_t bit_distance = 0;

  // Step 0: clean the packet buffer. Any pulse of 1 or 2 oversampled bits needs
  // to be cleaned out.
  for(size_t i = 0; i < received_bytes; i++) {
    if(GET_BIT_AT(packet_buffer, i) == 0) {
      if(GET_BIT_AT(packet_buffer, i+1) == 1 &&
         GET_BIT_AT(packet_buffer, i+2) == 0 ) {
         CLR_BIT_AT(packet_buffer, i+1);
        i+=1;
      }
      if(GET_BIT_AT(packet_buffer, i+1) == 1 &&
         GET_BIT_AT(packet_buffer, i+2) == 1 &&
         GET_BIT_AT(packet_buffer, i+3) == 0 ) {
        CLR_BIT_AT(packet_buffer, i+1);
        CLR_BIT_AT(packet_buffer, i+2);
        i+=2;
      }
    }
  }

  // Step 1: eat until rising edge of second HW pulse
  bit_index += bits_until_next_edge(packet_buffer, bit_index);

  // Step 2: eat until falling edge of second HW pulse
  bit_index += bits_until_next_edge(packet_buffer, bit_index);

  // Step 3: eat until rising edge of SW sync
  bit_index += bits_until_next_edge(packet_buffer, bit_index);

  // Step 4: eat until falling edge of SW sync and check the length matches
  bit_distance = bits_until_next_edge(packet_buffer, bit_index);
  if( bit_distance < 28 || bit_distance > 36 ) {
      printf("Repeated packet\n");

      // For a repeated packet, we need to eat 5 more hw pulses
      for(size_t j = 0; j < 10; j++) {
          bit_index += bits_until_next_edge(packet_buffer, bit_index);
      }
      bit_distance = bits_until_next_edge(packet_buffer, bit_index);
  }

  if( bit_distance < 28 || bit_distance > 36 ) {
      printf("SW pulse length does not match\n");
      return false;
  }

  bit_index += bits_until_next_edge(packet_buffer, bit_index);

  // At this point, we have 4-5 periods of 'low' time from the SW sync, and then
  // depending on whether the first bit is a 1 or 0, we might have more low time
  bit_distance = bits_until_next_edge(packet_buffer, bit_index);
  if(bit_distance > 5) {
      // initial transition is a 0->1 (1)
      SET_BIT_AT(decoded_buffer, decoded_bit_index);
      // Move cursor to middle of manchester bit
      bit_index += bits_until_next_edge(packet_buffer, bit_index);
  } else {
      // initial transition is a 1->0 (0)
      CLR_BIT_AT(decoded_buffer, decoded_bit_index);
      // Move cursor to middle of manchester bit
      bit_index += bits_until_next_edge(packet_buffer, bit_index);
      bit_index += bits_until_next_edge(packet_buffer, bit_index);
  }
  decoded_bit_index = 1;

  while(decoded_bit_index < 56) {
      // Manchester decoding is based on edge length + previous bit value
      bit_distance = bits_until_next_edge(packet_buffer, bit_index);
      if(GET_BIT_AT(decoded_buffer, decoded_bit_index - 1)) {
          // Previous bit was a 1, so if next edge is @ 4 this bit is a 1 too
          // If next edge is @ 8 the next bit is a 0
          if(bit_distance >= 6) {
              CLR_BIT_AT(decoded_buffer, decoded_bit_index);
              // Next edge is another middle of manchester bit
              bit_index += bits_until_next_edge(packet_buffer, bit_index);
          } else {
              SET_BIT_AT(decoded_buffer, decoded_bit_index);
              // Next edge is start of next manchester bit
              bit_index += bits_until_next_edge(packet_buffer, bit_index);
              // Next edge is another middle of manchester bit
              bit_index += bits_until_next_edge(packet_buffer, bit_index);
          }
      } else {
          // Previous bit was a 0 (1->0), so if next edge is at 4 than the next
          // bit is also a 0. If the next edge is significantly further than it
          // is a 1
          if(bit_distance >= 6) {
              SET_BIT_AT(decoded_buffer, decoded_bit_index);
              // Next edge is another middle of manchester bit
              bit_index += bits_until_next_edge(packet_buffer, bit_index);
          } else {
              CLR_BIT_AT(decoded_buffer, decoded_bit_index);
              // Next edge is start of next manchester bit
              bit_index += bits_until_next_edge(packet_buffer, bit_index);
              // Next edge is another middle of manchester bit
              bit_index += bits_until_next_edge(packet_buffer, bit_index);
          }
      }

      decoded_bit_index += 1;
  }

  // De-'obfuscate' the packet
  memcpy(packet_buffer, decoded_buffer, sizeof(decoded_buffer));
  for(size_t i = 1; i < sizeof(decoded_buffer); i++) {
    decoded_buffer[i] = packet_buffer[i] ^ packet_buffer[i - 1];
  }

  // Check the 'checksum'
  uint8_t cks = 0;
  for(size_t i = 0; i < sizeof(decoded_buffer); i++) {
    cks = cks ^ decoded_buffer[i] ^ (decoded_buffer[i] >> 4);
  }
  if((cks & 0xf) != 0) {
      printf("Checksum mismatch\n");
      return false;
  }

  /*
  // debug: print packet content
  printf("Deobfuscated packet: [");
  for(size_t i = 0; i < sizeof(decoded_buffer); i++) {
    printf("%02x ", decoded_buffer[i]);
  }
  printf("]\n");
  */

  return true;
}

static void parsePacket(void)
{
  // Information contained in a packet:
  // * rolling code
  // * remote ID
  // * button pressed
  uint32_t remote_address = decoded_buffer[6] << 16 |
                            decoded_buffer[5] << 8 |
                            decoded_buffer[4];
  uint16_t rolling_code = decoded_buffer[2] << 8 | decoded_buffer[3];
  uint8_t button = decoded_buffer[1] >> 4;

  printf("From remote %06x (seq %u): ", remote_address, rolling_code);
  switch(button) {
    case 1:
      printf("MY");
      break;
    case 2:
      printf("UP");
      break;
    case 3:
      printf("MY+UP");
      break;
    case 4:
      printf("DOWN");
      break;
    case 5:
      printf("MY+DOWN");
      break;
    case 6:
      printf("UP+DOWN");
      break;
    case 7:
      printf("MY+UP+DOWN");
      break;
    case 8:
      printf("PROG");
      break;
  }
  printf("\n");
}
