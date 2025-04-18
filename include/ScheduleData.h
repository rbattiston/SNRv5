#ifndef SCHEDULE_DATA_H
#define SCHEDULE_DATA_H

#include <Arduino.h> // For String
#include <vector>    // For std::vector

// Defines the C++ structures for parsing Schedule Template files (/data/daily_schedules/*.json)
// and Schedule Instance files (/data/cycle_schedule_instances/*.json).

// Corresponds to an object in the "autopilotWindows" array
struct AutopilotWindow {
    int startTime = -1; // Minutes from midnight
    int endTime = -1;   // Minutes from midnight
    float matricTension = 0.0; // Example control parameter
    float doseVolume = 0.0;
    int settlingTime = 0; // Minutes
    int doseDuration = 0; // Seconds (Added based on struct in Step 1 doc)

    // Check if the window parameters are valid
    bool isValid() const {
        // Check time bounds (0-1439)
        if (startTime < 0 || startTime > 1439 || endTime < 0 || endTime > 1439) {
            return false;
        }
        // Check start time is before end time 
        if (startTime >= endTime) {
            return false;
        }
        // Check if dosing parameters AND settling time are invalid (<= 0) - User specific logic
        if ((doseVolume <= 0.0 || doseDuration <= 0) && settlingTime <= 0) { 
             return false;
        }
        // Add other checks if needed (e.g., matricTension > 0?)
        return true; 
    }
};

// Corresponds to an object in the "durationEvents" array
struct DurationEvent {
    int startTime = -1; // Minutes from midnight
    int duration = 0;   // Seconds
    int endTime = -1;   // Minutes from midnight (calculated or provided)

    // Check if the event parameters are valid
    bool isValid() const {
        // Check time bounds (0-1439)
        if (startTime < 0 || startTime > 1439) {
            return false;
        }
        // Check duration is positive
        if (duration <= 0) {
            return false;
        }
        // Optional: Add endTime checks if needed
        return true; 
    }
};

// Corresponds to an object in the "volumeEvents" array
struct VolumeEvent {
    int startTime = -1; // Minutes from midnight
    float doseVolume = 0.0;
    // This field is ONLY present in Schedule Instance files, not Templates.
    int calculatedDuration = -1; // Seconds (Calculated based on output config)

    // Check if the event parameters are valid
    bool isValid() const {
        // Check time bounds (0-1439)
        if (startTime < 0 || startTime > 1439) {
            return false;
        }
        // Check dose volume is positive
        if (doseVolume <= 0.0) {
            return false;
        }
        // Optional: Add calculatedDuration checks if needed (e.g., >= -1)
        return true; 
    }
};

// Corresponds to the root object of a Schedule Template or Schedule Instance JSON
struct Schedule {
    String scheduleName;
    int lightsOnTime = -1;  // Minutes from midnight
    int lightsOffTime = -1; // Minutes from midnight
    String scheduleUID;     // UID of the original template (present in both Template and Instance)
    std::vector<AutopilotWindow> autopilotWindows;
    std::vector<DurationEvent> durationEvents;
    std::vector<VolumeEvent> volumeEvents;

    // Check if the schedule itself is valid
    bool isValid() const {
        // Check if scheduleName and scheduleUID are not empty
        if (scheduleName.isEmpty() || scheduleUID.isEmpty()) {
            return false;
        }

        // Add other validation checks if needed
        return true;
    }
};

#endif // SCHEDULE_DATA_H