# Step 3: Active Cycle Management (Backend Logic) - Detailed Plan (Revision 12)

## 1. Goal:

To implement the C++ `CycleManager` class and associated runtime logic responsible for:
*   Managing the lifecycle of Active Cycles: creation, loading, saving, deletion, state transitions (DRAFT, SAVED_DORMANT, SAVED_ACTIVE, PAUSED, COMPLETED, ERROR).
*   Persisting Active Cycle configurations (`/data/active_cycles/<cycleId>.json`), including the schedule sequence and associations to **one output point** and relevant input points.
*   Managing Schedule Instances (`/data/cycle_schedule_instances/<instanceId>.json`): Creating copies of Schedule Templates, calculating and storing `calculatedDuration` for volume events within instances (based on the single associated output), and deleting instances when cycles are removed.
*   Executing active cycles: Periodically checking active cycles, determining the current step and schedule instance, processing daily schedule events (Duration, Volume, Autopilot Windows) based on the current time using **edge triggering** to prevent duplicate activations, prioritizing Autopilot control over scheduled events when applicable and sensor status is valid.
*   Interacting with `OutputPointManager` to send control commands (`TURN_ON`, `TURN_OFF`, `TURN_ON_TIMED`) to the single associated output point.
*   Interacting with `InputPointManager` to retrieve processed values and status from associated input points for Autopilot control.
*   Implementing Autopilot control logic using parameters from the `autopilotWindows` in the schedule instance and sensor values identified via `associatedInputs`, including settling time logic.
*   Implementing reboot recovery logic specifically for "lighting" type outputs, ensuring others are OFF.

## 2. Prerequisites:

*   Step 1 (`Cycles_Step_1.md` - Rev 8) and Step 2 (`Cycles_Step_2.md` - Rev 3) plans finalized.
*   Functional `ConfigManager`, `OutputPointManager`, `InputPointManager` (including `getInputStatus`), `ScheduleManager` (including methods to load Schedule Templates and parse/save Schedule Instances).
*   C++ struct definitions from Step 1 available in headers (`CycleData.h`, `ScheduleData.h`, `OutputDefData.h`, etc.).
*   Reliable source for current time (`time_t`).
*   FreeRTOS environment.
*   LittleFS filesystem initialized.

## 3. Deliverables:

*   Implemented `CycleManager` class (`.h` and `.cpp`) containing:
    *   Methods for CRUD operations on Active Cycles.
    *   Methods for managing Schedule Instances.
    *   Methods for state transitions.
    *   Internal logic to manage loaded active cycles and Autopilot state (settling time).
*   A dedicated FreeRTOS task (`cycleProcessorTask`) responsible for periodically evaluating active cycles, incorporating edge-triggering logic for scheduled events.
*   Implementation of the core event processing logic (`processSingleCycle`, `processScheduleEvents`, `handleAutopilot`) reflecting the Autopilot priority, fallback logic, edge triggering, and settling time.
*   Implementation of the reboot recovery logic (lighting-specific).
*   Unit/integration tests.

## 4. Detailed Implementation Tasks & Pseudocode:

*(Note: Assumes C++ structs from Step 1 Rev 8 are defined. Requires access to instances of other managers.)*

### 4.1. `CycleManager` Class Definition

