#include "ScheduleManager.h"
#include "LockManager.h" // Need to interact with LockManager
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h> // V7
#include <vector>
#include <algorithm> // For std::sort, std::remove_if
#include <ctime>     // For timestamp in UID generation

// Make sure LockManager instance is declared globally (e.g., in main.cpp)
extern LockManager lockManager;

// --- ScheduleManager Implementation ---

/**
 * @brief Constructs a ScheduleManager object.
 *
 * Initializes the manager with the directory path for schedule files and the
 * path for the index file. Ensures the schedule directory path ends with a slash.
 *
 * @param scheduleDir Path to the directory where individual schedule JSON files are stored.
 * @param indexFile Path to the JSON file that stores the index of all schedules.
 */
ScheduleManager::ScheduleManager(const char* scheduleDir, const char* indexFile)
    : _scheduleDir(scheduleDir), _indexFile(indexFile) {
    // Ensure scheduleDir ends with a slash
    if (!_scheduleDir.endsWith("/")) {
        _scheduleDir += "/";
    }
}

/**
 * @brief Initializes the ScheduleManager.
 *
 * Ensures the schedule directory exists, creating it if necessary.
 * Loads the schedule index file (`allSchedules.json`). If loading fails or the
 * file doesn't exist, it attempts to create an empty index file.
 * Finally, it calls `maintainScheduleIndex()` to synchronize the index with
 * the actual files in the schedule directory and update lock statuses.
 *
 * @return True if initialization is successful (directory exists, index loaded or created),
 *         false if critical failures occur (e.g., cannot create directory or initial index file).
 */
bool ScheduleManager::begin() {
    Serial.println("Initializing ScheduleManager...");
    // Ensure the schedule directory exists
    if (!LittleFS.exists(_scheduleDir)) {
        Serial.printf("Schedule directory '%s' not found. Creating.\n", _scheduleDir.c_str());
        if (!LittleFS.mkdir(_scheduleDir)) {
            Serial.printf("FATAL: Failed to create schedule directory '%s'. Halting.\n", _scheduleDir.c_str());
            return false; // Critical failure
        }
    }

    // Load index and perform maintenance
    if (!loadScheduleIndex()) {
         Serial.println("Warning: Failed to load schedule index. Attempting to create/use empty index.");
         // Attempt to save an empty index to ensure the file exists
         if (!saveScheduleIndex()) {
             Serial.println("FATAL: Failed to create initial schedule index file. Halting.");
             return false;
         }
    }
    maintainScheduleIndex(); // Sync with actual files and lock status

    Serial.println("ScheduleManager initialized successfully.");
    return true;
}

// --- Index Management ---

/**
 * @brief Loads the schedule index from the JSON file (`allSchedules.json`).
 *
 * Clears the internal `_scheduleIndex` vector. Opens and parses the index file.
 * Populates the `_scheduleIndex` with valid `ScheduleFile` entries found.
 * Handles cases where the file doesn't exist or is empty (returns true, empty index).
 *
 * @return True if the index was loaded successfully (or file was empty/not found),
 *         false if the file exists but failed to parse (corrupted).
 */
bool ScheduleManager::loadScheduleIndex() {
    _scheduleIndex.clear();
    File file = LittleFS.open(_indexFile, "r");
    if (!file) {
        Serial.printf("Schedule index file not found: %s\n", _indexFile.c_str());
        // Return true, indicating okay to proceed with an empty index (will be created by save)
        return true;
    }
     if (file.size() == 0) {
        // File is empty, valid state
        file.close();
        return true;
    }

    // V7: Use JsonDocument
    JsonDocument doc;
    // Consider StaticJsonDocument if max size is known and memory is tight
    // StaticJsonDocument<4096> doc;

    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.printf("Failed to parse schedule index %s: %s\n", _indexFile.c_str(), error.c_str());
        return false; // Indicate failure
    }

    JsonArray array = doc.as<JsonArray>();
     if (array.isNull()) {
         Serial.printf("Schedule index file %s does not contain a valid JSON array.\n", _indexFile.c_str());
         return false; // Treat as error
    }

    for (JsonObject obj : array) { // V7: Iterate directly
        ScheduleFile sf;
        sf.scheduleUID = obj["scheduleUID"] | "";
        sf.persistentLockLevel = obj["locked"] | 0; // Default to unlocked
        // lockedBy is determined dynamically by LockManager, not stored persistently here

        if (sf.isValid()) {
            _scheduleIndex.push_back(sf);
        } else {
            Serial.printf("Warning: Skipping invalid entry in schedule index: UID=%s\n", sf.scheduleUID.c_str());
        }
    }
    Serial.printf("Loaded %d entries from schedule index.\n", _scheduleIndex.size());
    return true;
}

