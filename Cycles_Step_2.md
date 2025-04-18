# Step 2: I/O Foundation &amp; Low-Level Control (Direct I/O Focus) - Detailed Plan

## 1. Goal:

To implement the foundational C++ classes and logic required to manage and interact with the *directly connected* I/O points defined in `board_config.json`. This involves:
*   Parsing the relevant sections of the hardware I/O configuration (`board_config.json`'s `directIO` section) and the output function types (`relay_types.json`).
*   Initializing the hardware (GPIO pins) for direct relays, digital inputs, and analog I/O according to the configuration.
*   Creating internal mappings between system `pointId`s and physical resources (GPIO pins, ADC channels, DAC channels, shift register indices).
*   Implementing the `OutputPointManager` to:
    *   Load Output Point Definitions (`/data/output_definitions/*.json`) storing `assignedType` and `configValues` per `pointId`.
    *   Provide a unified command interface (`sendCommand(pointId, commandType, duration)`).
    *   Implement the command queue and processor task.
    *   Implement low-level control logic *only for direct relays* (Shift Register/Direct GPIO) within the command processor, triggered by commands for corresponding `pointId`s. (Modbus DO control deferred).
    *   Implement persistence for Output Point Definitions (`saveOutputPointDefinition`, `loadOutputPointDefinition`).
*   Implementing the `InputPointManager` to:
    *   Load Input Point Configurations (`/data/input_configs/*.json`).
    *   Provide a unified interface to get processed input values (`getProcessedValue(pointId)`).
    *   Implement low-level reading logic *only for direct inputs* (DI/AI) based on `pointId`. (Modbus DI/AI reading deferred).
    *   Provide a basic interface to retrieve the current raw or minimally processed value for direct input `pointId`s.
*   **Exclusions:** Modbus I/O control/reading and Active Cycle logic are explicitly excluded in this step.

## 2. Prerequisites:

*   Finalized data structures and JSON formats from Step 1 (documented in `Cycles_Step_1.md`).
*   Conceptual C++ struct definitions reflecting Step 1 (e.g., in `include/OutputTypeData.h`, `include/IOConfig.h`, `include/OutputDefData.h`, `include/InputConfigData.h`). Header files should be created/updated based on Step 1.
*   Basic file system utilities (reading config files, reading/writing definition/config files).
*   FreeRTOS environment configured for task and queue creation.

## 3. Deliverables:

*   Updated `ConfigManager` (or equivalent) capable of parsing `board_config.json` (`directIO` section) and `relay_types.json` into appropriate C++ structures (e.g., `IOConfiguration`, `std::vector<OutputTypeInfo>`).
*   Implemented `OutputPointManager` class (`.h` and `.cpp`) containing:
    *   Initialization logic for direct relays based on parsed configuration.
    *   Internal mapping from direct relay `pointId` to physical resource (index/pin).
    *   Functions for direct physical relay control (`setDirectRelayStatePhysical`).
    *   Command queue setup (`commandQueue`).
    *   Command processor task (`processCommandQueueTask`) handling commands for direct relay `pointId`s.
    *   Persistence methods (`saveOutputPointDefinition`, `loadOutputPointDefinition`) for direct relay definitions.
*   Implemented `InputPointManager` class (`.h` and `.cpp`) containing:
    *   Initialization logic for direct inputs (DI/AI) based on parsed configuration.
    *   Internal mapping from direct input `pointId` to physical resource (pin/ADC channel).
    *   **Periodic input reader task (`inputReaderTask`)** using FreeRTOS (`vTaskDelayUntil`) to read direct DI/AI values at a regular interval (e.g., 1-10 seconds).
    *   Internal storage (e.g., maps) for the latest read values, protected by a mutex (`valueMutex`).
    *   Functions for direct physical input reading (`readDirectDIState`, `readDirectAIValueRaw`) called by the reader task.
    *   Interface to get the *last read* value (`getCurrentValue(pointId)`, `getCurrentState(pointId)`) from internal storage (mutex-protected).
    *   Basic persistence methods (`saveInputPointConfig`, `loadInputPointConfig`) for direct inputs.
*   Unit tests (where feasible) for parsing, mapping, command queuing, direct I/O control/reading logic, and task interactions.

## 4. Detailed Implementation Tasks & Pseudocode:

*(Note: Assumes C++ structs like `IOConfiguration`, `OutputTypeInfo`, `OutputPointDefinition`, `InputPointConfig`, `OutputCommand` are defined based on Step 1)*

### 4.1. Configuration Parsing (`ConfigManager`)

*   **Task:** Enhance `ConfigManager::loadConfig` to fully parse the `directIO` section of `board_config.json` (relays, DI, AI, AO counts, pins, methods, pointId generation info) into the `IOConfiguration` struct. Continue parsing `relay_types.json` into `std::vector<OutputTypeInfo>`.
*   **Pseudocode (`ConfigManager::loadConfig()` - relevant parts):**
    ```c++
    bool ConfigManager::loadConfig(IOConfiguration& ioConfig, std::vector<OutputTypeInfo>& outputTypes) {
      configLoaded = false;
      ioConfig = IOConfiguration();
      outputTypes.clear();

      String boardJsonContent = readFileToString("/data/board_config.json");
      if (boardJsonContent.isEmpty()) { return false; }

      JsonDocument boardDoc;
      DeserializationError boardError = deserializeJson(boardDoc, boardJsonContent);
      if (boardError) { return false; }

      JsonObject directIO = boardDoc["directIO"];
      if (!directIO.isNull()) {
         JsonObject relays = directIO["relayOutputs"];
         if (!relays.isNull()) {
             ioConfig.directRelayOutputCount = relays["count"] | 0;
             ioConfig.directRelayControlMethod = relays["controlMethod"] | "DirectGPIO";
             ioConfig.directRelayPointIdPrefix = relays["pointIdPrefix"] | "DirectRelay_";
             ioConfig.directRelayPointIdStartIndex = relays["pointIdStartIndex"] | 0;
             JsonObject relayPins = relays["pins"];
             if (!relayPins.isNull()) { /* Parse pins into ioConfig based on method */ }
         }
         // Parse digitalInputs into ioConfig
         // Parse analogInputs into ioConfig
         // Parse analogOutputs into ioConfig
      }

      String typesJsonContent = readFileToString("/data/relay_types.json");
      if (typesJsonContent.isEmpty()) { return false; }
      JsonDocument typesDoc;
      DeserializationError typesError = deserializeJson(typesDoc, typesJsonContent);
      if (typesError) { return false; }
      JsonArray typesArray = typesDoc.as<JsonArray>();
      for (JsonObject typeObj : typesArray) {
          OutputTypeInfo typeInfo;
          // Parse fields into typeInfo
          if (!typeInfo.typeId.isEmpty()) { outputTypes.push_back(typeInfo); }
      }

      configLoaded = true;
      return true;
    }
    ```

### 4.2. `OutputPointManager` Implementation

*   **Task:** Create the manager responsible for handling all output points. In this step, focus on initializing and controlling *direct relays*.
*   **Header (`OutputPointManager.h` - Conceptual):**
    ```c++
    class OutputPointManager {
    public:
        bool begin(const IOConfiguration& ioConfig);
        bool sendCommand(const OutputCommand& command);
        bool saveOutputPointDefinition(const OutputPointDefinition& definition);
        bool loadOutputPointDefinition(const String& pointId, OutputPointDefinition& definition);
    private:
        IOConfiguration ioConfig;
        int directRelayCount = 0;
        std::map<String, int> directRelayPointIdToIndexMap;
        MutexHandle_t stateMutex;
        MutexHandle_t timerListMutex;
        QueueHandle_t commandQueue;
        TaskHandle_t commandProcessorTaskHandle;
        std::vector<TaskHandle_t> activeTimerTasks;

        void initializeDirectRelayHardware();
        void buildDirectRelayMap();
        void setDirectRelayStatePhysical(int relayIndex, bool on);
        static void commandProcessorTaskWrapper(void* parameter); // Static wrapper for the FreeRTOS task
        void processCommandQueueTask(); // The function implementing the task loop
        void startTimerTaskForDirectRelay(int relayIndex, unsigned long durationMs);
        void cancelTimerTaskForDirectRelay(int relayIndex);
        static void timedOffTaskWrapper(void* parameter);
        String getOutputDefinitionPath(const String& pointId);
        String sanitizeFilename(const String& input);
        bool readFileToJsonDocument(const String& path, JsonDocument& doc);
        bool writeJsonDocumentToFile(const String& path, JsonDocument& doc);
        void ensureDirectoryExists(const String& path);
    };
    ```
*   **Initialization (`OutputPointManager::begin`):**
    *   Store `ioConfig`.
    *   Call `initializeDirectRelayHardware()`.
    *   Call `buildDirectRelayMap()`.
    *   Create `commandQueue`.
    *   Create `commandProcessorTask`.
    *   Initialize mutexes.
*   **Physical Control (`setDirectRelayStatePhysical`):** Controls hardware based on index (ShiftReg/DirectGPIO), protects internal state representation (e.g., bitmask) with `stateMutex`.
    ```c++
    // void OutputPointManager::setDirectRelayStatePhysical(int relayIndex, bool on)
    //   if (relayIndex < 0 || relayIndex >= directRelayCount) { return; }
    //
    //   // --- Protect internal state access ---
    //   if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
    //       updateInternalDirectRelayState(relayIndex, on);
    //       uintN_t currentState = getInternalDirectRelayStateBitmask();
    //       xSemaphoreGive(stateMutex);
    //
    //       // --- Send state to hardware (outside mutex if possible) ---
    //       if (ioConfig.directRelayControlMethod.equalsIgnoreCase("ShiftRegister")) {
    //           // Shift out currentState
    //       } else {
    //           // digitalWrite pin based on index
    //       }
    //       // Log state change
    //   } else { /* Handle mutex take failure */ }
    ```
*   **Command Processing (`processCommandQueueTask`):** This function runs as a continuous FreeRTOS task, blocking on `xQueueReceive`. When a command arrives, it uses the `directRelayPointIdToIndexMap` to find the index for direct relays. It executes the command (ON/OFF/Timed), manages timer tasks (creating temporary `timedOffTaskWrapper` tasks for timed commands, cancelling existing timers using `cancelTimerTaskForDirectRelay`), and ignores commands for non-direct pointIds in this step. Access to the `activeTimerTasks` list must be protected by `timerListMutex`.
    ```c++
    // void OutputPointManager::processCommandQueueTask()
    //   while (true) {
    //     if (xQueueReceive(commandQueue, &receivedCommand, portMAX_DELAY) == pdPASS) {
    //       auto it = directRelayPointIdToIndexMap.find(receivedCommand.pointId);
    //       if (it != directRelayPointIdToIndexMap.end()) {
    //           int directRelayIndex = it->second;
    //           switch (receivedCommand.commandType) {
    //             case RelayCommandType::TURN_ON:
    //               cancelTimerTaskForDirectRelay(directRelayIndex); // Uses timerListMutex internally
    //               setDirectRelayStatePhysical(directRelayIndex, true); // Uses stateMutex internally
    //               break;
    //             case RelayCommandType::TURN_OFF:
    //               cancelTimerTaskForDirectRelay(directRelayIndex); // Uses timerListMutex internally
    //               setDirectRelayStatePhysical(directRelayIndex, false); // Uses stateMutex internally
    //               break;
    //             case RelayCommandType::TURN_ON_TIMED:
    //               cancelTimerTaskForDirectRelay(directRelayIndex); // Uses timerListMutex internally
    //               setDirectRelayStatePhysical(directRelayIndex, true); // Uses stateMutex internally
    //               startTimerTaskForDirectRelay(directRelayIndex, receivedCommand.durationMs); // Uses timerListMutex internally
    //               break;
    //           }
    //       } else { /* Ignore non-direct command */ }
    //     }
    //   }
    ```
*   **Timer Task Management Helpers (Mutex Usage):**
    ```c++
    // void OutputPointManager::startTimerTaskForDirectRelay(int relayIndex, unsigned long durationMs)
    //   // ... (Allocate params) ...
    //   TaskHandle_t newTaskHandle = NULL;
    //   BaseType_t taskCreated = xTaskCreate(timedOffTaskWrapper, ..., params, ..., &newTaskHandle);
    //   if (taskCreated == pdPASS && newTaskHandle != NULL) {
    //       if (xSemaphoreTake(timerListMutex, portMAX_DELAY) == pdTRUE) {
    //           // Ensure index is valid and potentially resize activeTimerTasks if needed
    //           if (relayIndex < activeTimerTasks.size()) {
    //               activeTimerTasks[relayIndex] = newTaskHandle;
    //           } else { /* Handle error or resize */ }
    //           xSemaphoreGive(timerListMutex);
    //       } else { /* Handle mutex error; maybe delete created task */ }
    //   } else { /* Handle task creation error */ }

    // void OutputPointManager::cancelTimerTaskForDirectRelay(int relayIndex)
    //   TaskHandle_t taskToCancel = NULL;
    //   if (xSemaphoreTake(timerListMutex, portMAX_DELAY) == pdTRUE) {
    //       if (relayIndex < activeTimerTasks.size() && activeTimerTasks[relayIndex] != NULL) {
    //           taskToCancel = activeTimerTasks[relayIndex];
    //           activeTimerTasks[relayIndex] = NULL; // Clear handle *inside* mutex lock
    //       }
    //       xSemaphoreGive(timerListMutex);
    //   } else { /* Handle mutex error */ }
    //   // Delete task *outside* mutex lock
    //   if (taskToCancel != NULL) {
    //       vTaskDelete(taskToCancel);
    //   }

    // static void OutputPointManager::timedOffTaskWrapper(void* parameter)
    //   // ... (Get params: idx, delaySec) ...
    //   vTaskDelay(pdMS_TO_TICKS(delaySec * 1000));
    //   OutputPointManager* instance = ...; // Get instance pointer if needed
    //   TaskHandle_t selfHandle = NULL; // Store own handle if needed for check
    //   bool shouldSendOffCommand = false;
    //   // Check if cancelled - requires access to the manager instance or passing self handle
    //   if (xSemaphoreTake(instance->timerListMutex, pdMS_TO_TICKS(100)) == pdTRUE) { // Use timeout
    //       if (idx < instance->activeTimerTasks.size() && instance->activeTimerTasks[idx] != NULL) {
    //           // Optional: Check if handle matches selfHandle if passed
    //           shouldSendOffCommand = true;
    //           instance->activeTimerTasks[idx] = NULL; // Clear handle
    //       }
    //       xSemaphoreGive(instance->timerListMutex);
    //   } else { /* Handle mutex timeout - maybe still send OFF? */ }
    //
    //   if (shouldSendOffCommand) {
    //       // Create OFF command
    //       // xQueueSend(instance->commandQueue, &offCmd, ...);
    //   }
    //   free(parameter); // Free params memory
    //   vTaskDelete(NULL); // Delete self

    // **Explanation of `cancelTimerTaskForDirectRelay`:**
    // Purpose: Prevents overlapping timed operations for the *same* relay. Ensures that
    //          if a new command (OFF or new timed ON) arrives while a timer is running
    //          for that specific relay, the old timer is stopped cleanly.
    // Triggering: Called by the commandProcessorTask *before* starting a new timed ON
    //             and when processing an explicit OFF command for a given relay index.
    // Mechanism:
    //   1. Takes `timerListMutex`.
    //   2. Checks `activeTimerTasks[relayIndex]` for a non-NULL handle.
    //   3. If found, copies the handle and sets `activeTimerTasks[relayIndex] = NULL`.
    //   4. Gives `timerListMutex`.
    //   5. If a handle was copied, calls `vTaskDelete()` on that handle.
    // Interaction with Timer Task: The timer task itself checks `activeTimerTasks[idx]`
    //   (using the mutex) after its delay. If it finds NULL, it knows it was cancelled
    //   and exits without sending the OFF command.
    ```
*   **Persistence (`saveOutputPointDefinition`, `loadOutputPointDefinition`):**
    *   Use `pointId` and `sanitizeFilename` to get path in `/data/output_definitions/`.
    *   `save`: Serializes `OutputPointDefinition` struct to JSON, writes file.
    *   `load`: Reads file, parses JSON into struct. Handles file not found.

### 4.3. `InputPointManager` Implementation (with FreeRTOS Task)

*   **Task:** Create the manager for input points. Initialize direct inputs (DI/AI) and implement a periodic FreeRTOS task to read their values, storing them internally. Provide methods to retrieve the latest stored values.
*   **Header (`InputPointManager.h` - Conceptual):**
    ```c++
    class InputPointManager {
    public:
        bool begin(const IOConfiguration& ioConfig);
        float getCurrentValue(const String& pointId);
        bool getCurrentState(const String& pointId);
        bool saveInputPointConfig(const InputPointConfig& config);
        bool loadInputPointConfig(const String& pointId, InputPointConfig& config);
    private:
        IOConfiguration ioConfig;
        std::map<String, int> directDIPointIdToPinMap;
        std::map<String, int> directAIPointIdToPinMap;
        MutexHandle_t valueMutex; // To protect access to last value maps
        TaskHandle_t inputReaderTaskHandle; // Handle for the reader task
        std::map<String, bool> lastDIStates; // Store last known states
        std::map<String, int> lastAIRawValues; // Store last known raw ADC values

        void initializeDirectInputHardware();
        void buildDirectInputMaps();
        bool readDirectDIState(int pin); // Reads hardware
        int readDirectAIValueRaw(int pin); // Reads hardware
        static void inputReaderTaskWrapper(void* parameter); // Static wrapper for FreeRTOS task
        void inputReaderTask(); // The function implementing the task loop
        String getInputConfigPath(const String& pointId);
        String sanitizeFilename(const String& input);
        bool readFileToJsonDocument(const String& path, JsonDocument& doc);
        bool writeJsonDocumentToFile(const String& path, JsonDocument& doc);
        void ensureDirectoryExists(const String& path);
    };
    ```
*   **Initialization (`InputPointManager::begin`):**
    *   Store `ioConfig`.
    *   Call `initializeDirectInputHardware()` (sets pin modes, configures ADC).
    *   Call `buildDirectInputMaps()` (populates maps).
    *   Initialize `valueMutex` (`xSemaphoreCreateMutex`).
    *   Create `inputReaderTask` (`xTaskCreatePinnedToCore`).
*   **Periodic Reading Task (`inputReaderTask`):**
    *   Runs as a continuous FreeRTOS task.
    *   Uses `vTaskDelayUntil` for periodic execution (e.g., every 1000ms).
    *   In its loop:
        *   Iterates through `directDIPointIdToPinMap`. For each entry (`pointId`, `pin`):
            *   `bool currentState = readDirectDIState(pin);`
            *   `if (xSemaphoreTake(valueMutex, portMAX_DELAY) == pdTRUE)` {
            *       `lastDIStates[pointId] = currentState;`
            *       `xSemaphoreGive(valueMutex);`
            *   }
        *   Iterates through `directAIPointIdToPinMap`. For each entry (`pointId`, `pin`):
            *   `int rawValue = readDirectAIValueRaw(pin);`
            *   `if (xSemaphoreTake(valueMutex, portMAX_DELAY) == pdTRUE)` {
            *       `lastAIRawValues[pointId] = rawValue;`
            *       `xSemaphoreGive(valueMutex);`
            *   }
*   **Physical Reading (`readDirectDIState`, `readDirectAIValueRaw`):** Implement `digitalRead()`, `analogRead()`. Called only by `inputReaderTask`.
*   **Value Retrieval (`getCurrentValue`, `getCurrentState`):**
    *   ```c++
    *   // float InputPointManager::getCurrentValue(const String& pointId)
    *   //   float value = DEFAULT_AI_ERROR_VALUE; // Define a suitable default/error value
    *   //   if (xSemaphoreTake(valueMutex, pdMS_TO_TICKS(100)) == pdTRUE) { // Use timeout
    *   //       auto it = lastAIRawValues.find(pointId);
    *   //       if (it != lastAIRawValues.end()) {
    *   //           value = (float)it->second; // Return raw value for now
    *   //       }
    *   //       xSemaphoreGive(valueMutex);
    *   //   } else { /* Handle mutex timeout */ }
    *   //   return value;
    *
    *   // bool InputPointManager::getCurrentState(const String& pointId)
    *   //   bool state = false; // Default state
    *   //   if (xSemaphoreTake(valueMutex, pdMS_TO_TICKS(100)) == pdTRUE) { // Use timeout
    *   //       auto it = lastDIStates.find(pointId);
    *   //       if (it != lastDIStates.end()) {
    *   //           state = it->second;
    *   //       }
    *   //       xSemaphoreGive(valueMutex);
    *   //   } else { /* Handle mutex timeout */ }
    *   //   return state;
    *   ```
    *   **Does NOT perform a hardware read directly.**
*   **Persistence (`saveInputPointConfig`, `loadInputPointConfig`):**
    *   Use `pointId` and `sanitizeFilename` to get path in `/data/input_configs/`.
    *   `save`: Serializes full `InputPointConfig` struct to JSON, writes file.
    *   `load`: Reads file, parses JSON into struct. Handles file not found.

## 5. Testing Considerations:

*   Test `ConfigManager` parsing of `directIO` section.
*   Test `OutputPointManager` init, map creation, command handling (direct relays), persistence, task creation, timer task logic.
*   Test `InputPointManager` init, map creation, persistence, task creation, periodic reading logic (checking stored values), value retrieval interface, mutex protection.
*   Verify correct hardware control/reading for direct I/O via the managers.
*   Verify commands for non-direct pointIds are ignored by `OutputPointManager`.
*   Verify retrieving values for non-direct pointIds returns default/error from `InputPointManager`.

---

## 6. Reboot Recovery Strategy (Step 2 Focus)

*   **Problem:** If the ESP32 reboots unexpectedly (power loss, crash) while direct relays are ON (especially during long timed activations like lighting), how does the system recover its state? FreeRTOS tasks and their states are lost on reboot.
*   **Step 2 Recovery Approach:**
    1.  **Initialization Default:** The `OutputPointManager::begin()` function initializes all direct relays to the OFF state by default (`setAllDirectRelaysOffPhysical()`) before any other logic runs. This ensures a safe starting state.
    2.  **No State Persistence (Yet):** In Step 2, we are *not* persisting the *runtime* state (ON/OFF) of the relays or the status of active cycles. The `/data/output_definitions/*.json` files only store the *configuration* (`assignedType`, `configValues`).
    3.  **Consequence:** Upon reboot, all direct relays will be turned OFF. Any timed activations that were in progress will be lost. The system will *not* automatically resume the state from before the reboot based solely on the Step 2 implementation.
*   **Future Steps (Recovery Enhancement):** True state recovery requires coordination with the Active Cycle logic (Step 3 and beyond):
    *   The `ActiveCycleConfiguration` (`/data/active_cycles/*.json`) stores the `cycleState` (e.g., "SAVED\_ACTIVE"), `currentStep`, and `stepStartDate`.
    *   On reboot, the `CycleManager` (implemented later) would need to:
        *   Load all Active Cycle configurations.
        *   Identify cycles that were in the "SAVED\_ACTIVE" state.
        *   Determine the correct state for associated output points based on the current time, the `stepStartDate`, the current step's schedule instance, and any relevant `autopilotWindows` or `associatedInputs`.
        *   Send appropriate initial commands (`TURN_ON`, `TURN_OFF`, or potentially `TURN_ON_TIMED` for the remaining duration) to the `OutputPointManager`'s queue to restore the expected operational state.
*   **Conclusion for Step 2:** The system starts safely with relays OFF. Full state recovery depends on higher-level cycle management logic to be implemented later.