*   **Task:** Define the header file (`CycleManager.h`) with the `CycleManager` class structure.
*   **Header (`CycleManager.h` - Conceptual):**
    ```c++
    #include <map> // Needed for std::map
    #include <time.h> // Needed for time_t
    #include "CycleData.h"
    #include "ScheduleData.h"
    // Include other necessary manager headers & InputPointStatus enum

    class CycleManager {
    public:
        CycleManager(OutputPointManager& outMgr, InputPointManager& inMgr, ScheduleManager& schedMgr, ConfigManager& cfgMgr);
        bool begin();

        // CRUD & State Transitions...
        bool createActiveCycle(const ActiveCycleConfiguration& cycleConfig); // Ensure only one output in assoc.
        bool loadActiveCycle(const String& cycleId, ActiveCycleConfiguration& cycleConfig);
        bool saveActiveCycle(const ActiveCycleConfiguration& cycleConfig); // Ensure only one output in assoc.
        bool deleteActiveCycle(const String& cycleId);
        bool activateCycle(const String& cycleId);
        bool deactivateCycle(const String& cycleId);
        bool completeCycle(const String& cycleId);
        CycleState getCycleState(const String& cycleId);

    private:
        OutputPointManager& outputMgr;
        InputPointManager& inputMgr;
        ScheduleManager& scheduleMgr;
        ConfigManager& configMgr;

        std::map<String, ActiveCycleConfiguration> activeCycles;
        MutexHandle_t cycleMapMutex;
        TaskHandle_t cycleProcessorTaskHandle;

        // Map to store last autopilot activation time per cycle (Key: cycleId, Value: time_t timestamp)
        std::map<String, time_t> lastAutopilotActivationTime;

        // Schedule Instance Management
        bool createScheduleInstance(const String& libraryScheduleId, const String& cycleId, int step, String& newInstanceId);
        bool deleteScheduleInstancesForCycle(const String& cycleId);
        // Updated helper signature - takes single output ID
        float calculateVolumeDurationSec(float doseVolumeMl, const String& outputPointId);

        // Runtime Processing
        static void cycleProcessorTaskWrapper(void* parameter);
        void cycleProcessorTask();
        // Updated signature to accept minuteChanged flag
        void processSingleCycle(ActiveCycleConfiguration& cycle, time_t now, int currentMinuteOfDay, bool minuteChanged);
        // Updated signature to accept minuteChanged flag
        void processScheduleEvents(ActiveCycleConfiguration& cycle, const Schedule& scheduleInstance, int currentMinuteOfDay, const String& targetOutputId, bool minuteChanged);
        // Updated signature - takes window, single output ID, single input ID
        void handleAutopilot(ActiveCycleConfiguration& cycle, const Schedule& scheduleInstance, int currentMinuteOfDay, const AutopilotWindow& activeWindow, const String& targetOutputId, const String& controlInputId);
        // handleVerification removed

        // Reboot Recovery
        void performRebootRecovery();

        // Persistence & Helpers
        String getActiveCyclePath(const String& cycleId);
        String getScheduleInstancePath(const String& instanceId);
        String findControlInput(const std::vector<AssociatedInput>& inputs); // Helper for Autopilot strategy
        String findPrimaryOutput(const std::vector<AssociatedOutput>& outputs); // Helper
        // File system helpers assumed available
        // Helper for reboot recovery (simplified for lighting)
        void calculateExpectedLightingState(const Schedule& scheduleInstance, int currentMinuteOfDay, bool& expectedState);
        // Helper for step calculation
        int calculateCurrentStepIndex(const ActiveCycleConfiguration& cycle, time_t now);
        // Helper for time conversion
        time_t iso8601ToTimeT(const String& isoTimestamp);
        String timeTToIso8601(time_t timestamp);
    };
    ```

### 4.2. Persistence (`saveActiveCycle`, `loadActiveCycle`, `deleteActiveCycle`)

*   **Task:** Implement saving/loading using ArduinoJSON v7, ensuring `associatedOutputs` has max 1 element and `associatedInputs` is simplified.
*   **Pseudocode (`saveActiveCycle` - ArduinoJSON v7 - Updated):**
    ```c++
    bool CycleManager::saveActiveCycle(const ActiveCycleConfiguration& cycleConfig) {
      // Add validation: Ensure cycleConfig.associatedOutputs.size() <= 1
      if (cycleConfig.associatedOutputs.size() > 1) { /* Log error */ return false; }

      String path = getActiveCyclePath(cycleConfig.cycleId);
      ensureDirectoryExists(path);
      JsonDocument doc;
      doc["cycleId"] = cycleConfig.cycleId;
      doc["cycleName"] = cycleConfig.cycleName;
      doc["cycleState"] = cycleStateToString(cycleConfig.cycleState);
      doc["cycleStartDate"] = cycleConfig.cycleStartDate;
      doc["currentStep"] = cycleConfig.currentStep;
      doc["stepStartDate"] = cycleConfig.stepStartDate;

      JsonArray seq = doc.addArray("cycleSequence");
      if (!seq.isNull()) {
          for (const auto& step : cycleConfig.cycleSequence) {
              JsonObject stepObj = seq.addObject();
              if (!stepObj.isNull()) {
                  stepObj["step"] = step.step;
                  stepObj["scheduleInstanceId"] = step.scheduleInstanceId;
                  stepObj["libraryScheduleId"] = step.libraryScheduleId;
                  stepObj["durationDays"] = step.durationDays;
              }
          }
      } else { return false; }

      JsonArray outputs = doc.addArray("associatedOutputs");
      if (!outputs.isNull()) {
          if (!cycleConfig.associatedOutputs.empty()) {
              JsonObject outObj = outputs.addObject();
              if (!outObj.isNull()) {
                  outObj["pointId"] = cycleConfig.associatedOutputs[0].pointId;
                  outObj["role"] = cycleConfig.associatedOutputs[0].role;
              }
          }
      } else { return false; }

      JsonArray inputs = doc.addArray("associatedInputs");
      if (!inputs.isNull()) {
          for (const auto& in : cycleConfig.associatedInputs) {
              JsonObject inObj = inputs.addObject();
              if (!inObj.isNull()) {
                  inObj["pointId"] = in.pointId;
                  inObj["role"] = in.role;
                  // Strategy removed from storage here
              }
          }
      } else { return false; }

      return writeJsonDocumentToFile(path, doc);
    }
    ```