/**
 * @brief Saves the current in-memory schedule index to the JSON file.
 *
 * Opens the index file in write mode (truncating existing content).
 * Serializes the internal `_scheduleIndex` vector into a JSON array containing
 * only the `scheduleUID` and persistent `locked` status for each entry.
 * The dynamic `lockedBy` field (editing lock) is not saved here.
 *
 * @return True if the index was saved successfully, false if file operations
 *         or JSON serialization failed.
 */
bool ScheduleManager::saveScheduleIndex() {
    File file = LittleFS.open(_indexFile, "w");
    if (!file) {
        Serial.printf("Failed to open schedule index for writing: %s\n", _indexFile.c_str());
        return false;
    }

    // V7: Use JsonDocument
    JsonDocument doc;
    // StaticJsonDocument<4096> doc;
    JsonArray array = doc.to<JsonArray>();

    for (const auto& sf : _scheduleIndex) {
        JsonObject obj = array.add<JsonObject>(); // V7: Use add<T>()
        obj["scheduleUID"] = sf.scheduleUID;
        obj["locked"] = sf.persistentLockLevel; // Store persistent lock level
        // Don't save editing lock info (lockedBy) here
    }

    if (serializeJson(doc, file) == 0) {
        Serial.printf("Failed to write schedule index to file: %s\n", _indexFile.c_str());
        file.close();
        return false;
    }

    file.close();
    // Serial.printf("Saved %d entries to schedule index.\n", _scheduleIndex.size()); // Debug
    return true;
}

/**
 * @brief Synchronizes the in-memory schedule index with the actual files on the filesystem.
 *
 * Performs the following steps:
 * 1. Lists all `.json` files in the schedule directory.
 * 2. Removes entries from the in-memory index that correspond to deleted files.
 * 3. Adds entries to the in-memory index for new files found in the directory.
 * 4. Updates the dynamic `lockedBy` field for each index entry based on the current
 *    status reported by the LockManager (this is not saved persistently in the index file).
 * 5. If any changes were made to the index (added/removed entries), saves the updated index file.
 */
void ScheduleManager::maintainScheduleIndex() {
    Serial.println("Maintaining schedule index...");
    bool indexChanged = false;

    // 1. List actual files in the schedule directory
    std::vector<String> actualFiles;
    File root = LittleFS.open(_scheduleDir);
    if (!root) {
        Serial.println("Failed to open schedule directory for maintenance.");
        return;
    }
    File file = root.openNextFile();
    while(file){
        if (!file.isDirectory()) {
            String filename = file.name();
            // Extract UID (filename without path and extension)
            int lastSlash = filename.lastIndexOf('/');
            int lastDot = filename.lastIndexOf('.');
            if (lastDot > lastSlash && filename.endsWith(".json")) { // Ensure dot is after slash and it's json
                 String uid = filename.substring(lastSlash + 1, lastDot);
                 if (!uid.isEmpty()) {
                    actualFiles.push_back(uid);
                 }
            }
        }
        file = root.openNextFile();
    }
    root.close();

    // 2. Remove entries from index that no longer have files
    auto removeIter = std::remove_if(_scheduleIndex.begin(), _scheduleIndex.end(),
        [&](const ScheduleFile& sf) {
            bool found = false;
            for (const String& actualUid : actualFiles) {
                if (sf.scheduleUID == actualUid) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                Serial.printf("Index Maintenance: Removing entry for deleted file: %s\n", sf.scheduleUID.c_str());
                indexChanged = true;
                return true; // Mark for removal
            }
            return false;
        });
    if (removeIter != _scheduleIndex.end()) { // Check if erase is needed
        _scheduleIndex.erase(removeIter, _scheduleIndex.end());
        indexChanged = true; // Mark changed if elements were removed
    }


    // 3. Add entries to index for files that are not listed
    for (const String& actualUid : actualFiles) {
        bool foundInIndex = false;
        for (const ScheduleFile& sf : _scheduleIndex) {
            if (sf.scheduleUID == actualUid) {
                foundInIndex = true;
                break;
            }
        }
        if (!foundInIndex) {
            Serial.printf("Index Maintenance: Adding entry for new file: %s\n", actualUid.c_str());
            ScheduleFile newSf;
            newSf.scheduleUID = actualUid;
            newSf.persistentLockLevel = 0; // Default to unlocked
            _scheduleIndex.push_back(newSf);
            indexChanged = true;
        }
    }

    // 4. Update dynamic 'lockedBy' based on LockManager (doesn't change persistent 'locked' field)
    // This info is primarily for the getScheduleList response, not saved in allSchedules.json
    for (ScheduleFile& sf : _scheduleIndex) {
         FileLock lockInfo;
         String resourceId = "schedule_" + sf.scheduleUID; // Consistent resource ID
         if (lockManager.isLocked(resourceId, &lockInfo)) {
             sf.lockedBy = lockInfo.username; // Set who holds the editing lock
         } else {
             sf.lockedBy = ""; // Clear if no editing lock
         }
    }


    // 5. Save index if changes were made
    if (indexChanged) {
        Serial.println("Index changed, saving...");
        if (!saveScheduleIndex()) {
            Serial.println("Error saving schedule index after maintenance!");
        }
    } else {
         // Serial.println("No changes to schedule index."); // Debug
    }
}

