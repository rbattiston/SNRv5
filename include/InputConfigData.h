#ifndef INPUT_CONFIG_DATA_H
#define INPUT_CONFIG_DATA_H

#include <Arduino.h> // For String
#include <vector>    // For std::vector
#include <ArduinoJson.h> // For JsonDocument/Object

// Defines the C++ structures for parsing individual input point configuration files
// (e.g., /data/input_configs/<pointId_sanitized>.json).

// Corresponds to "input_range" and "output_range" objects
struct Range {
    float min_voltage = 0.0; // Or min_pressure, etc. - adapt based on context if needed
    float max_voltage = 0.0; // Or max_pressure, etc.
    float min_pressure = 0.0;
    float max_pressure = 0.0;
};

// Corresponds to the "input_scaling" object
struct InputScalingConfig {
    String reference_type;
    float offset = 0.0;
    float multiplier = 1.0;
    float divisor = 1.0;
    String integration_control;
    Range input_range;
    Range output_range;
    String display_unit;
};

// Corresponds to an object in the "data_points" or "custom_points" array
struct CalibrationPoint {
    float voltage = 0.0;
    float pressure = 0.0;
    String timestamp;
    String notes;
};

// Corresponds to the "calibration" object
struct CalibrationConfig {
    bool enabled = false;
    std::vector<CalibrationPoint> data_points;
    std::vector<CalibrationPoint> custom_points;
};

// Corresponds to the "temperature_compensation" object
struct TempCompensationConfig {
    bool enabled = false;
    String source_pointId;
    float center_point = 25.0;
    float slope = 0.0;
    float offset = 0.0;
    int update_interval_minutes = 15;
};

// Corresponds to the "alarms" object
struct AlarmConfig {
    bool enabled = false;
    float low_limit = 0.0;
    float high_limit = 0.0;
    int delay_time_minutes = 0;
    String priority;
};

// Corresponds to the "inputConfig" object
struct InputConfig {
    String type;
    String subtype;
    String name;
    String manufacturer;
    String model;
    String unit;
    InputScalingConfig input_scaling;
    CalibrationConfig calibration;
    TempCompensationConfig temperature_compensation;
    AlarmConfig alarms;
};

// Corresponds to the root object of an input config JSON
struct InputPointConfig {
    String pointId;
    InputConfig inputConfig;

    // Deserialization helper using ArduinoJSON 7
    bool deserialize(const String& jsonString) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonString);
        if (error) {
            Serial.print(F("deserializeJson() failed for InputPointConfig: "));
            Serial.println(error.c_str());
            return false;
        }
        if (!doc["pointId"].is<const char*>() || !doc["inputConfig"].is<JsonObject>()) {
            Serial.println(F("InputPointConfig JSON missing 'pointId' or 'inputConfig'"));
            return false;
        }
        pointId = doc["pointId"].as<String>();
        JsonObject configObj = doc["inputConfig"].as<JsonObject>();
        inputConfig.type = configObj["type"] | "";
        inputConfig.subtype = configObj["subtype"] | "";
        inputConfig.name = configObj["name"] | "";
        inputConfig.manufacturer = configObj["manufacturer"] | "";
        inputConfig.model = configObj["model"] | "";
        inputConfig.unit = configObj["unit"] | "";
        // ... (rest of deserialization for nested objects/arrays as needed)
        // For brevity, not all fields are deserialized here.
        return true;
    }

    // Serialization helper using ArduinoJSON 7
    String serialize() const {
        JsonDocument doc;
        doc["pointId"] = pointId;
        JsonObject configObj = doc["inputConfig"].to<JsonObject>();
        configObj["type"] = inputConfig.type;
        configObj["subtype"] = inputConfig.subtype;
        configObj["name"] = inputConfig.name;
        configObj["manufacturer"] = inputConfig.manufacturer;
        configObj["model"] = inputConfig.model;
        configObj["unit"] = inputConfig.unit;
        // ... (rest of serialization for nested objects/arrays as needed)
        // For brevity, not all fields are serialized here.
        String output;
        serializeJsonPretty(doc, output);
        return output;
    }
};

#endif // INPUT_CONFIG_DATA_H