# Step 5: API Interface Implementation - Detailed Plan (Revision 5 - ArduinoJson v7, Errors, Examples)

## 1. Goal:

To develop the HTTP API endpoints required to expose the functionalities of the backend managers (`ConfigManager`, `OutputPointManager`, `InputPointManager`, `ScheduleManager`, `CycleManager`, `TemplateManager`) to the frontend web interface and potentially other clients. This involves:
*   Defining clear API routes (URLs) for various actions.
*   Specifying request and response formats (primarily JSON), **including detailed examples**.
*   Implementing handler functions for each route in C++ (likely within `src/ApiRoutes.cpp`).
*   Integrating with existing authentication (`AuthUtils`, `SessionManager`) and authorization (`UserRole`) mechanisms.
*   Integrating with the locking (`LockManager`) mechanism to prevent concurrent configuration changes, using a **30-minute timeout (plus 5s server buffer), refreshed manually by the user.**
*   Providing appropriate HTTP status codes and **structured JSON error responses** for clear client-side handling, as defined below.

## 2. Prerequisites:

*   Steps 1-4 plans finalized (`Cycles_Step_1.md` Rev 9, `Cycles_Step_2.md` Rev 3, `Cycles_Step_3.md` Rev 12, `Cycles_Step_4.md` Rev 1).
*   Conceptual implementation of all required backend managers (`ConfigManager`, `OutputPointManager`, `InputPointManager`, `ScheduleManager`, `CycleManager`, `TemplateManager`) based on the finalized plans.
*   Existing `ApiRoutes.cpp/.h` structure using ESPAsyncWebServer.
*   Functional `AuthUtils`, `SessionManager`, `UserManager` (including `UserRole` definition).
*   Functional `LockManager` for handling configuration locks **with timeout logic**.
*   **ArduinoJson v7** library available and understood.
*   Relevant C++ data structures defined.

## 3. Deliverables:

*   Implemented C++ handler functions for all defined API endpoints within `src/ApiRoutes.cpp`, **using ArduinoJson v7 API**.
*   Updated `include/ApiRoutes.h` with necessary declarations and helper function prototypes.
*   Clear documentation (within this file) outlining each endpoint, its method, required authorization role, **example request/response formats** (success and **structured error**), and locking requirements.
*   Integration tests for the API endpoints, including authorization, lock timeout, and various error conditions.

## 4. Detailed Implementation Tasks & Pseudocode:

### 4.1. API Structure & Common Handling

*   **Base Path:** `/api/v1/`
*   **Authentication & Authorization:** All `/api/v1/` routes require authentication and role-based authorization (`ADMIN` or `OPERATOR`). Helper functions (`handleAuthorizedRequest`, `handleLockedRequest`) will manage these checks using `SessionManager` and `UserRole`.
*   **Locking:** Routes modifying configuration data (POST/PUT/DELETE on definitions, templates, cycles) must check the lock via `handleLockedRequest`, ensuring the lock is held by the current session and has not timed out (30-minute duration + 5s server buffer).
*   **JSON Parsing:** Use ArduinoJson v7 for request body parsing and response serialization. **Recommend using the AsyncJson library addon for ESPAsyncWebServer for efficient body parsing.**
*   **Error Handling:** Use appropriate HTTP status codes (200, 201, 400, 401, 403, 404, 409, 500). **All error responses (`success: false`) MUST include a structured `error` object as defined in Section 4.1.1.** Check `overflowed()` after populating JSON documents where memory might be tight.

#### 4.1.1. Standard Error Response Format

All API responses indicating failure (`"success": false`) will include an `error` object with the following structure:

```json
{
  "success": false,
  "error": {
    "code": "ERROR_CODE_STRING", // Machine-readable error identifier (see list below)
    "message": "Human-readable description of the error.", // User-friendly message
    "details": { // Optional: Additional context specific to the error
      // Context-specific fields like "field", "value", "reason", "resourceId", etc.
    }
  }
}
```

The `sendJsonError` helper function (see Section 4.1.3) will be responsible for constructing this format.

#### 4.1.2. Common Error Codes and Examples

*(Note: This list is comprehensive but may be expanded during implementation.)*

**1. Authentication / Authorization Errors (HTTP 401 / 403):**

*   `UNAUTHENTICATED` (401): Request lacks valid session credentials.
    ```json
    { "success": false, "error": { "code": "UNAUTHENTICATED", "message": "Authentication required. Please log in." } }
    ```
*   `INSUFFICIENT_PERMISSIONS` (403): Authenticated user lacks the required role.
    ```json
    { "success": false, "error": { "code": "INSUFFICIENT_PERMISSIONS", "message": "Admin privileges required for this operation." } }
    ```

**2. Configuration Lock Errors (HTTP 403):**

*   `CONFIG_LOCKED_BY_OTHER`: Attempting locked operation when held by another session.
    ```json
    { "success": false, "error": { "code": "CONFIG_LOCKED_BY_OTHER", "message": "Configuration is locked by another session.", "details": { "holderSessionId": "...", "remainingSeconds": 123 } } }
    ```
*   `CONFIG_LOCK_EXPIRED`: Attempting locked operation with an expired lock.
    ```json
    { "success": false, "error": { "code": "CONFIG_LOCK_EXPIRED", "message": "Your configuration lock has expired. Please re-acquire the lock." } }
    ```
*   `CONFIG_LOCK_NOT_HELD`: Attempting locked operation without holding the lock.
    ```json
    { "success": false, "error": { "code": "CONFIG_LOCK_NOT_HELD", "message": "Configuration lock not held by this session. Please acquire the lock first." } }
    ```

**3. Validation Errors (HTTP 400):**

*   `VALIDATION_MISSING_FIELD`: Required field missing in request body.
    ```json
    { "success": false, "error": { "code": "VALIDATION_MISSING_FIELD", "message": "Missing required field.", "details": { "field": "cycleName" } } }
    ```
*   `VALIDATION_INVALID_TYPE`: Field has incorrect data type.
    ```json
    { "success": false, "error": { "code": "VALIDATION_INVALID_TYPE", "message": "Invalid data type for field.", "details": { "field": "durationDays", "expectedType": "integer", "receivedValue": "abc" } } }
    ```
*   `VALIDATION_INVALID_VALUE`: Field value fails constraints (range, enum, etc.).
    ```json
    { "success": false, "error": { "code": "VALIDATION_INVALID_VALUE", "message": "Invalid value provided for field.", "details": { "field": "durationDays", "reason": "Value must be greater than zero.", "receivedValue": -5 } } }
    ```
*   `VALIDATION_INVALID_REFERENCE`: Referenced resource ID doesn't exist.
    ```json
    { "success": false, "error": { "code": "VALIDATION_INVALID_REFERENCE", "message": "Referenced item does not exist.", "details": { "field": "associatedOutputs[0].pointId", "reason": "Output point 'NonExistentRelay' not found.", "receivedValue": "NonExistentRelay" } } }
    ```
*   `INVALID_JSON_FORMAT`: Request body is not valid JSON.
    ```json
    { "success": false, "error": { "code": "INVALID_JSON_FORMAT", "message": "Failed to parse request body as valid JSON." } }
    ```

**4. Resource Not Found Errors (HTTP 404):**

*   `RESOURCE_NOT_FOUND`: Requested resource ID does not exist.
    ```json
    { "success": false, "error": { "code": "RESOURCE_NOT_FOUND", "message": "The requested resource was not found.", "details": { "resourceType": "OutputPoint", "resourceId": "InvalidID" } } }
    ```

**5. Conflict Errors (HTTP 409):**

*   `RESOURCE_CONFLICT`: Trying to create a resource with an existing ID.
    ```json
    { "success": false, "error": { "code": "RESOURCE_CONFLICT", "message": "A resource with the specified ID already exists.", "details": { "resourceType": "CycleTemplate", "conflictingId": "standard_veg" } } }
    ```
*   `RESOURCE_IN_USE`: Trying to delete a resource currently used by another.
    ```json
    { "success": false, "error": { "code": "RESOURCE_IN_USE", "message": "Cannot delete resource because it is currently in use.", "details": { "resourceType": "ScheduleTemplate", "resourceId": "...", "usedBy": ["Cycle:VegZone1Cycle"] } } }
    ```
*   `OUTPUT_POINT_CONFLICT`: Trying to activate a cycle using an output already used by another active cycle.
    ```json
    { "success": false, "error": { "code": "OUTPUT_POINT_CONFLICT", "message": "Output point is already in use by another active cycle.", "details": { "conflictingPointId": "DirectRelay_0", "conflictingCycleId": "FlowerZone1Cycle" } } }
    ```

**6. Internal Server Errors (HTTP 500):**

*   `FILESYSTEM_WRITE_ERROR`: Failed to save file to LittleFS.
    ```json
    { "success": false, "error": { "code": "FILESYSTEM_WRITE_ERROR", "message": "Failed to save configuration file to storage." } }
    ```
*   `FILESYSTEM_READ_ERROR`: Failed to read file from LittleFS.
    ```json
    { "success": false, "error": { "code": "FILESYSTEM_READ_ERROR", "message": "Failed to load configuration file from storage." } }
    ```
*   `JSON_MEMORY_ERROR`: Insufficient memory during JSON processing (e.g., `overflowed()` is true).
    ```json
    { "success": false, "error": { "code": "JSON_MEMORY_ERROR", "message": "Internal server error: Insufficient memory for JSON processing." } }
    ```
*   `INTERNAL_SERVER_ERROR`: Generic catch-all for unexpected backend errors.
    ```json
    { "success": false, "error": { "code": "INTERNAL_SERVER_ERROR", "message": "An unexpected error occurred while processing the request." } }
    ```

#### 4.1.3. Helper Function Concepts (Updated)