/**
 * @brief Gets the list of schedules currently in the index.
 *
 * Updates the dynamic `lockedBy` status for each schedule based on the LockManager
 * before returning the cached index.
 *
 * @param list A reference to a std::vector<ScheduleFile> that will be populated
 *             with the current schedule index data.
 * @return True (currently always returns true, assuming the index is valid if begin() succeeded).
 */
bool ScheduleManager::getScheduleList(std::vector<ScheduleFile>& list) {
    // Update lock status before returning list
    for (ScheduleFile& sf : _scheduleIndex) {
         FileLock lockInfo;
         String resourceId = "schedule_" + sf.scheduleUID;
         if (lockManager.isLocked(resourceId, &lockInfo)) {
             sf.lockedBy = lockInfo.username;
         } else {
             sf.lockedBy = "";
         }
    }
    list = _scheduleIndex; // Return the cached index
    return true; // Assuming cache is valid if begin() succeeded
}


// --- Schedule Loading/Saving/Deleting ---

/**
 * @brief Loads a specific schedule from its JSON file.
 *
 * Constructs the file path based on the schedule directory and UID.
 * Opens the file, parses the JSON content, and populates the provided `Schedule` object.
 * Clears existing event vectors in the `schedule` object before parsing new ones.
 * Sorts the event vectors after loading.
 *
 * @param uid The unique identifier of the schedule to load.
 * @param schedule A reference to a `Schedule` object to be populated with the loaded data.
 * @return True if the schedule was loaded and parsed successfully and the basic
 *         schedule data is valid, false otherwise (file not found, parse error, invalid data).
 */
