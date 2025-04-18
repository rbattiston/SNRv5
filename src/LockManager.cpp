#include "LockManager.h"
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h> // V7
#include <vector>
#include <algorithm> // For std::remove_if

// --- LockManager Implementation ---

/**
 * @brief Constructs a LockManager object.
 *
 * Initializes the manager with the path to the lock file and sets the
 * initial time for the last cleanup check.
 *
 * @param lockFilePath The path within the LittleFS filesystem where the lock
 *                     file is stored (e.g., "/locks/locks.json").
 */
LockManager::LockManager(const char* lockFilePath) : _lockFilePath(lockFilePath) {
    lastCleanupTime = millis();
}

/**
 * @brief Initializes the LockManager.
 *
 * Ensures the parent directory for the lock file exists, creating it if necessary.
 * Checks if the lock file itself exists; if not, it creates an empty, valid
 * JSON array file. This ensures the manager starts in a consistent state.
 *
 * @return True if initialization is successful (directory and file exist or
 *         were created), false if directory creation fails.
 */
bool LockManager::begin() {
    Serial.println("Initializing LockManager...");
    // Ensure the parent directory exists
    String parentDir = "/locks";
    if (!LittleFS.exists(parentDir)) {
        Serial.printf("Lock directory '%s' not found. Creating.\n", parentDir.c_str());
        if (!LittleFS.mkdir(parentDir)) {
            Serial.printf("FATAL: Failed to create lock directory '%s'. Halting.\n", parentDir.c_str());
            return false;
        }
    }

    // Check if lock file exists, create an empty one if not
    if (!LittleFS.exists(_lockFilePath)) {
        Serial.printf("Lock file '%s' not found. Creating empty lock file.\n", _lockFilePath.c_str());
        std::vector<FileLock> emptyLocks;
        if (!saveAllLocks(emptyLocks)) {
            Serial.printf("FATAL: Failed to create empty lock file '%s'.\n", _lockFilePath.c_str());
            return false;
        }
    } else {
        // Optional: Validate existing lock file format on startup?
        Serial.printf("Lock file found: %s\n", _lockFilePath.c_str());
    }
    Serial.println("LockManager initialized successfully.");
    return true;
}

// Internal helper to load all active locks
/**
 * @brief Loads all active locks from the lock file into a vector.
 * @note Internal helper function.
 *
 * Opens the lock file, reads its content, and parses it as a JSON array
 * of FileLock objects. Populates the provided vector with valid locks found
 * in the file. Clears the vector before loading. Handles empty files and
 * JSON parsing errors gracefully.
 *
 * @param locks A reference to a std::vector<FileLock> to be populated with the loaded locks.
 * @return True if the file was opened and parsed successfully (even if empty),
 *         false if the file could not be opened or if JSON parsing failed.
 */
bool LockManager::loadAllLocks(std::vector<FileLock>& locks) { // Corrected: &
    locks.clear(); // Clear the vector before loading

    File lockFile = LittleFS.open(_lockFilePath, "r");
    if (!lockFile) {
        Serial.printf("Failed to open lock file for reading: %s\n", _lockFilePath.c_str());
        return false;
    }
    if (lockFile.size() == 0) {
        // File is empty, which is valid (no locks)
        lockFile.close();
        return true;
    }

    // V7: Use JsonDocument
    JsonDocument doc;
    // StaticJsonDocument<4096> doc; // Consider static if max size known

    DeserializationError error = deserializeJson(doc, lockFile);
    lockFile.close();

    if (error) {
        Serial.printf("Failed to parse lock file %s: %s\n", _lockFilePath.c_str(), error.c_str());
        Serial.println("Lock file might be corrupted. Treating as empty.");
        return false; // Indicate failure
    }

    JsonArray array = doc.as<JsonArray>();
    if (array.isNull()) {
         Serial.printf("Lock file %s does not contain a valid JSON array.\n", _lockFilePath.c_str());
         return false; // Treat as error
    }

    for (JsonObject lockObj : array) { // V7: Iterate directly
        FileLock lock;
        lock.resourceId = lockObj["resourceId"] | "";
        lock.lockType = stringToLockType(lockObj["lockType"] | "");
        lock.sessionId = lockObj["sessionId"] | "";
        lock.username = lockObj["username"] | "";
        lock.timestamp = lockObj["timestamp"] | 0UL;

        if (lock.isValid()) {
            locks.push_back(lock);
        } else {
             Serial.println("Warning: Found invalid lock data in lock file. Skipping.");
        }
    }
    // Serial.printf("Loaded %d locks from file.\n", locks.size()); // Debug
    return true;
}

