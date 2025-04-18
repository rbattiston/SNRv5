#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include "ConfigManager.h"
#include "UserManager.h"
#include <ESPAsyncWebServer.h>
#include "AuthUtils.h"
#include "SessionManager.h"
#include "LockManager.h"
#include "ScheduleManager.h" // Include ScheduleManager
#include <ArduinoJson.h>     // Include ArduinoJson
#include <WiFi.h>            // Required for request->host()
#include "ApiRoutes.h"       // Include the new API routing class
#include "esp_task_wdt.h"
#include "esp_system.h"
#include "InputPointManager.h"
#include "OutputPointManager.h"
#define DEBUG_OUTPUT_TEST_TASK 1
#define DEBUG_INPUT_TASK 0

InputPointManager inputManager;
OutputPointManager outputManager;

#if DEBUG_INPUT_TASK
void inputReaderTaskWrapper(void* parameter) {
    int loopCount = 0;
    while (true) {
        Serial.printf("[InputPointManager] inputReaderTaskWrapper: Starting input fetch loop #%d\n", ++loopCount);
        inputManager.inputReaderTask(); // Reads all inputs and prints debug info
        Serial.printf("[InputPointManager] inputReaderTaskWrapper: Completed input fetch loop #%d\n", loopCount);
        vTaskDelay(pdMS_TO_TICKS(10000)); // Wait 10 seconds
    }
}
#endif

#if DEBUG_OUTPUT_TEST_TASK
void outputTestTaskWrapper(void* parameter) {
    int loopCount = 0;
    while (true) {
        // 1. TURN ON relay 0
        OutputCommand cmdOn;
        cmdOn.pointId = "DirectRelay_0";
        cmdOn.commandType = RelayCommandType::TURN_ON;
        cmdOn.durationMs = 0;
        outputManager.sendCommand(cmdOn);
        Serial.printf("[OutputTestTask] Sent TURN_ON command to %s (loop %d)\n", cmdOn.pointId.c_str(), loopCount);

        vTaskDelay(pdMS_TO_TICKS(5000)); // Wait 5 seconds

        // 2. TURN OFF relay 0
        OutputCommand cmdOff;
        cmdOff.pointId = "DirectRelay_0";
        cmdOff.commandType = RelayCommandType::TURN_OFF;
        cmdOff.durationMs = 0;
        outputManager.sendCommand(cmdOff);
        Serial.printf("[OutputTestTask] Sent TURN_OFF command to %s (loop %d)\n", cmdOff.pointId.c_str(), loopCount);

        vTaskDelay(pdMS_TO_TICKS(5000)); // Wait 5 seconds

        // 3. TURN ON TIMED relay 0 for 2 seconds
        OutputCommand cmdTimed;
        cmdTimed.pointId = "DirectRelay_0";
        cmdTimed.commandType = RelayCommandType::TURN_ON_TIMED;
        cmdTimed.durationMs = 2000; // 2 seconds
        outputManager.sendCommand(cmdTimed);
        Serial.printf("[OutputTestTask] Sent TURN_ON_TIMED (2s) command to %s (loop %d)\n", cmdTimed.pointId.c_str(), loopCount);

        vTaskDelay(pdMS_TO_TICKS(5000)); // Wait 5 seconds before next cycle

        loopCount++;
    }
}
#endif

// Define the required filesystem structure
const char* DIRS_TO_CREATE[] = {
  "/users",
  // "/schedules", // Redundant? Using /daily_schedules/
  "/cycles",
  "/cycles/templates",
  "/cycles/active",
  "/certs",
  "/locks",
  "/www",
  "/daily_schedules" // Added schedule directory
};
const int NUM_DIRS = sizeof(DIRS_TO_CREATE) / sizeof(DIRS_TO_CREATE[0]);


// --- Global Instances ---
ConfigManager configManager;
UserManager userManager;
SessionManager sessionManager;
LockManager lockManager;
ScheduleManager scheduleManager; // Add global instance
ApiRoutes* apiRoutesPtr = nullptr; // Declare a global pointer

