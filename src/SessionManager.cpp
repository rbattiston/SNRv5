#include "SessionManager.h"
#include "LockManager.h" // Need to interact with LockManager
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h> // V7
#include <vector>
#include <algorithm> // For std::sort, std::remove_if
#include <ctime>     // For timestamp in UID generation
#include "AuthUtils.h" // For hex conversion and potentially hashing fingerprint
#include "esp_random.h" // For secure random session ID generation
#include "mbedtls/sha256.h" // For fingerprint hashing
#include <cstring> // For memcpy in hashing


// Make sure LockManager instance is declared globally (e.g., in main.cpp)
extern LockManager lockManager;

// --- SessionManager Implementation ---

/**
 * @brief Constructs a SessionManager object.
 *
 * Initializes the time for the last session cleanup check.
 */
SessionManager::SessionManager() {
    lastCleanupTime = millis(); // Initialize cleanup timer
}

// Generates a secure, high-entropy session ID (e.g., 32 random bytes -> 64 hex chars)
/**
 * @brief Generates a cryptographically secure random session ID.
 *
 * Uses the ESP32's hardware random number generator (`esp_fill_random`) to
 * create 32 random bytes and then converts them into a 64-character hexadecimal string.
 *
 * @return A String containing the generated hexadecimal session ID.
 */
String SessionManager::generateSessionId() {
    unsigned char randomBytes[32];
    esp_fill_random(randomBytes, sizeof(randomBytes));
    return AuthUtils::bytesToHex(randomBytes, sizeof(randomBytes));
}

// Generates a client fingerprint from the request (e.g., IP + User-Agent)
/**
 * @brief Generates a client fingerprint based on the request's source IP and User-Agent.
 *
 * Combines the client's remote IP address and the User-Agent header string.
 * Computes the SHA-256 hash of the combined string.
 * Converts the resulting hash into a hexadecimal string to serve as the fingerprint.
 * Used to add a layer of security against session hijacking (though not foolproof).
 *
 * @param request Pointer to the AsyncWebServerRequest object containing client information.
 * @return A String containing the hexadecimal representation of the SHA-256 hash
 *         of the combined IP and User-Agent, or an empty string if the request is null.
 */
String SessionManager::generateFingerprint(AsyncWebServerRequest *request) {
    if (!request) return "";

    String ip = request->client()->remoteIP().toString();
    String userAgent = request->header("User-Agent");

    String combined = ip + userAgent; // Combine IP and User-Agent

    // Hash the combined string using SHA-256
    unsigned char hashOutput[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0); // 0 for SHA-256
    mbedtls_sha256_update(&ctx, (const unsigned char*)combined.c_str(), combined.length());
    mbedtls_sha256_finish(&ctx, hashOutput);
    mbedtls_sha256_free(&ctx);

    return AuthUtils::bytesToHex(hashOutput, sizeof(hashOutput));
}

/**
 * @brief Creates a new user session and stores it in memory.
 *
 * Generates a unique session ID and a client fingerprint. Creates a new
 * `SessionData` object containing the username, role, creation time, initial
 * heartbeat time, and fingerprint. Stores the new session in the `activeSessions` map.
 *
 * @param username The username associated with the session.
 * @param role The UserRole associated with the session.
 * @param request Pointer to the AsyncWebServerRequest object used to generate the fingerprint.
 * @return A copy of the created `SessionData` object. Returns an invalid `SessionData`
 *         object (where `isValid()` is false) if session ID generation fails or the
 *         resulting session data is invalid.
 */
SessionData SessionManager::createSession(const String& username, UserRole role, AsyncWebServerRequest *request) {
    SessionData newSession;
    newSession.sessionId = generateSessionId();
    if (newSession.sessionId.isEmpty()) {
        Serial.println("Error: Failed to generate session ID.");
        return SessionData(); // Return invalid session
    }

    newSession.username = username;
    newSession.userRole = role;
    newSession.creationTime = millis(); // Use millis() for simplicity
    newSession.lastHeartbeat = newSession.creationTime;
    newSession.fingerprint = generateFingerprint(request);

    if (!newSession.isValid()) {
         Serial.println("Error: Newly created session data is invalid.");
         return SessionData(); // Return invalid session
    }

    // Store the session in the map
    activeSessions[newSession.sessionId] = newSession;

    Serial.printf("Session created: ID=%s, User=%s, Role=%s\n",
                  newSession.sessionId.c_str(),
                  newSession.username.c_str(),
                  roleToString(newSession.userRole).c_str());

    return newSession;
}

