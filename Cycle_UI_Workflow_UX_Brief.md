# UX Design Brief & Implementation Roadmap: Integrated I/O, Schedule & Cycle Management System (v3)

## 1. Overall Goal

To create a unified interface, primarily within `schedule.html` but potentially expanding to dedicated configuration pages, that allows users to:
1.  Define the system's hardware I/O capabilities (`board_config.json`), including direct I/O and Modbus devices/profiles.
2.  Configure individual **Input Points** (`pointId`s identified in `board_config.json`) with their specific settings (scaling, calibration, alarms) stored in `/data/input_configs/`.
3.  Configure individual **Output Points** (`pointId`s identified in `board_config.json`) by assigning them a function type (e.g., "solenoid_valve", "lighting" from `/data/relay_types.json`) and necessary parameters, stored in `/data/output_definitions/`.
4.  Manage a library of reusable **Schedule Templates** (`/data/daily_schedules/`) defining daily event sequences (Duration, Volume, Autopilot Windows with control parameters).
5.  Define, manage, and activate **Active Cycles** (`/data/active_cycles/`) which consist of:
    *   A sequence of **Schedule Instances** (copies of Schedule Templates stored in `/data/cycle_schedule_instances/`).
    *   An association with **one** pre-configured Output Point (`pointId`).
    *   Associations with one or more pre-configured Input Points (`pointId`s), defining their `role` within the cycle.
6.  Create and manage reusable **Cycle Templates** (`/data/cycle_templates/`) based on schedule sequences.
7.  Provide a robust and intuitive workflow for configuration, cycle creation, activation, and management, including validation (e.g., ensuring volume events only run on volume-capable outputs with valid configuration).
8. Don't forget to use only the latest arduinojson7 library function calls and don't use any of the deprecated ones - see the how_to_upgrade_from_ArduinoJSON6_to_ArduinoJSON7.md file in the root directory.

## 2. Technical Foundation Overview (Updated)

*   **Hardware Abstraction (`board_config.json`):** Defines all available I/O points (direct relays, DI, AI, AO, Modbus points via profiles) and assigns a unique system `pointId` to each. Specifies control methods (Shift Register, Direct GPIO) and communication parameters (Modbus interface details, device addresses).
*   **Input Point Configuration (`/data/input_configs/`):** Separate JSON file per input `pointId` detailing its type, scaling, calibration, alarms, etc. Allows for rich, specific input handling.
*   **Output Function Types (`/data/relay_types.json`):** Defines logical functions (valve, pump, light) applicable to output points (DO type `pointId`s). Specifies capabilities (`supportsVolume`, `supportsAutopilotInput`, etc.) and required configuration parameters (`configParams`). Includes `resumeStateOnReboot` flag.
*   **Output Point Definition (`/data/output_definitions/`):** Separate JSON file per output `pointId` linking it to an `assignedType` from `relay_types.json` and storing the corresponding `configValues` (e.g., name, flow rate).
*   **Schedule Templates & Instances:** Library templates (`/data/daily_schedules/`) define reusable event sequences including Autopilot parameters. Active Cycles use instances (`/data/cycle_schedule_instances/`) which are copies, potentially modified, and include pre-calculated durations for volume events.
*   **Active Cycle Configuration (`/data/active_cycles/`):** Defines a running cycle instance, linking a schedule sequence to exactly one output `pointId` and one or more input `pointId`s, specifying their roles.
*   **Asynchronous Command Processing:** Output point commands (`TURN_ON`, `TURN_OFF`, `TURN_ON_TIMED` identified by `pointId`) are sent to a FreeRTOS queue processed by a dedicated `OutputPointManager` task.
*   **Timed Activations:** Handled via temporary FreeRTOS tasks managed by `OutputPointManager`, ensuring reliable durations and cancellation.
*   **Input Processing:** A dedicated `InputPointManager` task periodically reads direct inputs, storing values (mutex-protected). Modbus inputs are read based on configured polling intervals (implementation deferred). Provides processed values on request.
*   **Autopilot Logic:** Resides primarily within `CycleManager`. Uses parameters from the active Schedule Instance's `autopilotWindow` and real-time sensor values (obtained via `InputPointManager` using `pointId`s from the Active Cycle's `associatedInputs`) to make control decisions. Falls back to scheduled events if associated sensors fail.
*   **Reboot Recovery:** `CycleManager` attempts to restore state for outputs flagged with `resumeStateOnReboot` based on active cycle status and current schedule events. Others default OFF.