// Internal helper to save all active locks
/**
 * @brief Saves a vector of active locks to the lock file.
 * @note Internal helper function.
 *
 * Opens the lock file in write mode (truncating existing content).
 * Serializes the provided vector of FileLock objects into a JSON array
 * and writes it to the file. Only valid locks from the vector are saved.
 *
 * @param locks A constant reference to a std::vector<FileLock> containing the locks to save.
 * @return True if the file was opened and the JSON was serialized successfully,
 *         false otherwise.
 */
bool LockManager::saveAllLocks(const std::vector<FileLock>& locks) { // Corrected: &
    File lockFile = LittleFS.open(_lockFilePath, "w"); // Open for writing (truncates)
    if (!lockFile) {
        Serial.printf("Failed to open lock file for writing: %s\n", _lockFilePath.c_str());
        return false;
    }

    // V7: Use JsonDocument
    JsonDocument doc;
    // StaticJsonDocument<4096> doc; // Consider static if max size known
    JsonArray array = doc.to<JsonArray>();

    for (const auto& lock : locks) {
        if (lock.isValid()) { // Only save valid locks
            JsonObject lockObj = array.add<JsonObject>(); // V7: Use add<T>()
            lockObj["resourceId"] = lock.resourceId;
            lockObj["lockType"] = lockTypeToString(lock.lockType);
            lockObj["sessionId"] = lock.sessionId;
            lockObj["username"] = lock.username;
            lockObj["timestamp"] = lock.timestamp;
        }
    }

    if (serializeJson(doc, lockFile) == 0) {
        Serial.printf("Failed to write lock data to file: %s\n", _lockFilePath.c_str());
        lockFile.close();
        return false;
    }

    lockFile.close();
    // Serial.printf("Saved %d locks to file.\n", locks.size()); // Debug
    return true;
}


/**
 * @brief Attempts to acquire a lock on a specific resource for a given session.
 *
 * Loads the current locks, checks if the resource is already locked by a
 * different session. If locked by the same session, it updates the timestamp
 * (idempotent acquire). If not locked, it creates a new lock entry with the
 * current timestamp and saves the updated lock list.
 *
 * @param resourceId The unique identifier of the resource to lock (e.g., "schedule_abc").
 * @param lockType The type of lock being acquired (e.g., EDITING_SCHEDULE).
 * @param session A constant reference to the SessionData object of the user acquiring the lock.
 * @return True if the lock was acquired successfully (or already held by the session),
 *         false if the resource is locked by another session, if parameters are invalid,
 *         or if loading/saving locks fails.
 */
