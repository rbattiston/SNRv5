#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "ScheduleData.h" // Include the data structures

// Forward declaration if needed, or include directly
// class LockManager;

// Represents an entry in the allSchedules.json index file
struct ScheduleFile {
    String scheduleUID;                 // Unique identifier of the schedule file
    String name;                // User-friendly name of the schedule
    int persistentLockLevel = 0; // 0: unlocked, 1: template lock, 2: cycle lock
    String lockedBy;            // Optional: Session ID or user who holds a dynamic lock (for UI display)

    // Check if the basic required fields are present
    bool isValid() const {
        return !scheduleUID.isEmpty() && !name.isEmpty();
    }
};

/**
 * @class ScheduleManager
 * @brief Manages the creation, loading, saving, deletion, and validation of irrigation schedules.
 *
 * This class handles interactions with schedule files stored in LittleFS, maintains an
 * index of available schedules (`allSchedules.json`), and provides methods for manipulating
 * schedule data, including adding and validating different types of irrigation events.
 */
class ScheduleManager {
public:
    /**
     * @brief Constructs a ScheduleManager object.
     * @param scheduleDir Path to the directory where schedule JSON files are stored. Defaults to "/daily_schedules/".
     * @param indexFile Path to the JSON file storing the index of all schedules. Defaults to "/allSchedules.json".
     */
    ScheduleManager(const char* scheduleDir = "/daily_schedules/", const char* indexFile = "/allSchedules.json");

    // Initializes the manager, loads index, performs maintenance.
    // Returns true on success, false on critical failure.
    /**
     * @brief Initializes the ScheduleManager.
     *
     * Ensures the schedule directory exists, loads the schedule index, and performs
     * maintenance to synchronize the index with filesystem contents.
     * @return True on successful initialization, false on critical failure (e.g., cannot create directory).
     */
    bool begin();

    // Gets the current list of schedules from the index.
    // Returns true on success, false if index couldn't be loaded.
    /**
     * @brief Gets the current list of schedules from the in-memory index.
     *
     * Updates the dynamic lock status (`lockedBy`) from LockManager before returning the list.
     * @param list Reference to a vector that will be populated with ScheduleFile entries.
     * @return True if the list was successfully retrieved (assumes index is valid after begin()).
     */
    bool getScheduleList(std::vector<ScheduleFile>& list); // Corrected: &

    // Loads the full details of a specific schedule.
    // Returns true on success, false if file not found or parse error.
    /**
     * @brief Loads the full details of a specific schedule from its file.
     * @param uid The unique identifier of the schedule to load.
     * @param schedule Reference to a Schedule object to be populated with the loaded data.
     * @return True on successful load and parse, false if the file is not found or contains invalid JSON/data.
     */
    bool loadSchedule(const String& uid, Schedule& schedule);

    // Saves a schedule to its corresponding file.
    // Returns true on success, false on write/serialization error.
    /**
     * @brief Saves a schedule object to its corresponding JSON file.
     *
     * Validates the schedule data before saving. If the schedule is new, it also updates the index.
     * @param schedule Constant reference to the Schedule object to save.
     * @return True on successful save, false if the schedule data is invalid or a write/serialization error occurs.
     */
    bool saveSchedule(const Schedule& schedule);

    // Deletes a schedule file and updates the index.
    // Returns true on success, false on file deletion error or index save error.
    /**
     * @brief Deletes a schedule file from the filesystem and removes its entry from the index.
     * @param uid The unique identifier of the schedule to delete.
     * @return True on successful deletion (file and index entry), false if the file doesn't exist,
     *         deletion fails, or saving the updated index fails.
     */
    bool deleteSchedule(const String& uid);

    // Creates a new Schedule object in memory with a generated UID.
    // Does NOT save it automatically.
    // Returns true on success, false if name is invalid.
    /**
     * @brief Creates a new, empty Schedule object in memory with a generated UID.
     *
     * Initializes a Schedule object with the given name and a unique ID. Does NOT save it to the filesystem.
     * @param name The desired name for the new schedule.
     * @param newSchedule Reference to a Schedule object that will be populated.
     * @return True if the name is valid and the object was created, false otherwise.
     */
    bool createSchedule(const String& name, Schedule& newSchedule);

    // --- Event Validation and Addition ---
    // These methods attempt to add events to an existing Schedule object in memory.
    // They perform validation checks (overlap, limits) before adding.
    // Return true if added successfully, false otherwise (e.g., validation failed).

