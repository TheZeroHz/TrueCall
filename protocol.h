// protocol.h
#pragma once
//
// Shared wire protocol for Devices A, B, C (ESP-NOW payloads)
//
// Place this file alongside your .ino files and #include "protocol.h".
//

// Use Arduino's fixed-width types if available; otherwise fall back to <stdint.h>
#if __has_include(<Arduino.h>)
  #include <Arduino.h>
#else
  #include <stdint.h>
#endif

// -------- Actions sent with DisplayPayload --------
enum Action : uint8_t {
  ACT_SELECT           = 1, // live selection / highlight (no commit)
  ACT_SHORT_PRESS      = 2, // single-tap confirm / action
  ACT_RESET            = 3, // reset UI / fall back to clock on C
  ACT_DOUBLE_CLICK     = 4, // attention or enter special mode
  ACT_EMERGENCY        = 5, // emergency signal/action
  ACT_QUANTITY_SELECT  = 6  // live quantity preview while rotating
};

// -------- Payload to displays (B and C) over ESP-NOW --------
// Make sure packed layout matches across devices
struct __attribute__((packed)) DisplayPayload {
  uint8_t  action;             // see Action enum
  int16_t  index;              // item index (Device A's order)
  char     name[32];           // human label (optional convenience)
  int16_t  quantity;           // for beverages/food; 0 if N/A
};

// -------- Simple command payload to ringer/LED device (C) -----
struct __attribute__((packed)) RingerMessage {
  char command[32];            // e.g. "RING", "STOP", "LED_ON", "LED_OFF"
  int  value;                  // optional parameter (often 0)
};

// (Optional) compile-time checks when using a C++ compiler
#ifdef __cplusplus
  static_assert(sizeof(DisplayPayload) == 1 + 2 + 32 + 2, "DisplayPayload size unexpected");
  static_assert(sizeof(RingerMessage)  == 32 + 4,         "RingerMessage size unexpected");
#endif
