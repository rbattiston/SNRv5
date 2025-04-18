#ifndef INPUT_POINT_MANAGER_H
#define INPUT_POINT_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <map>
#include "IOConfig.h"
#include "InputConfigData.h"

// InputPointManager handles direct input (DI/AI) initialization, mapping, periodic reading, and persistence.
// All ArduinoJSON usage follows the rules in how_to_upgrade_from_ArduinoJSON6_to_ArduinoJSON7.md.

class InputPointManager {
public:
    InputPointManager();

    // Initialize with parsed IOConfiguration
    bool begin(const IOConfiguration& ioConfig);

    // Get the last read value for an analog input pointId (raw ADC value)
    float getCurrentValue(const String& pointId);

    // Get the last read state for a digital input pointId (true=HIGH, false=LOW)
    bool getCurrentState(const String& pointId);

    // Persistence for input point configs (ArduinoJSON 7 compliant)
    bool saveInputPointConfig(const InputPointConfig& config);
    bool loadInputPointConfig(const String& pointId, InputPointConfig& config);
    void inputReaderTask();

private:
    IOConfiguration ioConfig;
    std::map<String, int> directDIPointIdToPinMap;
    std::map<String, int> directAIPointIdToPinMap;

    // Mutex and task handles (opaque types for now)
    void* valueMutex;
    void* inputReaderTaskHandle;

    std::map<String, bool> lastDIStates;
    std::map<String, int> lastAIRawValues;

    void initializeDirectInputHardware();
    void buildDirectInputMaps();
    bool readDirectDIState(int pin);
    int readDirectAIValueRaw(int pin);
    static void inputReaderTaskWrapper(void* parameter);

    // Persistence helpers (ArduinoJSON 7 compliant)
    String getInputConfigPath(const String& pointId);
    String sanitizeFilename(const String& input);
    bool readFileToJsonDocument(const String& path, JsonDocument& doc);
    bool writeJsonDocumentToFile(const String& path, JsonDocument& doc);
    void ensureDirectoryExists(const String& path);
};

#endif // INPUT_POINT_MANAGER_H