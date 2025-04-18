# Step 1: Data Structure &amp; JSON Format Refinement - Detailed Plan (Revision 9 - Final)

## 1. Goal:

To definitively establish the structure, field names, data types, and constraints for all JSON configuration/data files and their corresponding C++ data structures (structs and enums) used in the Schedule &amp; Cycle Management System. This finalized definition adopts a decoupled architecture:
*   **Hardware Definition:** `board_config.json` defines physical I/O and maps to system `pointId`s. Modbus devices are defined via profiles.
*   **Output Point Definition:** `/data/output_definitions/*.json` configures the function (`assignedType`) and parameters (`configValues`) for each output `pointId`.
*   **Input Point Definition:** `/data/input_configs/*.json` provides detailed configuration (scaling, calibration, alarms) for each input `pointId`.
*   **Schedule Template Definition:** `/data/daily_schedules/*.json` defines reusable daily event sequences, including `autopilotWindows` which contain control parameters (e.g., thresholds).
*   **Active Cycle Configuration:** `/data/active_cycles/*.json` defines a specific running or planned cycle, including its schedule sequence and *associations* with pre-configured input and output points, specifying only their `pointId` and `role` within that cycle.
*   **Schedule Instances:** `/data/cycle_schedule_instances/*.json` stores copies of Schedule Templates used by active cycles, including pre-calculated durations for volume events.
*   **Cycle Templates:** `/data/cycle_templates/*.json` stores reusable cycle sequence definitions (referencing Schedule Templates).
This ensures consistency across the backend (C++), frontend (JavaScript), and data storage (LittleFS).

## 2. Deliverables:

*   Finalized examples of each JSON file format (as detailed below).
*   Finalized C++ header file content (conceptualized below, e.g., `include/IOConfig.h`, `include/ModbusData.h`, `include/OutputTypeData.h`, `include/InputConfigData.h`, `include/OutputDefData.h`, `include/CycleData.h`, `include/ScheduleData.h`) defining the corresponding structs and enums.
*   This document outlining the finalized structures and key considerations.

## 3. Detailed Structure Definitions:

### 3.1. `board_config.json` (Final Structure)

*   **Purpose:** Defines the complete Input/Output (I/O) capabilities of the system, mapping physical hardware/protocols to unique system-level `pointId`s.
*   **Location:** `/data/board_config.json`
*   **JSON Example:**
    ```json
    {
      "boardName": "SNRv5 Advanced",
      "directIO": {
        "relayOutputs": {
          "count": 8,
          "controlMethod": "ShiftRegister",
          "pins": { "data": 23, "clock": 18, "latch": 5 },
          "pointIdPrefix": "DirectRelay_",
          "pointIdStartIndex": 0
        },
        "digitalInputs": {
          "count": 4,
          "pins": [34, 35, 36, 39],
          "pointIdPrefix": "DirectDI_",
          "pointIdStartIndex": 0
        },
        "analogInputs": [
          {
            "type": "0-5V",
            "count": 2,
            "resolutionBits": 12,
            "pins": [32, 33],
            "pointIdPrefix": "DirectAI_0-5V_",
            "pointIdStartIndex": 0
          }
        ],
        "analogOutputs": [
           {
             "type": "0-5V_DAC",
             "count": 1,
             "resolutionBits": 8,
             "pins": [25],
             "pointIdPrefix": "DirectAO_DAC_",
             "pointIdStartIndex": 0
           }
        ]
      },
      "modbusInterfaces": [
        {
          "interfaceId": "RS485_1",
          "uartPort": 2,
          "baudRate": 9600,
          "config": "SERIAL_8N1",
          "txPin": 17,
          "rxPin": 16,
          "rtsPin": 4
        }
      ],
      "modbusDevices": [
        {
          "deviceId": "TankSensor_A",
          "profileId": "xyz_level_sensor",
          "interfaceId": "RS485_1",
          "slaveAddress": 10,
          "pollingIntervalMs": 5000,
          "enabled": true,
          "overrideDescription": "Tank A Level Sensor"
        },
        {
          "deviceId": "MultiRelay_B",
          "profileId": "generic_4ch_relay",
          "interfaceId": "RS485_1",
          "slaveAddress": 11,
          "pollingIntervalMs": 10000,
          "enabled": true
        }
      ]
    }
    ```
