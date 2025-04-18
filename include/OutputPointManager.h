#ifndef OUTPUT_POINT_MANAGER_H
#define OUTPUT_POINT_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <map>
#include "IOConfig.h"
#include "OutputDefData.h"
#include "OutputTypeData.h"

// Enum for relay command types
enum class RelayCommandType {
    TURN_ON,
    TURN_OFF,
    TURN_ON_TIMED
};

// Command struct for the queue
struct OutputCommand {
    String pointId;
    RelayCommandType commandType;
    unsigned long durationMs; // For timed ON, 0 otherwise
};

class OutputPointManager {
public:
    OutputPointManager();

    // Initialize with parsed IOConfiguration
    bool begin(const IOConfiguration& ioConfig);

    // Send a command to the output point (by pointId)
    bool sendCommand(const OutputCommand& command);

    // Persistence for output point definitions
    bool saveOutputPointDefinition(const OutputPointDefinition& definition, const JsonObject& configValues);
    bool loadOutputPointDefinition(const String& pointId, OutputPointDefinition& definition, JsonObject& configValuesOut);

private:
    IOConfiguration ioConfig;
    int directRelayCount = 0;
    std::map<String, int> directRelayPointIdToIndexMap;

    // FreeRTOS handles (opaque types for now)
    void* stateMutex;
    void* timerListMutex;
    void* commandQueue;
    void* commandProcessorTaskHandle;
    std::vector<void*> activeTimerTasks;

    // Internal helpers
    void initializeDirectRelayHardware();
    void buildDirectRelayMap();
    void setDirectRelayStatePhysical(int relayIndex, bool on);
    static void commandProcessorTaskWrapper(void* parameter);
    void processCommandQueueTask();
    void startTimerTaskForDirectRelay(int relayIndex, unsigned long durationMs);
    void cancelTimerTaskForDirectRelay(int relayIndex);
    static void timedOffTaskWrapper(void* parameter);
    void Send_74HC595(uint8_t relayData, int dataPin, int clockPin, int latchPin);
    void Send_Bytes(uint8_t dat, int dataPin, int clockPin);

    // Persistence helpers
    String getOutputDefinitionPath(const String& pointId);
    String sanitizeFilename(const String& input);
    bool readFileToJsonDocument(const String& path, JsonDocument& doc);
    bool writeJsonDocumentToFile(const String& path, JsonDocument& doc);
    void ensureDirectoryExists(const String& path);
};

#endif // OUTPUT_POINT_MANAGER_H