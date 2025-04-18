#ifndef CYCLE_DATA_H
#define CYCLE_DATA_H

#include <Arduino.h> // For String
#include <vector>    // For std::vector

// Defines the C++ structures for parsing Active Cycle configuration files (/data/active_cycles/*.json)
// and Cycle Template files (/data/cycle_templates/*.json).

// --- Cycle Template Structures ---

// Corresponds to an object in the "sequence" array of a Cycle Template
struct CycleTemplateStep {
    int step = 0;
    String libraryScheduleId; // UID of the Schedule Template
    int durationDays = 0;
};

// Corresponds to the root object of a Cycle Template JSON
struct CycleTemplate {
    String templateId;
    String templateName;
    std::vector<CycleTemplateStep> sequence;
};

// --- Active Cycle Structures ---

// Corresponds to an object in the "cycleSequence" array of an Active Cycle
struct ActiveCycleStep {
    int step = 0;
    String scheduleInstanceId; // ID of the specific Schedule Instance file used
    String libraryScheduleId;  // UID of the original Schedule Template
    int durationDays = 0;
};

// Corresponds to an object in the "associatedOutputs" array of an Active Cycle
struct AssociatedOutput {
    String pointId; // Links to an OutputPointDefinition
    String role;    // Role within this specific cycle (e.g., "primary_actuator")
};

// Corresponds to an object in the "associatedInputs" array of an Active Cycle
struct AssociatedInput {
    String pointId; // Links to an InputPointConfig
    String role;    // Role within this specific cycle (e.g., "AUTOPILOT_CONTROL", "STATE_VERIFICATION")
    // Input strategy (primary/secondary/average) is handled by CycleManager logic based on role
};

// Enum for Cycle State
enum CycleState {
    DRAFT,
    SAVED_DORMANT,
    SAVED_ACTIVE,
    RUNNING, // May not be explicitly stored, but useful for runtime state
    PAUSED,  // May not be explicitly stored
    COMPLETED,
    ERROR
    // Add other states as needed
};

// Helper function to convert CycleState enum to String
String cycleStateToString(CycleState state) {
    switch (state) {
        case DRAFT: return "DRAFT";
        case SAVED_DORMANT: return "SAVED_DORMANT";
        case SAVED_ACTIVE: return "SAVED_ACTIVE";
        case RUNNING: return "RUNNING"; // Or maybe SAVED_ACTIVE is sufficient for storage
        case PAUSED: return "PAUSED";
        case COMPLETED: return "COMPLETED";
        case ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

// Helper function to convert String to CycleState enum
CycleState stringToCycleState(const String& stateStr) {
    if (stateStr.equalsIgnoreCase("DRAFT")) return DRAFT;
    if (stateStr.equalsIgnoreCase("SAVED_DORMANT")) return SAVED_DORMANT;
    if (stateStr.equalsIgnoreCase("SAVED_ACTIVE")) return SAVED_ACTIVE;
    // RUNNING and PAUSED might not be stored states, handle as needed
    if (stateStr.equalsIgnoreCase("COMPLETED")) return COMPLETED;
    if (stateStr.equalsIgnoreCase("ERROR")) return ERROR;
    return DRAFT; // Default or consider an UNKNOWN state
}


// Corresponds to the root object of an Active Cycle JSON
struct ActiveCycleConfiguration {
    String cycleId;
    String cycleName;
    CycleState cycleState = DRAFT; // Use enum for internal state
    String cycleStartDate; // ISO 8601 format string (e.g., "2025-04-08T00:00:00Z")
    int currentStep = 0;
    String stepStartDate;  // ISO 8601 format string
    std::vector<ActiveCycleStep> cycleSequence;
    std::vector<AssociatedOutput> associatedOutputs;
    std::vector<AssociatedInput> associatedInputs;

    // Note: Deserialization/Serialization helpers would be needed here or in CycleManager.
    // Need to handle conversion between CycleState enum and its string representation ("SAVED_ACTIVE", etc.)
};

#endif // CYCLE_DATA_H