*   **Key Fields & Concepts:** Defines physical layer and assigns unique `pointId` to each I/O point. `directIO` uses prefix/index to generate `pointId`s. `modbusDevices` links instances to profiles and communication parameters; final `pointId`s are generated dynamically (e.g., `deviceId` + `pointIdSuffix`).

### 3.2. Modbus Device Profile JSON (`/data/modbus_profiles/*.json`)

*   **Purpose:** Contains the reusable definition of Modbus points for a specific *type* of Modbus device.
*   **Location Example:** `/data/modbus_profiles/xyz_level_sensor.json`
*   **JSON Example:**
    ```json
    {
      "profileId": "xyz_level_sensor",
      "model": "XYZ Level Sensor v1.2",
      "manufacturer": "SensorCorp",
      "description": "Standard level sensor with pump control.",
      "points": [
        {
          "pointIdSuffix": "_Level",
          "ioType": "AI",
          "description": "Tank Level (%)",
          "modbus": {
            "registerType": "HoldingRegister",
            "address": 40001,
            "dataType": "UINT16",
            "scaleFactor": 0.1,
            "offset": 0.0,
            "units": "%"
          },
          "readOnly": true
        },
        {
          "pointIdSuffix": "_Pump",
          "ioType": "DO",
          "description": "Pump Enable",
          "modbus": {
            "registerType": "Coil",
            "address": 1,
            "dataType": "BOOLEAN"
          },
          "readOnly": false
        }
      ]
    }
    ```
*   **Key Fields & Concepts:** Template for Modbus device points. `pointIdSuffix` is combined with instance `deviceId` for the system `pointId`.

### 3.3. `relay_types.json` (Final Structure - Renamed Conceptually: Output Function Types)

*   **Purpose:** Defines the available logical functions (e.g., valve, pump, light) that a system output point (`pointId` of type DO) can be assigned. Specifies capabilities, essential parameters, and reboot behavior for that function.
*   **Location:** `/data/relay_types.json` (Consider renaming to `/data/output_types.json`)
*   **JSON Example:**
    ```json
    [
      {
        "typeId": "unconfigured",
        "displayName": "Unconfigured",
        "description": "Output point is not assigned a function.",
        "supportsVolume": false,
        "supportsAutopilotInput": false,
        "supportsVerificationInput": false,
        "configParams": [
           { "id": "name", "label": "Output Name", "type": "text", "required": false, "default": "Output X" }
        ],
        "resumeStateOnReboot": false
      },
      {
        "typeId": "solenoid_valve",
        "displayName": "Solenoid Valve",
        "description": "Controls fluid flow. Can use Duration, Volume, or Autopilot (linked input).",
        "supportsVolume": true,
        "supportsAutopilotInput": true,
        "supportsVerificationInput": false,
        "resumeStateOnReboot": false,
        "configParams": [
          { "id": "name", "label": "Valve Name", "type": "text", "required": true },
          { "id": "flowRateGPH", "label": "Flow Rate (GPH)", "type": "number", "required": false, "min": 0.01, "step": 0.01 },
          { "id": "flowRateLPH", "label": "Flow Rate (LPH)", "type": "number", "readonly": true },
          { "id": "emittersPerPlant", "label": "Emitters per Plant", "type": "number", "required": false, "min": 1, "default": 1, "step": 1 }
        ]
      },
      {
        "typeId": "pump",
        "displayName": "Pump",
        "description": "Controls a pump. Can use Duration or Autopilot (linked input).",
        "supportsVolume": false,
        "supportsAutopilotInput": true,
        "supportsVerificationInput": true,
        "resumeStateOnReboot": false,
        "configParams": [
          { "id": "name", "label": "Pump Name", "type": "text", "required": true }
        ]
      },
      {
        "typeId": "lighting",
        "displayName": "Lighting",
        "description": "Simple ON/OFF control for lights. Can use Autopilot (time-based) or Verification (linked input).",
        "supportsVolume": false,
        "supportsAutopilotInput": false,
        "supportsVerificationInput": true,
        "resumeStateOnReboot": true,
        "configParams": [
          { "id": "name", "label": "Light Name", "type": "text", "required": true },
          { "id": "wattage", "label": "Wattage (W)", "type": "number", "required": false, "min": 1 }
        ]
      }
    ]
    ```
*   **Key Fields & Concepts:** Defines *what an output can do* logically, its required parameters, and reboot behavior.

### 3.4. Output Point Definition (`/data/output_definitions/<pointId_sanitized>.json`) (New File Type)