bool LockManager::acquireLock(const String& resourceId, LockType lockType, const SessionData& session) {
    if (resourceId.isEmpty() || !session.isValid()) {
        Serial.println("Error: Invalid parameters for acquireLock.");
        return false;
    }

    std::vector<FileLock> currentLocks;
    if (!loadAllLocks(currentLocks)) {
        Serial.println("Error loading locks, cannot acquire new lock.");
        // This indicates a potentially serious filesystem/parsing issue.
        return false;
    }

    // Check if resource is already locked by *another* session
    for (const auto& existingLock : currentLocks) {
        if (existingLock.resourceId == resourceId) {
            if (existingLock.sessionId != session.sessionId) {
                // Locked by someone else
                Serial.printf("Resource '%s' already locked by session '%s' (User: %s).\n",
                              resourceId.c_str(), existingLock.sessionId.c_str(), existingLock.username.c_str());
                return false;
            } else {
                // Already locked by the *same* session - allow re-acquiring? Or just return true?
                // Let's update the timestamp and return true (idempotent acquire)
                Serial.printf("Resource '%s' already locked by this session. Updating timestamp.\n", resourceId.c_str());
                // Find the lock and update its timestamp (mutable vector needed, or reload/save)
                // Easiest is to remove the old one and add a new one.
                 currentLocks.erase(std::remove_if(currentLocks.begin(), currentLocks.end(),
                                   [&](const FileLock& l){ return l.resourceId == resourceId && l.sessionId == session.sessionId; }),
                                   currentLocks.end());
                 // Now add the new one below
                 break; // Exit loop after finding the lock
            }
        }
    }

    // --- TODO: Add check here: Is resource part of an ACTIVE cycle? ---
    // This requires access to cycle management logic, which isn't built yet.
    // bool isActive = CycleManager::isResourceInActiveCycle(resourceId);
    // if (isActive) {
    //    Serial.printf("Cannot acquire lock: Resource '%s' is part of an active cycle.\n", resourceId.c_str());
    //    return false;
    // }

    // Acquire the lock
    FileLock newLock;
    newLock.resourceId = resourceId;
    newLock.lockType = lockType;
    newLock.sessionId = session.sessionId;
    newLock.username = session.username; // Store username for convenience
    newLock.timestamp = millis();

    if (!newLock.isValid()) {
        Serial.println("Error: Constructed lock data is invalid.");
        return false;
    }

    currentLocks.push_back(newLock);

    if (saveAllLocks(currentLocks)) {
        Serial.printf("Lock acquired for resource '%s' by session '%s' (User: %s).\n",
                      resourceId.c_str(), session.sessionId.c_str(), session.username.c_str());
        return true;
    } else {
        Serial.println("Error saving locks after acquiring new lock.");
        return false;
    }
}

/**
 * @brief Releases a specific lock held by a specific session.
 *
 * Loads the current locks, finds the lock matching the resource ID and session ID,
 * removes it from the list, and saves the updated list.
 *
 * @param resourceId The unique identifier of the resource whose lock is to be released.
 * @param sessionId The ID of the session that holds the lock.
 * @return True if the lock was found and successfully removed and saved,
 *         false if the lock was not found for the given resource/session,
 *         or if loading/saving locks fails.
 */
bool LockManager::releaseLock(const String& resourceId, const String& sessionId) {
     if (resourceId.isEmpty() || sessionId.isEmpty()) return false;

    std::vector<FileLock> currentLocks;
    if (!loadAllLocks(currentLocks)) {
        Serial.println("Error loading locks, cannot release lock.");
        return false;
    }

    size_t initialSize = currentLocks.size();
    currentLocks.erase(std::remove_if(currentLocks.begin(), currentLocks.end(),
                       [&](const FileLock& lock){ return lock.resourceId == resourceId && lock.sessionId == sessionId; }),
                       currentLocks.end());

    if (currentLocks.size() < initialSize) {
        // Lock was found and removed
        if (saveAllLocks(currentLocks)) {
             Serial.printf("Lock released for resource '%s' by session '%s'.\n", resourceId.c_str(), sessionId.c_str());
             return true;
        } else {
            Serial.println("Error saving locks after releasing lock.");
            // Attempt to restore? Or just report error? Report error for now.
            return false;
        }
    } else {
        // Lock not found for this resource/session combination
        // Serial.printf("Lock not found for release: Resource '%s', Session '%s'.\n", resourceId.c_str(), sessionId.c_str()); // Debug
        return false;
    }
}

/**
 * @brief Releases all locks held by a specific session.
 *
 * Loads the current locks, removes all entries associated with the given session ID,
 * and saves the updated list. Useful for cleaning up locks on user logout or session expiry.
 *
 * @param sessionId The ID of the session whose locks should be released.
 * @return The number of locks that were successfully released and saved. Returns 0
 *         if no locks were found for the session or if loading/saving fails.
 */
int LockManager::releaseLocksForSession(const String& sessionId) {
    if (sessionId.isEmpty()) return 0;

    std::vector<FileLock> currentLocks;
    if (!loadAllLocks(currentLocks)) {
        Serial.println("Error loading locks, cannot release locks for session.");
        return 0;
    }

    size_t initialSize = currentLocks.size();
    currentLocks.erase(std::remove_if(currentLocks.begin(), currentLocks.end(),
                       [&](const FileLock& lock){ return lock.sessionId == sessionId; }),
                       currentLocks.end());

    int releasedCount = initialSize - currentLocks.size();

    if (releasedCount > 0) {
        if (saveAllLocks(currentLocks)) {
             Serial.printf("Released %d lock(s) for session '%s'.\n", releasedCount, sessionId.c_str());
        } else {
            Serial.printf("Error saving locks after releasing %d lock(s) for session '%s'.\n", releasedCount, sessionId.c_str());
            // Locks might be released in memory but not saved - problematic.
            // Return 0 to indicate failure? Or return count but log error? Log error, return count.
        }
    } else {
         // Serial.printf("No locks found to release for session '%s'.\n", sessionId.c_str()); // Debug
    }
    return releasedCount;
}