// Web Servers
AsyncWebServer httpServer(80);
bool httpsEnabled = false; // Flag to indicate if certs exist (for cookie flags etc.)

// --- Helper Functions ---

// Adds common security headers to responses (only call when HTTPS is enabled)
// Removed addSecurityHeaders - Logic moved to ApiRoutes class

// Helper function to read a file into a String
/**
 * @brief Reads the entire content of a file into a String object.
 *
 * Opens the specified file from the given filesystem, reads its content
 * character by character, and returns it as a String. Handles file not found
 * or directory errors.
 *
 * @param fs Reference to the filesystem object (e.g., LittleFS).
 * @param path The absolute path to the file within the filesystem.
 * @return A String containing the file content, or an empty String if the file
 *         could not be opened or read.
 */
String readFileToString(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\r\n", path);
    File file = fs.open(path, "r");
    if(!file || file.isDirectory()){
        Serial.println("- empty file or failed to open file");
        return String();
    }
    String fileContent;
    while(file.available()){
        fileContent += (char)file.read();
    }
    file.close();
    return fileContent;
}


// --- Web Server Request Handlers ---

// Removed all handle... functions - Logic moved to ApiRoutes class


// Function to register common application routes
// Removed registerRoutes function - Logic moved to ApiRoutes class

// --- End Web Server Request Handlers ---


/**
 * @brief Initializes the ESP32 application.
 *
 * This function runs once at startup. It performs the following tasks:
 * 1. Initializes the Serial communication.
 * 2. Initializes and mounts the LittleFS filesystem, formatting if necessary.
 * 3. Creates the required directory structure within LittleFS if it doesn't exist.
 * 4. Loads the application configuration using ConfigManager.
 * 5. Initializes the UserManager, LockManager, and ScheduleManager.
 * 6. Connects to the configured WiFi network or starts an Access Point (AP) if connection fails.
 * 7. Checks for the presence of HTTPS certificate files and sets the `httpsEnabled` flag.
 * 8. Initializes the ApiRoutes handler.
 * 9. Sets up and starts the asynchronous HTTP web server on port 80, registering API routes and a 404 handler.
 */
