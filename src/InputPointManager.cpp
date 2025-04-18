#include "InputPointManager.h"
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h> // V7, see how_to_upgrade_from_ArduinoJSON6_to_ArduinoJSON7.md

#define DEBUG_INPUT_MANAGER 1

InputPointManager::InputPointManager()
    : valueMutex(nullptr), inputReaderTaskHandle(nullptr) {}

bool InputPointManager::begin(const IOConfiguration& config) {
    ioConfig = config;
    buildDirectInputMaps();
    initializeDirectInputHardware();
    // TODO: Initialize FreeRTOS mutex and create inputReaderTask
    // valueMutex = xSemaphoreCreateMutex(); // Example for FreeRTOS
    // xTaskCreatePinnedToCore(inputReaderTaskWrapper, "InputReaderTask", 4096, this, 1, &inputReaderTaskHandle, 1);
    return true;
}

void InputPointManager::buildDirectInputMaps() {
    directDIPointIdToPinMap.clear();
    directAIPointIdToPinMap.clear();
    // Digital Inputs
    String prefix = ioConfig.directIO.digitalInputs.pointIdPrefix;
    int startIdx = ioConfig.directIO.digitalInputs.pointIdStartIndex;
    for (int i = 0; i < ioConfig.directIO.digitalInputs.count; ++i) {
        String pointId = prefix + String(startIdx + i);
        int pin = (i < ioConfig.directIO.digitalInputs.pins.size()) ? ioConfig.directIO.digitalInputs.pins[i] : -1;
        directDIPointIdToPinMap[pointId] = pin;
    }
    // Analog Inputs
    for (const auto& aiConfig : ioConfig.directIO.analogInputs) {
        String prefix = aiConfig.pointIdPrefix;
        int startIdx = aiConfig.pointIdStartIndex;
        for (int i = 0; i < aiConfig.count; ++i) {
            String pointId = prefix + String(startIdx + i);
            int pin = (i < aiConfig.pins.size()) ? aiConfig.pins[i] : -1;
            directAIPointIdToPinMap[pointId] = pin;
        }
    }
}

void InputPointManager::initializeDirectInputHardware() {
    // Set up DI pins as INPUT, configure ADC for AI pins
    for (const auto& pair : directDIPointIdToPinMap) {
        int pin = pair.second;
        if (pin >= 0) {
            pinMode(pin, INPUT);
            #if DEBUG_INPUT_MANAGER
            Serial.printf("[InputPointManager] Set pinMode INPUT for DI pin %d (pointId: %s)\n", pin, pair.first.c_str());
            #endif
        }
    }
    for (const auto& pair : directAIPointIdToPinMap) {
        int pin = pair.second;
        if (pin >= 0) {
            // On ESP32, analogRead does not require explicit pinMode, but you may configure attenuation, etc.
            #if DEBUG_INPUT_MANAGER
            Serial.printf("[InputPointManager] Registered AI pin %d (pointId: %s)\n", pin, pair.first.c_str());
            #endif
        }
    }
}

float InputPointManager::getCurrentValue(const String& pointId) {
    auto it = lastAIRawValues.find(pointId);
    if (it != lastAIRawValues.end()) {
        return static_cast<float>(it->second);
    }
    return -1.0f; // Error value
}

bool InputPointManager::getCurrentState(const String& pointId) {
    auto it = lastDIStates.find(pointId);
    if (it != lastDIStates.end()) {
        return it->second;
    }
    return false; // Default state
}

// Periodic input reading task (for direct DI/AI)
void InputPointManager::inputReaderTask() {
    // This would run as a FreeRTOS task in production
    // Example loop:
    while (true) {
        // Read all digital inputs
        for (const auto& pair : directDIPointIdToPinMap) {
            const String& pointId = pair.first;
            int pin = pair.second;
            bool state = false;
            if (pin >= 0) {
                state = digitalRead(pin);
                lastDIStates[pointId] = state;
                #if DEBUG_INPUT_MANAGER
                Serial.printf("[InputPointManager] DI %s (pin %d) = %d\n", pointId.c_str(), pin, state ? 1 : 0);
                #endif
            }
        }
        // Read all analog inputs
        for (const auto& pair : directAIPointIdToPinMap) {
            const String& pointId = pair.first;
            int pin = pair.second;
            int value = 0;
            if (pin >= 0) {
                value = analogRead(pin);
                lastAIRawValues[pointId] = value;
                #if DEBUG_INPUT_MANAGER
                Serial.printf("[InputPointManager] AI %s (pin %d) = %d\n", pointId.c_str(), pin, value);
                #endif
            }
        }
        // vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(1000)); // For FreeRTOS
        delay(1000); // For single-threaded test/debug
    }
}

// Static wrapper for FreeRTOS task
void InputPointManager::inputReaderTaskWrapper(void* parameter) {
    InputPointManager* self = static_cast<InputPointManager*>(parameter);
    if (self) self->inputReaderTask();
}

// Physical reading stubs (now implemented)
bool InputPointManager::readDirectDIState(int pin) {
    if (pin >= 0) {
        return digitalRead(pin);
    }
    return false;
}

int InputPointManager::readDirectAIValueRaw(int pin) {
    if (pin >= 0) {
        return analogRead(pin);
    }
    return 0;
}

// Persistence helpers using ArduinoJSON 7 (see how_to_upgrade_from_ArduinoJSON6_to_ArduinoJSON7.md)

bool InputPointManager::saveInputPointConfig(const InputPointConfig& config) {
    String path = getInputConfigPath(config.pointId);
    ensureDirectoryExists("/data/input_configs/");
    File file = LittleFS.open(path, "w");
    if (!file) {
        #if DEBUG_INPUT_MANAGER
        Serial.println("[InputPointManager] Failed to open input config file for writing.");
        #endif
        return false;
    }
    String jsonString = config.serialize();
    size_t written = file.print(jsonString);
    file.close();
    return (written > 0);
}

bool InputPointManager::loadInputPointConfig(const String& pointId, InputPointConfig& config) {
    String path = getInputConfigPath(pointId);
    File file = LittleFS.open(path, "r");
    if (!file) {
        #if DEBUG_INPUT_MANAGER
        Serial.println("[InputPointManager] Failed to open input config file for reading.");
        #endif
        return false;
    }
    String jsonString;
    while (file.available()) {
        jsonString += (char)file.read();
    }
    file.close();
    return config.deserialize(jsonString);
}

// Helper functions

String InputPointManager::getInputConfigPath(const String& pointId) {
    return "/data/input_configs/" + sanitizeFilename(pointId) + ".json";
}

String InputPointManager::sanitizeFilename(const String& input) {
    String out = input;
    out.replace("/", "_");
    out.replace("\\", "_");
    return out;
}

bool InputPointManager::readFileToJsonDocument(const String& path, JsonDocument& doc) {
    File file = LittleFS.open(path, "r");
    if (!file) return false;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    return !error;
}

bool InputPointManager::writeJsonDocumentToFile(const String& path, JsonDocument& doc) {
    File file = LittleFS.open(path, "w");
    if (!file) return false;
    bool ok = (serializeJson(doc, file) > 0);
    file.close();
    return ok;
}

void InputPointManager::ensureDirectoryExists(const String& path) {
    if (!LittleFS.exists(path)) {
        LittleFS.mkdir(path);
    }
}