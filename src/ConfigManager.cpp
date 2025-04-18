#include "ConfigManager.h"
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h> // V7

/**
 * @brief Constructs a ConfigManager object.
 *
 * Initializes the manager with the path to the configuration file.
 *
 * @param configFilePath The path within the LittleFS filesystem where the
 *                       configuration file is stored (e.g., "/config.json").
 */
ConfigManager::ConfigManager(const char* configFilePath) : _configFilePath(configFilePath) {
    // Constructor can be expanded if needed
}

/**
 * @brief Loads the application configuration from the specified file.
 *
 * Attempts to open and read the configuration file from LittleFS.
 * If the file doesn't exist, it calls `createDefaultConfigFile()` to create one
 * with default settings.
 * If the file exists but cannot be opened or parsed (corrupted JSON), it logs
 * an error and attempts to create a default configuration file, overwriting the
 * potentially corrupted one.
 * Populates the internal `config` struct with values read from the file, using
 * default values if specific keys are missing in the JSON.
 *
 * @return True if the configuration was loaded successfully (either from an
 *         existing file or by creating a default one), false if file operations
 *         or parsing failed critically.
 */
bool ConfigManager::loadConfig() {
    Serial.printf("Attempting to load configuration from: %s\n", _configFilePath.c_str());
    if (!LittleFS.exists(_configFilePath)) {
        Serial.println("Config file not found. Creating default config.");
        return createDefaultConfigFile();
    }

    File configFile = LittleFS.open(_configFilePath, "r");
    if (!configFile) {
        Serial.println("Failed to open config file for reading.");
        return false;
    }

    // V7: Use JsonDocument (or StaticJsonDocument if size is fixed)
    // StaticJsonDocument<1024> doc; // Example if using static
    JsonDocument doc;

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close(); // Close the file ASAP

    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        Serial.println("Config file might be corrupted. Attempting to create default.");
        // Optionally, backup the corrupted file before overwriting
        return createDefaultConfigFile(); // Overwrite corrupted file with default
    }

    // Extract values, providing defaults if keys are missing
    config.wifi_ssid = doc["wifi_ssid"] | ""; // Use default "" if key missing
    config.wifi_password = doc["wifi_password"] | "";
    config.ap_ssid = doc["ap_ssid"] | "ESP32-WebApp"; // Default AP SSID
    config.ap_password = doc["ap_password"] | "password"; // Default AP Password

    // Load other settings...
    // config.device_name = doc["device_name"] | "MyESP32";

    Serial.println("Configuration loaded successfully:");
    Serial.printf("  WiFi SSID: %s\n", config.wifi_ssid.c_str());
    // Avoid printing password directly in production logs
    // Serial.printf("  WiFi Password: %s\n", config.wifi_password.c_str());
    Serial.printf("  AP SSID: %s\n", config.ap_ssid.c_str());
    // Serial.printf("  AP Password: %s\n", config.ap_password.c_str());

    return true;
}

/**
 * @brief Saves the current application configuration to the specified file.
 *
 * Opens the configuration file in write mode (creating it if it doesn't exist,
 * truncating it if it does). Serializes the current state of the internal `config`
 * struct into JSON format and writes it to the file.
 *
 * @return True if the configuration was saved successfully, false if file
 *         operations or JSON serialization failed.
 */
bool ConfigManager::saveConfig() {
    Serial.printf("Attempting to save configuration to: %s\n", _configFilePath.c_str());
    File configFile = LittleFS.open(_configFilePath, "w"); // Open for writing (creates if not exists, truncates if exists)
    if (!configFile) {
        Serial.println("Failed to open config file for writing.");
        return false;
    }

    // V7: Use JsonDocument (or StaticJsonDocument)
    // StaticJsonDocument<1024> doc;
    JsonDocument doc;

    // Set the values in the document
    doc["wifi_ssid"] = config.wifi_ssid;
    doc["wifi_password"] = config.wifi_password;
    doc["ap_ssid"] = config.ap_ssid;
    doc["ap_password"] = config.ap_password;
    // doc["device_name"] = config.device_name;

    // Serialize JSON to file
    if (serializeJson(doc, configFile) == 0) {
        Serial.println("Failed to write to config file (serializeJson returned 0).");
        configFile.close();
        return false;
    }

    configFile.close(); // Close the file
    Serial.println("Configuration saved successfully.");
    return true;
}

