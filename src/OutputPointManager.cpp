#include "OutputPointManager.h"
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h> // V7, see how_to_upgrade_from_ArduinoJSON6_to_ArduinoJSON7.md

#define DEBUG_OUTPUT_MANAGER 1

// FreeRTOS includes
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

uint8_t relayState = 0; // Member variable, not static in a function
std::vector<TaskHandle_t> activeTimerTasks; // One per relay

OutputPointManager::OutputPointManager()
    : directRelayCount(0), stateMutex(nullptr), timerListMutex(nullptr),
      commandQueue(nullptr), commandProcessorTaskHandle(nullptr) {}

void OutputPointManager::Send_Bytes(uint8_t dat, int dataPin, int clockPin) {
    for (int i = 8; i >= 1; i--) {
        digitalWrite(dataPin, (dat & 0x80) ? HIGH : LOW);
        dat <<= 1;
        digitalWrite(clockPin, LOW);
        digitalWrite(clockPin, HIGH);
    }
}

void OutputPointManager::Send_74HC595(uint8_t relayData, int dataPin, int clockPin, int latchPin) {
    Send_Bytes(relayData, dataPin, clockPin);
    Send_Bytes(0x00, dataPin, clockPin); // Not used for relays, but matches your working code
    Send_Bytes(0x00, dataPin, clockPin); // Not used for relays
    digitalWrite(latchPin, LOW);
    digitalWrite(latchPin, HIGH);
}

bool OutputPointManager::begin(const IOConfiguration& config) {
    ioConfig = config;
    directRelayCount = ioConfig.directIO.relayOutputs.count;
    buildDirectRelayMap();
    initializeDirectRelayHardware();
    activeTimerTasks.resize(directRelayCount, nullptr);

    // Create FreeRTOS queue for OutputCommand
    commandQueue = xQueueCreate(10, sizeof(OutputCommand));
    if (!commandQueue) {
        #if DEBUG_OUTPUT_MANAGER
        Serial.println("[OutputPointManager] Failed to create command queue.");
        #endif
        return false;
    }

    // Create command processor task
    BaseType_t taskCreated = xTaskCreatePinnedToCore(
        commandProcessorTaskWrapper,
        "OutputCmdProcTask",
        4096,
        this,
        1,
        (TaskHandle_t*)&commandProcessorTaskHandle,
        1
    );
    if (taskCreated != pdPASS) {
        #if DEBUG_OUTPUT_MANAGER
        Serial.println("[OutputPointManager] Failed to create command processor task.");
        #endif
        return false;
    }

    #if DEBUG_OUTPUT_MANAGER
    Serial.println("[OutputPointManager] OutputPointManager initialized and command processor task started.");
    #endif
    return true;
}

void OutputPointManager::buildDirectRelayMap() {
    #if DEBUG_OUTPUT_MANAGER
    Serial.printf("[OutputPointManager] buildDirectRelayMap: Starting relay mapping process\n");
    Serial.printf("[OutputPointManager] Total relay count: %d\n", directRelayCount);
    #endif
    
    directRelayPointIdToIndexMap.clear();
    
    #if DEBUG_OUTPUT_MANAGER
    Serial.printf("[OutputPointManager] Cleared existing relay map\n");
    #endif
    
    String prefix = ioConfig.directIO.relayOutputs.pointIdPrefix;
    int startIdx = ioConfig.directIO.relayOutputs.pointIdStartIndex;
    
    #if DEBUG_OUTPUT_MANAGER
    Serial.printf("[OutputPointManager] Relay ID prefix: '%s', starting index: %d\n", 
                  prefix.c_str(), startIdx);
    #endif
    
    for (int i = 0; i < directRelayCount; ++i) {
        String pointId = prefix + String(startIdx + i);
        directRelayPointIdToIndexMap[pointId] = i;
        
        #if DEBUG_OUTPUT_MANAGER
        Serial.printf("[OutputPointManager] Mapped relay %d: '%s' -> index %d\n", 
                     i, pointId.c_str(), i);
        #endif
    }
    
    #if DEBUG_OUTPUT_MANAGER
    Serial.printf("[OutputPointManager] buildDirectRelayMap: Completed with %d relay mappings\n", 
                 directRelayPointIdToIndexMap.size());
    #endif
}