bool ScheduleManager::loadSchedule(const String& uid, Schedule& schedule) {
    String filePath = _scheduleDir + uid + ".json";
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        Serial.printf("Failed to open schedule file for reading: %s\n", filePath.c_str());
        return false;
    }

    // V7: Use JsonDocument
    JsonDocument doc;
    // StaticJsonDocument<8192> doc; // Consider if size is predictable

    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.printf("Failed to parse schedule file %s: %s\n", filePath.c_str(), error.c_str());
        return false;
    }

    JsonObject obj = doc.as<JsonObject>();

    schedule.scheduleName = obj["scheduleName"] | "";
    schedule.lightsOnTime = obj["lightsOnTime"] | 0;
    schedule.lightsOffTime = obj["lightsOffTime"] | 0;
    schedule.scheduleUID = obj["scheduleUID"] | uid; // Use UID from filename if missing

    // Clear existing vectors
    schedule.autopilotWindows.clear();
    schedule.durationEvents.clear();
    schedule.volumeEvents.clear();

    // Parse Autopilot Windows (V7 API)
    JsonArray apArray = obj["autopilotWindows"];
    Serial.printf("loadSchedule: Found autopilotWindows array? %s\n", apArray.isNull() ? "NO" : "YES"); // DEBUG
    if (!apArray.isNull()) {
        Serial.printf("loadSchedule: autopilotWindows array size: %d\n", apArray.size()); // DEBUG
        for (JsonObject apObj : apArray) {
            AutopilotWindow apw;
            apw.startTime = apObj["startTime"] | 0;
            apw.endTime = apObj["endTime"] | 0;
            apw.matricTension = apObj["matricTension"] | 0.0f;
            apw.doseVolume = apObj["doseVolume"] | 0;
            apw.settlingTime = apObj["settlingTime"] | 0;
            // DEBUG Log parsed values
            Serial.printf("loadSchedule: Parsed APW: start=%d, end=%d, tension=%.2f, dose=%d, settle=%d\n",
                          apw.startTime, apw.endTime, apw.matricTension, apw.doseVolume, apw.settlingTime);
            bool valid = apw.isValid(); // DEBUG check validity
            Serial.printf("loadSchedule: APW isValid? %s\n", valid ? "YES" : "NO");
            if (valid) schedule.autopilotWindows.push_back(apw);
        }
    }

    // Parse Duration Events (V7 API)
    JsonArray durArray = obj["durationEvents"];
    Serial.printf("loadSchedule: Found durationEvents array? %s\n", durArray.isNull() ? "NO" : "YES"); // DEBUG
     if (!durArray.isNull()) {
        Serial.printf("loadSchedule: durationEvents array size: %d\n", durArray.size()); // DEBUG
        for (JsonObject durObj : durArray) {
            DurationEvent de;
            de.startTime = durObj["startTime"] | 0;
            de.duration = durObj["duration"] | 0;
            de.endTime = durObj["endTime"] | 0; // Load calculated end time
            // DEBUG Log parsed values
            Serial.printf("loadSchedule: Parsed DUR: start=%d, duration=%d, end=%d\n",
                          de.startTime, de.duration, de.endTime);
            bool valid = de.isValid(); // DEBUG check validity
            Serial.printf("loadSchedule: DUR isValid? %s\n", valid ? "YES" : "NO");
            if (valid) schedule.durationEvents.push_back(de);
        }
    }

     // Parse Volume Events (V7 API)
    JsonArray volArray = obj["volumeEvents"];
    Serial.printf("loadSchedule: Found volumeEvents array? %s\n", volArray.isNull() ? "NO" : "YES"); // DEBUG
     if (!volArray.isNull()) {
        Serial.printf("loadSchedule: volumeEvents array size: %d\n", volArray.size()); // DEBUG
        for (JsonObject volObj : volArray) {
            VolumeEvent ve;
            ve.startTime = volObj["startTime"] | 0;
            ve.doseVolume = volObj["doseVolume"] | 0;
             // DEBUG Log parsed values
            Serial.printf("loadSchedule: Parsed VOL: start=%d, dose=%d\n",
                          ve.startTime, ve.doseVolume);
            bool valid = ve.isValid(); // DEBUG check validity
            Serial.printf("loadSchedule: VOL isValid? %s\n", valid ? "YES" : "NO");
            if (valid) schedule.volumeEvents.push_back(ve);
        }
    }

    // Sort events after loading (optional, but good practice)
    std::sort(schedule.autopilotWindows.begin(), schedule.autopilotWindows.end(), compareAutopilotWindows);
    std::sort(schedule.durationEvents.begin(), schedule.durationEvents.end(), compareDurationEvents);
    std::sort(schedule.volumeEvents.begin(), schedule.volumeEvents.end(), compareVolumeEvents);


    return schedule.isValid(); // Check if basic schedule info is valid
}

/**
 * @brief Saves a schedule object to its corresponding JSON file.
 *
 * Validates the schedule object first. Constructs the file path based on the
 * schedule's UID. Opens the file in write mode (truncating existing content).
 * Serializes the `Schedule` object (including its event vectors) into JSON format
 * and writes it to the file.
 * If the schedule being saved is new (not found in the current index), it adds
 * an entry to the in-memory index and saves the index file immediately.
 *
 * @param schedule A constant reference to the `Schedule` object to save.
 * @return True if the schedule is valid and was saved successfully, false otherwise.
 */
