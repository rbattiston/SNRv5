#ifndef SESSION_DATA_H
#define SESSION_DATA_H

#include <Arduino.h>
#include "UserAccount.h" // Include UserRole enum

// Structure to hold active session information
/**
 * @struct SessionData
 * @brief Represents the data associated with an active user session.
 *
 * Stores the unique session ID, associated username and role, timestamps for
 * creation and last activity, and a client fingerprint for basic security checks.
 * Used by SessionManager to track logged-in users.
 */
struct SessionData {
    String sessionId = "";      ///< Unique, high-entropy session identifier (e.g., 64 hex chars).
    String username = "";       ///< Username associated with this session.
    UserRole userRole = UNKNOWN; ///< Role (e.g., OWNER, MANAGER, VIEWER) of the logged-in user.
    unsigned long creationTime = 0; ///< Timestamp (millis()) when the session was created.
    unsigned long lastHeartbeat = 0; ///< Timestamp (millis()) of the last validated request associated with this session.
    String fingerprint = "";    ///< Client fingerprint (e.g., SHA-256 hash of IP address + User-Agent string).

    // Basic validation
    /**
     * @brief Checks if the SessionData structure contains valid essential information.
     * @return True if sessionId, username are not empty, userRole is not UNKNOWN,
     *         and timestamps are positive. False otherwise.
     */
    bool isValid() const {
        return !sessionId.isEmpty() && !username.isEmpty() && userRole != UNKNOWN && creationTime > 0 && lastHeartbeat > 0;
    }
};

#endif // SESSION_DATA_H