/**
 * @brief Validates an existing session based on a request's session cookie.
 *
 * Extracts the session ID from the "session_id" cookie in the request headers.
 * Looks up the session ID in the `activeSessions` map.
 * Checks if the session has timed out based on `SESSION_TIMEOUT_MS`.
 * Checks if the client fingerprint matches the one stored in the session.
 * If the session is valid (found, not timed out, fingerprint matches), updates
 * its `lastHeartbeat` timestamp and returns a pointer to the `SessionData`.
 * If validation fails for any reason, the session is removed internally, and
 * nullptr is returned.
 *
 * @param request Pointer to the AsyncWebServerRequest object containing the session cookie.
 * @return A pointer to the valid `SessionData` object if validation succeeds,
 *         nullptr otherwise. The returned pointer points to data within the
 *         `activeSessions` map and should not be deleted by the caller.
 */
SessionData* SessionManager::validateSession(AsyncWebServerRequest *request) {
    if (!request) return nullptr;

    // Get session ID from cookie (assuming cookie name is "session_id")
    const AsyncWebHeader* cookieHeader = request->getHeader("Cookie"); // Added const
    String sessionId = "";
    if (cookieHeader) {
        // Basic cookie parsing - find "session_id=value"
        String cookies = cookieHeader->value();
        int startIndex = cookies.indexOf("session_id=");
        if (startIndex != -1) {
            startIndex += strlen("session_id=");
            int endIndex = cookies.indexOf(';', startIndex);
            if (endIndex == -1) { // Last cookie
                sessionId = cookies.substring(startIndex);
            } else {
                sessionId = cookies.substring(startIndex, endIndex);
            }
            // Trim potential whitespace
             sessionId.trim();
        }
    }

    if (sessionId.isEmpty()) {
        // Serial.println("Validation failed: No session cookie found."); // Debug only
        return nullptr; // No session ID cookie
    }

    // Find session in the map
    auto it = activeSessions.find(sessionId);
    if (it == activeSessions.end()) {
        // Serial.printf("Validation failed: Session ID '%s' not found.\n", sessionId.c_str()); // Debug only
        // Optionally clear the invalid cookie on the client here?
        return nullptr; // Session not found
    }

    SessionData& session = it->second; // Get reference to the session data

    // Check for timeout
    unsigned long currentTime = millis();
    if (currentTime - session.lastHeartbeat > SESSION_TIMEOUT_MS) {
        Serial.printf("Session expired: ID=%s, User=%s\n", sessionId.c_str(), session.username.c_str());
        removeSessionInternal(sessionId); // Remove the expired session
        // Optionally clear the expired cookie on the client here?
        return nullptr; // Session expired
    }

    // Check fingerprint
    String currentFingerprint = generateFingerprint(request);
    if (session.fingerprint != currentFingerprint) {
        Serial.printf("Session validation failed: Fingerprint mismatch for ID=%s, User=%s\n", sessionId.c_str(), session.username.c_str());
        Serial.printf("  Stored: %s\n", session.fingerprint.c_str());
        Serial.printf("  Current: %s\n", currentFingerprint.c_str());
        removeSessionInternal(sessionId); // Treat fingerprint mismatch as potential hijack, invalidate session
        // Optionally clear the invalid cookie on the client here?
        return nullptr; // Fingerprint mismatch
    }

    // Session is valid, update heartbeat
    session.lastHeartbeat = currentTime;
    // Serial.printf("Session validated: ID=%s, User=%s\n", sessionId.c_str(), session.username.c_str()); // Debug only

    return &session; // Return pointer to the valid session data
}

/**
 * @brief Invalidates a specific session by its ID.
 *
 * Finds the session in the `activeSessions` map and removes it using `removeSessionInternal`.
 * This also triggers the release of any locks held by the session via the LockManager.
 *
 * @param sessionId The ID of the session to invalidate.
 * @return True if the session was found and removed, false otherwise.
 */
bool SessionManager::invalidateSession(const String& sessionId) {
    if (sessionId.isEmpty()) return false;

    auto it = activeSessions.find(sessionId);
    if (it != activeSessions.end()) {
        Serial.printf("Invalidating session by ID: %s, User: %s\n", sessionId.c_str(), it->second.username.c_str());
        removeSessionInternal(sessionId);
        return true;
    }
    return false;
}

