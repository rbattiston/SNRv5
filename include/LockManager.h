#ifndef LOCK_MANAGER_H
#define LOCK_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "FileLock.h"
#include "SessionData.h" // To get session info for acquiring locks

/**
 * @def LOCK_TIMEOUT_MS
 * @brief Defines the duration (in milliseconds) after which an inactive lock is considered expired.
 *        Set to 0 to disable automatic lock expiration. Default: 30 minutes.
 */
// Define optional lock timeout (e.g., 30 minutes) - 0 means no timeout
#define LOCK_TIMEOUT_MS (30 * 60 * 1000)
// Define cleanup check interval (e.g., every 5 minutes)
/**
 * @def LOCK_CLEANUP_INTERVAL_MS
 * @brief Defines how often (in milliseconds) the `cleanupExpiredLocks` function should check for expired locks.
 *        Default: 5 minutes.
 */
#define LOCK_CLEANUP_INTERVAL_MS (5 * 60 * 1000)


/**
 * @class LockManager
 * @brief Manages exclusive locks on resources using a persistent JSON file.
 *
 * This class provides mechanisms to acquire, release, and check locks on resources
 * identified by string IDs (e.g., "schedule_abc"). Locks are associated with user
 * sessions and stored persistently in a JSON file (`/locks/active_locks.json` by default).
 * It includes functionality to automatically clean up expired locks based on a timeout.
 */
class LockManager {
public:
    /**
     * @brief Constructs a LockManager object.
     * @param lockFilePath The path within LittleFS to the file used for storing active locks.
     *                     Defaults to "/locks/active_locks.json".
     */
    LockManager(const char* lockFilePath = "/locks/active_locks.json");

    // Initializes the lock manager (e.g., ensures lock file exists or is empty)
    /**
     * @brief Initializes the LockManager.
     *
     * Ensures the lock file exists (creating an empty one if necessary) and its parent directory.
     * @return True if initialization is successful, false on critical failure (e.g., cannot create directory/file).
     */
    bool begin();

    // Attempts to acquire a lock for a given resource by a specific session.
    // Checks if the resource is already locked by another session.
    // Returns true on success, false if already locked by another or error.
    /**
     * @brief Attempts to acquire an exclusive lock for a given resource by a specific session.
     *
     * Checks if the resource is already locked by another session. If successful, the lock
     * information is recorded persistently. If the lock is already held by the same session,
     * the timestamp is updated (idempotent).
     *
     * @param resourceId The unique identifier of the resource to lock.
     * @param lockType The type of lock being acquired (e.g., EDITING_SCHEDULE).
     * @param session A constant reference to the SessionData of the user acquiring the lock.
     * @return True if the lock was acquired successfully or already held by the session,
     *         false if the resource is locked by another session or if an error occurred.
     */
    bool acquireLock(const String& resourceId, LockType lockType, const SessionData& session);

    // Releases a specific lock held by a specific session.
    // Returns true if the lock was found and released, false otherwise.
    /**
     * @brief Releases a specific lock held by a specific session.
     *
     * Removes the lock entry from the persistent store if the provided sessionId matches
     * the one holding the lock for the given resourceId.
     *
     * @param resourceId The unique identifier of the resource whose lock is to be released.
     * @param sessionId The ID of the session attempting to release the lock.
     * @return True if the lock was found and successfully released, false otherwise (lock not found,
     *         session ID mismatch, or error).
     */
    bool releaseLock(const String& resourceId, const String& sessionId);

    // Releases all locks held by a specific session (e.g., on logout or timeout).
    // Returns the number of locks released.
    /**
     * @brief Releases all locks currently held by a specific session.
     *
     * Typically used on user logout or session expiration to clean up any lingering locks.
     *
     * @param sessionId The ID of the session whose locks should be released.
     * @return The number of locks that were successfully released.
     */
    int releaseLocksForSession(const String& sessionId);

    // Checks if a resource is currently locked.
    // Optionally returns the lock info if locked.
    // Returns true if locked, false otherwise.
    /**
     * @brief Checks if a resource is currently locked by any session.
     *
     * @param resourceId The unique identifier of the resource to check.
     * @param lockInfo Optional. If provided (not nullptr) and the resource is locked,
     *                 this pointer will be populated with the details of the current lock.
     * @return True if the resource is currently locked, false otherwise or if an error occurs reading locks.
     */
    bool isLocked(const String& resourceId, FileLock* lockInfo = nullptr);

    // Gets the lock information for a specific resource, if it's locked.
    // Returns true and populates lockInfo if locked, false otherwise.
    /**
     * @brief Gets the lock information for a specific resource, if it is currently locked.
     *
     * Convenience function that uses `isLocked` to retrieve lock details.
     *
     * @param resourceId The unique identifier of the resource.
     * @param lockInfo A reference to a FileLock struct where the lock details will be copied if found.
     * @return True if the resource is locked and `lockInfo` was populated, false otherwise.
     */
    bool getLockInfo(const String& resourceId, FileLock& lockInfo);

    // Periodically called (e.g., from loop()) to clean up potentially expired locks based on LOCK_TIMEOUT_MS.
    /**
     * @brief Performs cleanup of expired locks based on `LOCK_TIMEOUT_MS`.
     *
     * This function should be called periodically (e.g., in the main loop). It checks if the
     * `LOCK_CLEANUP_INTERVAL_MS` has passed and, if so, removes any locks whose timestamp
     * indicates they have expired. Only runs if `LOCK_TIMEOUT_MS` is greater than 0.
     */
    void cleanupExpiredLocks();

private:
    String _lockFilePath; ///< Path to the file storing active locks.
    unsigned long lastCleanupTime = 0; ///< Timestamp of the last expired lock cleanup check.

    // Internal helper to load all active locks from the JSON file.
    // Returns true on success, false on failure (file read/parse error).
    // Populates the provided vector. Clears the vector first.
    /**
     * @brief Internal helper to load all active locks from the JSON file.
     *
     * Reads the lock file and parses the JSON array into the provided vector.
     * Clears the vector before loading.
     * @param locks Reference to a vector to store the loaded FileLock objects.
     * @return True on success (file read and parsed, even if empty), false on failure.
     */
    bool loadAllLocks(std::vector<FileLock>& locks); // Note: Corrected to &

    // Internal helper to save all active locks to the JSON file.
    // Returns true on success, false on failure (file write/serialize error).
    /**
     * @brief Internal helper to save all active locks to the JSON file.
     *
     * Serializes the provided vector of FileLock objects into a JSON array and
     * overwrites the lock file.
     * @param locks Constant reference to a vector containing the FileLock objects to save.
     * @return True on success, false on failure.
     */
    bool saveAllLocks(const std::vector<FileLock>& locks); // Note: Corrected to &
};

#endif // LOCK_MANAGER_H