bool ScheduleManager::saveSchedule(const Schedule& schedule) {
     if (!schedule.isValid()) {
        Serial.println("Attempted to save invalid schedule data.");
        return false;
    }
    String filePath = _scheduleDir + schedule.scheduleUID + ".json";
    File file = LittleFS.open(filePath, "w");
    if (!file) {
        Serial.printf("Failed to open schedule file for writing: %s\n", filePath.c_str());
        return false;
    }

    // V7: Use JsonDocument
    JsonDocument doc;
    // StaticJsonDocument<8192> doc; // Consider if size is predictable

    doc["scheduleName"] = schedule.scheduleName;
    doc["lightsOnTime"] = schedule.lightsOnTime;
    doc["lightsOffTime"] = schedule.lightsOffTime;
    doc["scheduleUID"] = schedule.scheduleUID;

    // *** ADDED DEBUG LOGS ***
    Serial.printf("saveSchedule: Event counts before serialization: AP=%d, DUR=%d, VOL=%d\n",
                  schedule.autopilotWindows.size(),
                  schedule.durationEvents.size(),
                  schedule.volumeEvents.size());
    // ***********************

    JsonArray apArray = doc["autopilotWindows"].to<JsonArray>(); // V7: Use to<T>()
    for (const auto& apw : schedule.autopilotWindows) {
        JsonObject apObj = apArray.add<JsonObject>(); // V7: Use add<T>()
        apObj["startTime"] = apw.startTime;
        apObj["endTime"] = apw.endTime;
        apObj["matricTension"] = apw.matricTension;
        apObj["doseVolume"] = apw.doseVolume;
        apObj["settlingTime"] = apw.settlingTime;
    }

    JsonArray durArray = doc["durationEvents"].to<JsonArray>(); // V7: Use to<T>()
     for (const auto& de : schedule.durationEvents) {
        JsonObject durObj = durArray.add<JsonObject>(); // V7: Use add<T>()
        durObj["startTime"] = de.startTime;
        durObj["duration"] = de.duration;
        durObj["endTime"] = de.endTime; // Save calculated end time
    }

    JsonArray volArray = doc["volumeEvents"].to<JsonArray>(); // V7: Use to<T>()
     for (const auto& ve : schedule.volumeEvents) {
        JsonObject volObj = volArray.add<JsonObject>(); // V7: Use add<T>()
        volObj["startTime"] = ve.startTime;
        volObj["doseVolume"] = ve.doseVolume;
    }

    size_t bytesWritten = serializeJson(doc, file); // Capture bytes written
    if (bytesWritten == 0) {
        Serial.printf("Failed to write schedule data to file: %s\n", filePath.c_str());
        file.close();
        return false;
    }

    file.close();
    Serial.printf("Successfully saved schedule: %s (%d bytes written)\n", filePath.c_str(), bytesWritten); // Log bytes written

    // --- Update in-memory index if this is a new schedule ---
    bool foundInIndex = false;
    for (const auto& sf : _scheduleIndex) {
        if (sf.scheduleUID == schedule.scheduleUID) {
            foundInIndex = true;
            break;
        }
    }
    if (!foundInIndex) {
        Serial.printf("Adding newly saved schedule '%s' to in-memory index.\n", schedule.scheduleUID.c_str());
        ScheduleFile newSf;
        newSf.scheduleUID = schedule.scheduleUID;
        newSf.persistentLockLevel = 0; // Assume unlocked initially
        newSf.lockedBy = "";
        _scheduleIndex.push_back(newSf);
        // Optionally save the index file immediately? Or rely on next maintain/shutdown?
        // Let's save it immediately for consistency.
        saveScheduleIndex();
    }
    // --- End index update ---

    return true;
}

/**
 * @brief Deletes a schedule file and removes its entry from the index.
 *
 * Constructs the file path based on the UID. Deletes the file from LittleFS.
 * Removes the corresponding entry from the in-memory `_scheduleIndex`.
 * Saves the updated index file.
 *
 * @param uid The unique identifier of the schedule to delete.
 * @return True if the file was deleted and the index was updated and saved successfully.
 *         False if the file doesn't exist, file deletion fails, or saving the index fails.
 */
bool ScheduleManager::deleteSchedule(const String& uid) {
    String filePath = _scheduleDir + uid + ".json";
    if (!LittleFS.exists(filePath)) {
        Serial.printf("Schedule file not found for deletion: %s\n", filePath.c_str());
        return false; // Or true if "already deleted" is okay? Let's say false.
    }

    if (!LittleFS.remove(filePath)) {
        Serial.printf("Failed to delete schedule file: %s\n", filePath.c_str());
        return false;
    }

    Serial.printf("Deleted schedule file: %s\n", filePath.c_str());

    // Remove from index and save
    bool removedFromIndex = false;
    auto removeIter = std::remove_if(_scheduleIndex.begin(), _scheduleIndex.end(),
        [&](const ScheduleFile& sf) {
            if (sf.scheduleUID == uid) {
                removedFromIndex = true;
                return true;
            }
            return false;
        });

    if (removedFromIndex) {
        _scheduleIndex.erase(removeIter, _scheduleIndex.end());
        if (!saveScheduleIndex()) {
            Serial.println("Error saving schedule index after deleting file!");
            // File is deleted, but index is inconsistent. Critical?
            return false; // Indicate failure due to index save error
        }
    } else {
         Serial.printf("Warning: Deleted file %s was not found in the index.\n", uid.c_str());
    }

    return true;
}