*   **Purpose:** Configures a specific system output point (`pointId`) by assigning it a function type (from `relay_types.json`) and providing the necessary parameter values for that function. Decoupled from Active Cycles.
*   **Location:** `/data/output_definitions/`
*   **Filename:** Based on the system `pointId`, sanitized.
*   **JSON Example (`/data/output_definitions/DirectRelay_0.json`):**
    ```json
    {
      "pointId": "DirectRelay_0",
      "assignedType": "solenoid_valve",
      "configValues": {
        "name": "Zone 1 Veg Valve",
        "flowRateGPH": 5.0,
        "emittersPerPlant": 4
      }
    }
    ```
*   **Key Fields & Concepts:** Links `pointId` to `assignedType` and stores `configValues`. Defines *what the output point is*.

### 3.5. Input Point Configuration (`/data/input_configs/<pointId_sanitized>.json`)

*   **Purpose:** Stores the detailed configuration for a single *system input point* (`pointId`).
*   **Location:** `/data/input_configs/`
*   **Filename:** Based on the system `pointId`, sanitized.
*   **JSON Example (`/data/input_configs/Tensiometer_Z1.json`):**
    ```json
    {
      "pointId": "Tensiometer_Z1",
      "inputConfig": {
        "type": "tensiometer",
        "subtype": "vacuum_pressure",
        "name": "Veg Tensiometer Zone 1",
        "manufacturer": "Titan 900",
        "model": "Lyon Leaf",
        "unit": "kPa",
        "input_scaling": {
          "reference_type": "dc_voltage",
          "offset": 0.0,
          "multiplier": 1.0,
          "divisor": 1.0,
          "integration_control": "integrated",
          "input_range": { "min_voltage": 1.020561, "max_voltage": 2.99900 },
          "output_range": { "min_pressure": 0.0, "max_pressure": 30.0 },
          "display_unit": "kPa"
        },
        "calibration": {
          "enabled": true,
          "data_points": [
            { "voltage": 1.020561, "pressure": 0.0, "timestamp": "2023-10-01T12:00:00Z" },
            { "voltage": 2.99900, "pressure": 30.0, "timestamp": "2023-10-01T12:05:00Z" }
          ],
          "custom_points": [
            { "voltage": 1.5, "pressure": 10.0, "timestamp": "2023-10-01T12:10:00Z", "notes": "User-defined midpoint" }
          ]
        },
        "temperature_compensation": {
          "enabled": false,
          "source_pointId": "TempSensor_Room1",
          "center_point": 25.0,
          "slope": 0.147,
          "offset": 0.0,
          "update_interval_minutes": 15
        },
        "alarms": {
          "enabled": true,
          "low_limit": -3.0,
          "high_limit": 30.0,
          "delay_time_minutes": 1,
          "priority": "silent"
        }
      }
    }
    ```
*   **Key Fields & Concepts:** Defines *what the input point is* and how to interpret its raw data (scaling, calibration, etc.). Runtime state is excluded.

### 3.6. Schedule Template Definition (`/data/daily_schedules/*.json`)

*   **Purpose:** Defines a reusable template for a single day's events. **Renamed from "Daily Schedule" for clarity.**
*   **Location:** `/data/daily_schedules/`
*   **Filename:** Based on `scheduleUID`.
*   **JSON Example:**
    ```json
    {
      "scheduleName": "My Grow Schedule Template", // Name reflects it's a template
      "lightsOnTime": 360,
      "lightsOffTime": 1080,
      "scheduleUID": "sched_template_1678886400", // Unique template ID
      "autopilotWindows": [
        {
          "startTime": 600,
          "endTime": 960,
          "matricTension": 15.5, // Control parameter defined here
          "doseVolume": 250,
          "settlingTime": 30,
          "doseDuration": 0 // Added based on struct
        }
      ],
      "durationEvents": [
        { "startTime": 480, "duration": 120, "endTime": 482 }
      ],
      "volumeEvents": [
        { "startTime": 720, "doseVolume": 150 }
        // Note: calculatedDuration is NOT stored here
      ]
    }
    ```
*   **Key Fields & Concepts:** Defines *when* actions should occur and the *parameters* for those actions (like `matricTension`). This is the reusable library item.

### 3.7. Active Cycle Configuration (`/data/active_cycles/<cycleId>.json`) (Refactored - Simplified Associations)