    /**
     * @brief Validates and adds a single AutoPilotWindow event to a schedule object in memory.
     * @param schedule Reference to the Schedule object to modify.
     * @param event The AutoPilotWindow event to add.
     * @return True if the event is valid and added successfully, false otherwise.
     */
    bool validateAndAddEvent(Schedule& schedule, const AutopilotWindow& event);
    /**
     * @brief Validates and adds multiple DurationEvent events to a schedule object in memory.
     * @param schedule Reference to the Schedule object to modify.
     * @param events Vector of DurationEvent events to add.
     * @return True if all events are valid and added successfully, false otherwise.
     */
    bool validateAndAddEvents(Schedule& schedule, const std::vector<DurationEvent>& events); // Corrected: &
    /**
     * @brief Validates and adds multiple VolumeEvent events to a schedule object in memory.
     * @param schedule Reference to the Schedule object to modify.
     * @param events Vector of VolumeEvent events to add.
     * @return True if all events are valid and added successfully, false otherwise.
     */
    bool validateAndAddEvents(Schedule& schedule, const std::vector<VolumeEvent>& events); // Corrected: &

    // Gets the persistent lock level (template/cycle lock) for a schedule UID from the index.
    // Returns lock level (0, 1, 2) or -1 if UID not found in index.
    /**
     * @brief Gets the persistent lock level (template/cycle lock) for a schedule UID.
     *
     * Reads the lock status stored in the in-memory schedule index.
     * @param uid The unique identifier of the schedule.
     * @return The persistent lock level (0, 1, or 2) or -1 if the UID is not found in the index.
     */
    int getPersistentLockLevel(const String& uid);

    // --- Public Static Helpers ---
    // Helper to sort event vectors by start time (made public for std::sort)
    /**
     * @brief Static comparison function for sorting DurationEvents by start time.
     * @param a First DurationEvent.
     * @param b Second DurationEvent.
     * @return True if a starts before b.
     */
    static bool compareDurationEvents(const DurationEvent& a, const DurationEvent& b);
    /**
     * @brief Static comparison function for sorting VolumeEvents by start time.
     * @param a First VolumeEvent.
     * @param b Second VolumeEvent.
     * @return True if a starts before b.
     */
    static bool compareVolumeEvents(const VolumeEvent& a, const VolumeEvent& b);
    /**
     * @brief Static comparison function for sorting AutoPilotWindows by start time.
     * @param a First AutoPilotWindow.
     * @param b Second AutoPilotWindow.
     * @return True if a starts before b.
     */
    static bool compareAutopilotWindows(const AutopilotWindow& a, const AutopilotWindow& b);


private:
    String _scheduleDir; ///< Path to the directory containing schedule files.
    String _indexFile;   ///< Path to the schedule index file (allSchedules.json).
    std::vector<ScheduleFile> _scheduleIndex; ///< In-memory cache of the schedule index.

    // Loads the index file into _scheduleIndex. Returns true on success.
    /**
     * @brief Internal helper to load the schedule index file into the _scheduleIndex vector.
     * @return True on success (file read/parsed or file not found/empty), false on parse error.
     */
    bool loadScheduleIndex();
    // Saves the _scheduleIndex to the index file. Returns true on success.
    /**
     * @brief Internal helper to save the _scheduleIndex vector to the index file.
     * @return True on successful save, false otherwise.
     */
    bool saveScheduleIndex();
    // Syncs _scheduleIndex with files on disk and lock status.
    /**
     * @brief Internal helper to synchronize the _scheduleIndex with actual files on disk and update lock status.
     *        Saves the index if changes were detected.
     */
    void maintainScheduleIndex();

    // Generates a unique ID for a new schedule.
    /**
     * @brief Internal helper to generate a unique ID for a new schedule based on name and timestamp.
     * @param name The base name for the schedule.
     * @return A unique identifier string.
     */
    String generateUID(const String& name);
    // Sanitizes a name to be filesystem-friendly.
    /**
     * @brief Internal helper to sanitize a name to be filesystem-friendly.
     * @param name The input name string.
     * @return A sanitized string suitable for filenames.
     */
    String sanitizeFilename(const String& name);

    // --- Internal Validation Helpers ---
    /** @brief Checks if a new Autopilot Window overlaps with existing ones. */
    bool checkAutopilotOverlap(const Schedule& schedule, const AutopilotWindow& newEvent);
    /** @brief Checks if adding events exceeds the combined Duration/Volume event limit. */
    bool checkDurationVolumeLimit(const Schedule& schedule, size_t newEventCount);
    /** @brief Checks if a new Duration Event overlaps with existing ones. */
    bool checkDurationOverlap(const Schedule& schedule, const DurationEvent& newEvent);
    /** @brief Checks if a new Volume Event overlaps with existing Volume or Duration events. */
    bool checkVolumeOverlap(const Schedule& schedule, const VolumeEvent& newEvent);
    /** @brief Checks if a time (minutes since midnight) is within the valid range 0-1439. */
    bool checkTimeBounds(int timeMinutes); // Checks 0-1439

};

#endif // SCHEDULE_MANAGER_H