## 3. Proposed UI Structure (Refined Concept)

The complexity suggests moving beyond just `schedule.html`. A potential structure:

1.  **Dashboard (`dashboard.html`):** Overview of active cycles, system status, key input readings.
2.  **I/O Configuration Page(s):**
    *   **Board Configuration View:** Display `board_config.json` content (read-only?).
    *   **Output Point Configuration:** List all output `pointId`s. Allow users to assign a function type (`assignedType`) and configure its parameters (`configValues`), saving to `/data/output_definitions/`.
    *   **Input Point Configuration:** List all input `pointId`s. Allow users to configure scaling, calibration, alarms, etc., saving to `/data/input_configs/`. (May require type-specific UI forms).
    *   **Modbus Profiles:** UI to view, upload, and potentially edit `/data/modbus_profiles/`.
3.  **Scheduling Page (`schedule.html` - Refocused):**
    *   **Tab 1: Schedule Templates:** Manage the library (`/data/daily_schedules/`) - Create, view, edit, copy, delete schedule templates (including Autopilot Windows, Duration, Volume events).
    *   **Tab 2: Cycle Templates:** Manage reusable cycle sequences (`/data/cycle_templates/`) - Create (likely from an Active Cycle), view, delete. References Schedule Template UIDs.
    *   **Tab 3: Active Cycles:** Manage active cycle instances (`/data/active_cycles/`).
        *   List existing Active Cycles (showing name, state, current step).
        *   Button: "Create New Active Cycle".
        *   View/Edit Area (when cycle selected):
            *   Cycle Name, Start Date.
            *   **Associated Output:** Select ONE pre-configured output `pointId`. Display its definition details (read-only here).
            *   **Associated Inputs:** Select one or more pre-configured input `pointId`s. Define their `role` for this cycle.
            *   **Cycle Sequence:** Drag Schedule Templates from a library panel to build the sequence. Set `durationDays` for each step. This action triggers creation of Schedule Instances in the backend.
            *   Button: "Edit Schedule Instance" (opens modal, see below).
            *   Status Indicator (DRAFT, DORMANT, ACTIVE, etc.).
            *   Action Buttons: `Save Cycle`, `Activate Cycle`, `Deactivate Cycle`, `Delete Cycle`, `Create Template from Cycle`.
4.  **Schedule Instance Editing Modal:**
    *   Triggered from the Active Cycle view.
    *   Loads the specific Schedule Instance file (`/data/cycle_schedule_instances/`).
    *   Allows editing events *for this instance only*.
    *   Recalculates `calculatedDuration` for volume events upon saving if needed.
    *   Option to promote changes back to a *new* Schedule Template in the library.

## 4. High-Level Workflow (Updated)

1.  **(Setup)** Define Hardware: Configure `board_config.json` (manual edit or future UI). Upload/Define Modbus Profiles.
2.  **(Setup)** Configure I/O Points: Use I/O Configuration UI to assign function types/parameters to Output Points (`/data/output_definitions/`) and configure Input Points (`/data/input_configs/`).
3.  **Define Schedule Templates:** Use Scheduling Page -> Schedule Templates Tab to create reusable daily sequences (`/data/daily_schedules/`).
4.  **Create Active Cycle:** Use Scheduling Page -> Active Cycles Tab -> "Create New".
    *   Give cycle a name.
    *   Associate the single desired Output Point (`pointId`).
    *   Associate necessary Input Points (`pointId`s) and define their roles.
    *   Build the `cycleSequence` by dragging Schedule Templates. Set duration for each step. (Backend creates Schedule Instances).
    *   Save Cycle (State: DRAFT or SAVED_DORMANT).
5.  **(Optional) Edit Instance:** Select cycle, click "Edit Schedule Instance" for a step, modify events in modal, save instance.
6.  **Activate:** Select cycle, click "Activate Cycle". (Backend changes state, `cycleProcessorTask` starts execution).
7.  **Monitor:** View status on Dashboard or Active Cycles tab.
8.  **Deactivate/Complete:** Use buttons on Active Cycles tab.
9.  **(Optional) Create Cycle Template:** Select a saved Active Cycle, click "Create Template from Cycle".

## 5. Implementation Roadmap (7 Steps - Detailed)

1.  **Data Structure & JSON Format Refinement:**
    *   **Goal:** Finalize definitions for all configuration and data files (`board_config.json`, `relay_types.json`, Modbus profiles, Output/Input definitions, Schedule Templates, Active Cycles, Schedule Instances, Cycle Templates) and corresponding C++ structs. Establish clear relationships and `pointId` usage.
    *   **Status:** **Complete** (Documented in `Cycles_Step_1.md`)