*   **Purpose:** Defines a specific, potentially running, growing cycle. It contains metadata, the sequence of schedule *instances* to execute, and *associates* pre-configured output and input points by `pointId`, defining only their `role` within this specific cycle.
*   **Location:** `/data/active_cycles/`
*   **Filename:** Based on a unique `cycleId`.
*   **JSON Example (`/data/active_cycles/VegZone1Cycle.json`):**
    ```json
    {
      "cycleId": "VegZone1Cycle",
      "cycleName": "Vegetative Cycle Zone 1",
      "cycleState": "SAVED_ACTIVE",
      "cycleStartDate": "2025-04-08T00:00:00Z",
      "currentStep": 1,
      "stepStartDate": "2025-04-08T00:00:00Z",
      "cycleSequence": [
        {
          "step": 1,
          "scheduleInstanceId": "instance_mygrow_xyz", // Link to the specific instance file used
          "libraryScheduleId": "sched_template_1678886400", // Original template UID
          "durationDays": 14
        }
      ],
      "associatedOutputs": [ // Links OUTPUT pointIds to this cycle
        {
          "pointId": "DirectRelay_0", // Links to /data/output_definitions/DirectRelay_0.json
          "role": "primary_actuator" // Role within this cycle
        },
        {
          "pointId": "MultiRelay_B_Relay1",
          "role": "nutrient_pump_A"
        }
      ],
      "associatedInputs": [ // Links INPUT pointIds to this cycle
        {
          "pointId": "Tensiometer_Z1", // Links to /data/input_configs/Tensiometer_Z1.json
          "role": "AUTOPILOT_CONTROL" // How this input is used
          // Strategy (primary/secondary/average) removed - handled by logic if multiple inputs share a role
        },
        {
          "pointId": "Tensiometer_Z1_Backup",
          "role": "AUTOPILOT_CONTROL"
        },
        {
          "pointId": "LightSensor_Room1",
          "role": "STATE_VERIFICATION"
        }
        // Note: Control parameters like thresholds/hysteresis are NOT stored here.
        // They are defined within the Schedule Instance's autopilotWindows.
      ]
    }
    ```
*   **Key Fields & Concepts:** Defines a running cycle instance. Links a schedule sequence (`cycleSequence` using `scheduleInstanceId`) with specific, pre-configured I/O points (`associatedOutputs`, `associatedInputs`), defining only their `role` for this specific cycle. Input strategy (primary/secondary/average) needs to be handled implicitly by the logic processing inputs with the same role. Control parameters (thresholds) come from the Schedule Instance.

### 3.8. Schedule Instance Files (`/data/cycle_schedule_instances/*.json`) (Updated)

*   **Purpose:** Stores a specific *copy* of a Schedule Template used within an active cycle step. This copy includes pre-calculated durations for volume events based on the associated output configuration at the time of instance creation/update. It inherits control parameters (like `matricTension`) from the template it was copied from, but could potentially be modified later for this instance only.
*   **Location:** `/data/cycle_schedule_instances/`
*   **Filename:** Based on the `scheduleInstanceId`.
*   **JSON Structure:** Based on the Schedule Template JSON structure (Section 3.6), with an added field in `volumeEvents`:
    ```json
    {
      "scheduleName": "My Grow Schedule Template (Instance for VegZone1Cycle)", // Instance name
      "lightsOnTime": 360,
      "lightsOffTime": 1080,
      "scheduleUID": "sched_template_1678886400", // Original Template UID
      "autopilotWindows": [
         {
          "startTime": 600,
          "endTime": 960,
          "matricTension": 15.5, // Inherited/copied parameter
          "doseVolume": 250,
          "settlingTime": 30,
          "doseDuration": 0
        }
      ],
      "durationEvents": [ /* ... */ ],
      "volumeEvents": [
        {
          "startTime": 720,
          "doseVolume": 150,
          "calculatedDuration": 15 // Added: Duration in seconds calculated from volume/flow/emitters
        }
      ]
    }
    ```
    *   `calculatedDuration` (Integer, Required within Volume Events in Instances): The duration in seconds required to dispense the `doseVolume`, calculated using the flow rate and emitter count from the associated output point's definition when the instance is saved or updated within an active cycle context.

### 3.9. `cycle_templates/*.json`