```c++
// --- In ApiRoutes.h or similar ---
#pragma once

#include <functional>
#include <time.h>
#include "ESPAsyncWebServer.h"
#include "ArduinoJson.h"
#include "AuthUtils.h"
#include "LockManager.h"
#include "ConfigManager.h"
#include "OutputPointManager.h"
#include "InputPointManager.h"
#include "ScheduleManager.h"
#include "CycleManager.h"
#include "TemplateManager.h"
#include "SessionManager.h" // Added
#include "OutputTypeData.h"
#include "OutputDefData.h"
#include "InputConfigData.h"
#include "ScheduleData.h"
#include "CycleData.h"
#include "UserAccount.h" // Needed for UserRole

// Define function types for handlers
using ArAuthorizedRequestHandlerFunction = std::function<void(AsyncWebServerRequest *request, SessionData* session)>; // Pass session data
using ArAuthorizedLockedRequestHandlerFunction = std::function<void(AsyncWebServerRequest *request, SessionData* session)>; // Pass session data

/* enum class LockAcquisitionResult { ... }; */ // In LockManager.h

class ApiRoutes {
private:
    AsyncWebServer& server;
    AuthUtils& authUtils;
    LockManager& lockManager;
    ConfigManager& configManager;
    OutputPointManager& outputPointManager;
    InputPointManager& inputPointManager;
    ScheduleManager& scheduleManager;
    CycleManager& cycleManager;
    TemplateManager& templateManager;
    SessionManager& sessionManager; // Added

    // Updated Helper method declarations
    void handleAuthorizedRequest(AsyncWebServerRequest *request, UserRole requiredRole, ArAuthorizedRequestHandlerFunction handler);
    void handleLockedRequest(AsyncWebServerRequest *request, UserRole requiredRole, ArAuthorizedLockedRequestHandlerFunction handler);
    // Updated sendJsonResponse to handle structured errors via sendJsonError
    void sendJsonResponse(AsyncWebServerRequest *request, int code, bool success, const String& message, JsonDocument* dataDoc = nullptr);
    void sendJsonError(AsyncWebServerRequest *request, int code, const String& errorCode, const String& message, JsonObject details = JsonObject()); // New helper

    // Specific handler method declarations... (as before)
    void handleGetSystemStatus(AsyncWebServerRequest *request, SessionData* session); // Pass session
    void handleGetLockStatus(AsyncWebServerRequest *request, SessionData* session);
    void handlePostLock(AsyncWebServerRequest *request, SessionData* session);
    void handleDeleteLock(AsyncWebServerRequest *request, SessionData* session);
    void handleGetOutputTypes(AsyncWebServerRequest *request, SessionData* session);
    void handleGetBoardConfig(AsyncWebServerRequest *request, SessionData* session);
    void handlePostReboot(AsyncWebServerRequest *request, SessionData* session);
    void handleGetOutputs(AsyncWebServerRequest *request, SessionData* session);
    void handleGetOutputDetail(AsyncWebServerRequest *request, SessionData* session);
    void handleGetOutputState(AsyncWebServerRequest *request, SessionData* session);
    void handleGetInputs(AsyncWebServerRequest *request, SessionData* session);
    void handleGetInputDetail(AsyncWebServerRequest *request, SessionData* session);
    void handleGetInputValue(AsyncWebServerRequest *request, SessionData* session);
    void handleGetScheduleTemplates(AsyncWebServerRequest *request, SessionData* session);
    void handleGetScheduleTemplateDetail(AsyncWebServerRequest *request, SessionData* session);
    void handleDeleteScheduleTemplate(AsyncWebServerRequest *request, SessionData* session);
    void handleGetCycleTemplates(AsyncWebServerRequest *request, SessionData* session);
    void handleGetCycleTemplateDetail(AsyncWebServerRequest *request, SessionData* session);
    void handleDeleteCycleTemplate(AsyncWebServerRequest *request, SessionData* session);
    void handleGetActiveCycles(AsyncWebServerRequest *request, SessionData* session);
    void handleGetActiveCycleDetail(AsyncWebServerRequest *request, SessionData* session);
    void handleDeleteActiveCycle(AsyncWebServerRequest *request, SessionData* session);
    void handlePostActivateCycle(AsyncWebServerRequest *request, SessionData* session);
    void handlePostDeactivateCycle(AsyncWebServerRequest *request, SessionData* session);
    void handleGetScheduleInstanceDetail(AsyncWebServerRequest *request, SessionData* session);


public:
    ApiRoutes(AsyncWebServer& s, AuthUtils& auth, LockManager& lock, ConfigManager& cfgMgr, OutputPointManager& outMgr, InputPointManager& inMgr, ScheduleManager& schedMgr, CycleManager& cycMgr, TemplateManager& tmplMgr, SessionManager& sessMgr) // Added SessionManager
      : server(s), authUtils(auth), lockManager(lock), configManager(cfgMgr), outputPointManager(outMgr), inputPointManager(inMgr), scheduleManager(schedMgr), cycleManager(cycMgr), templateManager(tmplMgr), sessionManager(sessMgr) {} // Added SessionManager

    void registerRoutes();
};


// --- In ApiRoutes.cpp ---
#include "ApiRoutes.h"
// #include <AsyncJson.h>

// Modified Authentication/Authorization Helper
void ApiRoutes::handleAuthorizedRequest(AsyncWebServerRequest *request, UserRole requiredRole, ArAuthorizedRequestHandlerFunction handler) {
    SessionData* session = sessionManager.validateSession(request); // Use SessionManager directly
    if (!session) {
        sendJsonError(request, 401, "UNAUTHENTICATED", "Authentication required or session expired.");
        return;
    }

    // Authorization Check (Assumes ADMIN > OPERATOR > UNKNOWN enum values)
    if (session->role < requiredRole) {
         sendJsonError(request, 403, "INSUFFICIENT_PERMISSIONS", "Insufficient permissions for this operation.");
         return;
    }
    handler(request, session); // Pass valid session data
}

// Modified Locked Request Helper (includes Auth/Authz)
void ApiRoutes::handleLockedRequest(AsyncWebServerRequest *request, UserRole requiredRole, ArAuthorizedLockedRequestHandlerFunction handler) {
    SessionData* session = sessionManager.validateSession(request);
    if (!session) {
        sendJsonError(request, 401, "UNAUTHENTICATED", "Authentication required or session expired.");
        return;
    }

    // Authorization Check
    if (session->role < requiredRole) {
         sendJsonError(request, 403, "INSUFFICIENT_PERMISSIONS", "Insufficient permissions for this operation.");
         return;
    }

    // Lock Check
    if (!lockManager.isHeldBySession(session->sessionId)) { // Assumes isHeldBySession checks timeout
         if (lockManager.isLocked()) {
             time_t now = time(nullptr);
             if (now >= lockManager.getLockAcquisitionTime() + lockManager.getLockTimeoutSeconds()) {
                 sendJsonError(request, 403, "CONFIG_LOCK_EXPIRED", "Configuration lock expired. Please re-acquire.");
             } else {
                 JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
                 details["holderSessionId"] = lockManager.getLockHolderSessionId();
                 details["remainingSeconds"] = (lockManager.getLockAcquisitionTime() + lockManager.getLockTimeoutSeconds()) - now;
                 sendJsonError(request, 403, "CONFIG_LOCKED_BY_OTHER", "Configuration locked by another session.", details);
             }
         } else {
             sendJsonError(request, 403, "CONFIG_LOCK_NOT_HELD", "Configuration lock not held by this session.");
         }
         return;
    }
    // If we reach here, user is authorized, lock is held by them and is not expired
    handler(request, session); // Pass valid session data
}


// Helper to send standard JSON success response (ArduinoJson v7)
void ApiRoutes::sendJsonResponse(AsyncWebServerRequest *request, int code, bool success, const String& message, JsonDocument* dataDoc = nullptr) {
    if (!success) {
        // For errors, call sendJsonError instead to ensure structure
        sendJsonError(request, code, "INTERNAL_SERVER_ERROR", message.isEmpty() ? "An unspecified error occurred." : message);
        return;
    }

    JsonDocument responseDoc;
    responseDoc["success"] = true;
    if (!message.isEmpty()) {
         responseDoc["message"] = message;
    }
    if (dataDoc && !dataDoc->isNull()) {
        responseDoc["data"] = *dataDoc;
    }

    if (responseDoc.overflowed()) {
        sendJsonError(request, 500, "JSON_MEMORY_ERROR", "Internal Server Error: JSON Response Overflow");
        return;
    }
    String response;
    serializeJson(responseDoc, response);
    request->send(code, "application/json", response);
}

// Helper to send structured JSON error response (ArduinoJson v7)
void ApiRoutes::sendJsonError(AsyncWebServerRequest *request, int code, const String& errorCode, const String& message, JsonObject details = JsonObject()) {
    JsonDocument responseDoc;
    responseDoc["success"] = false;
    JsonObject errorObj = responseDoc["error"].to<JsonObject>(); // v7
    errorObj["code"] = errorCode;
    errorObj["message"] = message;
    if (!details.isNull()) {
        errorObj["details"] = details; // Deep copy details if provided
    }

    if (responseDoc.overflowed()) {
        // Fallback for overflow during error creation itself
        request->send(500, "application/json", "{\"success\": false, \"error\": {\"code\":\"JSON_MEMORY_ERROR\",\"message\":\"Internal Server Error: JSON Response Overflow\"}}");
        return;
    }
    String response;
    serializeJson(responseDoc, response);
    request->send(code, "application/json", response);
}


void ApiRoutes::registerRoutes() {
    // --- System Routes ---
    // Status: OPERATOR ok
    server.on("/api/v1/system/status", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handleGetSystemStatus(req, session); });
    });
    // Lock status: OPERATOR ok
    server.on("/api/v1/system/lock", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handleGetLockStatus(req, session); });
    });
    // Acquire/Refresh lock: Needs ADMIN
    server.on("/api/v1/system/lock", HTTP_POST, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::ADMIN, [this](AsyncWebServerRequest *req, SessionData* session){ handlePostLock(req, session); });
    });
    // Release lock: Needs ADMIN
    server.on("/api/v1/system/lock", HTTP_DELETE, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::ADMIN, [this](AsyncWebServerRequest *req, SessionData* session){ handleDeleteLock(req, session); });
    });
    // Get types: OPERATOR ok
    server.on("/api/v1/system/output_types", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handleGetOutputTypes(req, session); });
    });
    // Get board config: OPERATOR ok
    server.on("/api/v1/system/board_config", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handleGetBoardConfig(req, session); });
    });
    // Reboot: Needs ADMIN and lock
    server.on("/api/v1/system/reboot", HTTP_POST, [this](AsyncWebServerRequest *request){
        handleLockedRequest(request, UserRole::ADMIN, [this](AsyncWebServerRequest *req, SessionData* session){ handlePostReboot(req, session); });
    });

    // --- Output Point Routes ---
    // List: OPERATOR ok
    server.on("/api/v1/outputs", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handleGetOutputs(req, session); });
    });
    // Get Detail: OPERATOR ok
    server.on("^\\/api\\/v1\\/outputs\\/([^\\/]+)$", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handleGetOutputDetail(req, session); });
    });
    // Get State: OPERATOR ok
    server.on("^\\/api\\/v1\\/outputs\\/([^\\/]+)\\/state$", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handleGetOutputState(req, session); });
    });
    // POST Create/Update: Needs ADMIN and lock (Requires AsyncJson)
    /* AsyncCallbackJsonWebHandler* postOutputHandler = new AsyncCallbackJsonWebHandler("/api/v1/outputs", ... ); server.addHandler(postOutputHandler); */
    // POST Control: Needs OPERATOR, no lock (Requires AsyncJson)
    /* AsyncCallbackJsonWebHandler* controlHandler = new AsyncCallbackJsonWebHandler("^\\/api\\/v1\\/outputs\\/([^\\/]+)\\/control$", HTTP_POST, ... ); server.addHandler(controlHandler); */

    // --- Input Point Routes ---
    // List: OPERATOR ok
    server.on("/api/v1/inputs", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handleGetInputs(req, session); });
    });
    // Get Detail: OPERATOR ok
    server.on("^\\/api\\/v1\\/inputs\\/([^\\/]+)$", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handleGetInputDetail(req, session); });
    });
    // Get Value: OPERATOR ok
    server.on("^\\/api\\/v1\\/inputs\\/([^\\/]+)\\/value$", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handleGetInputValue(req, session); });
    });
    // POST Create/Update: Needs ADMIN and lock (Requires AsyncJson)

    // --- Schedule Template Routes ---
    // List: OPERATOR ok
    server.on("/api/v1/schedules/templates", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handleGetScheduleTemplates(req, session); });
    });
    // Get Detail: OPERATOR ok
    server.on("^\\/api\\/v1\\/schedules\\/templates\\/([^\\/]+)$", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handleGetScheduleTemplateDetail(req, session); });
    });
    // DELETE: Needs ADMIN and lock
    server.on("^\\/api\\/v1\\/schedules\\/templates\\/([^\\/]+)$", HTTP_DELETE, [this](AsyncWebServerRequest *request){
        handleLockedRequest(request, UserRole::ADMIN, [this](AsyncWebServerRequest *req, SessionData* session){ handleDeleteScheduleTemplate(req, session); });
    });
    // POST Create: Needs ADMIN and lock (Requires AsyncJson)
    // PUT Update: Needs ADMIN and lock (Requires AsyncJson)

    // --- Cycle Template Routes ---
    // List: OPERATOR ok
    server.on("/api/v1/cycles/templates", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handleGetCycleTemplates(req, session); });
    });
    // Get Detail: OPERATOR ok
    server.on("^\\/api\\/v1\\/cycles\\/templates\\/([^\\/]+)$", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handleGetCycleTemplateDetail(req, session); });
    });
    // DELETE: Needs ADMIN and lock
    server.on("^\\/api\\/v1\\/cycles\\/templates\\/([^\\/]+)$", HTTP_DELETE, [this](AsyncWebServerRequest *request){
        handleLockedRequest(request, UserRole::ADMIN, [this](AsyncWebServerRequest *req, SessionData* session){ handleDeleteCycleTemplate(req, session); });
    });
    // POST Create: Needs ADMIN and lock (Requires AsyncJson)
    // PUT Update: Needs ADMIN and lock (Requires AsyncJson)

    // --- Active Cycle & Instance Routes ---
    // List Active: OPERATOR ok
    server.on("/api/v1/cycles/active", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handleGetActiveCycles(req, session); });
    });
    // Get Active Detail: OPERATOR ok
    server.on("^\\/api\\/v1\\/cycles\\/active\\/([^\\/]+)$", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handleGetActiveCycleDetail(req, session); });
    });
    // DELETE Active: Needs ADMIN and lock
    server.on("^\\/api\\/v1\\/cycles\\/active\\/([^\\/]+)$", HTTP_DELETE, [this](AsyncWebServerRequest *request){
        handleLockedRequest(request, UserRole::ADMIN, [this](AsyncWebServerRequest *req, SessionData* session){ handleDeleteActiveCycle(req, session); });
    });
    // Activate/Deactivate: OPERATOR ok
    server.on("^\\/api\\/v1\\/cycles\\/active\\/([^\\/]+)\\/activate$", HTTP_POST, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handlePostActivateCycle(req, session); });
    });
    server.on("^\\/api\\/v1\\/cycles\\/active\\/([^\\/]+)\\/deactivate$", HTTP_POST, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handlePostDeactivateCycle(req, session); });
    });
    // Get Instance Detail: OPERATOR ok
    server.on("^\\/api\\/v1\\/cycles\\/instances\\/([^\\/]+)$", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session){ handleGetScheduleInstanceDetail(req, session); });
    });
    // POST Create Active: Needs ADMIN and lock (Requires AsyncJson)
    // PUT Update Active: Needs ADMIN and lock (Requires AsyncJson)
    // PUT Update Instance: Needs ADMIN and lock (Requires AsyncJson)

} // End of registerRoutes()

// --- Handler Implementations ---

void ApiRoutes::handleGetSystemStatus(AsyncWebServerRequest *request, SessionData* session) {
    JsonDocument doc; // v7
    doc["timestamp"] = configManager.getCurrentTimestamp();
    doc["lockHeldBy"] = lockManager.getLockHolderSessionId();
    doc["isLocked"] = lockManager.isEffectivelyLocked();
    sendJsonResponse(request, 200, true, "", &doc);
}

void ApiRoutes::handleGetLockStatus(AsyncWebServerRequest *request, SessionData* session) {
    time_t now = time(nullptr);
    time_t lockAcquiredAt = lockManager.getLockAcquisitionTime();
    long serverTimeoutSeconds = lockManager.getLockTimeoutSeconds();
    long uiTimeoutSeconds = 1800;
    bool effectivelyLocked = lockManager.isEffectivelyLocked();
    long remainingSeconds = 0;
    if (effectivelyLocked) {
        remainingSeconds = (lockAcquiredAt + uiTimeoutSeconds) - now;
        if (remainingSeconds < 0) remainingSeconds = 0;
    }
    JsonDocument doc; // v7
    doc["isLocked"] = effectivelyLocked;
    doc["heldByCurrentSession"] = effectivelyLocked && lockManager.isHeldBySession(session->sessionId);
    doc["lockHolderSessionId"] = effectivelyLocked ? lockManager.getLockHolderSessionId() : "";
    doc["lockAcquiredTimestamp"] = effectivelyLocked ? lockAcquiredAt : 0;
    doc["lockTimeoutSeconds"] = uiTimeoutSeconds;
    doc["lockRemainingSeconds"] = remainingSeconds;
    sendJsonResponse(request, 200, true, "", &doc);
}

void ApiRoutes::handlePostLock(AsyncWebServerRequest *request, SessionData* session) {
    LockAcquisitionResult result = lockManager.acquireOrRefreshLock(session->sessionId);
    switch(result) {
        case LockAcquisitionResult::SUCCESS_ACQUIRED:
            sendJsonResponse(request, 200, true, "Lock acquired (30 min timeout)"); break;
        case LockAcquisitionResult::SUCCESS_REFRESHED:
            sendJsonResponse(request, 200, true, "Lock refreshed (30 min timeout)"); break;
        case LockAcquisitionResult::FAILED_HELD_BY_OTHER:
            {
                time_t now = time(nullptr);
                time_t lockAcquiredAt = lockManager.getLockAcquisitionTime();
                long remaining = (lockAcquiredAt + lockManager.getLockTimeoutSeconds()) - now;
                if (remaining < 0) remaining = 0;
                JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
                details["holderSessionId"] = lockManager.getLockHolderSessionId();
                details["remainingSeconds"] = remaining;
                sendJsonError(request, 403, "CONFIG_LOCKED_BY_OTHER", "Lock held by another session.", details);
            }
            break;
        default: sendJsonError(request, 500, "INTERNAL_SERVER_ERROR", "Failed to acquire/refresh lock"); break;
    }
}

void ApiRoutes::handleDeleteLock(AsyncWebServerRequest *request, SessionData* session) {
    if (lockManager.releaseLock(session->sessionId)) {
        sendJsonResponse(request, 200, true, "Lock released");
    } else {
        sendJsonError(request, 403, "CONFIG_LOCK_NOT_HELD", "Failed to release lock (not held by this session or expired)");
    }
}

void ApiRoutes::handleGetOutputTypes(AsyncWebServerRequest *request, SessionData* session) {
    const std::vector<OutputTypeInfo>& types = configManager.getOutputTypes();
    JsonDocument doc; // v7
    JsonArray array = doc.to<JsonArray>(); // v7
    for(const auto& typeInfo : types) {
        JsonObject obj = array.add<JsonObject>(); // v7
        obj["typeId"] = typeInfo.typeId;
        obj["displayName"] = typeInfo.displayName;
        obj["description"] = typeInfo.description;
        obj["supportsVolume"] = typeInfo.supportsVolume;
        obj["supportsAutopilotInput"] = typeInfo.supportsAutopilotInput;
        obj["resumeStateOnReboot"] = typeInfo.resumeStateOnReboot;
        JsonArray params = obj["configParams"].to<JsonArray>(); // v7
        for(const auto& param : typeInfo.configParams) {
            JsonObject pObj = params.add<JsonObject>(); // v7
            pObj["id"] = param.id;
            pObj["label"] = param.label;
            pObj["type"] = param.type;
            pObj["required"] = param.required;
            // Add other fields...
        }
    }
    sendJsonResponse(request, 200, true, "", &doc);
}

void ApiRoutes::handleGetBoardConfig(AsyncWebServerRequest *request, SessionData* session) {
    String boardConfigJson = configManager.getBoardConfigJson();
    if (!boardConfigJson.isEmpty()) {
         request->send(200, "application/json", boardConfigJson);
    } else {
         sendJsonError(request, 500, "FILESYSTEM_READ_ERROR", "Failed to retrieve board configuration");
    }
}

void ApiRoutes::handlePostReboot(AsyncWebServerRequest *request, SessionData* session) {
    sendJsonResponse(request, 200, true, "Rebooting system...");
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP.restart();
}

// --- Output Point Handlers ---
void ApiRoutes::handleGetOutputs(AsyncWebServerRequest *request, SessionData* session) {
    std::vector<OutputPointBasicInfo> outputList;
    if (outputPointManager.listOutputPoints(outputList)) {
        JsonDocument doc; // v7
        JsonArray array = doc.to<JsonArray>(); // v7
        for(const auto& info : outputList) {
            JsonObject obj = array.add<JsonObject>(); // v7
            obj["pointId"] = info.pointId;
            obj["assignedType"] = info.assignedType;
            obj["name"] = info.name;
        }
        sendJsonResponse(request, 200, true, "", &doc);
    } else {
        sendJsonError(request, 500, "INTERNAL_SERVER_ERROR", "Failed to list outputs");
    }
}

void ApiRoutes::handleGetOutputDetail(AsyncWebServerRequest *request, SessionData* session) {
    String pointId = request->pathArg(0);
    OutputPointDefinition definition;
    if (outputPointManager.loadOutputPointDefinition(pointId, definition)) {
        JsonDocument doc; // v7
        doc["pointId"] = definition.pointId;
        doc["assignedType"] = definition.assignedType;
        JsonDocument configValuesDoc; // v7: Temp doc
        DeserializationError error = deserializeJson(configValuesDoc, definition.configValuesJson);
        if (error == DeserializationError::Ok) {
            doc["configValues"] = configValuesDoc.as<JsonObject>(); // v7: Deep copy
        } else {
             doc["configValues"] = nullptr;
             // Log error
        }
        sendJsonResponse(request, 200, true, "", &doc);
    } else {
        JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
        details["resourceType"] = "OutputPoint"; details["resourceId"] = pointId;
        sendJsonError(request, 404, "RESOURCE_NOT_FOUND", "Output point not found", details);
    }
}

void ApiRoutes::handleGetOutputState(AsyncWebServerRequest *request, SessionData* session) {
    String pointId = request->pathArg(0);
    bool currentState = outputPointManager.getCurrentState(pointId);
    JsonDocument doc; // v7
    doc["pointId"] = pointId;
    doc["state"] = currentState ? "ON" : "OFF";
    sendJsonResponse(request, 200, true, "", &doc);
}

// --- Input Point Handlers ---
void ApiRoutes::handleGetInputs(AsyncWebServerRequest *request, SessionData* session) {
    std::vector<InputPointBasicInfo> inputList;
    if (inputPointManager.listInputPoints(inputList)) {
        JsonDocument doc; // v7
        JsonArray array = doc.to<JsonArray>(); // v7
        for(const auto& info : inputList) {
            JsonObject obj = array.add<JsonObject>(); // v7
            obj["pointId"] = info.pointId;
            obj["type"] = info.type;
            obj["name"] = info.name;
        }
        sendJsonResponse(request, 200, true, "", &doc);
    } else {
        sendJsonError(request, 500, "INTERNAL_SERVER_ERROR", "Failed to list inputs");
    }
}

void ApiRoutes::handleGetInputDetail(AsyncWebServerRequest *request, SessionData* session) {
    String pointId = request->pathArg(0);
    InputPointConfig config;
    if (inputPointManager.loadInputPointConfig(pointId, config)) {
        JsonDocument doc; // v7
        // Serialize InputPointConfig struct to doc...
        doc["pointId"] = config.pointId;
        JsonObject cfgObj = doc["inputConfig"].to<JsonObject>(); // v7
        cfgObj["type"] = config.inputConfig.type;
        cfgObj["subtype"] = config.inputConfig.subtype;
        cfgObj["name"] = config.inputConfig.name;
        // ... serialize scaling, calibration, compensation, alarms etc. using v7 methods ...
        sendJsonResponse(request, 200, true, "", &doc);
    } else {
        JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
        details["resourceType"] = "InputPoint"; details["resourceId"] = pointId;
        sendJsonError(request, 404, "RESOURCE_NOT_FOUND", "Input point not found", details);
    }
}

void ApiRoutes::handleGetInputValue(AsyncWebServerRequest *request, SessionData* session) {
    String pointId = request->pathArg(0);
    float value = inputPointManager.getCurrentValue(pointId);
    InputPointStatus status = inputPointManager.getInputStatus(pointId);
    String unit = inputPointManager.getUnit(pointId);

    JsonDocument doc; // v7
    doc["pointId"] = pointId;
    doc["value"] = value;
    doc["unit"] = unit;
    doc["status"] = inputPointStatusToString(status);

    sendJsonResponse(request, 200, true, "", &doc);
}

// --- Schedule Template Handlers ---
void ApiRoutes::handleGetScheduleTemplates(AsyncWebServerRequest *request, SessionData* session) {
    std::vector<ScheduleTemplateInfo> templateList;
    if (scheduleManager.listScheduleTemplates(templateList)) {
        JsonDocument doc; // v7
        JsonArray array = doc.to<JsonArray>(); // v7
        for(const auto& info : templateList) {
            JsonObject obj = array.add<JsonObject>(); // v7
            obj["scheduleUID"] = info.scheduleUID;
            obj["scheduleName"] = info.scheduleName;
        }
        sendJsonResponse(request, 200, true, "", &doc);
    } else {
        sendJsonError(request, 500, "INTERNAL_SERVER_ERROR", "Failed to list schedule templates");
    }
}

void ApiRoutes::handleGetScheduleTemplateDetail(AsyncWebServerRequest *request, SessionData* session) {
    String scheduleUID = request->pathArg(0);
    Schedule scheduleData;
    if (scheduleManager.loadScheduleTemplate(scheduleUID, scheduleData)) {
        JsonDocument doc; // v7
        // Serialize scheduleData struct into doc... using v7 methods
        sendJsonResponse(request, 200, true, "", &doc);
    } else {
        JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
        details["resourceType"] = "ScheduleTemplate"; details["resourceId"] = scheduleUID;
        sendJsonError(request, 404, "RESOURCE_NOT_FOUND", "Schedule template not found", details);
    }
}

void ApiRoutes::handleDeleteScheduleTemplate(AsyncWebServerRequest *request, SessionData* session) {
    String scheduleUID = request->pathArg(0);
    if (scheduleManager.deleteScheduleTemplate(scheduleUID)) {
        sendJsonResponse(request, 200, true, "Schedule template deleted");
    } else {
        JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
        details["resourceType"] = "ScheduleTemplate"; details["resourceId"] = scheduleUID;
        sendJsonError(request, 404, "RESOURCE_NOT_FOUND", "Schedule template not found or deletion failed", details);
    }
}

// --- Cycle Template Handlers ---
void ApiRoutes::handleGetCycleTemplates(AsyncWebServerRequest *request, SessionData* session) {
    std::vector<BasicTemplateInfo> templateList;
    if (templateManager.listTemplates(templateList)) {
        JsonDocument doc; // v7
        JsonArray array = doc.to<JsonArray>(); // v7
        for(const auto& info : templateList) {
            JsonObject obj = array.add<JsonObject>(); // v7
            obj["templateId"] = info.templateId;
            obj["templateName"] = info.templateName;
        }
        sendJsonResponse(request, 200, true, "", &doc);
    } else {
        sendJsonError(request, 500, "INTERNAL_SERVER_ERROR", "Failed to list cycle templates");
    }
}

void ApiRoutes::handleGetCycleTemplateDetail(AsyncWebServerRequest *request, SessionData* session) {
    String templateId = request->pathArg(0);
    CycleTemplate templateData;
    if (templateManager.loadTemplate(templateId, templateData)) {
        JsonDocument doc; // v7
        // Serialize templateData struct into doc...
        doc["templateId"] = templateData.templateId;
        doc["templateName"] = templateData.templateName;
        JsonArray seq = doc["sequence"].to<JsonArray>(); // v7
        for(const auto& step : templateData.sequence) {
            JsonObject sObj = seq.add<JsonObject>(); // v7
            sObj["step"] = step.step;
            sObj["libraryScheduleId"] = step.libraryScheduleId;
            sObj["durationDays"] = step.durationDays;
        }
        sendJsonResponse(request, 200, true, "", &doc);
    } else {
        JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
        details["resourceType"] = "CycleTemplate"; details["resourceId"] = templateId;
        sendJsonError(request, 404, "RESOURCE_NOT_FOUND", "Cycle template not found", details);
    }
}

void ApiRoutes::handleDeleteCycleTemplate(AsyncWebServerRequest *request, SessionData* session) {
    String templateId = request->pathArg(0);
    if (templateManager.deleteTemplate(templateId)) {
        sendJsonResponse(request, 200, true, "Cycle template deleted");
    } else {
        JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
        details["resourceType"] = "CycleTemplate"; details["resourceId"] = templateId;
        sendJsonError(request, 404, "RESOURCE_NOT_FOUND", "Cycle template not found or deletion failed", details);
    }
}

// --- Active Cycle Handlers ---
void ApiRoutes::handleGetActiveCycles(AsyncWebServerRequest *request, SessionData* session) {
    std::vector<ActiveCycleBasicInfo> cycleList;
    if (cycleManager.listActiveCycles(cycleList)) {
        JsonDocument doc; // v7
        JsonArray array = doc.to<JsonArray>(); // v7
        for(const auto& info : cycleList) {
            JsonObject obj = array.add<JsonObject>(); // v7
            obj["cycleId"] = info.cycleId;
            obj["cycleName"] = info.cycleName;
            obj["cycleState"] = cycleStateToString(info.cycleState);
        }
        sendJsonResponse(request, 200, true, "", &doc);
    } else {
        sendJsonError(request, 500, "INTERNAL_SERVER_ERROR", "Failed to list active cycles");
    }
}

void ApiRoutes::handleGetActiveCycleDetail(AsyncWebServerRequest *request, SessionData* session) {
    String cycleId = request->pathArg(0);
    ActiveCycleConfiguration cycleConfig;
    if (cycleManager.loadActiveCycle(cycleId, cycleConfig)) {
        JsonDocument doc; // v7
        // Serialize cycleConfig struct into doc... using v7 methods
        doc["cycleId"] = cycleConfig.cycleId;
        doc["cycleName"] = cycleConfig.cycleName;
        doc["cycleState"] = cycleStateToString(cycleConfig.cycleState);
        doc["cycleStartDate"] = cycleConfig.cycleStartDate;
        doc["currentStep"] = cycleConfig.currentStep;
        doc["stepStartDate"] = cycleConfig.stepStartDate;
        JsonArray seq = doc["cycleSequence"].to<JsonArray>(); // v7
        for(const auto& step : cycleConfig.cycleSequence) {
             JsonObject sObj = seq.add<JsonObject>(); // v7
             sObj["step"] = step.step;
             sObj["libraryScheduleId"] = step.libraryScheduleId;
             sObj["scheduleInstanceId"] = step.scheduleInstanceId;
             sObj["durationDays"] = step.durationDays;
        }
        JsonArray outs = doc["associatedOutputs"].to<JsonArray>(); // v7
        for(const auto& out : cycleConfig.associatedOutputs) {
             JsonObject oObj = outs.add<JsonObject>(); // v7
             oObj["pointId"] = out.pointId;
             oObj["role"] = out.role;
        }
        JsonArray ins = doc["associatedInputs"].to<JsonArray>(); // v7
        for(const auto& in : cycleConfig.associatedInputs) {
             JsonObject iObj = ins.add<JsonObject>(); // v7
             iObj["pointId"] = in.pointId;
             iObj["role"] = in.role;
        }
        sendJsonResponse(request, 200, true, "", &doc);
    } else {
        JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
        details["resourceType"] = "ActiveCycle"; details["resourceId"] = cycleId;
        sendJsonError(request, 404, "RESOURCE_NOT_FOUND", "Active cycle not found", details);
    }
}

void ApiRoutes::handleDeleteActiveCycle(AsyncWebServerRequest *request, SessionData* session) {
    String cycleId = request->pathArg(0);
    if (cycleManager.deleteActiveCycle(cycleId)) {
        sendJsonResponse(request, 200, true, "Active cycle deleted");
    } else {
        JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
        details["resourceType"] = "ActiveCycle"; details["resourceId"] = cycleId;
        sendJsonError(request, 404, "RESOURCE_NOT_FOUND", "Active cycle not found or deletion failed", details);
    }
}

void ApiRoutes::handlePostActivateCycle(AsyncWebServerRequest *request, SessionData* session) {
    String cycleId = request->pathArg(0);
    if (cycleManager.activateCycle(cycleId)) {
        sendJsonResponse(request, 200, true, "Cycle activated");
    } else {
        // Could be not found, or already active, or other validation error
        JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
        details["resourceType"] = "ActiveCycle"; details["resourceId"] = cycleId;
        sendJsonError(request, 404, "RESOURCE_NOT_FOUND", "Cycle not found or activation failed", details); // Or 400/409
    }
}

void ApiRoutes::handlePostDeactivateCycle(AsyncWebServerRequest *request, SessionData* session) {
    String cycleId = request->pathArg(0);
    if (cycleManager.deactivateCycle(cycleId)) {
        sendJsonResponse(request, 200, true, "Cycle deactivated");
    } else {
        JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
        details["resourceType"] = "ActiveCycle"; details["resourceId"] = cycleId;
        sendJsonError(request, 404, "RESOURCE_NOT_FOUND", "Cycle not found or deactivation failed", details); // Or 400/409
    }
}

// --- Schedule Instance Handlers ---
void ApiRoutes::handleGetScheduleInstanceDetail(AsyncWebServerRequest *request, SessionData* session) {
    String instanceId = request->pathArg(0);
    Schedule scheduleData;
    String instancePath = scheduleManager.getScheduleInstancePath(instanceId);
    if (scheduleManager.loadScheduleInstance(instancePath, scheduleData)) {
        JsonDocument doc; // v7
        // Serialize scheduleData struct into doc... using v7 methods
        sendJsonResponse(request, 200, true, "", &doc);
    } else {
        JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
        details["resourceType"] = "ScheduleInstance"; details["resourceId"] = instanceId;
        sendJsonError(request, 404, "RESOURCE_NOT_FOUND", "Schedule instance not found", details);
    }
}

// Note: POST/PUT handlers using AsyncJsonWebHandler are still conceptual examples
// and require the library to be properly integrated and the handler logic adapted.

```

