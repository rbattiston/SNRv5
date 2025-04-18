#ifndef IO_CONFIG_H
#define IO_CONFIG_H

#include <Arduino.h> // For String
#include <vector>    // For std::vector

// Defines the C++ structures for parsing the board_config.json file.

// Corresponds to "pins" object within "relayOutputs"
struct RelayControlPins {
    int data = -1;
    int clock = -1;
    int latch = -1;
    int oe = -1; // Output Enable pin
};
// Corresponds to "relayOutputs" object
struct DirectRelayConfig {
    int count = 0;
    String controlMethod; // e.g., "ShiftRegister", "DirectGPIO"
    RelayControlPins pins;
    String pointIdPrefix;
    int pointIdStartIndex = 0;
};

// Corresponds to "digitalInputs" object
struct DirectDigitalInputConfig {
    int count = 0;
    std::vector<int> pins;
    String pointIdPrefix;
    int pointIdStartIndex = 0;
};

// Corresponds to an object within the "analogInputs" array
struct DirectAnalogInputConfig {
    String type; // e.g., "0-5V"
    int count = 0;
    int resolutionBits = 12;
    std::vector<int> pins;
    String pointIdPrefix;
    int pointIdStartIndex = 0;
};

// Corresponds to an object within the "analogOutputs" array
struct DirectAnalogOutputConfig {
    String type; // e.g., "0-5V_DAC"
    int count = 0;
    int resolutionBits = 8;
    std::vector<int> pins;
    String pointIdPrefix;
    int pointIdStartIndex = 0;
};

// Corresponds to the "directIO" object
struct DirectIOConfig {
    DirectRelayConfig relayOutputs;
    DirectDigitalInputConfig digitalInputs;
    std::vector<DirectAnalogInputConfig> analogInputs;
    std::vector<DirectAnalogOutputConfig> analogOutputs;
};

// === Step 2: Aggregated I/O Configuration for Output/Input Managers ===
struct IOConfiguration {
    DirectIOConfig directIO;
    // Future: Add Modbus config, boardName, etc. as needed
};

// Corresponds to an object within the "modbusInterfaces" array
struct ModbusInterfaceConfig {
    String interfaceId;
    int uartPort = -1;
    long baudRate = 9600;
    String config; // e.g., "SERIAL_8N1"
    int txPin = -1;
    int rxPin = -1;
    int rtsPin = -1; // Optional, -1 if not used
};

// Corresponds to an object within the "modbusDevices" array
struct ModbusDeviceConfig {
    String deviceId;
    String profileId;
    String interfaceId;
    int slaveAddress = -1;
    long pollingIntervalMs = 10000;
    bool enabled = false;
    String overrideDescription; // Optional
};

// Corresponds to the root object of board_config.json
struct BoardConfig {
    String boardName;
    DirectIOConfig directIO;
    std::vector<ModbusInterfaceConfig> modbusInterfaces;
    std::vector<ModbusDeviceConfig> modbusDevices;
};

#endif // IO_CONFIG_H