// --- Schedule Creation ---

/**
 * @brief Creates a new, empty Schedule object with a generated UID.
 *
 * Initializes a `Schedule` object with default values, sets the provided name,
 * and generates a unique identifier using `generateUID()`.
 *
 * @param name The desired name for the new schedule.
 * @param newSchedule A reference to a `Schedule` object that will be populated
 *                    with the details of the newly created (but not yet saved) schedule.
 * @return True if the name is not empty and the resulting `Schedule` object is valid,
 *         false otherwise.
 */
bool ScheduleManager::createSchedule(const String& name, Schedule& newSchedule) {
    if (name.isEmpty()) return false;

    newSchedule = Schedule(); // Reset to default values
    newSchedule.scheduleName = name;
    newSchedule.scheduleUID = generateUID(name);
    // Default lights on/off? Let's leave them 0 for now.
    // newSchedule.lightsOnTime = 6 * 60; // e.g., 6 AM
    // newSchedule.lightsOffTime = 18 * 60; // e.g., 6 PM

    return newSchedule.isValid();
}

/**
 * @brief Generates a unique identifier (UID) for a schedule.
 *
 * Sanitizes the provided name using `sanitizeFilename()` and appends the current
 * timestamp to ensure uniqueness. Limits the sanitized name part to 20 characters.
 *
 * @param name The base name for the schedule.
 * @return A String containing the generated unique identifier.
 */
String ScheduleManager::generateUID(const String& name) {
    String sanitized = sanitizeFilename(name);
    // Add timestamp to ensure uniqueness
    time_t now;
    time(&now);
    // Limit UID length if necessary
    return sanitized.substring(0, 20) + "_" + String(now);
}

/**
 * @brief Sanitizes a string to make it suitable for use in a filename.
 *
 * Replaces spaces with underscores and removes any characters not in the allowed set
 * (alphanumeric, underscore, hyphen). If the resulting string is empty, it defaults to "schedule".
 *
 * @param name The input string to sanitize.
 * @return A sanitized String suitable for use in filenames.
 */
String ScheduleManager::sanitizeFilename(const String& name) {
    String sanitized = name;
    sanitized.replace(" ", "_");
    // Remove characters not suitable for filenames
    String allowedChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
    String result = "";
    for (char c : sanitized) {
        if (allowedChars.indexOf(c) != -1) {
            result += c;
        }
    }
    if (result.isEmpty()) {
        result = "schedule"; // Default if name becomes empty
    }
    return result;
}


// --- Event Validation and Addition ---

// TODO: Implement detailed validation logic here based on requirements
/**
 * @brief Validates and adds a single Autopilot Window event to a schedule.
 *
 * Checks if the event itself is valid, if its times are within bounds (0-1439),
 * and if it overlaps with existing Autopilot windows in the schedule.
 * If valid and non-overlapping, adds the event to the schedule's vector and resorts the vector.
 *
 * @param schedule Reference to the Schedule object to modify.
 * @param event The AutopilotWindow event to validate and add.
 * @return True if the event was successfully added, false otherwise (invalid or overlaps).
 */
bool ScheduleManager::validateAndAddEvent(Schedule& schedule, const AutopilotWindow& event) {
    if (!event.isValid()) return false;
    if (!checkTimeBounds(event.startTime) || !checkTimeBounds(event.endTime)) return false;
    if (checkAutopilotOverlap(schedule, event)) return false;

    schedule.autopilotWindows.push_back(event);
    std::sort(schedule.autopilotWindows.begin(), schedule.autopilotWindows.end(), compareAutopilotWindows);
    return true;
}