*   **Pseudocode (`loadActiveCycle` - ArduinoJSON v7 - Updated):**
    ```c++
    bool CycleManager::loadActiveCycle(const String& cycleId, ActiveCycleConfiguration& cycleConfig) {
      String path = getActiveCyclePath(cycleId);
      JsonDocument doc;
      if (!readFileToJsonDocument(path, doc)) { return false; }

      cycleConfig.cycleId = doc["cycleId"] | "";
      cycleConfig.cycleName = doc["cycleName"] | "";
      cycleConfig.cycleState = stringToCycleState(doc["cycleState"] | "");
      cycleConfig.cycleStartDate = doc["cycleStartDate"] | "";
      cycleConfig.currentStep = doc["currentStep"] | 0;
      cycleConfig.stepStartDate = doc["stepStartDate"] | "";

      cycleConfig.cycleSequence.clear();
      JsonArray seq = doc["cycleSequence"].as<JsonArray>();
      if (!seq.isNull()) { /* deserialize sequence */ }

      cycleConfig.associatedOutputs.clear();
      JsonArray outputs = doc["associatedOutputs"].as<JsonArray>();
      if (!outputs.isNull() && !outputs.empty()) {
          JsonObject outObj = outputs[0];
          if (!outObj.isNull()) {
              AssociatedOutput assocOut;
              assocOut.pointId = outObj["pointId"] | "";
              assocOut.role = outObj["role"] | "";
              if (!assocOut.pointId.isEmpty()) {
                  cycleConfig.associatedOutputs.push_back(assocOut);
              }
          }
      }

      cycleConfig.associatedInputs.clear();
      JsonArray inputs = doc["associatedInputs"].as<JsonArray>();
      if (!inputs.isNull()) {
          for (JsonObject inObj : inputs) {
              AssociatedInput assocIn;
              assocIn.pointId = inObj["pointId"] | "";
              assocIn.role = inObj["role"] | "";
              // Strategy removed from loading here
              if (!assocIn.pointId.isEmpty() && !assocIn.role.isEmpty()) {
                  cycleConfig.associatedInputs.push_back(assocIn);
              }
          }
      }
      return !cycleConfig.cycleId.isEmpty();
    }
    ```
*   **Pseudocode (`deleteActiveCycle`):**
    ```c++
    bool CycleManager::deleteActiveCycle(const String& cycleId) {
      deleteScheduleInstancesForCycle(cycleId); // Delete associated instances first
      // Also clear any state associated with this cycle
      lastAutopilotActivationTime.erase(cycleId);
      String path = getActiveCyclePath(cycleId);
      return LittleFS.remove(path); // Use LittleFS API
    }
    ```

### 4.3. Schedule Instance Management (`createScheduleInstance`, `deleteScheduleInstancesForCycle`)

