#ifndef OUTPUT_TYPE_DATA_H
#define OUTPUT_TYPE_DATA_H

#include <Arduino.h> // For String
#include <vector>    // For std::vector
#include <ArduinoJson.h> // For JsonVariant (or specific types if preferred)

// Defines the C++ structures for parsing the relay_types.json file
// (which defines logical output function types).

// Corresponds to an object within the "configParams" array
struct OutputTypeConfigParam {
    String id;
    String label;
    String type; // e.g., "text", "number", "boolean", "select"
    bool required = false;
    bool readonly = false; // Added based on example
    // Optional constraints (use JsonVariant for flexibility or specific members)
    JsonVariant defaultValue; // Can hold string, number, bool
    JsonVariant min;          // For number type
    JsonVariant max;          // For number type
    JsonVariant step;         // For number type
    // Consider adding options for "select" type if needed:
    // std::vector<String> options;
};

// Corresponds to an object in the root array of relay_types.json
struct OutputTypeDefinition {
    String typeId;
    String displayName;
    String description;
    bool supportsVolume = false;
    bool supportsAutopilotInput = false;
    bool supportsVerificationInput = false;
    bool resumeStateOnReboot = false;
    std::vector<OutputTypeConfigParam> configParams;
};

// Typedef for the root structure (vector of definitions)
typedef std::vector<OutputTypeDefinition> OutputTypesConfig;

#endif // OUTPUT_TYPE_DATA_H