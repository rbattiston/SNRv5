#ifndef MODBUS_DATA_H
#define MODBUS_DATA_H

#include <Arduino.h> // For String
#include <vector>    // For std::vector

// Defines the C++ structures for parsing Modbus device profile JSON files
// (e.g., /data/modbus_profiles/*.json).

// Corresponds to the "modbus" object within a "points" array element
struct ModbusPointParams {
    String registerType; // e.g., "HoldingRegister", "Coil", "InputRegister", "DiscreteInput"
    int address = -1;
    String dataType;     // e.g., "UINT16", "BOOLEAN", "FLOAT32", etc.
    float scaleFactor = 1.0;
    float offset = 0.0;
    String units;        // Optional
};

// Corresponds to an object within the "points" array in a Modbus profile
struct ModbusProfilePoint {
    String pointIdSuffix; // Appended to deviceId to create the system pointId
    String ioType;        // e.g., "AI", "DI", "AO", "DO"
    String description;
    ModbusPointParams modbus;
    bool readOnly = false;
};

// Corresponds to the root object of a Modbus device profile JSON
struct ModbusDeviceProfile {
    String profileId;
    String model;
    String manufacturer;
    String description;
    std::vector<ModbusProfilePoint> points;
};

#endif // MODBUS_DATA_H