*   **Task:** Update `createScheduleInstance` to use the single associated output for volume duration calculation.
*   **Pseudocode (`createScheduleInstance` - Updated Volume Calc):**
    ```c++
    bool CycleManager::createScheduleInstance(const String& libraryScheduleId, const String& cycleId, int step, String& newInstanceId) {
      Schedule libraryScheduleTemplate;
      if (!scheduleMgr.loadScheduleTemplate(libraryScheduleId, libraryScheduleTemplate)) { return false; }

      Schedule instanceSchedule = libraryScheduleTemplate;
      newInstanceId = "instance_" + libraryScheduleId + "_" + String(random(0xFFFF), HEX);
      instanceSchedule.scheduleName += " (Instance for " + cycleId + ")";

      ActiveCycleConfiguration cycleConfig;
      if (!loadActiveCycle(cycleId, cycleConfig)) { return false; }

      // Get the single associated output ID (if exists)
      String targetOutputId = "";
      if (!cycleConfig.associatedOutputs.empty()) {
          targetOutputId = cycleConfig.associatedOutputs[0].pointId;
      }

      for (VolumeEvent& volEvent : instanceSchedule.volumeEvents) {
          float durationSec = 0.0;
          if (!targetOutputId.isEmpty()) {
              // Pass the single output ID to the helper
              durationSec = calculateVolumeDurationSec(volEvent.doseVolume, targetOutputId);
          }
          volEvent.calculatedDurationSeconds = (unsigned long)ceil(durationSec);
          if (durationSec <= 0) { /* Log warning */ }
      }

      String instancePath = getScheduleInstancePath(newInstanceId);
      ensureDirectoryExists(instancePath);
      return scheduleMgr.saveScheduleInstance(instancePath, instanceSchedule);
    }

    // Helper updated to take single output point ID
    float CycleManager::calculateVolumeDurationSec(float doseVolumeMl, const String& outputPointId) {
        OutputPointDefinition outDef;
        OutputTypeInfo typeInfo;
        // Load definition and type info for the specific output point
        if (outputMgr.loadOutputPointDefinition(outputPointId, outDef) &&
            outputMgr.getOutputTypeInfo(outDef.assignedType, typeInfo) && // Assumes OutputMgr provides this
            typeInfo.supportsVolume)
        {
            JsonDocument configDoc;
            DeserializationError err = deserializeJson(configDoc, outDef.configValuesJson);
            if (err) { return 0.0; }
            float flowGPH = configDoc["flowRateGPH"] | 0.0;
            int emitters = configDoc["emittersPerPlant"] | 1;
            if (flowGPH > 0 && emitters > 0) {
                float flowMlPerSec = (flowGPH * 3785.41) / 3600.0; // Approx conversion
                float totalFlowRateMlPerSec = flowMlPerSec * emitters;
                if (totalFlowRateMlPerSec > 0) {
                    return doseVolumeMl / totalFlowRateMlPerSec;
                }
            }
        }
        return 0.0; // Cannot calculate
    }
    ```
*   **Pseudocode (`deleteScheduleInstancesForCycle`):**
    ```c++
    bool CycleManager::deleteScheduleInstancesForCycle(const String& cycleId) {
      ActiveCycleConfiguration cycleConfig;
      if (!loadActiveCycle(cycleId, cycleConfig)) { return false; } // Load the cycle to get instance IDs

      bool success = true;
      for (const auto& step : cycleConfig.cycleSequence) {
          String path = getScheduleInstancePath(step.scheduleInstanceId);
          if (LittleFS.exists(path)) { // Use LittleFS API
              if (!LittleFS.remove(path)) {
                  // Log error
                  success = false;
              }
          }
      }
      return success;
    }
    ```

### 4.4. Runtime Cycle Processing Task (`cycleProcessorTask`)

*   **Task:** Create a persistent FreeRTOS task that runs periodically (e.g., every second or minute) to check and advance active cycles. Implement Autopilot vs. Scheduled event priority logic and edge triggering for scheduled events.
*   **Pseudocode (`cycleProcessorTask` - Updated with Edge Triggering):**
    ```c++
    void CycleManager::cycleProcessorTask() {
      TickType_t xLastWakeTime = xTaskGetTickCount();
      const TickType_t xFrequency = pdMS_TO_TICKS(1000); // Check frequency (e.g., 1 second)
      int previousMinuteOfDay = -1; // Initialize outside the loop

      while(true) {
          vTaskDelayUntil(&xLastWakeTime, xFrequency);

          time_t now = time(nullptr); // Get current time
          struct tm* timeinfo = localtime(&now);
          // Handle potential null timeinfo if time is not set
          if (!timeinfo) {
              previousMinuteOfDay = -1; // Reset if time is lost
              continue;
          }
          int currentMinuteOfDay = timeinfo->tm_hour * 60 + timeinfo->tm_min;

          // Determine if the minute has changed since the last iteration
          bool minuteChanged = (currentMinuteOfDay != previousMinuteOfDay);

          // Lock map for safe iteration
          if (xSemaphoreTake(cycleMapMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
              // Iterate using indices or copy keys to avoid iterator invalidation on erase
              std::vector<String> cycleIdsToProcess;
              for(auto const& [key, val] : activeCycles) {
                  cycleIdsToProcess.push_back(key);
              }

              for (const String& cycleId : cycleIdsToProcess) {
                  // Re-check if cycle still exists in map after potential erase
                  auto it = activeCycles.find(cycleId);
                  if (it == activeCycles.end()) continue;

                  ActiveCycleConfiguration& cycle = it->second; // Get reference

                  if (cycle.cycleState == CycleState::SAVED_ACTIVE) {
                      // Pass the minuteChanged flag to processSingleCycle
                      processSingleCycle(cycle, now, currentMinuteOfDay, minuteChanged);
                  }

                  // Check if processSingleCycle changed state (re-find iterator)
                  it = activeCycles.find(cycleId); // Find again in case processSingleCycle modified map
                  if (it != activeCycles.end()) {
                     if (it->second.cycleState == CycleState::COMPLETED || it->second.cycleState == CycleState::ERROR) {
                         // Clear state when cycle completes or errors
                         lastAutopilotActivationTime.erase(cycleId);
                         activeCycles.erase(it); // Erase from map if done/error
                     }
                  }
              }
              xSemaphoreGive(cycleMapMutex);
          } else { /* Log mutex timeout */ }

          // Update previousMinuteOfDay for the next iteration *after* processing all cycles
          previousMinuteOfDay = currentMinuteOfDay;
      }
    }
    ```