*   **Purpose:** Defines a reusable template for a cycle *sequence*, referencing Schedule Template UIDs.
*   **Location:** `/data/cycle_templates/`
*   **JSON Structure:**
    ```json
    {
        "templateId": "standard_veg_cycle",
        "templateName": "Standard Vegetative Cycle",
        "sequence": [
            { "step": 1, "libraryScheduleId": "sched_template_1678886400", "durationDays": 14 },
            { "step": 2, "libraryScheduleId": "another_schedule_template_uid", "durationDays": 7 }
        ]
    }
    ```
*   **Key Fields & Concepts:** Contains `templateId`, `templateName`, and `sequence` (array of steps with `libraryScheduleId` referencing Schedule Template UIDs, and `durationDays`). Does *not* contain associated I/O points.

### 3.10. C++ Representation Updates (Conceptual Summary - Updated)

*   **Headers:** `IOConfig.h`, `ModbusData.h`, `OutputTypeData.h`, `InputConfigData.h`, `OutputDefData.h`, `CycleData.h`, `ScheduleData.h`.
*   **Structs:** Mirror JSON.
    *   Need `OutputPointDefinition` (`OutputDefData.h`).
    *   Need `ActiveCycleConfiguration` (with nested `AssociatedOutput`, `AssociatedInput` structs - `AssociatedInput` simplified to `pointId` and `role`) (`CycleData.h`).
    *   Need `InputPointConfig` (`InputConfigData.h`).
    *   Need `Schedule` (representing both Template and Instance structure), `AutopilotWindow`, `DurationEvent`, `VolumeEvent` (now includes `calculatedDurationSeconds`) (`ScheduleData.h`).
    *   **Need `CycleTemplate` and `CycleTemplateStep` (`CycleData.h`):**
        ```c++
        #include <vector>
        #include "Arduino.h" // For String

        // Represents one step within a Cycle Template's sequence
        struct CycleTemplateStep {
            int step;                 // Step number (e.g., 1, 2, ...)
            String libraryScheduleId; // UID of the Schedule Template to use for this step
            int durationDays;         // How many days this step lasts
        };

        // Represents a reusable Cycle Template
        struct CycleTemplate {
            String templateId;                  // Unique identifier for this template
            String templateName;                // User-friendly name
            std::vector<CycleTemplateStep> sequence; // The sequence of steps
        };
        ```
*   **Managers:** `ConfigManager`, `ModbusProfileManager`, `OutputPointManager`, `InputPointManager`, `ModbusManager`, `ScheduleManager` (handles Templates and Instances), `CycleManager`, `TemplateManager`.

## 4. Key Considerations for Finalization:

*   **`pointId` / `scheduleUID` / `cycleId` / `templateId` Uniqueness & Generation:** Crucial for linking everything.
*   **Configuration UI:** Need separate UI sections for Board/Modbus, Output Points, Input Points, Schedule Templates, Active Cycles (associating I/O and creating instances), and Cycle Templates.
*   **Autopilot Logic:** The `CycleManager` reads control parameters (`matricTension`, `doseVolume`, `settlingTime`) from the active `autopilotWindow` within the current Schedule *Instance*. It uses the `associatedInputs` (from the Active Cycle config) to identify *which* sensor(s) provide the real-time value and applies the `strategy` (primary/secondary/average - *needs definition where strategy is stored/handled*) to make control decisions.
*   **Verification Logic:** Define how `STATE_VERIFICATION` inputs are used (e.g., read input after command, compare to a threshold potentially defined in the *Input Point Config* or a default) and what happens on failure.
*   **Association Roles/Strategies:** Define a clear vocabulary for `role` in `associatedInputs` and `associatedOutputs`. Determine where/how the input `strategy` (primary/secondary/average) is defined and applied if it's removed from the association itself.
*   **Abstraction:** Define clear Manager interfaces.
*   **Error Handling:** Plan for missing configs, invalid associations, communication errors.
*   **File Paths:** Confirm directory structure (`/data/output_definitions/`, `/data/input_configs/`, `/data/active_cycles/`, `/data/cycle_schedule_instances/`, `/data/modbus_profiles/`, `/data/daily_schedules/`, `/data/cycle_templates/`).
*   **Runtime vs. Config:** Maintain separation.
*   **Reboot Recovery:** Logic remains similar, but needs to load the correct schedule *instance* to determine expected state.

## 5. Review Process:

*   Review this complete, restructured Step 1 definition (Revision 9).
*   Address any remaining "Key Considerations", especially regarding input strategy handling.
*   Formally approve before proceeding to Step 2.
