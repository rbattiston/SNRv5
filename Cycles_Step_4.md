# Step 4: Cycle Template Management (Backend Logic) - Detailed Plan (Revision 1)

## 1. Goal:

To implement the C++ `TemplateManager` class responsible for managing the lifecycle of reusable Cycle Templates. This includes:
*   Creating new Cycle Templates.
*   Reading (loading) existing Cycle Templates by their `templateId`.
*   Updating existing Cycle Templates.
*   Deleting Cycle Templates.
*   Listing available Cycle Templates.
*   Persisting Cycle Template definitions to LittleFS (`/data/cycle_templates/<templateId>.json`).

Cycle Templates store only the sequence of steps, referencing Schedule Template UIDs (`libraryScheduleId`) and defining the duration for each step. They do **not** store I/O associations.

## 2. Prerequisites:

*   Step 1 (`Cycles_Step_1.md` - Rev 8) plan finalized, defining the `CycleTemplate` struct and JSON format.
*   Step 2 (`Cycles_Step_2.md` - Rev 3) plan finalized.
*   Step 3 (`Cycles_Step_3.md` - Rev 11) plan finalized.
*   Functional `ConfigManager` (potentially for accessing Schedule Template UIDs, though maybe not strictly required by `TemplateManager` itself).
*   C++ struct definitions from Step 1 available in headers (`CycleData.h`).
*   FreeRTOS environment (for mutexes if needed).
*   LittleFS filesystem initialized and helper functions for file I/O available.

## 3. Deliverables:

*   Implemented `TemplateManager` class (`.h` and `.cpp`) containing:
    *   Methods for CRUD operations on Cycle Templates (`createTemplate`, `loadTemplate`, `saveTemplate`, `deleteTemplate`).
    *   Method for listing available templates (`listTemplates`).
    *   Internal logic for handling template data.
*   Implementation of persistence logic using ArduinoJSON v7 for serialization/deserialization.
*   Unit/integration tests for the `TemplateManager` functionality.

## 4. Detailed Implementation Tasks & Pseudocode:

*(Note: Assumes C++ structs from Step 1 Rev 8 are defined, specifically `CycleTemplate` and nested `CycleTemplateStep`.)*

### 4.1. `TemplateManager` Class Definition

*   **Task:** Define the header file (`TemplateManager.h`) with the `TemplateManager` class structure.
*   **Header (`TemplateManager.h` - Conceptual):**
    ```c++
    #pragma once

    #include <vector>
    #include <map> // Optional: If caching loaded templates
    #include "Arduino.h" // For String
    #include "CycleData.h" // Contains CycleTemplate struct definition
    #include "freertos/FreeRTOS.h"
    #include "freertos/semphr.h" // For Mutex

    // Forward declaration if needed
    // class ConfigManager;

    struct BasicTemplateInfo {
        String templateId;
        String templateName;
    };

    class TemplateManager {
    public:
        // Constructor - May not need other managers directly
        TemplateManager(/* ConfigManager& cfgMgr */);
        bool begin(); // Initialize mutex, check directory

        // CRUD Operations
        bool createTemplate(const CycleTemplate& newTemplate); // Saves the new template
        bool loadTemplate(const String& templateId, CycleTemplate& templateData);
        bool saveTemplate(const CycleTemplate& templateData); // Updates existing or creates if not found
        bool deleteTemplate(const String& templateId);

        // Listing Operation
        bool listTemplates(std::vector<BasicTemplateInfo>& templateList);

    private:
        // ConfigManager& configMgr; // Reference if needed for validation (e.g., check if libraryScheduleId exists)
        MutexHandle_t templateMutex; // Mutex for thread safety

        // Optional: Cache for frequently accessed templates
        // std::map<String, CycleTemplate> templateCache;

        // Helper Functions
        String getTemplatePath(const String& templateId);
        bool validateTemplateData(const CycleTemplate& templateData); // Basic validation (e.g., non-empty ID/Name/Sequence)
        // Assumed file system helpers: ensureDirectoryExists, writeJsonDocumentToFile, readFileToJsonDocument
    };
    ```

### 4.2. Persistence (`saveTemplate`, `loadTemplate`, `deleteTemplate`)

*   **Task:** Implement saving, loading, and deleting Cycle Template JSON files using ArduinoJSON v7.
*   **Directory:** `/data/cycle_templates/`
*   **Filename:** `<templateId_sanitized>.json` (Sanitize `templateId` to be filesystem-safe).
*   **Pseudocode (`saveTemplate` - ArduinoJSON v7):**
    ```c++
    bool TemplateManager::saveTemplate(const CycleTemplate& templateData) {
      if (!validateTemplateData(templateData)) {
          // Log error: Invalid template data
          return false;
      }

      String path = getTemplatePath(templateData.templateId);
      ensureDirectoryExists(path); // Ensure /data/cycle_templates exists

      JsonDocument doc; // Use DynamicJsonDocument if size is highly variable
      doc["templateId"] = templateData.templateId;
      doc["templateName"] = templateData.templateName;

      JsonArray seq = doc.addArray("sequence");
      if (seq.isNull()) { /* Log error, return false */ }

      for (const auto& step : templateData.sequence) {
          JsonObject stepObj = seq.addObject();
          if (stepObj.isNull()) { /* Log error, return false */ }
          stepObj["step"] = step.step;
          stepObj["libraryScheduleId"] = step.libraryScheduleId;
          stepObj["durationDays"] = step.durationDays;
      }

      bool success = false;
      if (xSemaphoreTake(templateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          success = writeJsonDocumentToFile(path, doc);
          xSemaphoreGive(templateMutex);
      } else {
          // Log error: Mutex timeout during save
      }
      return success;
    }
    ```