*   **Pseudocode (`processSingleCycle` - Updated Signature & Logic):**
    ```c++
    // Updated signature to accept minuteChanged flag
    void CycleManager::processSingleCycle(ActiveCycleConfiguration& cycle, time_t now, int currentMinuteOfDay, bool minuteChanged) {
      // 1. Check step completion & advance if needed
      time_t stepStartTime = iso8601ToTimeT(cycle.stepStartDate); // Helper needed
      int currentStepIndex = cycle.currentStep - 1;
      if (currentStepIndex < 0 || currentStepIndex >= cycle.cycleSequence.size()) {
          cycle.cycleState = CycleState::ERROR; // Set error state
          saveActiveCycle(cycle); // Persist error state
          // Log error: Invalid current step index
          return;
      }
      int stepDurationDays = cycle.cycleSequence[currentStepIndex].durationDays;
      if (now >= stepStartTime + (stepDurationDays * 86400L)) {
          if (cycle.currentStep >= cycle.cycleSequence.size()) {
              completeCycle(cycle.cycleId); // Mark as completed (updates state in map & file)
              return; // Stop processing this cycle for this tick
          } else {
              // Advance to next step
              cycle.currentStep++;
              cycle.stepStartDate = timeTToIso8601(now); // Start next step now
              saveActiveCycle(cycle); // Persist step change
              currentStepIndex = cycle.currentStep - 1; // Update index for event processing below
          }
      }

      // 2. Load current schedule instance
      const String& instanceId = cycle.cycleSequence[currentStepIndex].scheduleInstanceId;
      Schedule scheduleInstance;
      if (!scheduleMgr.loadScheduleInstance(getScheduleInstancePath(instanceId), scheduleInstance)) {
          cycle.cycleState = CycleState::ERROR; // Set error state
          saveActiveCycle(cycle); // Persist error state
          // Log error: Failed to load schedule instance
          return;
      }

      // 3. Determine Control Logic Path
      bool runAutopilot = false;
      bool runScheduledEvents = true; // Default: run scheduled events
      const AutopilotWindow* activeWindow = nullptr; // Store the active window if found
      String controlInputId = ""; // Store the controlling input ID if found

      // Find the single associated output (if any)
      String primaryOutputId = findPrimaryOutput(cycle.associatedOutputs); // Use helper

      if (!primaryOutputId.isEmpty()) {
          OutputPointDefinition outDef;
          OutputTypeInfo typeInfo;
          // Check if this output supports Autopilot
          if (outputMgr.loadOutputPointDefinition(primaryOutputId, outDef) &&
              configMgr.getOutputTypeInfo(outDef.assignedType, typeInfo) && // Use ConfigManager
              typeInfo.supportsAutopilotInput)
          {
              // Check if currently within any autopilot window defined in the schedule instance
              for (const auto& window : scheduleInstance.autopilotWindows) {
                  if (currentMinuteOfDay >= window.startTime && currentMinuteOfDay < window.endTime) {
                      activeWindow = &window;
                      break;
                  }
              }

              if (activeWindow) {
                  // Find the associated AUTOPILOT_CONTROL input(s) for this cycle
                  controlInputId = findControlInput(cycle.associatedInputs); // Helper needed
                  InputPointStatus sensorStatus = InputPointStatus::ERROR; // Default to error
                  if (!controlInputId.isEmpty()) {
                      sensorStatus = inputMgr.getInputStatus(controlInputId); // Need this method
                  } else {
                      // Log warning: Autopilot window active but no control input associated
                  }

                  if (sensorStatus == InputPointStatus::OK) {
                      // Sensor OK: Autopilot takes precedence
                      runAutopilot = true;
                      runScheduledEvents = false; // Do not run scheduled Duration/Volume events for the primary output
                  } else {
                      // Sensor Failed: Fallback to scheduled events
                      runAutopilot = false;
                      runScheduledEvents = true;
                      // Log warning: Autopilot sensor failed for cycle X, input Y, using schedule backup.
                  }
              } else {
                  // Not in an autopilot window: Run scheduled events
                  runAutopilot = false;
                  runScheduledEvents = true;
              }
          } else {
              // Output doesn't support autopilot: Run scheduled events
              runAutopilot = false;
              runScheduledEvents = true;
          }
      } else {
          // No output associated: Cannot run schedule events or autopilot involving output
          runAutopilot = false;
          runScheduledEvents = false;
      }

      // 4. Execute Control Logic
      if (runAutopilot && activeWindow != nullptr && !controlInputId.isEmpty()) {
          // Autopilot logic is independent of minute change for triggering
          handleAutopilot(cycle, scheduleInstance, currentMinuteOfDay, *activeWindow, primaryOutputId, controlInputId);
      }
      if (runScheduledEvents) {
          // Process normal schedule events (Duration, Volume, Lights)
          // Pass the primaryOutputId and minuteChanged flag for context
          processScheduleEvents(cycle, scheduleInstance, currentMinuteOfDay, primaryOutputId, minuteChanged);
      }

      // 5. Handle State Verification (REMOVED)
    }
    ```