/**
 * @brief Provides access to the loaded application configuration.
 *
 * Returns a reference to the internal `AppConfig` struct, allowing other parts
 * of the application to read the current configuration values.
 *
 * @return A reference to the `AppConfig` struct containing the configuration.
 */
AppConfig& ConfigManager::getConfig() {
    return config;
}

/**
 * @brief Creates a default configuration file.
 *
 * Resets the internal `config` struct to its default values (as defined in
 * the `AppConfig` struct definition or explicitly here) and then calls `saveConfig()`
 * to write these default values to the configuration file, overwriting any
 * existing content.
 *
 * @return True if the default configuration was successfully saved, false otherwise.
 */
bool ConfigManager::createDefaultConfigFile() {
    Serial.println("Creating default configuration file...");
    // The config object already holds the default values defined in the struct
    // Initialize defaults explicitly if not done in struct definition
    config = AppConfig(); // Reset to defaults if needed
    return saveConfig(); // Save the current (default) config state
}

// === Step 2: I/O Foundation Methods ===

bool ConfigManager::loadBoardIOConfig(IOConfiguration& ioConfig) {
    ioConfig = IOConfiguration(); // Reset to defaults

    File file = LittleFS.open("/board_config.json", "r");
    if (!file) {
        Serial.println("Failed to open /data/board_config.json");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return false;
    }

    JsonObject directIO = doc["directIO"];
    if (directIO.isNull()) {
        Serial.println("No directIO section in board_config.json");
        return false;
    }

    // Parse relayOutputs
    JsonObject relays = directIO["relayOutputs"];
    if (!relays.isNull()) {
        ioConfig.directIO.relayOutputs.count = relays["count"] | 0;
        ioConfig.directIO.relayOutputs.controlMethod = relays["controlMethod"] | "DirectGPIO";
        ioConfig.directIO.relayOutputs.pointIdPrefix = relays["pointIdPrefix"] | "DirectRelay_";
        ioConfig.directIO.relayOutputs.pointIdStartIndex = relays["pointIdStartIndex"] | 0;
        JsonObject relayPins = relays["pins"];
        if (!relayPins.isNull()) {
            ioConfig.directIO.relayOutputs.pins.data = relayPins["data"] | -1;
            ioConfig.directIO.relayOutputs.pins.clock = relayPins["clock"] | -1;
            ioConfig.directIO.relayOutputs.pins.latch = relayPins["latch"] | -1;
            ioConfig.directIO.relayOutputs.pins.oe = relayPins["oe"] | -1; 
        }
    }

    // Parse digitalInputs
    JsonObject digitalInputs = directIO["digitalInputs"];
    if (!digitalInputs.isNull()) {
        ioConfig.directIO.digitalInputs.count = digitalInputs["count"] | 0;
        ioConfig.directIO.digitalInputs.pointIdPrefix = digitalInputs["pointIdPrefix"] | "DirectDI_";
        ioConfig.directIO.digitalInputs.pointIdStartIndex = digitalInputs["pointIdStartIndex"] | 0;
        JsonArray pinsArray = digitalInputs["pins"].is<JsonArray>() ? digitalInputs["pins"].as<JsonArray>() : JsonArray();
        ioConfig.directIO.digitalInputs.pins.clear();
        for (JsonVariant v : pinsArray) {
            ioConfig.directIO.digitalInputs.pins.push_back(v.as<int>());
        }
    }

    // Parse analogInputs
    ioConfig.directIO.analogInputs.clear();
    JsonArray analogInputs = directIO["analogInputs"].is<JsonArray>() ? directIO["analogInputs"].as<JsonArray>() : JsonArray();
    for (JsonObject ai : analogInputs) {
        DirectAnalogInputConfig aiConfig;
        aiConfig.type = ai["type"] | "";
        aiConfig.count = ai["count"] | 0;
        aiConfig.resolutionBits = ai["resolutionBits"] | 12;
        aiConfig.pointIdPrefix = ai["pointIdPrefix"] | "";
        aiConfig.pointIdStartIndex = ai["pointIdStartIndex"] | 0;
        JsonArray pinsArray = ai["pins"].is<JsonArray>() ? ai["pins"].as<JsonArray>() : JsonArray();
        aiConfig.pins.clear();
        for (JsonVariant v : pinsArray) {
            aiConfig.pins.push_back(v.as<int>());
        }
        ioConfig.directIO.analogInputs.push_back(aiConfig);
    }

    // Parse analogOutputs
    ioConfig.directIO.analogOutputs.clear();
    JsonArray analogOutputs = directIO["analogOutputs"].is<JsonArray>() ? directIO["analogOutputs"].as<JsonArray>() : JsonArray();
    for (JsonObject ao : analogOutputs) {
        DirectAnalogOutputConfig aoConfig;
        aoConfig.type = ao["type"] | "";
        aoConfig.count = ao["count"] | 0;
        aoConfig.resolutionBits = ao["resolutionBits"] | 8;
        aoConfig.pointIdPrefix = ao["pointIdPrefix"] | "";
        aoConfig.pointIdStartIndex = ao["pointIdStartIndex"] | 0;
        JsonArray pinsArray = ao["pins"].is<JsonArray>() ? ao["pins"].as<JsonArray>() : JsonArray();
        aoConfig.pins.clear();
        for (JsonVariant v : pinsArray) {
            aoConfig.pins.push_back(v.as<int>());
        }
        ioConfig.directIO.analogOutputs.push_back(aoConfig);
    }

    Serial.println("Parsed directIO config from board_config.json successfully.");
    return true;
}

