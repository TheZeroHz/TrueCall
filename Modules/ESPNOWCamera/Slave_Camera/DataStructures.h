// DataStructures.h
// Shared data structures for Master and Slave
#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <Arduino.h>

// Image data packet (240 bytes payload)
typedef struct {
  uint16_t packetNum;
  uint16_t totalPackets;
  uint16_t dataSize;
  uint8_t data[240];
} ImagePacket;

// Image header information
typedef struct {
  uint32_t imageSize;
  uint16_t width;
  uint16_t height;
  uint8_t format;
  uint8_t quality;
  uint8_t resolutionMode;
} ImageHeader;

// Command packet from master to slave
typedef struct {
  char command[32];
  uint32_t timestamp;
} CommandPacket;

// Text message packet (bidirectional)
typedef struct {
  char message[200];
  uint32_t timestamp;
  uint8_t fromMaster;  // 1 = from master, 0 = from slave
} TextMessagePacket;

// Complete image structure for display
typedef struct {
  uint8_t* imageData;
  uint32_t imageSize;
  uint16_t width;
  uint16_t height;
  uint8_t resolutionMode;
  uint32_t timestamp;
} CompleteImage;

#endif