/**
 * @brief Validates and adds multiple Duration Events to a schedule.
 *
 * Checks if adding the new events would exceed the combined limit for duration/volume events.
 * Validates each individual event (validity, time bounds, overlap with existing duration events,
 * overlap of start time with existing volume events).
 * If all events are valid and non-overlapping, adds them to the schedule's vector and resorts the vector.
 *
 * @param schedule Reference to the Schedule object to modify.
 * @param events A vector of DurationEvent objects to validate and add.
 * @return True if all events were successfully added, false otherwise.
 */
bool ScheduleManager::validateAndAddEvents(Schedule& schedule, const std::vector<DurationEvent>& events) {
    if (!checkDurationVolumeLimit(schedule, events.size())) return false;

    // Validate each event individually first
    for(const auto& event : events) {
        if (!event.isValid()) return false;
        if (!checkTimeBounds(event.startTime) || !checkTimeBounds(event.endTime)) return false;
        if (checkDurationOverlap(schedule, event)) return false;
        // Corrected: Create temporary VolumeEvent properly
        VolumeEvent tempVolEvent;
        tempVolEvent.startTime = event.startTime;
        tempVolEvent.doseVolume = 0; // Dose volume doesn't matter for overlap check here
        if (checkVolumeOverlap(schedule, tempVolEvent)) return false; // Check start time against volume events
    }

    // If all are valid individually and don't overlap existing, add them
    for(const auto& event : events) {
        schedule.durationEvents.push_back(event);
    }
    std::sort(schedule.durationEvents.begin(), schedule.durationEvents.end(), compareDurationEvents);
    return true;
}

/**
 * @brief Validates and adds multiple Volume Events to a schedule.
 *
 * Checks if adding the new events would exceed the combined limit for duration/volume events.
 * Validates each individual event (validity, time bounds, overlap with existing volume events,
 * overlap of start time with existing duration events).
 * If all events are valid and non-overlapping, adds them to the schedule's vector and resorts the vector.
 *
 * @param schedule Reference to the Schedule object to modify.
 * @param events A vector of VolumeEvent objects to validate and add.
 * @return True if all events were successfully added, false otherwise.
 */
bool ScheduleManager::validateAndAddEvents(Schedule& schedule, const std::vector<VolumeEvent>& events) {
     if (!checkDurationVolumeLimit(schedule, events.size())) return false;

    // Validate each event individually first
    for(const auto& event : events) {
        if (!event.isValid()) return false;
        if (!checkTimeBounds(event.startTime)) return false;
         if (checkVolumeOverlap(schedule, event)) return false;
         // Corrected: Create temporary DurationEvent properly
         DurationEvent tempDurEvent;
         tempDurEvent.startTime = event.startTime;
         tempDurEvent.duration = 0; // Duration doesn't matter for start time check
         tempDurEvent.endTime = event.startTime; // End time is same as start for 0 duration
         if (checkDurationOverlap(schedule, tempDurEvent)) return false; // Check start time against duration events
    }

    // If all are valid individually and don't overlap existing, add them
    for(const auto& event : events) {
        schedule.volumeEvents.push_back(event);
    }
    std::sort(schedule.volumeEvents.begin(), schedule.volumeEvents.end(), compareVolumeEvents);
    return true;
}

/**
 * @brief Gets the persistent lock level for a given schedule UID.
 *
 * Looks up the schedule UID in the in-memory index and returns the value
 * of its `locked` field (0=unlocked, 1=template lock, 2=cycle lock).
 * This represents locks that prevent editing/deletion, not temporary editing locks.
 *
 * @param uid The unique identifier of the schedule.
 * @return The persistent lock level (0, 1, or 2), or -1 if the UID is not found in the index.
 */
int ScheduleManager::getPersistentLockLevel(const String& uid) {
    for (const auto& sf : _scheduleIndex) {
        if (sf.scheduleUID == uid) {
            return sf.persistentLockLevel; // Return the persistent lock level (0, 1, or 2)
        }
    }
    return -1; // Not found
}


// --- Internal Validation Helpers ---

/**
 * @brief Checks if a time (in minutes since midnight) is within the valid range (0-1439).
 * @param timeMinutes Time in minutes since midnight.
 * @return True if the time is valid, false otherwise.
 */
bool ScheduleManager::checkTimeBounds(int timeMinutes) {
    return timeMinutes >= 0 && timeMinutes <= 1439;
}

/**
 * @brief Checks if a new Autopilot Window overlaps with any existing ones in a schedule.
 * @param schedule The schedule containing existing windows.
 * @param newEvent The new Autopilot Window to check.
 * @return True if an overlap is detected, false otherwise.
 */