*   **Pseudocode (`processScheduleEvents` - Updated with Edge Triggering):**
    ```c++
    // Updated signature to accept minuteChanged flag
    void CycleManager::processScheduleEvents(ActiveCycleConfiguration& cycle, const Schedule& scheduleInstance, int currentMinuteOfDay, const String& targetOutputId, bool minuteChanged) {
      if (targetOutputId.isEmpty()) return; // Cannot process output events without a target

      // Load output definition once for type checking
      OutputPointDefinition outDef;
      OutputTypeInfo typeInfo; // Needed for volume check later
      if (!outputMgr.loadOutputPointDefinition(targetOutputId, outDef)) {
          // Log error: Failed to load output definition for targetOutputId
          return;
      }

      // --- Only check start times if the minute has just changed ---
      if (minuteChanged) {
          // Process Duration and Volume events ONLY if the output is a solenoid valve
          if (outDef.assignedType == "solenoid_valve") {
              // Check Duration Events
              for (const auto& event : scheduleInstance.durationEvents) {
                  if (event.startTime == currentMinuteOfDay) {
                      OutputCommand cmd = {targetOutputId, RelayCommandType::TURN_ON_TIMED, (unsigned long)event.duration * 1000};
                      outputMgr.sendCommand(cmd);
                      // Log debug: Triggered duration event for cycle X, output Y at minute Z
                  }
              }
              // Check Volume Events
              if (outputMgr.getOutputTypeInfo(outDef.assignedType, typeInfo) && typeInfo.supportsVolume)
              {
                  for (const auto& event : scheduleInstance.volumeEvents) {
                      if (event.startTime == currentMinuteOfDay) {
                          // Check if flow rate/emitters are valid in outDef.configValues
                          JsonDocument configDoc;
                          DeserializationError err = deserializeJson(configDoc, outDef.configValuesJson);
                          float flowGPH = configDoc["flowRateGPH"] | 0.0;
                          int emitters = configDoc["emittersPerPlant"] | 1;

                          if (!err && flowGPH > 0 && emitters > 0 && event.calculatedDurationSeconds > 0) {
                             OutputCommand cmd = {targetOutputId, RelayCommandType::TURN_ON_TIMED, (unsigned long)event.calculatedDurationSeconds * 1000};
                             outputMgr.sendCommand(cmd);
                             // Log debug: Triggered volume event for cycle X, output Y at minute Z
                          } else {
                              // Log warning: Skipping volume event for output X due to invalid config or zero duration.
                          }
                      }
                  }
              } else {
                  // Log warning: Skipping volume events for output X as it doesn't support volume (or type info failed).
              }
          } // End of solenoid_valve specific events

          // Check Lights ON/OFF (Independent of solenoid check)
          if (outDef.assignedType == "lighting") {
              if (currentMinuteOfDay == scheduleInstance.lightsOnTime) {
                  OutputCommand cmd = {targetOutputId, RelayCommandType::TURN_ON, 0};
                  outputMgr.sendCommand(cmd);
                  // Log debug: Triggered lights ON for cycle X, output Y at minute Z
              } else if (currentMinuteOfDay == scheduleInstance.lightsOffTime) {
                  OutputCommand cmd = {targetOutputId, RelayCommandType::TURN_OFF, 0};
                  outputMgr.sendCommand(cmd);
                  // Log debug: Triggered lights OFF for cycle X, output Y at minute Z
              }
          }
      } // --- End of minuteChanged check ---
    }
    ```