void OutputPointManager::initializeDirectRelayHardware() {
    String method = ioConfig.directIO.relayOutputs.controlMethod;
    if (method.equalsIgnoreCase("DirectGPIO")) {
        // For each relay, set the control pin as OUTPUT and set to OFF
        for (int i = 0; i < directRelayCount; ++i) {
            int pin = 0; /* get pin for relayIndex i from your mapping */
            if (pin >= 0) {
                pinMode(pin, OUTPUT);
                digitalWrite(pin, LOW); // OFF
            }
        }
    } else if (method.equalsIgnoreCase("ShiftRegister")) {
        int dataPin = ioConfig.directIO.relayOutputs.pins.data;
        int clockPin = ioConfig.directIO.relayOutputs.pins.clock;
        int latchPin = ioConfig.directIO.relayOutputs.pins.latch;
        int oePin = ioConfig.directIO.relayOutputs.pins.oe;
        pinMode(dataPin, OUTPUT);
        pinMode(clockPin, OUTPUT);
        pinMode(latchPin, OUTPUT);
        pinMode(oePin, OUTPUT);
        digitalWrite(oePin, HIGH);    // Disable outputs during initialization
        digitalWrite(clockPin, LOW);
        digitalWrite(latchPin, HIGH);
        relayState = 0;
        Send_74HC595(relayState, dataPin, clockPin, latchPin);
        digitalWrite(oePin, LOW);     // Enable outputs after initialization
    }
}

void OutputPointManager::setDirectRelayStatePhysical(int relayIndex, bool on) {
    String method = ioConfig.directIO.relayOutputs.controlMethod;
    if (method.equalsIgnoreCase("DirectGPIO")) {
        int pin = 0; // LOOK HERE - get the pin for relayIndex from your mapping, NEED TO IMPLEMENT MAPPING
        if (pin >= 0) {
            digitalWrite(pin, on ? HIGH : LOW);
        }
    } else if (method.equalsIgnoreCase("ShiftRegister")) {
        if (on) {
            relayState |= (1 << relayIndex);
        } else {
            relayState &= ~(1 << relayIndex);
        }
        int dataPin = ioConfig.directIO.relayOutputs.pins.data;
        int clockPin = ioConfig.directIO.relayOutputs.pins.clock;
        int latchPin = ioConfig.directIO.relayOutputs.pins.latch;
        int oePin = ioConfig.directIO.relayOutputs.pins.oe;
        #if DEBUG_OUTPUT_MANAGER
        Serial.printf("[OutputPointManager] setDirectRelayStatePhysical: relayIndex=%d, on=%d\n", relayIndex, on);
        Serial.printf("[OutputPointManager] Pins: data=%d, clock=%d, latch=%d, oe=%d\n", dataPin, clockPin, latchPin, oePin);
        Serial.printf("[OutputPointManager] relayState before Send_74HC595: 0x%02X\n", relayState);
        #endif
        Send_74HC595(relayState, dataPin, clockPin, latchPin);
        #if DEBUG_OUTPUT_MANAGER
        Serial.printf("[OutputPointManager] relayState after Send_74HC595: 0x%02X\n", relayState);
        #endif
    }
}

bool OutputPointManager::sendCommand(const OutputCommand& command) {
    
    #if DEBUG_OUTPUT_MANAGER
    Serial.printf("Inside  OutputPointManager::sendCommand\n");
    #endif

    if (!commandQueue) return false;

    #if DEBUG_OUTPUT_MANAGER
    Serial.printf("commandQueue returns true\n");
    #endif

    BaseType_t result = xQueueSendToBack((QueueHandle_t)commandQueue, &command, 0);
    #if DEBUG_OUTPUT_MANAGER
    Serial.printf("[OutputPointManager] sendCommand: pointId=%s, type=%d, durationMs=%lu, result=%d\n",
                  command.pointId.c_str(), static_cast<int>(command.commandType), command.durationMs, result == pdPASS);
    #endif
    return (result == pdPASS);
}

// FreeRTOS task wrapper
void OutputPointManager::commandProcessorTaskWrapper(void* parameter) {
    OutputPointManager* self = static_cast<OutputPointManager*>(parameter);
    if (self) self->processCommandQueueTask();
}