### 4.2. Endpoint Definitions & Handler Pseudocode (Continued - Placeholders for brevity removed, full structure shown)

**(Note: The actual implementation within each handler needs careful error checking, JSON serialization/deserialization logic for the specific structs, and calls to the correct manager methods.)**

**C. InputPointManager Endpoints:**

*   `GET /api/v1/inputs`: List basic info (pointId, type, name). Auth required.
    ```c++
    // In ApiRoutes::registerRoutes()
    server.on("/api/v1/inputs", HTTP_GET, [this](AsyncWebServerRequest *request){ handleGetInputs(request); });
    ```
    ```c++
    // In ApiRoutes.cpp
    void ApiRoutes::handleGetInputs(AsyncWebServerRequest *request) {
        handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session) {
            std::vector<InputPointBasicInfo> inputList; // Assume struct { String pointId; String type; String name; }
            if (inputPointManager.listInputPoints(inputList)) { // Assume method exists
                JsonDocument doc; // v7
                JsonArray array = doc.to<JsonArray>(); // v7
                for(const auto& info : inputList) {
                    JsonObject obj = array.add<JsonObject>(); // v7
                    obj["pointId"] = info.pointId;
                    obj["type"] = info.type; // Type from InputPointConfig
                    obj["name"] = info.name; // Name from InputPointConfig
                }
                sendJsonResponse(req, 200, true, "", &doc);
            } else {
                sendJsonError(req, 500, "INTERNAL_SERVER_ERROR", "Failed to list inputs");
            }
        });
    }
    ```