### 4.5. Autopilot Logic (`handleAutopilot`)

*   **Task:** Update `handleAutopilot` to use parameters from the `activeWindow`, implement `settlingTime` logic, and handle `doseDuration` or `doseVolume` with validity checks.
*   **Pseudocode (`handleAutopilot` - Updated with Settling Time & No Hysteresis):**
    ```c++
    // Assumes it's only called when within the window and sensor is OK
    void CycleManager::handleAutopilot(ActiveCycleConfiguration& cycle, const Schedule& scheduleInstance, int currentMinuteOfDay, const AutopilotWindow& activeWindow, const String& targetOutputId, const String& controlInputId) {

      // --- Settling Time Check ---
      time_t now = time(nullptr); // Get current absolute time
      auto it = lastAutopilotActivationTime.find(cycle.cycleId);
      if (it != lastAutopilotActivationTime.end()) { // Check if there was a previous activation for this cycle
          time_t lastActivation = it->second;
          // Convert settlingTime from minutes to seconds
          unsigned long settlingSeconds = (unsigned long)activeWindow.settlingTime * 60;
          if (now < lastActivation + settlingSeconds) {
              // Still within settling time, do nothing for this cycle's autopilot this tick
              // Log debug: Cycle X autopilot skipped due to settling time.
              return;
          }
      }
      // --- End Settling Time Check ---

      float currentValue = inputMgr.getCurrentValue(controlInputId);
      // Add proper error check for currentValue if needed
      if (/* value indicates error */) {
          // Log error: Invalid sensor reading for controlInputId
          return;
      }

      // Use threshold from the activeWindow in the schedule instance
      float threshold = activeWindow.matricTension;
      // Hysteresis logic removed

      bool isCurrentlyOn = outputMgr.getCurrentState(targetOutputId);

      // Determine desired state based ONLY on threshold
      bool shouldBeOn = (currentValue <= threshold);

      // Only trigger if it should be ON and is currently OFF
      if (shouldBeOn && !isCurrentlyOn) {
          // Settling time check moved to the beginning

          unsigned long durationMs = 0;
          if (activeWindow.doseDuration > 0) {
              // Use direct duration from autopilot window
              durationMs = (unsigned long)activeWindow.doseDuration * 1000;
          } else if (activeWindow.doseVolume > 0) {
              // Calculate duration from volume, checking output config validity
              OutputPointDefinition outDef;
              OutputTypeInfo typeInfo;
              // Check if the target output supports volume and has valid config
              if (outputMgr.loadOutputPointDefinition(targetOutputId, outDef) &&
                  outputMgr.getOutputTypeInfo(outDef.assignedType, typeInfo) &&
                  typeInfo.supportsVolume)
              {
                  JsonDocument configDoc;
                  DeserializationError err = deserializeJson(configDoc, outDef.configValuesJson);
                  float flowGPH = configDoc["flowRateGPH"] | 0.0;
                  int emitters = configDoc["emittersPerPlant"] | 1;
                  if (!err && flowGPH > 0 && emitters > 0) {
                      float flowMlPerSec = (flowGPH * 3785.41) / 3600.0;
                      float totalFlowRateMlPerSec = flowMlPerSec * emitters;
                      if (totalFlowRateMlPerSec > 0) {
                          durationMs = (unsigned long)ceil(activeWindow.doseVolume / totalFlowRateMlPerSec) * 1000;
                      }
                  }
              }
              if (durationMs == 0) {
                  // Log warning: Cannot run volume dose for Autopilot, invalid output config or zero flow.
                  return; // Do not send command
              }
          }

          if (durationMs > 0) {
              OutputCommand cmd = {targetOutputId, RelayCommandType::TURN_ON_TIMED, durationMs};
              if (outputMgr.sendCommand(cmd)) { // Check if command was accepted
                 // Record the time of this successful activation using 'now' from the start of the function
                 lastAutopilotActivationTime[cycle.cycleId] = now;
                 // Log debug: Autopilot activated for cycle X, duration Y ms.
              } else {
                 // Log error: Failed to send Autopilot command for cycle X.
              }
          } else {
              // Log warning: Autopilot triggered for cycle X but dose duration/volume resulted in 0ms.
          }
      }
      // Note: Still assumes Autopilot only triggers ON doses.
    }
    ```