2.  **I/O Foundation & Low-Level Control (Direct I/O Focus):**
    *   **Goal:** Implement C++ managers (`OutputPointManager`, `InputPointManager`) to parse `board_config.json` (direct I/O) and `relay_types.json`. Initialize direct hardware (GPIOs, ADC, Shift Register). Map `pointId`s to physical resources. Implement command queue/task for direct relay control (ON/OFF/Timed) via `pointId`. Implement periodic task for direct input reading. Handle basic persistence of I/O definitions (`/data/output_definitions/`, `/data/input_configs/`).
    *   **Status:** **Complete** (Documented in `Cycles_Step_2.md`)

3.  **Active Cycle Management (Backend Logic):**
    *   **Goal:** Implement `CycleManager` for Active Cycle CRUD (`/data/active_cycles/`), state management, and runtime execution. Implement Schedule Instance creation (copying templates, calculating volume durations) and deletion (`/data/cycle_schedule_instances/`). Create periodic task (`cycleProcessorTask`) to evaluate active cycles, process schedule events (Duration, Volume, Autopilot Windows), handle Autopilot logic (prioritizing over scheduled events when sensors OK, using schedule parameters), handle basic Verification logic, interact with I/O managers via `pointId`, and manage reboot recovery based on `resumeStateOnReboot` flag.
    *   **Status:** **Complete** (Documented in `Cycles_Step_3.md`)

4.  **Cycle Template Management (Backend Logic):**
    *   **Goal:** Implement `TemplateManager` for creating, reading, updating, and deleting reusable Cycle Templates (`/data/cycle_templates/*.json`). These templates store only the `cycleSequence` referencing Schedule Template UIDs.
    *   **Status:** **Pending Detail**

5.  **API Interface Implementation:**
    *   **Goal:** Develop HTTP API endpoints (likely in `ApiRoutes.cpp`) to expose all necessary functionalities from the various managers (`ConfigManager`, `OutputPointManager`, `InputPointManager`, `ScheduleManager`, `CycleManager`, `TemplateManager`) to the frontend. Define request/response formats (JSON). Implement authentication and locking checks.
    *   **Status:** **Pending Detail**

6.  **Frontend UI Implementation:**
    *   **Goal:** Build the web user interface (HTML, CSS, JavaScript) based on the refined UI structure (Dashboard, I/O Config pages, Scheduling page with tabs for Schedule Templates, Cycle Templates, Active Cycles). Implement interactions for configuring I/O, managing templates, creating/managing/activating cycles (including I/O association and instance editing via modal), and displaying status, all by calling the backend API.
    *   **Status:** **Pending Detail**

7.  **Integration & Testing:**
    *   **Goal:** Ensure all backend managers, API routes, and frontend components work together correctly. Perform thorough testing of all features, workflows, edge cases (sensor failures, reboot recovery), and error handling.
    *   **Status:** **Pending Detail**

## 6. Addressing Challenges & Mitigation Strategies (Updated)

*   **UI Complexity:** Decoupled configuration requires more UI sections (I/O Config, Schedule Templates, Active Cycles). Mitigation: Clear navigation, logical grouping, potentially wizards for setup.
*   **State Management:** Increased complexity with separate configs and runtime states. Mitigation: Robust manager classes, clear state definitions (`CycleState`), careful mutex usage, clear API responses.
*   **Configuration Interdependencies:** Active Cycles rely on pre-configured I/O points and Schedule Templates. Mitigation: UI validation (prevent associating non-existent points), backend checks, clear error messages.
*   **Autopilot Logic Definition:** Interaction between schedule parameters (`autopilotWindows`) and associated inputs needs precise definition and implementation. Mitigation: Address in "Key Considerations" during Step 1 finalization (done), careful implementation in `CycleManager`.
*   **`pointId` Management:** Ensuring uniqueness and correct mapping is critical. Mitigation: Clear generation strategy in `board_config.json`, robust parsing and mapping logic in managers.
*   **Resource Usage (Tasks/Memory):** FreeRTOS task/queue/mutex usage needs careful implementation. Mitigation: Use persistent tasks where appropriate (I/O managers, cycle processor), use timeouts for mutexes, monitor memory usage.

---

This updated brief reflects the decoupled architecture and provides a clearer high-level view aligned with the detailed technical plan.