*   `GET /api/v1/inputs/{pointId}`: Get detailed config. Auth required.
    ```c++
    // In ApiRoutes::registerRoutes()
    server.on("^\\/api\\/v1\\/inputs\\/([^\\/]+)$", HTTP_GET, [this](AsyncWebServerRequest *request){ handleGetInputDetail(request); });
    ```
    ```c++
    // In ApiRoutes.cpp
    void ApiRoutes::handleGetInputDetail(AsyncWebServerRequest *request) {
         handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session) {
            String pointId = req->pathArg(0);
            InputPointConfig config;
            if (inputPointManager.loadInputPointConfig(pointId, config)) {
                JsonDocument doc; // v7
                // Serialize InputPointConfig struct to doc...
                doc["pointId"] = config.pointId;
                JsonObject cfgObj = doc["inputConfig"].to<JsonObject>(); // v7
                cfgObj["type"] = config.inputConfig.type;
                cfgObj["subtype"] = config.inputConfig.subtype;
                cfgObj["name"] = config.inputConfig.name;
                // ... serialize scaling, calibration, compensation, alarms etc. using v7 methods ...
                sendJsonResponse(req, 200, true, "", &doc);
            } else {
                JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
                details["resourceType"] = "InputPoint"; details["resourceId"] = pointId;
                sendJsonError(req, 404, "RESOURCE_NOT_FOUND", "Input point not found", details);
            }
        });
    }
    ```