### 4.6. Reboot Recovery (`performRebootRecovery`)

*   **Task:** Implement recovery logic to restore state ONLY for "lighting" outputs based on their schedule. All other output types associated with active cycles must be turned OFF.
*   **Pseudocode (`performRebootRecovery` - Lighting Specific):**
    ```c++
    void CycleManager::performRebootRecovery() {
      // Clear any stale autopilot state on reboot
      lastAutopilotActivationTime.clear();

      // Load all active cycle configurations from /data/active_cycles/ into activeCycles map
      // ... (Load files into activeCycles map) ...

      time_t now = time(nullptr);
      int currentMinuteOfDay = ...; // Calculate from 'now'

      for (auto& pair : activeCycles) {
          ActiveCycleConfiguration& cycle = pair.second;
          if (cycle.cycleState != CycleState::SAVED_ACTIVE) { continue; }
          if (cycle.associatedOutputs.empty()) { continue; } // Skip if no output associated

          String targetOutputId = cycle.associatedOutputs[0].pointId; // Only one output

          // Load Output Definition to check type
          OutputPointDefinition outDef;
          if (!outputMgr.loadOutputPointDefinition(targetOutputId, outDef)) {
              // Log error: Failed to load output definition during recovery for targetOutputId
              continue;
          }

          // --- Logic based on Output Type ---
          if (outDef.assignedType == "lighting") {
              // Lighting type: Restore state based on schedule

              // Determine current step index based on cycle/step start dates and durations
              int currentStepIndex = calculateCurrentStepIndex(cycle, now);
              if (currentStepIndex < 0) { /* Cycle completed or error */ continue; }
              cycle.currentStep = currentStepIndex + 1; // Update in-memory state

              // Load the schedule instance for the current step
              const String& instanceId = cycle.cycleSequence[currentStepIndex].scheduleInstanceId;
              Schedule scheduleInstance;
              if (!scheduleMgr.loadScheduleInstance(getScheduleInstancePath(instanceId), scheduleInstance)) {
                  // Log error: Failed to load schedule instance during recovery
                  continue;
              }

              // Determine expected state based ONLY on schedule events active *right now*
              bool expectedState = false;
              // Simplified helper needed: just checks lightsOnTime/lightsOffTime vs currentMinuteOfDay
              calculateExpectedLightingState(scheduleInstance, currentMinuteOfDay, expectedState);

              // Send command to restore state
              if (expectedState) {
                  OutputCommand cmd = {targetOutputId, RelayCommandType::TURN_ON, 0};
                  outputMgr.sendCommand(cmd);
              } else {
                  OutputCommand cmd = {targetOutputId, RelayCommandType::TURN_OFF, 0};
                  outputMgr.sendCommand(cmd);
              }
          } else {
              // NOT Lighting type: Ensure it is turned OFF
              // Log debug: Ensuring non-lighting output targetOutputId is OFF after reboot.
              OutputCommand cmd = {targetOutputId, RelayCommandType::TURN_OFF, 0};
              outputMgr.sendCommand(cmd);
          }
      }
    }
    ```

## 5. Testing Considerations:

*   Test Active Cycle CRUD ensuring only one output can be associated.
*   Test volume duration calculation with single output config.
*   Test runtime processing:
    *   **Scheduled Event Edge Triggering:** Verify that Duration, Volume, and Lighting events trigger exactly once when the clock transitions into the scheduled minute, even if the processor task runs multiple times within that minute.
    *   Autopilot runs when sensor OK & in window (using schedule params) **and settling time has elapsed**.
    *   Autopilot is skipped if sensor OK & in window but **settling time has NOT elapsed**.
    *   Scheduled events run when outside window OR sensor fails in window.
    *   Volume/Duration events target the single associated output **and only if it's a solenoid_valve**.
    *   Volume/Autopilot Volume events are skipped if output config is invalid.
    *   Lighting events run only if output type is "lighting".
*   Test reboot recovery:
    *   Ensure "lighting" outputs restore their correct ON/OFF state based on the schedule at the time of reboot.
    *   Ensure all other output types associated with active cycles are explicitly turned OFF on reboot.
    *   Autopilot state (settling time) should not resume after reboot.
*   Test settling time logic across midnight boundaries (should work due to `time_t`).

---