bool ScheduleManager::checkAutopilotOverlap(const Schedule& schedule, const AutopilotWindow& newEvent) {
    for (const auto& existing : schedule.autopilotWindows) {
        // Rules from requirements:
        if (newEvent.startTime == existing.startTime) return true; // Starts at same time
        if (newEvent.endTime == existing.endTime) return true; // Ends at same time (unlikely needed if start check passes, but included)
        if (newEvent.startTime < existing.startTime && newEvent.endTime > existing.endTime) return true; // Envelops existing
        if (newEvent.startTime > existing.startTime && newEvent.startTime < existing.endTime) return true; // Starts within existing
        if (newEvent.endTime > existing.startTime && newEvent.endTime < existing.endTime) return true; // Ends within existing
    }
    return false;
}

/**
 * @brief Checks if adding a number of new events would exceed the maximum combined limit (100)
 *        for Duration and Volume events in a schedule.
 * @param schedule The schedule containing existing events.
 * @param newEventCount The number of new Duration or Volume events being considered.
 * @return True if the limit is not exceeded, false otherwise.
 */
bool ScheduleManager::checkDurationVolumeLimit(const Schedule& schedule, size_t newEventCount) {
    return (schedule.durationEvents.size() + schedule.volumeEvents.size() + newEventCount) <= 100;
}

/**
 * @brief Checks if a new Duration Event overlaps with any existing Duration Events in a schedule.
 * @param schedule The schedule containing existing events.
 * @param newEvent The new Duration Event to check.
 * @return True if an overlap is detected, false otherwise.
 */
bool ScheduleManager::checkDurationOverlap(const Schedule& schedule, const DurationEvent& newEvent) {
     for (const auto& existing : schedule.durationEvents) {
         // Check if start times match
         if (newEvent.startTime == existing.startTime) return true;
         // Check if new event starts during existing event
         if (newEvent.startTime > existing.startTime && newEvent.startTime < existing.endTime) return true;
         // Check if new event ends during existing event (only if duration > 0)
         if (newEvent.duration > 0 && newEvent.endTime > existing.startTime && newEvent.endTime < existing.endTime) return true;
         // Check if new event envelops existing event (only if duration > 0)
         if (newEvent.duration > 0 && newEvent.startTime < existing.startTime && newEvent.endTime > existing.endTime) return true;
     }
     return false;
}

/**
 * @brief Checks if a new Volume Event overlaps with any existing Volume or Duration Events.
 *        Volume events cannot start at the same time as another Volume event.
 *        Volume events cannot start during a Duration event.
 * @param schedule The schedule containing existing events.
 * @param newEvent The new Volume Event to check.
 * @return True if an overlap is detected, false otherwise.
 */
bool ScheduleManager::checkVolumeOverlap(const Schedule& schedule, const VolumeEvent& newEvent) {
    for (const auto& existingVol : schedule.volumeEvents) {
        // Check if start times match
        if (newEvent.startTime == existingVol.startTime) return true;
    }
     for (const auto& existingDur : schedule.durationEvents) {
         // Check if volume event starts during a duration event
         if (newEvent.startTime > existingDur.startTime && newEvent.startTime < existingDur.endTime) return true;
     }
    return false;
}


// --- Sorting Helpers ---
// Static definitions remain outside the class body
/**
 * @brief Comparison function for sorting DurationEvents by start time. (Static)
 * @param a First DurationEvent.
 * @param b Second DurationEvent.
 * @return True if a starts before b.
 */
bool ScheduleManager::compareDurationEvents(const DurationEvent& a, const DurationEvent& b) {
    return a.startTime < b.startTime;
}

/**
 * @brief Comparison function for sorting VolumeEvents by start time. (Static)
 * @param a First VolumeEvent.
 * @param b Second VolumeEvent.
 * @return True if a starts before b.
 */
bool ScheduleManager::compareVolumeEvents(const VolumeEvent& a, const VolumeEvent& b) {
    return a.startTime < b.startTime;
}

/**
 * @brief Comparison function for sorting AutoPilotWindows by start time. (Static)
 * @param a First AutopilotWindow.
 * @param b Second AutopilotWindow.
 * @return True if a starts before b.
 */
bool ScheduleManager::compareAutopilotWindows(const AutopilotWindow& a, const AutopilotWindow& b) {
    return a.startTime < b.startTime;
}