*   `POST /api/v1/inputs/{pointId}`: Create/update config (Locked). Use AsyncJsonWebHandler.
    ```c++
    // In ApiRoutes::registerRoutes() - Requires AsyncJsonWebHandler
    /*
    AsyncCallbackJsonWebHandler* postInputHandler = new AsyncCallbackJsonWebHandler(
        "^\\/api\\/v1\\/inputs\\/([^\\/]+)$", HTTP_POST, // Or PUT? POST often used for create/update
        [this](AsyncWebServerRequest *request, JsonVariant &json) {
            handleLockedRequest(request, UserRole::ADMIN, [this, &json](AsyncWebServerRequest *req, SessionData* session) { // Pass ADMIN role
                String pointId = req->pathArg(0); // Check path arg access with AsyncJson
                JsonObject jsonObj = json.as<JsonObject>();
                InputPointConfig config;
                // Deserialize jsonObj into config struct...
                // Ensure pointId from path is used/validated
                config.pointId = pointId;

                if (// validation fails // false) {
                     sendJsonError(req, 400, "VALIDATION_INVALID_VALUE", "Invalid input configuration"); return;
                }
                if (inputPointManager.saveInputPointConfig(config)) {
                    sendJsonResponse(req, 200, true, "Input configuration saved");
                } else {
                    sendJsonError(req, 500, "FILESYSTEM_WRITE_ERROR", "Failed to save input configuration");
                }
            });
        }, 4096); // Adjust size
    server.addHandler(postInputHandler);
    */
    ```