/**
 * @brief Checks if a specific resource is currently locked.
 *
 * Loads the current locks and searches for an entry matching the given resource ID.
 * Optionally copies the lock information if found.
 *
 * @param resourceId The unique identifier of the resource to check.
 * @param lockInfo Optional. A pointer to a FileLock struct. If provided and the
 *                 resource is locked, the details of the lock will be copied here.
 * @return True if the resource is currently locked, false otherwise or if loading locks fails.
 */
bool LockManager::isLocked(const String& resourceId, FileLock* lockInfo) {
    std::vector<FileLock> currentLocks;
    // Don't print error here, just check status
    if (!loadAllLocks(currentLocks)) {
        return false; // Treat load failure as "not locked" for safety? Or indicate error? Let's say not locked.
    }

    for (const auto& lock : currentLocks) {
        if (lock.resourceId == resourceId) {
            if (lockInfo) {
                *lockInfo = lock; // Copy lock info if pointer provided
            }
            return true; // Resource is locked
        }
    }
    return false; // Resource not found in locks
}

/**
 * @brief Gets the lock information for a specific resource, if it is locked.
 *
 * Convenience function that calls `isLocked` to find the lock and populate
 * the provided `lockInfo` struct.
 *
 * @param resourceId The unique identifier of the resource to get lock info for.
 * @param lockInfo A reference to a FileLock struct where the lock details will be
 *                 copied if the resource is found to be locked.
 * @return True if the resource is locked and `lockInfo` was populated, false otherwise.
 */
bool LockManager::getLockInfo(const String& resourceId, FileLock& lockInfo) {
     return isLocked(resourceId, &lockInfo); // Use isLocked to find and populate
}


/**
 * @brief Periodically cleans up locks that have exceeded their timeout duration.
 *
 * This function should be called regularly within the main application loop.
 * It checks if the configured cleanup interval has passed since the last run.
 * If so, it loads all locks, removes any whose timestamp is older than the
 * current time minus `LOCK_TIMEOUT_MS`, and saves the updated list.
 * Requires `LOCK_TIMEOUT_MS` to be defined and greater than 0 for cleanup to occur.
 */
void LockManager::cleanupExpiredLocks() {
    #if LOCK_TIMEOUT_MS > 0
    unsigned long currentTime = millis();
    if (currentTime - lastCleanupTime >= LOCK_CLEANUP_INTERVAL_MS) {
        // Serial.println("Running lock cleanup..."); // Debug
        std::vector<FileLock> currentLocks;
        if (!loadAllLocks(currentLocks)) {
            Serial.println("Error loading locks during cleanup.");
            lastCleanupTime = currentTime; // Still update time to prevent spamming errors
            return;
        }

        size_t initialSize = currentLocks.size();
        currentLocks.erase(std::remove_if(currentLocks.begin(), currentLocks.end(),
                           [&](const FileLock& lock){
                               if (currentTime - lock.timestamp > LOCK_TIMEOUT_MS) {
                                   Serial.printf("Lock expired: Resource '%s', Session '%s', User '%s'\n",
                                                 lock.resourceId.c_str(), lock.sessionId.c_str(), lock.username.c_str());
                                   return true; // Mark for removal
                               }
                               return false;
                           }),
                           currentLocks.end());

        int cleanedCount = initialSize - currentLocks.size();
        if (cleanedCount > 0) {
            if (saveAllLocks(currentLocks)) {
                Serial.printf("Lock cleanup finished. Removed %d expired lock(s).\n", cleanedCount);
            } else {
                 Serial.printf("Error saving locks after cleaning %d expired lock(s).\n", cleanedCount);
            }
        }
        lastCleanupTime = currentTime;
    }
    #endif // LOCK_TIMEOUT_MS > 0
}