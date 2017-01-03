

#ifndef _DEBUG_H_
#define _DEBUG_H_

// Uncomment/comment to turn on/off debug output messages.
//#define LIB_DEBUG
// Uncomment/comment to turn on/off error output messages.
//#define LIB_ERROR

// Set where debug messages will be printed.
#define DebugStream		Serial
// If using something like Zero or Due, change the above to SerialUSB

// Define actual debug output functions when necessary.
#ifdef LIB_DEBUG
  #define DEBUG_PRINT(...) { DebugStream.print(__VA_ARGS__); }
  #define DEBUG_PRINTLN(...) { DebugStream.println(__VA_ARGS__); }
  #define DEBUG_PRINTBUFFER(buffer, len) { printBuffer(buffer, len); }
#else
  #define DEBUG_PRINT(...) {}
  #define DEBUG_PRINTLN(...) {}
  #define DEBUG_PRINTBUFFER(buffer, len) {}
#endif

#ifdef LIB_ERROR
  #define ERROR_PRINT(...) { DEBUG_PRINTER.print(__VA_ARGS__); }
  #define ERROR_PRINTLN(...) { DEBUG_PRINTER.println(__VA_ARGS__); }
  #define ERROR_PRINTBUFFER(buffer, len) { printBuffer(buffer, len); }
#else
  #define ERROR_PRINT(...) {}
  #define ERROR_PRINTLN(...) {}
  #define ERROR_PRINTBUFFER(buffer, len) {}
#endif

#endif