*   `GET /api/v1/inputs/{pointId}/value`: Get current value/status. Auth required.
    ```c++
    // In ApiRoutes::registerRoutes()
    server.on("^\\/api\\/v1\\/inputs\\/([^\\/]+)\\/value$", HTTP_GET, [this](AsyncWebServerRequest *request){ handleGetInputValue(request); });
    ```
    ```c++
    // In ApiRoutes.cpp
    void ApiRoutes::handleGetInputValue(AsyncWebServerRequest *request) {
         handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session) {
            String pointId = req->pathArg(0);
            float value = inputPointManager.getCurrentValue(pointId);
            InputPointStatus status = inputPointManager.getInputStatus(pointId);
            String unit = inputPointManager.getUnit(pointId);

            JsonDocument doc; // v7
            doc["pointId"] = pointId;
            doc["value"] = value; // Consider precision formatting if needed, e.g., String(value, 2)
            doc["unit"] = unit;
            doc["status"] = inputPointStatusToString(status); // Need conversion helper

            sendJsonResponse(req, 200, true, "", &doc);
        });
    }
    ```

**D. ScheduleManager (Templates) Endpoints:**

*   `GET /api/v1/schedules/templates`: List templates (UID, name). Auth required.
    ```c++
    // In ApiRoutes::registerRoutes()
    server.on("/api/v1/schedules/templates", HTTP_GET, [this](AsyncWebServerRequest *request){ handleGetScheduleTemplates(request); });
    ```
    ```c++
    // In ApiRoutes.cpp
    void ApiRoutes::handleGetScheduleTemplates(AsyncWebServerRequest *request) {
         handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session) {
            std::vector<ScheduleTemplateInfo> templateList; // Assume struct { String scheduleUID; String scheduleName; }
            if (scheduleManager.listScheduleTemplates(templateList)) {
                JsonDocument doc; // v7
                JsonArray array = doc.to<JsonArray>(); // v7
                for(const auto& info : templateList) {
                    JsonObject obj = array.add<JsonObject>(); // v7
                    obj["scheduleUID"] = info.scheduleUID;
                    obj["scheduleName"] = info.scheduleName;
                }
                sendJsonResponse(req, 200, true, "", &doc);
            } else {
                sendJsonError(req, 500, "INTERNAL_SERVER_ERROR", "Failed to list schedule templates");
            }
        });
    }
    ```
*   `POST /api/v1/schedules/templates`: Create template (Locked). Use AsyncJsonWebHandler.
    ```c++
    // In ApiRoutes::registerRoutes() - Requires AsyncJsonWebHandler
    /*
    AsyncCallbackJsonWebHandler* postSchedTmplHandler = new AsyncCallbackJsonWebHandler("/api/v1/schedules/templates",
        [this](AsyncWebServerRequest *request, JsonVariant &json) {
            handleLockedRequest(request, UserRole::ADMIN, [this, &json](AsyncWebServerRequest *req, SessionData* session) {
                Schedule scheduleData;
                // Deserialize json into scheduleData struct...
                scheduleData.scheduleUID = ""; // Manager should generate UID

                String newUID;
                if (scheduleManager.saveScheduleTemplate(scheduleData, newUID)) {
                    JsonDocument doc; doc["scheduleUID"] = newUID; // v7
                    sendJsonResponse(req, 201, true, "Schedule template created", &doc);
                } else {
                    sendJsonError(req, 500, "FILESYSTEM_WRITE_ERROR", "Failed to create schedule template");
                }
            });
        }, 4096);
    server.addHandler(postSchedTmplHandler);
    */
    ```
*   `GET /api/v1/schedules/templates/{scheduleUID}`: Get template detail. Auth required.
    ```c++
    // In ApiRoutes::registerRoutes()
    server.on("^\\/api\\/v1\\/schedules\\/templates\\/([^\\/]+)$", HTTP_GET, [this](AsyncWebServerRequest *request){ handleGetScheduleTemplateDetail(request); });
    ```
    ```c++
    // In ApiRoutes.cpp
    void ApiRoutes::handleGetScheduleTemplateDetail(AsyncWebServerRequest *request) {
         handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session) {
            String scheduleUID = req->pathArg(0);
            Schedule scheduleData;
            if (scheduleManager.loadScheduleTemplate(scheduleUID, scheduleData)) {
                JsonDocument doc; // v7
                // Serialize scheduleData struct into doc... using v7 methods
                sendJsonResponse(req, 200, true, "", &doc);
            } else {
                JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
                details["resourceType"] = "ScheduleTemplate"; details["resourceId"] = scheduleUID;
                sendJsonError(req, 404, "RESOURCE_NOT_FOUND", "Schedule template not found", details);
            }
        });
    }
    ```
*   `PUT /api/v1/schedules/templates/{scheduleUID}`: Update template (Locked). Use AsyncJsonWebHandler.
    ```c++
    // In ApiRoutes::registerRoutes() - Requires AsyncJsonWebHandler
    /*
    AsyncCallbackJsonWebHandler* putSchedTmplHandler = new AsyncCallbackJsonWebHandler(
        "^\\/api\\/v1\\/schedules\\/templates\\/([^\\/]+)$", HTTP_PUT,
        [this](AsyncWebServerRequest *request, JsonVariant &json) {
            handleLockedRequest(request, UserRole::ADMIN, [this, &json](AsyncWebServerRequest *req, SessionData* session) {
                String scheduleUID = req->pathArg(0);
                Schedule scheduleData;
                // Deserialize json into scheduleData struct...
                JsonObject jsonObj = json.as<JsonObject>();
                if ((jsonObj["scheduleUID"] | "") != scheduleUID) { sendJsonError(req, 400, "VALIDATION_INVALID_VALUE", "Schedule UID mismatch"); return; }
                scheduleData.scheduleUID = scheduleUID;

                if (scheduleManager.saveScheduleTemplate(scheduleData)) {
                    sendJsonResponse(req, 200, true, "Schedule template updated");
                } else {
                    sendJsonError(req, 500, "FILESYSTEM_WRITE_ERROR", "Failed to update schedule template");
                }
            });
        }, 4096);
    server.addHandler(putSchedTmplHandler);
    */
    ```
*   `DELETE /api/v1/schedules/templates/{scheduleUID}`: Delete template (Locked).
    ```c++
    // In ApiRoutes::registerRoutes()
    server.on("^\\/api\\/v1\\/schedules\\/templates\\/([^\\/]+)$", HTTP_DELETE, [this](AsyncWebServerRequest *request){
        handleLockedRequest(request, UserRole::ADMIN, [this](AsyncWebServerRequest *req, SessionData* session){ handleDeleteScheduleTemplate(req, session); });
    });
    ```
    ```c++
    // In ApiRoutes.cpp
    void ApiRoutes::handleDeleteScheduleTemplate(AsyncWebServerRequest *request, SessionData* session) {
        String scheduleUID = request->pathArg(0);
        if (scheduleManager.deleteScheduleTemplate(scheduleUID)) {
            sendJsonResponse(request, 200, true, "Schedule template deleted");
        } else {
            JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
            details["resourceType"] = "ScheduleTemplate"; details["resourceId"] = scheduleUID;
            sendJsonError(request, 404, "RESOURCE_NOT_FOUND", "Schedule template not found or deletion failed", details);
        }
    }
    ```

**E. TemplateManager (Cycle Templates) Endpoints:**

*   `GET /api/v1/cycles/templates`: List templates (ID, name). Auth required.
    ```c++
    // In ApiRoutes::registerRoutes()
    server.on("/api/v1/cycles/templates", HTTP_GET, [this](AsyncWebServerRequest *request){ handleGetCycleTemplates(request); });
    ```
    ```c++
    // In ApiRoutes.cpp
    void ApiRoutes::handleGetCycleTemplates(AsyncWebServerRequest *request) {
         handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session) {
            std::vector<BasicTemplateInfo> templateList;
            if (templateManager.listTemplates(templateList)) {
                JsonDocument doc; // v7
                JsonArray array = doc.to<JsonArray>(); // v7
                for(const auto& info : templateList) {
                    JsonObject obj = array.add<JsonObject>(); // v7
                    obj["templateId"] = info.templateId;
                    obj["templateName"] = info.templateName;
                }
                sendJsonResponse(req, 200, true, "", &doc);
            } else {
                sendJsonError(req, 500, "INTERNAL_SERVER_ERROR", "Failed to list cycle templates");
            }
        });
    }
    ```
*   `POST /api/v1/cycles/templates`: Create template (Locked). Use AsyncJsonWebHandler.
    ```c++
    // In ApiRoutes::registerRoutes() - Requires AsyncJsonWebHandler
    /*
    AsyncCallbackJsonWebHandler* postCycleTmplHandler = new AsyncCallbackJsonWebHandler("/api/v1/cycles/templates",
        [this](AsyncWebServerRequest *request, JsonVariant &json) {
            handleLockedRequest(request, UserRole::ADMIN, [this, &json](AsyncWebServerRequest *req, SessionData* session) {
                CycleTemplate templateData;
                // Deserialize json into templateData struct...
                templateData.templateId = jsonObj["templateId"] | ""; // Or generate

                if (templateManager.saveTemplate(templateData)) {
                    JsonDocument doc; doc["templateId"] = templateData.templateId; // v7
                    sendJsonResponse(req, 201, true, "Cycle template created", &doc);
                } else {
                    sendJsonError(req, 500, "FILESYSTEM_WRITE_ERROR", "Failed to create cycle template");
                }
            });
        }, 2048);
    server.addHandler(postCycleTmplHandler);
    */
    ```