// Command processor task
void OutputPointManager::processCommandQueueTask() {
    OutputCommand cmd;
    while (true) {
        if (xQueueReceive((QueueHandle_t)commandQueue, &cmd, portMAX_DELAY) == pdPASS) {
            #if DEBUG_OUTPUT_MANAGER
            Serial.printf("[OutputPointManager] Processing command: pointId=%s, type=%d, durationMs=%lu\n",
                          cmd.pointId.c_str(), static_cast<int>(cmd.commandType), cmd.durationMs);
            #endif
            auto it = directRelayPointIdToIndexMap.find(cmd.pointId);
            if (it != directRelayPointIdToIndexMap.end()) {
                int relayIndex = it->second;
                switch (cmd.commandType) {
                    case RelayCommandType::TURN_ON:
                        setDirectRelayStatePhysical(relayIndex, true);
                        break;
                    case RelayCommandType::TURN_OFF:
                        setDirectRelayStatePhysical(relayIndex, false);
                        break;
                    case RelayCommandType::TURN_ON_TIMED:
                        setDirectRelayStatePhysical(relayIndex, true);
                        // Cancel any existing timer for this relay
                        if (relayIndex < activeTimerTasks.size() && activeTimerTasks[relayIndex] != nullptr) {
                            vTaskDelete(activeTimerTasks[relayIndex]);
                            activeTimerTasks[relayIndex] = nullptr;
                        }
                        // Start a new timer task for this relay
                        struct TimerParams {
                            OutputPointManager* manager;
                            int relayIndex;
                            unsigned long durationMs;
                        };
                        TimerParams* params = new TimerParams{this, relayIndex, cmd.durationMs};
                        xTaskCreate(
                            [](void* p) {
                                TimerParams* tp = static_cast<TimerParams*>(p);
                                vTaskDelay(pdMS_TO_TICKS(tp->durationMs));
                                // Send TURN_OFF command for this relay
                                OutputCommand offCmd;
                                offCmd.pointId = "DirectRelay_" + String(tp->relayIndex); // Adjust as needed for your pointId mapping
                                offCmd.commandType = RelayCommandType::TURN_OFF;
                                offCmd.durationMs = 0;
                                tp->manager->sendCommand(offCmd);
                                // Clean up
                                tp->manager->activeTimerTasks[tp->relayIndex] = nullptr;
                                delete tp;
                                vTaskDelete(nullptr);
                            },
                            "RelayTimerTask",
                            2048,
                            params,
                            1,
                            &activeTimerTasks[relayIndex]
                        );
                    break;
                }
            } else {
                #if DEBUG_OUTPUT_MANAGER
                Serial.printf("[OutputPointManager] Unknown pointId: %s\n", cmd.pointId.c_str());
                #endif
            }
        }
    }
}

// Persistence helpers using ArduinoJSON 7

// Save OutputPointDefinition to file (ArduinoJSON 7 compliant)
bool OutputPointManager::saveOutputPointDefinition(const OutputPointDefinition& definition, const JsonObject& configValues) {
    String path = getOutputDefinitionPath(definition.pointId);
    ensureDirectoryExists("/data/output_definitions/");
    File file = LittleFS.open(path, "w");
    if (!file) {
        Serial.println("Failed to open output definition file for writing.");
        return false;
    }
    String jsonString = definition.serialize(configValues);
    size_t written = file.print(jsonString);
    file.close();
    return (written > 0);
}

// Load OutputPointDefinition from file (ArduinoJSON 7 compliant)
bool OutputPointManager::loadOutputPointDefinition(const String& pointId, OutputPointDefinition& definition, JsonObject& configValuesOut) {
    String path = getOutputDefinitionPath(pointId);
    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.println("Failed to open output definition file for reading.");
        return false;
    }
    String jsonString;
    while (file.available()) {
        jsonString += (char)file.read();
    }
    file.close();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return false;
    }
    if (!definition.deserialize(jsonString)) {
        return false;
    }
    if (doc["configValues"].is<JsonObject>()) {
        configValuesOut = doc["configValues"].as<JsonObject>();
    } else {
        configValuesOut = doc["configValues"].to<JsonObject>();
    }
    return true;
}

// Helper functions

String OutputPointManager::getOutputDefinitionPath(const String& pointId) {
    return "/data/output_definitions/" + sanitizeFilename(pointId) + ".json";
}

String OutputPointManager::sanitizeFilename(const String& input) {
    String out = input;
    out.replace("/", "_");
    out.replace("\\", "_");
    return out;
}

bool OutputPointManager::readFileToJsonDocument(const String& path, JsonDocument& doc) {
    File file = LittleFS.open(path, "r");
    if (!file) return false;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    return !error;
}

bool OutputPointManager::writeJsonDocumentToFile(const String& path, JsonDocument& doc) {
    File file = LittleFS.open(path, "w");
    if (!file) return false;
    bool ok = (serializeJson(doc, file) > 0);
    file.close();
    return ok;
}

void OutputPointManager::ensureDirectoryExists(const String& path) {
    if (!LittleFS.exists(path)) {
        LittleFS.mkdir(path);
    }
}