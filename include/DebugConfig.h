#ifndef DEBUG_CONFIG_H
#define DEBUG_CONFIG_H


/**
 * @file DebugConfig.h
 * @brief Centralized configuration for conditional debug logging macros.
 *
 * This file defines preprocessor flags and macros to enable or disable
 * Serial debug output for specific modules (e.g., API routes, Schedule API).
 * To enable logging for a module, define its corresponding `ENABLE_..._DEBUG_LOGGING` flag.
 * To disable logging (e.g., for release builds), comment out or `#undef` the flag.
 * When disabled, the logging macros compile to nothing, minimizing performance impact.
 */
#include <Arduino.h> // Required for Serial

// --- Conditional Debug Logging for API Routes ---
// Define this flag to enable detailed logging in ApiRoutes.
// Comment it out or undefine it for release builds to remove logging code.
/**
 * @def ENABLE_API_DEBUG_LOGGING
 * @brief Define this macro to enable detailed Serial logging within the ApiRoutes class.
 *        Comment out or undefine for release builds to disable these logs.
 */
#define ENABLE_API_DEBUG_LOGGING

#ifdef ENABLE_API_DEBUG_LOGGING
  /** @brief Prints debug messages via Serial (enabled). */
  #define API_DEBUG_PRINT(...)    Serial.print(__VA_ARGS__)
  /** @brief Prints debug messages with a newline via Serial (enabled). */
  #define API_DEBUG_PRINTLN(...)  Serial.println(__VA_ARGS__)
  /** @brief Prints formatted debug messages via Serial (enabled). */
  #define API_DEBUG_PRINTF(...)   Serial.printf(__VA_ARGS__)
#else
  // Define as empty when the flag is not set (compiler optimizes away)
  /** @brief Prints debug messages (disabled, compiles to nothing). */
  #define API_DEBUG_PRINT(...)    ((void)0)
  /** @brief Prints debug messages with a newline (disabled, compiles to nothing). */
  #define API_DEBUG_PRINTLN(...)  ((void)0)
  /** @brief Prints formatted debug messages (disabled, compiles to nothing). */
  #define API_DEBUG_PRINTF(...)   ((void)0)
#endif

// --- Conditional Debug Logging for Schedule API ---
// Define this flag to enable detailed logging for schedule operations.
// Comment it out or undefine it for release builds.
/**
 * @def ENABLE_SCHEDULE_API_DEBUG_LOGGING
 * @brief Define this macro to enable detailed Serial logging specifically for
 *        schedule-related API operations within ApiRoutes and ScheduleManager.
 *        Comment out or undefine for release builds to disable these logs.
 */
#define ENABLE_SCHEDULE_API_DEBUG_LOGGING

#ifdef ENABLE_SCHEDULE_API_DEBUG_LOGGING
  /** @brief Prints schedule API debug messages via Serial (enabled). */
  #define SCH_API_DEBUG_PRINT(...)    Serial.print(__VA_ARGS__)
  /** @brief Prints schedule API debug messages with a newline via Serial (enabled). */
  #define SCH_API_DEBUG_PRINTLN(...)  Serial.println(__VA_ARGS__)
  /** @brief Prints formatted schedule API debug messages via Serial (enabled). */
  #define SCH_API_DEBUG_PRINTF(...)   Serial.printf(__VA_ARGS__)
#else
  /** @brief Prints schedule API debug messages (disabled, compiles to nothing). */
  #define SCH_API_DEBUG_PRINT(...)    ((void)0)
  /** @brief Prints schedule API debug messages with a newline (disabled, compiles to nothing). */
  #define SCH_API_DEBUG_PRINTLN(...)  ((void)0)
  /** @brief Prints formatted schedule API debug messages (disabled, compiles to nothing). */
  #define SCH_API_DEBUG_PRINTF(...)   ((void)0)
#endif


#endif // DEBUG_CONFIG_H