*   `GET /api/v1/cycles/templates/{templateId}`: Get template detail. Auth required.
    ```c++
    // In ApiRoutes::registerRoutes()
    server.on("^\\/api\\/v1\\/cycles\\/templates\\/([^\\/]+)$", HTTP_GET, [this](AsyncWebServerRequest *request){ handleGetCycleTemplateDetail(request); });
    ```
    ```c++
    // In ApiRoutes.cpp
    void ApiRoutes::handleGetCycleTemplateDetail(AsyncWebServerRequest *request) {
         handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session) {
            String templateId = req->pathArg(0);
            CycleTemplate templateData;
            if (templateManager.loadTemplate(templateId, templateData)) {
                JsonDocument doc; // v7
                // Serialize templateData struct into doc...
                doc["templateId"] = templateData.templateId;
                doc["templateName"] = templateData.templateName;
                JsonArray seq = doc["sequence"].to<JsonArray>(); // v7
                for(const auto& step : templateData.sequence) {
                    JsonObject sObj = seq.add<JsonObject>(); // v7
                    sObj["step"] = step.step;
                    sObj["libraryScheduleId"] = step.libraryScheduleId;
                    sObj["durationDays"] = step.durationDays;
                }
                sendJsonResponse(req, 200, true, "", &doc);
            } else {
                JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
                details["resourceType"] = "CycleTemplate"; details["resourceId"] = templateId;
                sendJsonError(req, 404, "RESOURCE_NOT_FOUND", "Cycle template not found", details);
            }
        });
    }
    ```
*   `PUT /api/v1/cycles/templates/{templateId}`: Update template (Locked). Use AsyncJsonWebHandler.
    ```c++
    // In ApiRoutes::registerRoutes() - Requires AsyncJsonWebHandler
    /*
    AsyncCallbackJsonWebHandler* putCycleTmplHandler = new AsyncCallbackJsonWebHandler(
        "^\\/api\\/v1\\/cycles\\/templates\\/([^\\/]+)$", HTTP_PUT,
        [this](AsyncWebServerRequest *request, JsonVariant &json) {
            handleLockedRequest(request, UserRole::ADMIN, [this, &json](AsyncWebServerRequest *req, SessionData* session) {
                String templateId = req->pathArg(0);
                CycleTemplate templateData;
                // Deserialize json into templateData struct...
                JsonObject jsonObj = json.as<JsonObject>();
                if ((jsonObj["templateId"] | "") != templateId) { sendJsonError(req, 400, "VALIDATION_INVALID_VALUE", "Template ID mismatch"); return; }
                templateData.templateId = templateId;

                if (templateManager.saveTemplate(templateData)) {
                    sendJsonResponse(req, 200, true, "Cycle template updated");
                } else {
                    sendJsonError(req, 500, "FILESYSTEM_WRITE_ERROR", "Failed to update cycle template");
                }
            });
        }, 2048);
    server.addHandler(putCycleTmplHandler);
    */
    ```
*   `DELETE /api/v1/cycles/templates/{templateId}`: Delete template (Locked).
    ```c++
    // In ApiRoutes::registerRoutes()
    server.on("^\\/api\\/v1\\/cycles\\/templates\\/([^\\/]+)$", HTTP_DELETE, [this](AsyncWebServerRequest *request){
        handleLockedRequest(request, UserRole::ADMIN, [this](AsyncWebServerRequest *req, SessionData* session){ handleDeleteCycleTemplate(req, session); });
    });
    ```
    ```c++
    // In ApiRoutes.cpp
    void ApiRoutes::handleDeleteCycleTemplate(AsyncWebServerRequest *request, SessionData* session) {
        String templateId = request->pathArg(0);
        if (templateManager.deleteTemplate(templateId)) {
            sendJsonResponse(request, 200, true, "Cycle template deleted");
        } else {
            JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
            details["resourceType"] = "CycleTemplate"; details["resourceId"] = templateId;
            sendJsonError(request, 404, "RESOURCE_NOT_FOUND", "Cycle template not found or deletion failed", details);
        }
    }
    ```

**F. CycleManager (Active Cycles & Instances) Endpoints:**

*   `GET /api/v1/cycles/active`: List active cycles (ID, name, state). Auth required (OPERATOR).
    ```c++
    // In ApiRoutes::registerRoutes()
    server.on("/api/v1/cycles/active", HTTP_GET, [this](AsyncWebServerRequest *request){ handleGetActiveCycles(request); });
    ```
    ```c++
    // In ApiRoutes.cpp
    void ApiRoutes::handleGetActiveCycles(AsyncWebServerRequest *request) {
         handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session) {
            std::vector<ActiveCycleBasicInfo> cycleList;
            if (cycleManager.listActiveCycles(cycleList)) {
                JsonDocument doc; // v7
                JsonArray array = doc.to<JsonArray>(); // v7
                for(const auto& info : cycleList) {
                    JsonObject obj = array.add<JsonObject>(); // v7
                    obj["cycleId"] = info.cycleId;
                    obj["cycleName"] = info.cycleName;
                    obj["cycleState"] = cycleStateToString(info.cycleState);
                }
                sendJsonResponse(req, 200, true, "", &doc);
            } else {
                sendJsonError(req, 500, "INTERNAL_SERVER_ERROR", "Failed to list active cycles");
            }
        });
    }
    ```
*   `POST /api/v1/cycles/active`: Create active cycle (Locked). Use AsyncJsonWebHandler. Auth required (ADMIN).
    ```c++
    // In ApiRoutes::registerRoutes() - Requires AsyncJsonWebHandler
    /*
    AsyncCallbackJsonWebHandler* postActiveCycleHandler = new AsyncCallbackJsonWebHandler("/api/v1/cycles/active",
        [this](AsyncWebServerRequest *request, JsonVariant &json) {
            handleLockedRequest(request, UserRole::ADMIN, [this, &json](AsyncWebServerRequest *req, SessionData* session) {
                JsonObject jsonObj = json.as<JsonObject>();
                ActiveCycleConfiguration cycleConfig;
                // Deserialize base fields...
                cycleConfig.cycleId = cycleManager.generateNewCycleId();
                // ... (rest of deserialization and instance creation logic from previous example, using v7 methods) ...

                if (cycleManager.saveActiveCycle(cycleConfig)) {
                    JsonDocument doc; doc["cycleId"] = cycleConfig.cycleId; // v7
                    sendJsonResponse(req, 201, true, "Active cycle created", &doc);
                } else {
                    sendJsonError(req, 500, "FILESYSTEM_WRITE_ERROR", "Failed to save active cycle");
                }
            });
        }, 8192);
    server.addHandler(postActiveCycleHandler);
    */
    ```
*   `GET /api/v1/cycles/active/{cycleId}`: Get active cycle detail. Auth required (OPERATOR).
    ```c++
    // In ApiRoutes::registerRoutes()
    server.on("^\\/api\\/v1\\/cycles\\/active\\/([^\\/]+)$", HTTP_GET, [this](AsyncWebServerRequest *request){ handleGetActiveCycleDetail(request); });
    ```
    ```c++
    // In ApiRoutes.cpp
    void ApiRoutes::handleGetActiveCycleDetail(AsyncWebServerRequest *request) {
         handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session) {
            String cycleId = req->pathArg(0);
            ActiveCycleConfiguration cycleConfig;
            if (cycleManager.loadActiveCycle(cycleId, cycleConfig)) {
                JsonDocument doc; // v7
                // Serialize cycleConfig struct into doc... using v7 methods
                doc["cycleId"] = cycleConfig.cycleId;
                // ... etc ...
                JsonArray seq = doc["cycleSequence"].to<JsonArray>(); // v7
                // ... populate seq ...
                JsonArray outs = doc["associatedOutputs"].to<JsonArray>(); // v7
                // ... populate outs ...
                JsonArray ins = doc["associatedInputs"].to<JsonArray>(); // v7
                // ... populate ins ...
                sendJsonResponse(req, 200, true, "", &doc);
            } else {
                JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
                details["resourceType"] = "ActiveCycle"; details["resourceId"] = cycleId;
                sendJsonError(req, 404, "RESOURCE_NOT_FOUND", "Active cycle not found", details);
            }
        });
    }
    ```
*   `PUT /api/v1/cycles/active/{cycleId}`: Update active cycle (Locked). Use AsyncJsonWebHandler. Auth required (ADMIN).
    ```c++
    // In ApiRoutes::registerRoutes() - Requires AsyncJsonWebHandler
    /*
    AsyncCallbackJsonWebHandler* putActiveCycleHandler = new AsyncCallbackJsonWebHandler(
        "^\\/api\\/v1\\/cycles\\/active\\/([^\\/]+)$", HTTP_PUT,
        [this](AsyncWebServerRequest *request, JsonVariant &json) {
            handleLockedRequest(request, UserRole::ADMIN, [this, &json](AsyncWebServerRequest *req, SessionData* session) {
                String cycleId = req->pathArg(0);
                ActiveCycleConfiguration cycleConfig;
                if (!cycleManager.loadActiveCycle(cycleId, cycleConfig)) {
                     JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>(); details["resourceId"] = cycleId;
                     sendJsonError(req, 404, "RESOURCE_NOT_FOUND", "Active cycle not found", details); return;
                }

                JsonObject jsonObj = json.as<JsonObject>();
                // Merge changes - START SIMPLE: Only allow updating cycleName
                if (jsonObj["cycleName"].is<String>()) { // v7 check
                    cycleConfig.cycleName = jsonObj["cycleName"].as<String>();
                }
                // ... potentially more complex merge logic ...

                if (cycleManager.saveActiveCycle(cycleConfig)) {
                    sendJsonResponse(req, 200, true, "Active cycle updated");
                } else {
                    sendJsonError(req, 500, "FILESYSTEM_WRITE_ERROR", "Failed to update active cycle");
                }
            });
        }, 8192);
    server.addHandler(putActiveCycleHandler);
    */
    ```