void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }
  Serial.println("\n\nStarting setup...");

  // Initialize LittleFS
  Serial.println("Initializing LittleFS...");
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed! Formatting...");
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed even after formatting. Halting.");
        while (1) yield();
    }
  } else {
    Serial.println("LittleFS mounted successfully.");
  }

  // Create directory structure
  Serial.println("Checking/Creating directory structure...");
  for (int i = 0; i < NUM_DIRS; i++) {
    const char* dirPath = DIRS_TO_CREATE[i];
    if (!LittleFS.exists(dirPath)) {
      if (!LittleFS.mkdir(dirPath)) {
         Serial.printf("Failed to create directory: %s\n", dirPath);
      }
    }
  }
  Serial.println("Directory structure check complete.");


  // Load configuration
  Serial.println("Loading configuration...");
  if (!configManager.loadConfig()) {
    Serial.println("Failed to load configuration. Using defaults.");
  } else {
     Serial.println("Configuration loaded successfully.");
  }


  // Initialize UserManager
  Serial.println("Initializing UserManager...");
  if (!userManager.begin()) {
    Serial.println("UserManager initialization failed. Halting.");
    while(1) yield();
  }

  // Initialize LockManager
  Serial.println("Initializing LockManager...");
  if (!lockManager.begin()) {
    Serial.println("LockManager initialization failed. Halting.");
    while(1) yield();
  }

  // Initialize ScheduleManager
  Serial.println("Initializing ScheduleManager...");
  if (!scheduleManager.begin()) {
    Serial.println("ScheduleManager initialization failed. Halting.");
    while(1) yield();
  }

  // Load I/O configuration
  IOConfiguration ioConfig;
  if (configManager.loadBoardIOConfig(ioConfig)) {
      inputManager.begin(ioConfig);
      if (!outputManager.begin(ioConfig)) {
        Serial.println("[main] OutputPointManager initialization failed. Halting.");
        while (1) yield();
    } else {
        Serial.println("[main] OutputPointManager initialized successfully.");
    }
  }

  // Start FreeRTOS input reader task (debug only)
  #if DEBUG_INPUT_TASK
  xTaskCreatePinnedToCore(
      inputReaderTaskWrapper,
      "InputReaderTask",
      4096,
      nullptr,
      1,
      nullptr,
      1
  );
  #endif

  #if DEBUG_OUTPUT_TEST_TASK
  xTaskCreatePinnedToCore(
      outputTestTaskWrapper,
      "OutputTestTask",
      4096,
      nullptr,
      1,
      nullptr,
      1
  );
  #endif


  // --- Initialize WiFi ---
  Serial.println("Connecting to WiFi...");
  WiFi.begin(configManager.getConfig().wifi_ssid.c_str(), configManager.getConfig().wifi_password.c_str());
  int connectTimeout = 20; // ~10 seconds
  while (WiFi.status() != WL_CONNECTED && connectTimeout > 0) {
    delay(500);
    Serial.print(".");
    connectTimeout--;
  }
  if (WiFi.status() == WL_CONNECTED) {
     Serial.println("\nWiFi connected.");
     Serial.print("IP Address: ");
     Serial.println(WiFi.localIP());
  } else {
     Serial.println("\nWiFi connection failed. Starting AP mode...");
     WiFi.mode(WIFI_AP);
     WiFi.softAP(configManager.getConfig().ap_ssid.c_str(), configManager.getConfig().ap_password.c_str());
     Serial.print("AP IP Address: ");
     Serial.println(WiFi.softAPIP());
  }
  // --- End WiFi Init ---


  // --- Check for HTTPS Certificates ---
  Serial.println("Checking for HTTPS certificates...");
  const char* certPath = "/certs/cert.pem";
  const char* keyPath = "/certs/key.pem";
  bool certsExist = LittleFS.exists(certPath) && LittleFS.exists(keyPath);

  if (certsExist) {
    // Set flag for cookie attributes, but don't start HTTPS server
    httpsEnabled = true;
    Serial.println("HTTPS certificates found. Secure cookie flag will be used.");
    Serial.println("NOTE: Actual HTTPS server is NOT started in this basic example.");
    Serial.println("      Requires manual SSL context setup or different library (e.g., esp_http_server).");
  } else {
    httpsEnabled = false;
    Serial.println("HTTPS certificates not found.");
  }
  // --- End Certificate Check ---


  // --- Setup and Start HTTP Server ---
  Serial.println("Setting up HTTP server...");
  // Initialize and register API routes using the new class
  apiRoutesPtr = new ApiRoutes(userManager, sessionManager, scheduleManager, lockManager, httpsEnabled); // Allocate on heap
  apiRoutesPtr->registerRoutes(httpServer); // Register app routes on HTTP using the pointer
  // Add onNotFound handler here AFTER specific routes in registerRoutes
  httpServer.onNotFound([](AsyncWebServerRequest *request){
       Serial.printf("NOT FOUND: HTTP %s request to %s\n", request->methodToString(), request->url().c_str());
       // Handle OPTIONS for CORS preflight if needed (though typically handled by specific routes or middleware)
       if(request->method() == HTTP_OPTIONS){
           request->send(204);
       } else {
           request->send(404, "text/plain", "Not found");
       }
   });
  httpServer.begin();
  Serial.println("HTTP Server started on port 80.");
  // --- End Server Setup ---


  Serial.println("Setup complete.");
}

/**
 * @brief Main application loop.
 *
 * This function runs repeatedly after `setup()` completes.
 * It calls the cleanup functions for the SessionManager and LockManager
 * to handle expired sessions and locks.
 * The AsyncWebServer handles client requests asynchronously in the background,
 * so no explicit server handling or delays are typically needed here.
 */
void loop() {
  sessionManager.cleanupExpiredSessions();
  lockManager.cleanupExpiredLocks();

  // No delay needed if using AsyncWebServer correctly
}