*   **Pseudocode (`loadTemplate` - ArduinoJSON v7):**
    ```c++
    bool TemplateManager::loadTemplate(const String& templateId, CycleTemplate& templateData) {
      String path = getTemplatePath(templateId);
      JsonDocument doc;
      bool fileReadSuccess = false;

      if (xSemaphoreTake(templateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          fileReadSuccess = readFileToJsonDocument(path, doc);
          xSemaphoreGive(templateMutex);
      } else {
          // Log error: Mutex timeout during load
          return false;
      }

      if (!fileReadSuccess) { return false; }

      templateData.templateId = doc["templateId"] | "";
      templateData.templateName = doc["templateName"] | "";

      templateData.sequence.clear();
      JsonArray seq = doc["sequence"].as<JsonArray>();
      if (!seq.isNull()) {
          templateData.sequence.reserve(seq.size()); // Optimize vector allocation
          for (JsonObject stepObj : seq) {
              CycleTemplateStep stepData;
              stepData.step = stepObj["step"] | 0;
              stepData.libraryScheduleId = stepObj["libraryScheduleId"] | "";
              stepData.durationDays = stepObj["durationDays"] | 0;
              // Add validation if needed (e.g., step > 0, duration > 0, non-empty libraryScheduleId)
              if (stepData.step > 0 && !stepData.libraryScheduleId.isEmpty() && stepData.durationDays > 0) {
                 templateData.sequence.push_back(stepData);
              } else {
                 // Log warning: Invalid step data in template templateId
              }
          }
      }

      // Basic validation after loading
      return !templateData.templateId.isEmpty() && !templateData.templateName.isEmpty();
    }
    ```
*   **Pseudocode (`deleteTemplate`):**
    ```c++
    bool TemplateManager::deleteTemplate(const String& templateId) {
      String path = getTemplatePath(templateId);
      bool success = false;
      if (xSemaphoreTake(templateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          if (LittleFS.exists(path)) {
              success = LittleFS.remove(path);
          } else {
              success = false; // File didn't exist
          }
          xSemaphoreGive(templateMutex);
      } else {
          // Log error: Mutex timeout during delete
      }
      return success;
    }
    ```

### 4.3. Listing Templates (`listTemplates`)

*   **Task:** Implement a method to scan the `/data/cycle_templates/` directory and return basic info for each template found.
*   **Pseudocode (`listTemplates`):**
    ```c++
    bool TemplateManager::listTemplates(std::vector<BasicTemplateInfo>& templateList) {
      templateList.clear();
      File root = LittleFS.open("/data/cycle_templates");
      if (!root || !root.isDirectory()) {
          // Log error or warning: Template directory not found or not a directory.
          return false; // Indicate directory issue
      }

      File file = root.openNextFile();
      while (file) {
          if (!file.isDirectory()) {
              String filename = file.name();
              if (filename.endsWith(".json")) {
                  // Attempt to load just enough info (ID and Name) for the list
                  // This avoids loading the full sequence for every file, improving performance.
                  JsonDocument doc;
                  // Need a helper to read only specific fields or use smaller JsonDocument
                  if (readFileToJsonDocument(file.path(), doc)) { // Assuming file.path() gives full path
                      BasicTemplateInfo info;
                      info.templateId = doc["templateId"] | "";
                      info.templateName = doc["templateName"] | "";
                      if (!info.templateId.isEmpty()) { // Basic check
                          templateList.push_back(info);
                      } else {
                          // Log warning: Found JSON file without templateId in template dir: filename
                      }
                  } else {
                      // Log warning: Failed to parse JSON file in template dir: filename
                  }
              }
          }
          file.close(); // Close the file handle
          file = root.openNextFile();
      }
      root.close(); // Close the directory handle
      return true; // Operation completed, list might be empty
    }
    ```

### 4.4. Helper Functions

*   **`getTemplatePath`:** Constructs the full path `/data/cycle_templates/<templateId_sanitized>.json`. Needs sanitization logic for `templateId`.
*   **`validateTemplateData`:** Performs basic checks (non-empty ID, name, sequence; steps have valid data). Optional: Could check if referenced `libraryScheduleId`s exist using `ScheduleManager` if a reference is passed in.

## 5. Testing Considerations:

*   Test `createTemplate`, `loadTemplate`, `saveTemplate` (update), `deleteTemplate` operations.
*   Test `listTemplates` with an empty directory, a directory with valid templates, and a directory with invalid/corrupt files.
*   Test handling of invalid template data during save/load (e.g., missing fields, empty sequence).
*   Test filename sanitization and path generation.
*   Test thread safety if multiple tasks could potentially call `TemplateManager` methods concurrently (using the mutex).
*   Verify that templates do not store I/O associations.

---