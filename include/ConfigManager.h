#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h> // Use ArduinoJson V7
#include <vector>
#include "IOConfig.h"
#include "OutputTypeData.h"

// Structure to hold application configuration
struct AppConfig {
    // WiFi Station Mode Credentials
    String wifi_ssid = "S3CURE_WIFI";
    String wifi_password = "Hoyt1000!";

    // WiFi AP Mode Credentials (Defaults)
    String ap_ssid = "ESP32-WebApp";
    String ap_password = "password";
    // Add other system settings as needed
};

class ConfigManager {
public:
    /**
     * @brief Constructs a ConfigManager object.
     * @param configFilePath The path within LittleFS to the configuration file. Defaults to "/config.json".
     */
    ConfigManager(const char* configFilePath = "/config.json");

    // Loads configuration from the file system
    // Returns true on success, false on failure (e.g., file not found, parse error)
    /**
     * @brief Loads configuration from the file system into the internal AppConfig struct.
     *
     * If the file doesn't exist or is invalid, it attempts to create a default configuration file.
     * @return True on successful load (or successful default creation), false on critical failure.
     */
    bool loadConfig();

    // Saves the current configuration to the file system
    // Returns true on success, false on failure
    /**
     * @brief Saves the current configuration (held in the internal AppConfig struct) to the file system.
     * @return True on successful save, false on failure.
     */
    bool saveConfig();

    // Provides access to the configuration data
    /**
     * @brief Provides access to the loaded configuration data.
     * @return A reference to the internal AppConfig struct.
     */
    AppConfig& getConfig();

    // === Step 2: I/O Foundation Methods ===
    bool loadBoardIOConfig(IOConfiguration& ioConfig);
    bool loadRelayTypes(std::vector<OutputTypeDefinition>& outputTypes);

private:
    AppConfig config; ///< Internal structure holding the currently loaded configuration data.
    String _configFilePath; ///< Path to the configuration file in the filesystem.

    // Helper to create a default config file if it doesn't exist
    /**
     * @brief Creates a default configuration file using the default values in AppConfig.
     *
     * Called by loadConfig if the file is missing or corrupt. It resets the internal
     * config struct to defaults and then calls saveConfig().
     * @return True if the default file was saved successfully, false otherwise.
     */
    bool createDefaultConfigFile();
};

#endif // CONFIG_MANAGER_H