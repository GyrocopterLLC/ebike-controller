/******************************************************************************
 * Filename: data_packet.c
 * Description: Creates and decodes data packets sent between this controller
 *              and other devices. Packets use a defined framing and order
 *              with CRC to ensure data integrity.
 *
 * Packet structure:
 *  - Start of packet (2 bytes)
 *  --- 0x9A, 0xCC
 *  - Packet type (1 byte)
 *  - nPacket type (1 byte, inverse of previous byte)
 *  - Data length (2 bytes)
 *  - Data (n bytes, depending on type)
 *  - CRC-32 (4 bytes)
 *
 ******************************************************************************

Copyright (c) 2019 David Miller

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

#include "main.h"

uint8_t data_create_packet(uint8_t type, uint8_t* data, uint16_t datalen) {
  uint16_t place = 0;
  uint32_t crc;

  // Fail out if the packet can't fit in the buffer
  if(datalen + 8 > PACKET_MAX_LENGTH) {
    data_packet.TxReady = 0;
    return DATA_PACKET_FAIL;
  }

  data_packet.TxBuffer[place++] = PACKET_START_0;
  data_packet.TxBuffer[place++] = PACKET_START_1;
  data_packet.TxBuffer[place++] = type;
  data_packet.TxBuffer[place++] = type ^ 0xFF;
  data_packet.TxBuffer[place++] = (uint8_t)((datalen & 0xFF00) >> 8);
  data_packet.TxBuffer[place++] = (uint8_t)(datalen & 0x00FF);
  for(uint16_t i = 0; i < datalen; i++) {
    data_packet.TxBuffer[place++] = data[i];
  }

  crc = CRC32_Generate(data_packet.TxBuffer, datalen + 6);
  data_packet.TxBuffer[place++] = (uint8_t)((crc & 0xFF000000) >> 24);
  data_packet.TxBuffer[place++] = (uint8_t)((crc & 0x00FF0000) >> 16);
  data_packet.TxBuffer[place++] = (uint8_t)((crc & 0x0000FF00) >> 8);
  data_packet.TxBuffer[place++] = (uint8_t)(crc & 0x000000FF);
  data_packet.TxReady = 1;
  return DATA_PACKET_SUCCESS;

}

uint8_t data_extract_packet(uint8_t* buf, uint16_t buflen) {
  uint16_t place;
  uint8_t SOP_found = 0;
  uint32_t crc_local;
  uint32_t crc_remote;
  uint16_t data_length;
  uint8_t packet_type;
  // Search forward to find SOP
  while((place < buflen-1) && (!SOP_found)) {
    if(buf[place] == PACKET_START_0 && buf[place+1] == PACKET_START_1) {
      SOP_found = 1;
    }
    place++;
  }
  if(!SOP_found) {
    return DATA_PACKET_FAIL;
  }
  packet_type = buf[place++];
  if(buf[place++] != (packet_type^0xFF)) {
    return DATA_PACKET_FAIL;
  }
  data_length = ((uint16_t)(buf[place]) >> 8) + (uint16_t)(buf[place+1]);
  place += 2;
  crc_local = CRC32_Generate(buf[place-6],data_length+6);
  crc_remote = ((uint32_t)(buf[place+data_length]) << 24)
             + ((uint32_t)(buf[place+data_length+1]) << 16)
             + ((uint32_t)(buf[place+data_length+2]) << 8)
             + (uint32_t)(buf[place+data_length+3]);
  if(crc_local != crc_remote) {
    return DATA_PACKET_FAIL;
  }
  data_packet.PacketType = packet_type;
  data_packet.DataLength = data_length;
  memcpy(data_packet.Data, &(buf[place]), data_packet.DataLength);
  return DATA_PACKET_SUCCESS;
}