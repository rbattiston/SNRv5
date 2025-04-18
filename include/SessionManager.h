#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <Arduino.h>
#include <map> // For storing sessions in memory
#include "SessionData.h"
#include <ESPAsyncWebServer.h> // For accessing request details

/**
 * @def SESSION_TIMEOUT_MS
 * @brief Defines the duration (in milliseconds) of inactivity after which a session expires.
 *        Default: 15 minutes.
 */
// Define session duration (e.g., 15 minutes)
#define SESSION_TIMEOUT_MS (15 * 60 * 1000)
// Define cleanup check interval (e.g., every 1 minute)
/**
 * @def SESSION_CLEANUP_INTERVAL_MS
 * @brief Defines how often (in milliseconds) the `cleanupExpiredSessions` function should run.
 *        Default: 1 minute.
 */
#define SESSION_CLEANUP_INTERVAL_MS (1 * 60 * 1000)

/**
 * @class SessionManager
 * @brief Manages active user sessions in memory.
 *
 * Handles the creation, validation (including timeout and fingerprint checks),
 * invalidation, and periodic cleanup of user sessions. Sessions are stored
 * in an in-memory map. Interacts with LockManager to release locks when sessions end.
 */
class SessionManager {
public:
    /**
     * @brief Constructs a SessionManager object.
     */
    SessionManager();

    // Creates a new session for a user.
    // Returns the generated SessionData (including the new session ID).
    // Returns an invalid SessionData (empty sessionId) on failure.
    /**
     * @brief Creates a new session for a user upon successful login.
     *
     * Generates a unique session ID and fingerprint, stores the session data
     * (username, role, timestamps) in the active sessions map.
     * @param username The username of the logged-in user.
     * @param role The UserRole of the logged-in user.
     * @param request Pointer to the AsyncWebServerRequest object (used for fingerprinting).
     * @return A copy of the created SessionData. Returns an invalid SessionData object
     *         (sessionId is empty) on failure.
     */
    SessionData createSession(const String& username, UserRole role, AsyncWebServerRequest *request);

    // Validates a session based on the session ID from a request cookie.
    // Checks for existence, expiry, and fingerprint match.
    // Updates the last heartbeat timestamp if valid.
    // Returns a pointer to the valid SessionData if found and valid, nullptr otherwise.
    /**
     * @brief Validates a session based on the session ID cookie from a request.
     *
     * Checks for session existence, expiration (based on last heartbeat and SESSION_TIMEOUT_MS),
     * and matches the client fingerprint. Updates the last heartbeat timestamp if valid.
     * @param request Pointer to the AsyncWebServerRequest object containing the session cookie.
     * @return A pointer to the valid SessionData within the internal map if validation succeeds,
     *         nullptr otherwise. The pointer is only valid until the session expires or is invalidated.
     */
    SessionData* validateSession(AsyncWebServerRequest *request);

    // Invalidates/removes a specific session by ID.
    // Returns true if the session was found and removed, false otherwise.
    /**
     * @brief Invalidates (removes) a specific session by its ID.
     *
     * Also triggers the release of any locks held by this session via LockManager.
     * @param sessionId The ID of the session to invalidate.
     * @return True if the session was found and removed, false otherwise.
     */
    bool invalidateSession(const String& sessionId);

    // Invalidates/removes a session based on the request cookie.
    // Returns true if a session was found and removed, false otherwise.
    /**
     * @brief Invalidates (removes) the session associated with a web request's cookie.
     *
     * Extracts the session ID from the cookie and calls the internal removal logic.
     * Also triggers the release of any locks held by this session via LockManager.
     * @param request Pointer to the AsyncWebServerRequest object containing the session cookie.
     * @return True if a session was found via the cookie and removed, false otherwise.
     */
    bool invalidateSession(AsyncWebServerRequest *request);

    // Periodically called (e.g., from loop()) to clean up expired sessions.
    /**
     * @brief Performs cleanup of expired sessions based on `SESSION_TIMEOUT_MS`.
     *
     * This function should be called periodically (e.g., in the main loop). It checks if the
     * `SESSION_CLEANUP_INTERVAL_MS` has passed and, if so, removes any sessions whose
     * last heartbeat indicates they have expired. Also triggers lock release for removed sessions.
     */
    void cleanupExpiredSessions();

    // Generates a client fingerprint from the request (e.g., IP + User-Agent)
    /**
     * @brief Static utility function to generate a client fingerprint from request details.
     *
     * Creates a hash (SHA-256) based on the client's IP address and User-Agent string.
     * @param request Pointer to the AsyncWebServerRequest object.
     * @return A hexadecimal string representation of the fingerprint hash.
     */
    static String generateFingerprint(AsyncWebServerRequest *request);

    // Generates a secure, high-entropy session ID
    /**
     * @brief Static utility function to generate a secure, high-entropy session ID.
     *
     * Uses `esp_fill_random` to generate random bytes and converts them to a hex string.
     * @return A unique session ID string (typically 64 hex characters).
     */
    static String generateSessionId();

private:
    std::map<String, SessionData> activeSessions; ///< In-memory map storing active sessions, keyed by session ID.
    unsigned long lastCleanupTime = 0; ///< Timestamp of the last expired session cleanup check.

    // Internal helper to remove a session and trigger associated cleanup (like lock release)
    /**
     * @brief Internal helper to remove a session from the map and trigger associated cleanup.
     *
     * This function handles the actual removal from the `activeSessions` map and calls
     * `LockManager::releaseLocksForSession` to ensure resource locks are freed.
     * @param sessionId The ID of the session to remove.
     */
    void removeSessionInternal(const String& sessionId);
};

#endif // SESSION_MANAGER_H