*   `DELETE /api/v1/cycles/active/{cycleId}`: Delete active cycle (Locked). Auth required (ADMIN).
    ```c++
    // In ApiRoutes::registerRoutes()
    server.on("^\\/api\\/v1\\/cycles\\/active\\/([^\\/]+)$", HTTP_DELETE, [this](AsyncWebServerRequest *request){
        handleLockedRequest(request, UserRole::ADMIN, [this](AsyncWebServerRequest *req, SessionData* session){ handleDeleteActiveCycle(req, session); });
    });
    ```
    ```c++
    // In ApiRoutes.cpp
    void ApiRoutes::handleDeleteActiveCycle(AsyncWebServerRequest *request, SessionData* session) {
        String cycleId = request->pathArg(0);
        if (cycleManager.deleteActiveCycle(cycleId)) {
            sendJsonResponse(request, 200, true, "Active cycle deleted");
        } else {
            JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
            details["resourceType"] = "ActiveCycle"; details["resourceId"] = cycleId;
            sendJsonError(request, 404, "RESOURCE_NOT_FOUND", "Active cycle not found or deletion failed", details);
        }
    }
    ```
*   `POST /api/v1/cycles/active/{cycleId}/activate`: Activate cycle. Auth required (OPERATOR).
    ```c++
    // In ApiRoutes::registerRoutes()
    server.on("^\\/api\\/v1\\/cycles\\/active\\/([^\\/]+)\\/activate$", HTTP_POST, [this](AsyncWebServerRequest *request){ handlePostActivateCycle(request); });
    ```
    ```c++
    // In ApiRoutes.cpp
    void ApiRoutes::handlePostActivateCycle(AsyncWebServerRequest *request) {
         handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session) {
            String cycleId = req->pathArg(0);
            if (cycleManager.activateCycle(cycleId)) {
                sendJsonResponse(req, 200, true, "Cycle activated");
            } else {
                JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>(); details["resourceId"] = cycleId;
                sendJsonError(req, 404, "RESOURCE_NOT_FOUND", "Cycle not found or activation failed", details); // Or 400/409
            }
        });
    }
    ```
*   `POST /api/v1/cycles/active/{cycleId}/deactivate`: Deactivate cycle. Auth required (OPERATOR).
    ```c++
    // In ApiRoutes::registerRoutes()
    server.on("^\\/api\\/v1\\/cycles\\/active\\/([^\\/]+)\\/deactivate$", HTTP_POST, [this](AsyncWebServerRequest *request){ handlePostDeactivateCycle(request); });
    ```
    ```c++
    // In ApiRoutes.cpp
    void ApiRoutes::handlePostDeactivateCycle(AsyncWebServerRequest *request) {
         handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session) {
            String cycleId = req->pathArg(0);
            if (cycleManager.deactivateCycle(cycleId)) {
                sendJsonResponse(req, 200, true, "Cycle deactivated");
            } else {
                JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>(); details["resourceId"] = cycleId;
                sendJsonError(req, 404, "RESOURCE_NOT_FOUND", "Cycle not found or deactivation failed", details); // Or 400/409
            }
        });
    }
    ```
*   `GET /api/v1/cycles/instances/{instanceId}`: Get instance detail. Auth required (OPERATOR).
    ```c++
    // In ApiRoutes::registerRoutes()
    server.on("^\\/api\\/v1\\/cycles\\/instances\\/([^\\/]+)$", HTTP_GET, [this](AsyncWebServerRequest *request){ handleGetScheduleInstanceDetail(request); });
    ```
    ```c++
    // In ApiRoutes.cpp
    void ApiRoutes::handleGetScheduleInstanceDetail(AsyncWebServerRequest *request) {
         handleAuthorizedRequest(request, UserRole::OPERATOR, [this](AsyncWebServerRequest *req, SessionData* session) {
            String instanceId = req->pathArg(0);
            Schedule scheduleData;
            String instancePath = scheduleManager.getScheduleInstancePath(instanceId);
            if (scheduleManager.loadScheduleInstance(instancePath, scheduleData)) {
                JsonDocument doc; // v7
                // Serialize scheduleData struct into doc... using v7 methods
                sendJsonResponse(req, 200, true, "", &doc);
            } else {
                JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>();
                details["resourceType"] = "ScheduleInstance"; details["resourceId"] = instanceId;
                sendJsonError(req, 404, "RESOURCE_NOT_FOUND", "Schedule instance not found", details);
            }
        });
    }
    ```
*   `PUT /api/v1/cycles/instances/{instanceId}`: Update instance (Locked). **(NEW)** Use AsyncJsonWebHandler. Auth required (ADMIN).
    ```c++
    // In ApiRoutes::registerRoutes() - Requires AsyncJsonWebHandler
    /*
    AsyncCallbackJsonWebHandler* instanceUpdateHandler = new AsyncCallbackJsonWebHandler(
        "^\\/api\\/v1\\/cycles\\/instances\\/([^\\/]+)$", HTTP_PUT,
        [this](AsyncWebServerRequest *request, JsonVariant &json) {
            handleLockedRequest(request, UserRole::ADMIN, [this, &json](AsyncWebServerRequest *req, SessionData* session) {
                String instanceId = req->pathArg(0);
                JsonObject jsonObj = json.as<JsonObject>();
                Schedule updatedInstanceData;
                // Deserialize json into updatedInstanceData... using v7 methods
                if (// deserialization fails // false) { sendJsonError(req, 400, "INVALID_JSON_FORMAT", "Invalid format"); return; }

                String parentCycleId = cycleManager.findParentCycleForInstance(instanceId);
                if (parentCycleId.isEmpty()) {
                    JsonDocument detailsDoc; JsonObject details = detailsDoc.to<JsonObject>(); details["resourceId"] = instanceId;
                    sendJsonError(req, 404, "RESOURCE_NOT_FOUND", "Instance not found or not linked", details); return;
                }
                ActiveCycleConfiguration parentCycle;
                if (!cycleManager.loadActiveCycle(parentCycleId, parentCycle)) { sendJsonError(req, 500, "INTERNAL_SERVER_ERROR", "Failed to load parent cycle"); return; }
                String primaryOutputId = cycleManager.findPrimaryOutput(parentCycle.associatedOutputs);

                // Recalculate volume durations
                for (VolumeEvent& volEvent : updatedInstanceData.volumeEvents) {
                    float durationSec = 0.0;
                    if (!primaryOutputId.isEmpty()) {
                        durationSec = cycleManager.calculateVolumeDurationSec(volEvent.doseVolume, primaryOutputId);
                    }
                    volEvent.calculatedDurationSeconds = (unsigned long)ceil(durationSec);
                }

                String instancePath = scheduleManager.getScheduleInstancePath(instanceId);
                if (scheduleManager.saveScheduleInstance(instancePath, updatedInstanceData)) {
                    sendJsonResponse(req, 200, true, "Instance updated");
                } else {
                    sendJsonError(req, 500, "FILESYSTEM_WRITE_ERROR", "Failed to save instance");
                }
            });
        }, 4096);
    server.addHandler(instanceUpdateHandler);
    */
    ```

## 5. Testing Considerations:

*   Test each endpoint with valid requests (authenticated, authorized, unlocked/locked appropriately).
*   Test authentication failures (no session, invalid session) -> Expect 401 `UNAUTHENTICATED`.
*   Test authorization failures (e.g., Operator trying Admin actions) -> Expect 403 `INSUFFICIENT_PERMISSIONS`.
*   **Test Lock Timeout (30 min + 5s buffer):**
    *   Acquire lock (Admin), wait > 30m 5s, verify `GET /api/v1/system/lock` shows expired/unlocked (`isLocked: false`).
    *   Acquire lock (Admin), wait > 30m 5s, verify another Admin *can* acquire the lock (`POST /api/v1/system/lock` returns SUCCESS_ACQUIRED).
    *   Acquire lock (Admin), wait > 30m 5s, verify the original Admin *cannot* perform locked actions (e.g., `POST /api/v1/outputs`) and gets 403 `CONFIG_LOCK_EXPIRED`.
    *   Acquire lock (Admin), wait < 30m, call `POST /api/v1/system/lock` again (manual refresh), verify timestamp is updated and timeout extended via `GET /api/v1/system/lock` (check `lockRemainingSeconds` counts down from 1800).
    *   Acquire lock (Admin A), verify another Admin B *cannot* acquire lock before timeout (`POST /api/v1/system/lock` returns 403 `CONFIG_LOCKED_BY_OTHER`).
    *   Acquire lock (Admin A), verify another Admin B *cannot* perform locked actions (e.g., `POST /api/v1/outputs` returns 403 `CONFIG_LOCKED_BY_OTHER`).
*   Test invalid request bodies (missing fields, incorrect data types) -> Expect 400 with appropriate `VALIDATION_*` code.
*   Test invalid JSON format -> Expect 400 `INVALID_JSON_FORMAT`.
*   Test handling of non-existent resources (e.g., GET/PUT/DELETE with invalid ID) -> Expect 404 `RESOURCE_NOT_FOUND`.
*   Test conflict scenarios (creating existing ID, deleting used resource, activating cycle with used output) -> Expect 409 with appropriate `*_CONFLICT` or `RESOURCE_IN_USE` code.
*   Test internal errors (filesystem, memory) -> Expect 500 with appropriate `FILESYSTEM_*`, `JSON_MEMORY_ERROR`, or `INTERNAL_SERVER_ERROR` code.
*   Test response data formats against specifications for both success and **structured error** cases.
*   **Test ArduinoJson v7 Usage:** Ensure no memory leaks occur due to elastic capacity. Check `overflowed()` in critical response generation.
*   Test `PUT /api/v1/cycles/instances/{instanceId}`: Verify volume duration recalculation. Test with cycles having no primary output. Test with invalid instance IDs.
*   Use tools like Postman or curl for manual testing.
*   Consider automated integration tests if feasible.

---