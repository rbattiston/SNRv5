#ifndef FILE_LOCK_H
#define FILE_LOCK_H

#include <Arduino.h>

// Define lock types (can be expanded if needed)
/**
 * @enum LockType
 * @brief Defines the different types of locks that can be applied to resources.
 *
 * Used by LockManager to specify the nature of a lock (e.g., preventing edits).
 */
enum LockType {
    EDITING_SCHEDULE, ///< Lock acquired when a user is actively editing a schedule.
    EDITING_TEMPLATE  ///< Lock acquired when a user is actively editing a cycle template.
    // Add other types like ACTIVE_CYCLE_PREVENTION if needed separately
};

// Function to convert LockType enum to String
/**
 * @brief Converts a LockType enum value to its string representation.
 * @param type The LockType enum value.
 * @return A String representing the lock type (e.g., "editing_schedule"), or "unknown".
 */
inline String lockTypeToString(LockType type) { // Added inline
    switch (type) {
        case EDITING_SCHEDULE: return "editing_schedule";
        case EDITING_TEMPLATE: return "editing_template";
        default: return "unknown";
    }
}

// Function to convert String to LockType enum
/**
 * @brief Converts a string representation to its corresponding LockType enum value.
 * @param typeStr The string representation of the lock type (case-insensitive).
 * @return The corresponding LockType enum value, or `(LockType)-1` if the string is unknown/invalid.
 */
inline LockType stringToLockType(const String& typeStr) { // Added inline
    if (typeStr.equalsIgnoreCase("editing_schedule")) return EDITING_SCHEDULE;
    if (typeStr.equalsIgnoreCase("editing_template")) return EDITING_TEMPLATE;
    return (LockType)-1; // Indicate unknown/invalid
}


// Structure to hold file lock information
/**
 * @struct FileLock
 * @brief Represents an active lock on a specific resource.
 *
 * Stores information about who holds the lock (session ID, username), what resource
 * is locked, the type of lock, and when it was acquired. Used by LockManager
 * to track and manage concurrent access.
 */
struct FileLock {
    String resourceId = "";     ///< Unique identifier of the locked resource (e.g., "schedule_abc", "template_xyz").
    LockType lockType = (LockType)-1; ///< The type of lock held (e.g., EDITING_SCHEDULE). Initialized to invalid.
    String sessionId = "";      ///< The session ID of the user currently holding the lock.
    String username = "";       ///< The username of the user holding the lock (stored for convenience/logging).
    unsigned long timestamp = 0; ///< Timestamp (result of millis()) when the lock was acquired/last refreshed.

    // Basic validation
    /**
     * @brief Checks if the FileLock structure contains valid data.
     * @return True if resourceId, sessionId are not empty, lockType is valid, and timestamp is positive. False otherwise.
     */
    bool isValid() const {
        return !resourceId.isEmpty() && lockType != (LockType)-1 && !sessionId.isEmpty() && timestamp > 0;
    }
};

#endif // FILE_LOCK_H