/**
 * @brief Invalidates the session associated with a given web request.
 *
 * Extracts the session ID from the request's cookie and calls `invalidateSession(sessionId)`.
 *
 * @param request Pointer to the AsyncWebServerRequest object containing the session cookie.
 * @return True if a session ID was found in the cookie and the corresponding session
 *         was successfully invalidated, false otherwise.
 */
bool SessionManager::invalidateSession(AsyncWebServerRequest *request) {
     if (!request) return false;

    // Extract session ID from cookie (same logic as in validateSession)
    const AsyncWebHeader* cookieHeader = request->getHeader("Cookie"); // Added const
    String sessionId = "";
     if (cookieHeader) {
        String cookies = cookieHeader->value();
        int startIndex = cookies.indexOf("session_id=");
        if (startIndex != -1) {
            startIndex += strlen("session_id=");
            int endIndex = cookies.indexOf(';', startIndex);
            if (endIndex == -1) sessionId = cookies.substring(startIndex);
            else sessionId = cookies.substring(startIndex, endIndex);
            sessionId.trim();
        }
    }

    if (!sessionId.isEmpty()) {
        return invalidateSession(sessionId);
    }
    return false;
}


/**
 * @brief Periodically cleans up expired sessions from the active sessions map.
 *
 * This function should be called regularly (e.g., in the main loop).
 * It checks if the configured cleanup interval (`SESSION_CLEANUP_INTERVAL_MS`)
 * has passed since the last cleanup. If so, it iterates through all active sessions,
 * identifies those whose `lastHeartbeat` is older than `SESSION_TIMEOUT_MS`,
 * and removes them using `removeSessionInternal`.
 */
void SessionManager::cleanupExpiredSessions() {
    unsigned long currentTime = millis();
    // Check if it's time to run cleanup
    if (currentTime - lastCleanupTime >= SESSION_CLEANUP_INTERVAL_MS) {
        // Serial.println("Running session cleanup..."); // Debug only
        int cleanedCount = 0;
        // Iterate through the map - C++11 range-based for is convenient
        // Need to be careful when removing elements while iterating.
        // A common pattern is to collect IDs to remove first, then remove them.
        std::vector<String> idsToRemove;
        // Use C++11 compatible loop if C++17 isn't enabled, or enable C++17
        for (const auto& pair : activeSessions) { // C++11 compatible loop
             const String& id = pair.first;
             const SessionData& session = pair.second;
             if (currentTime - session.lastHeartbeat > SESSION_TIMEOUT_MS) {
                 idsToRemove.push_back(id);
             }
        }
        /* // C++17 version (requires build_flags = -std=c++17)
        for (auto const& [id, session] : activeSessions) {
            if (currentTime - session.lastHeartbeat > SESSION_TIMEOUT_MS) {
                idsToRemove.push_back(id);
            }
        }
        */

        for (const String& id : idsToRemove) {
             Serial.printf("Cleaning up expired session: ID=%s, User=%s\n", id.c_str(), activeSessions[id].username.c_str());
             removeSessionInternal(id); // Remove the session and handle locks
             cleanedCount++;
        }

        if (cleanedCount > 0) {
             Serial.printf("Session cleanup finished. Removed %d expired session(s).\n", cleanedCount);
        }
        lastCleanupTime = currentTime; // Reset the timer
    }
}

// Internal helper to remove session and trigger lock cleanup
/**
 * @brief Internal helper function to remove a session and release its locks.
 * @note Should not be called directly from outside the class. Use invalidateSession instead.
 *
 * Finds the session by ID in the `activeSessions` map.
 * Calls `lockManager.releaseLocksForSession()` to release any locks held by this session.
 * Erases the session entry from the `activeSessions` map.
 *
 * @param sessionId The ID of the session to remove.
 */
void SessionManager::removeSessionInternal(const String& sessionId) {
    auto it = activeSessions.find(sessionId);
    if (it != activeSessions.end()) {
        String username = it->second.username; // Get username before erasing

        // --- Phase 4 Integration: Release Locks ---
        // Call LockManager to release locks associated with this sessionId
        int locksReleased = lockManager.releaseLocksForSession(sessionId);
        if (locksReleased > 0) {
             Serial.printf("Released %d lock(s) due to session removal for %s.\n", locksReleased, username.c_str());
        }

        // Erase the session from the map
        activeSessions.erase(it);
        Serial.printf("Session removed from memory: ID=%s\n", sessionId.c_str());
    }
}