bool ConfigManager::loadRelayTypes(std::vector<OutputTypeDefinition>& outputTypes) {
    outputTypes.clear();

    File file = LittleFS.open("/data/relay_types.json", "r");
    if (!file) {
        Serial.println("Failed to open /data/relay_types.json");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return false;
    }

    JsonArray typesArray = doc.is<JsonArray>() ? doc.as<JsonArray>() : JsonArray();
    for (JsonObject typeObj : typesArray) {
        OutputTypeDefinition typeDef;
        typeDef.typeId = typeObj["typeId"] | "";
        typeDef.displayName = typeObj["displayName"] | "";
        typeDef.description = typeObj["description"] | "";
        typeDef.supportsVolume = typeObj["supportsVolume"] | false;
        typeDef.supportsAutopilotInput = typeObj["supportsAutopilotInput"] | false;
        typeDef.supportsVerificationInput = typeObj["supportsVerificationInput"] | false;
        typeDef.resumeStateOnReboot = typeObj["resumeStateOnReboot"] | false;
        typeDef.configParams.clear();
        JsonArray paramsArray = typeObj["configParams"].is<JsonArray>() ? typeObj["configParams"].as<JsonArray>() : JsonArray();
        for (JsonObject paramObj : paramsArray) {
            OutputTypeConfigParam param;
            param.id = paramObj["id"] | "";
            param.label = paramObj["label"] | "";
            param.type = paramObj["type"] | "";
            param.required = paramObj["required"] | false;
            param.readonly = paramObj["readonly"] | false;
            // Skipping defaultValue, min, max, step for brevity; add as needed
            typeDef.configParams.push_back(param);
        }
        if (!typeDef.typeId.isEmpty()) {
            outputTypes.push_back(typeDef);
        }
    }

    Serial.println("Parsed relay_types.json successfully.");
    return true;
}