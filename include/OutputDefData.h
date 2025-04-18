#ifndef OUTPUT_DEF_DATA_H
#define OUTPUT_DEF_DATA_H

#include <Arduino.h>     // For String
#include <ArduinoJson.h> // For JsonObject

// Defines the C++ structure for parsing individual output point definition files
// (e.g., /data/output_definitions/<pointId_sanitized>.json).

// Corresponds to the root object of an output definition JSON
struct OutputPointDefinition {
    String pointId;
    String assignedType;

    // Deserialize from JSON string into this struct, using a local JsonDocument
    bool deserialize(const String& jsonString) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonString);
        if (error) {
            Serial.print(F("deserializeJson() failed for OutputPointDefinition: "));
            Serial.println(error.c_str());
            return false;
        }
        if (!doc["pointId"].is<const char*>() || !doc["assignedType"].is<const char*>()) {
            Serial.println(F("OutputPointDefinition JSON missing 'pointId' or 'assignedType'"));
            return false;
        }
        pointId = doc["pointId"].as<String>();
        assignedType = doc["assignedType"].as<String>();
        // configValues is not stored as a member; use doc["configValues"].as<JsonObject>() as needed
        return true;
    }

    // Serialize this struct to a JSON string, using a local JsonDocument and a configValues object
    String serialize(const JsonObject& configValues) const {
        JsonDocument doc;
        doc["pointId"] = pointId;
        doc["assignedType"] = assignedType;
        JsonObject valuesObj = doc["configValues"].to<JsonObject>();
        for (JsonPair kv : configValues) {
            valuesObj[kv.key()] = kv.value();
        }
        String output;
        serializeJsonPretty(doc, output);
        return output;
    }
};

#endif // OUTPUT_DEF_DATA_H