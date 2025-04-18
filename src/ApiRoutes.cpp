#include "ApiRoutes.h"
#include "DebugConfig.h" // Include for SCH_API_DEBUG macros
#include <FS.h>         // Required for Filesystem operations
#include <LittleFS.h> // Required for serveStatic
#include "AuthUtils.h" // For password verification
#include <Arduino.h>   // For Serial
#include <functional>  // For std::bind or lambdas

// Constructor implementation
/**
 * @brief Constructs an ApiRoutes object.
 *
 * Initializes the API routes handler with references to necessary managers
 * (UserManager, SessionManager, ScheduleManager, LockManager) and sets
 * whether HTTPS is enabled.
 *
 * @param userMgr Reference to the UserManager instance.
 * @param sessionMgr Reference to the SessionManager instance.
 * @param scheduleMgr Reference to the ScheduleManager instance.
 * @param lockMgr Reference to the LockManager instance.
 * @param https Boolean indicating if HTTPS is enabled for secure cookies and headers.
 */
ApiRoutes::ApiRoutes(UserManager& userMgr, SessionManager& sessionMgr, ScheduleManager& scheduleMgr, LockManager& lockMgr, bool https)
    : userManager(userMgr),
      sessionManager(sessionMgr),
      scheduleManager(scheduleMgr),
      lockManager(lockMgr),
      httpsEnabled(https)
{
    API_DEBUG_PRINTLN("ApiRoutes initialized.");
}

// --- Private Helper Functions ---

// Adds common security headers to responses (only call when HTTPS is enabled)
/**
 * @brief Adds common security headers to an HTTP response.
 *
 * This function enhances security by adding headers like Strict-Transport-Security,
 * Content-Security-Policy, X-Content-Type-Options, X-Frame-Options,
 * Referrer-Policy, and Permissions-Policy. It should only be called when HTTPS
 * is enabled to ensure the headers are effective and appropriate.
 *
 * @param response Pointer to the AsyncWebServerResponse object to which headers will be added.
 *                 If null or if HTTPS is not enabled, the function returns without adding headers.
 */
void ApiRoutes::addSecurityHeaders(AsyncWebServerResponse *response) {
    if (!response || !this->httpsEnabled) return;
    API_DEBUG_PRINTLN("Adding security headers.");
    response->addHeader("Strict-Transport-Security", "max-age=31536000; includeSubDomains");
    response->addHeader("Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self'; object-src 'none'; frame-ancestors 'none';");
    response->addHeader("X-Content-Type-Options", "nosniff");
    response->addHeader("X-Frame-Options", "DENY");
    response->addHeader("Referrer-Policy", "no-referrer");
    response->addHeader("Permissions-Policy", "microphone=(), geolocation=()");
}


// --- Private Request Handlers ---

/**
 * @brief Handles POST requests to the /api/login endpoint.
 *
 * Attempts to authenticate a user based on the provided username and password
 * in the POST request body. If authentication is successful, it creates a new
 * session, sets a session cookie (HttpOnly, SameSite=Strict, Secure if HTTPS),
 * and sends a success response. If authentication fails or parameters are missing,
 * it sends an appropriate error response (400 Bad Request, 401 Unauthorized,
 * 500 Internal Server Error).
 *
 * @param request Pointer to the AsyncWebServerRequest object containing the login request details.
 *                Expects 'username' and 'password' as POST parameters.
 */
void ApiRoutes::handleLogin(AsyncWebServerRequest *request) {
    API_DEBUG_PRINTLN("API: handleLogin request received.");
    if (!request->hasParam("username", true) || !request->hasParam("password", true)) {
        API_DEBUG_PRINTLN("API: handleLogin - Bad Request: Missing username or password.");
        request->send(400, "text/plain", "Bad Request: Missing username or password.");
        return;
    }
    String username = request->getParam("username", true)->value();
    String password = request->getParam("password", true)->value();
    API_DEBUG_PRINTF("API: handleLogin - Attempting login for user: %s\n", username.c_str());

    UserAccount user;
    API_DEBUG_PRINTF("API: handleLogin - Address of this->userManager: %p\n", &(this->userManager)); // Check address
    API_DEBUG_PRINTLN("API: handleLogin - Calling findUserByUsername...");
    bool userFound = this->userManager.findUserByUsername(username, user);
    API_DEBUG_PRINTF("API: handleLogin - findUserByUsername returned: %s\n", userFound ? "true" : "false");

    if (userFound) {
        API_DEBUG_PRINTLN("API: handleLogin - User found. Calling verifyPassword...");
        // Add checks for potentially null pointers before calling verifyPassword
        if (user.hashedPassword.isEmpty() || user.salt.isEmpty()) {
             API_DEBUG_PRINTLN("API: handleLogin - Error: User found but has empty password or salt!");
             request->send(500, "text/plain", "Internal Server Error: User data corrupted.");
             return; // Prevent calling verifyPassword with invalid data
        }
        bool passwordVerified = AuthUtils::verifyPassword(password, user.hashedPassword, user.salt);
        API_DEBUG_PRINTF("API: handleLogin - verifyPassword returned: %s\n", passwordVerified ? "true" : "false");

        if (passwordVerified) {
            API_DEBUG_PRINTF("API: handleLogin - Login successful for user: %s\n", username.c_str());
            SessionData newSession = this->sessionManager.createSession(user.username, user.role, request);
            if (newSession.isValid()) {
                API_DEBUG_PRINTF("API: handleLogin - Session created: %s\n", newSession.sessionId.c_str());
                AsyncResponseStream *response = request->beginResponseStream("text/plain");
                String cookieHeader = "session_id=" + newSession.sessionId + "; Path=/; Max-Age=900; HttpOnly; SameSite=Strict";
                if (this->httpsEnabled) { cookieHeader += "; Secure"; }
                response->addHeader("Set-Cookie", cookieHeader);
                this->addSecurityHeaders(response); // Use class method
                response->print("Login Successful");
                request->send(response);
            } else {
                API_DEBUG_PRINTLN("API: handleLogin - Failed to create session.");
                request->send(500, "text/plain", "Internal Server Error: Could not create session.");
            }
            return;
        } else {
            API_DEBUG_PRINTF("API: handleLogin - Login failed: Incorrect password for user: %s\n", username.c_str());
            request->send(401, "text/plain", "Unauthorized: Invalid credentials.");
        }
    } else {
        API_DEBUG_PRINTF("API: handleLogin - Login failed: User not found: %s\n", username.c_str());
        request->send(401, "text/plain", "Unauthorized: Invalid credentials.");
    }
}

/**
 * @brief Handles POST requests to the /api/logout endpoint.
 *
 * Invalidates the current user session based on the session cookie present
 * in the request. It then clears the session cookie by setting its Max-Age to 0
 * and sends a success response.
 *
 * @param request Pointer to the AsyncWebServerRequest object containing the logout request details.
 *                Uses the 'session_id' cookie to identify the session to invalidate.
 */
void ApiRoutes::handleLogout(AsyncWebServerRequest *request) {
     API_DEBUG_PRINTLN("API: handleLogout request received.");
    this->sessionManager.invalidateSession(request);
    AsyncResponseStream *response = request->beginResponseStream("text/plain");
    String clearCookieHeader = "session_id=; Path=/; Max-Age=0; Expires=Thu, 01 Jan 1970 00:00:00 GMT; HttpOnly; SameSite=Strict";
     if (this->httpsEnabled) { clearCookieHeader += "; Secure"; }
    response->addHeader("Set-Cookie", clearCookieHeader);
    this->addSecurityHeaders(response); // Use class method
    response->print("Logout Successful");
    request->send(response);
    API_DEBUG_PRINTLN("API: handleLogout - Logout successful.");
    return; // Explicit return added
}

// API endpoint to get current user information
/**
 * @brief Handles GET requests to the /api/user endpoint.
 *
 * Validates the user's session using the session cookie. If the session is valid,
 * it retrieves the username and role associated with the session and returns them
 * in a JSON response. If the session is invalid or not present, it returns a
 * 401 Unauthorized error.
 *
 * @param request Pointer to the AsyncWebServerRequest object containing the request details.
 *                Uses the 'session_id' cookie for session validation.
 */
void ApiRoutes::handleGetUserInfo(AsyncWebServerRequest *request) {
    API_DEBUG_PRINTLN("API: handleGetUserInfo request received.");
    SessionData* session = this->sessionManager.validateSession(request);
    if (!session) {
        API_DEBUG_PRINTLN("API: handleGetUserInfo - Not authenticated.");
        request->send(401, "application/json", "{\"error\":\"Not authenticated\"}");
        return;
    }
    API_DEBUG_PRINTF("API: handleGetUserInfo - User: %s, Role: %s\n", session->username.c_str(), roleToString(session->userRole).c_str());
    JsonDocument doc;
    doc["username"] = session->username;
    doc["role"] = roleToString(session->userRole);
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonResponse);
    this->addSecurityHeaders(response); // Use class method
    request->send(response);
}

// --- Schedule API Handlers ---

// GET /api/schedules - List all schedules
/**
 * @brief Handles GET requests to the /api/schedules endpoint.
 *
 * Retrieves a list of all available schedules (UID, lock status, lockedBy user)
 * after validating the user's session. Returns the list as a JSON array.
 * Requires an authenticated session. Returns 401 if not authenticated,
 * 500 if the schedule list cannot be loaded.
 *
 * @param request Pointer to the AsyncWebServerRequest object.
 */
void ApiRoutes::handleGetSchedules(AsyncWebServerRequest *request) {
    SCH_API_DEBUG_PRINTLN("API: handleGetSchedules request received.");
    SessionData* session = this->sessionManager.validateSession(request);
    if (!session) { SCH_API_DEBUG_PRINTLN("API: handleGetSchedules - Not authenticated."); request->send(401, "application/json", "{\"error\":\"Not authenticated\"}"); return; }

    std::vector<ScheduleFile> scheduleList;
    if (!this->scheduleManager.getScheduleList(scheduleList)) { SCH_API_DEBUG_PRINTLN("API: handleGetSchedules - Failed to load schedule list."); request->send(500, "application/json", "{\"error\":\"Failed to load schedule list\"}"); return; }

    SCH_API_DEBUG_PRINTF("API: handleGetSchedules - Found %d schedules.\n", scheduleList.size());
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();
    for (const auto& sf : scheduleList) { JsonObject obj = array.add<JsonObject>(); obj["uid"] = sf.scheduleUID; obj["locked"] = sf.persistentLockLevel; obj["lockedBy"] = sf.lockedBy; }
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonResponse);
    this->addSecurityHeaders(response); // Use class method
    request->send(response);
}

// GET /api/schedule?uid={uid} - Get details of a specific schedule
/**
 * @brief Handles GET requests to the /api/schedule endpoint with a UID parameter.
 *
 * Retrieves the full details of a specific schedule identified by the 'uid'
 * query parameter. Validates the user's session first. Returns the schedule
 * data as a JSON object. Requires an authenticated session and a valid 'uid'
 * parameter. Returns 401 if not authenticated, 400 if 'uid' is missing,
 * 404 if the schedule is not found or fails to load.
 *
 * @param request Pointer to the AsyncWebServerRequest object. Expects 'uid' query parameter.
 */
void ApiRoutes::handleGetSchedule(AsyncWebServerRequest *request) {
    SCH_API_DEBUG_PRINTLN("API: handleGetSchedule request received.");
    SessionData* session = this->sessionManager.validateSession(request);
    if (!session) { SCH_API_DEBUG_PRINTLN("API: handleGetSchedule - Not authenticated."); request->send(401, "application/json", "{\"error\":\"Not authenticated\"}"); return; }
    if (!request->hasParam("uid")) { SCH_API_DEBUG_PRINTLN("API: handleGetSchedule - Missing UID parameter."); request->send(400, "application/json", "{\"error\":\"Missing schedule UID parameter\"}"); return; }

    String uid = request->getParam("uid")->value();
    SCH_API_DEBUG_PRINTF("API: handleGetSchedule - Requesting schedule UID: %s\n", uid.c_str());
    Schedule schedule;
    if (!this->scheduleManager.loadSchedule(uid, schedule)) { SCH_API_DEBUG_PRINTF("API: handleGetSchedule - Schedule not found or failed to load: %s\n", uid.c_str()); request->send(404, "application/json", "{\"error\":\"Schedule not found or failed to load\"}"); return; }

    // *** ADDED DEBUG LOG ***
    SCH_API_DEBUG_PRINTF("API: handleGetSchedule - Loaded schedule object event counts: AP=%d, DUR=%d, VOL=%d\n",
                         schedule.autopilotWindows.size(),
                         schedule.durationEvents.size(),
                         schedule.volumeEvents.size());
    // ***********************

    SCH_API_DEBUG_PRINTF("API: handleGetSchedule - Loaded schedule: %s\n", schedule.scheduleName.c_str());
    JsonDocument doc;
    doc["scheduleName"] = schedule.scheduleName; doc["lightsOnTime"] = schedule.lightsOnTime; doc["lightsOffTime"] = schedule.lightsOffTime; doc["scheduleUID"] = schedule.scheduleUID;
    JsonArray apArray = doc["autopilotWindows"].to<JsonArray>();
    for (const auto& apw : schedule.autopilotWindows) { JsonObject apObj = apArray.add<JsonObject>(); apObj["startTime"] = apw.startTime; apObj["endTime"] = apw.endTime; apObj["matricTension"] = apw.matricTension; apObj["doseVolume"] = apw.doseVolume; apObj["settlingTime"] = apw.settlingTime; }
    JsonArray durArray = doc["durationEvents"].to<JsonArray>();
    for (const auto& de : schedule.durationEvents) { JsonObject durObj = durArray.add<JsonObject>(); durObj["startTime"] = de.startTime; durObj["duration"] = de.duration; durObj["endTime"] = de.endTime; }
    JsonArray volArray = doc["volumeEvents"].to<JsonArray>();
    for (const auto& ve : schedule.volumeEvents) { JsonObject volObj = volArray.add<JsonObject>(); volObj["startTime"] = ve.startTime; volObj["doseVolume"] = ve.doseVolume; }
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonResponse);
    this->addSecurityHeaders(response); // Use class method
    request->send(response);
}

// DELETE /api/schedule?uid={uid} - Delete a schedule
/**
 * @brief Handles DELETE requests to the /api/schedule endpoint with a UID parameter.
 *
 * Deletes a specific schedule identified by the 'uid' query parameter.
 * Requires an authenticated session with MANAGER or ADMIN role.
 * Checks for persistent locks (template/cycle) and edit locks before deletion.
 * Releases any edit lock held by the requesting user upon successful deletion.
 * Returns 401 if not authenticated, 403 if permission denied (role or lock),
 * 400 if 'uid' is missing, 404 if schedule not found, 409 if locked by another user,
 * 500 if deletion fails.
 *
 * @param request Pointer to the AsyncWebServerRequest object. Expects 'uid' query parameter.
 */
void ApiRoutes::handleDeleteSchedule(AsyncWebServerRequest *request) {
    SCH_API_DEBUG_PRINTLN("API: handleDeleteSchedule request received.");
    SessionData* session = this->sessionManager.validateSession(request);
    if (!session) { SCH_API_DEBUG_PRINTLN("API: handleDeleteSchedule - Not authenticated."); request->send(401, "application/json", "{\"error\":\"Not authenticated\"}"); return; }
    if (session->userRole < MANAGER) { SCH_API_DEBUG_PRINTF("API: handleDeleteSchedule - Permission denied for user %s (role %d).\n", session->username.c_str(), session->userRole); request->send(403, "application/json", "{\"error\":\"Permission denied\"}"); return; }
    if (!request->hasParam("uid")) { SCH_API_DEBUG_PRINTLN("API: handleDeleteSchedule - Missing UID parameter."); request->send(400, "application/json", "{\"error\":\"Missing schedule UID parameter\"}"); return; }

    String uid = request->getParam("uid")->value();
    SCH_API_DEBUG_PRINTF("API: handleDeleteSchedule - Attempting to delete schedule UID: %s by user %s\n", uid.c_str(), session->username.c_str());
    String resourceId = "schedule_" + uid;
    int persistentLockLevel = this->scheduleManager.getPersistentLockLevel(uid);

    if (persistentLockLevel == 1 || persistentLockLevel == 2) { SCH_API_DEBUG_PRINTF("API: handleDeleteSchedule - Schedule %s is locked by template/cycle.\n", uid.c_str()); request->send(403, "application/json", "{\"error\":\"Schedule is locked by a template or active cycle and cannot be deleted.\"}"); return; }
    if (persistentLockLevel < 0) { SCH_API_DEBUG_PRINTF("API: handleDeleteSchedule - Schedule %s not found in index.\n", uid.c_str()); request->send(404, "application/json", "{\"error\":\"Schedule not found in index.\"}"); return; }

    FileLock lockInfo;
    if (this->lockManager.isLocked(resourceId, &lockInfo) && lockInfo.sessionId != session->sessionId) { SCH_API_DEBUG_PRINTF("API: handleDeleteSchedule - Schedule %s is locked by user %s.\n", uid.c_str(), lockInfo.username.c_str()); request->send(409, "application/json", "{\"error\":\"Schedule is currently being edited by " + lockInfo.username + "\"}"); return; }

    if (this->scheduleManager.deleteSchedule(uid)) {
        SCH_API_DEBUG_PRINTF("API: handleDeleteSchedule - Schedule %s deleted successfully.\n", uid.c_str());
        this->lockManager.releaseLock(resourceId, session->sessionId); // Release edit lock if held
        request->send(200, "application/json", "{\"message\":\"Schedule deleted successfully\"}");
    } else {
        SCH_API_DEBUG_PRINTF("API: handleDeleteSchedule - Failed to delete schedule %s.\n", uid.c_str());
        request->send(500, "application/json", "{\"error\":\"Failed to delete schedule\"}");
    }
}

// Route-Specific Body Handler for POST/PUT /api/schedule
/**
 * @brief Handles the body content for POST and PUT requests to /api/schedule.
 *
 * This function is registered as the body handler for schedule creation (POST)
 * and updates (PUT). It accumulates the request body chunks into a buffer.
 * Once the entire body is received, it parses the JSON content.
 *
 * For POST: Creates a new schedule with the provided name and data. Requires
 *           MANAGER or ADMIN role. Returns 201 Created with the new schedule UID
 *           and name on success.
 * For PUT: Updates an existing schedule identified by the 'uid' query parameter.
 *          Requires MANAGER or ADMIN role. Checks for persistent locks and
 *          acquires an edit lock (implicitly if not already held). Returns 200 OK
 *          on success.
 *
 * Handles authentication, authorization, JSON parsing errors, payload size limits,
 * lock conflicts, and file system errors, returning appropriate HTTP status codes.
 * Cleans up the allocated buffer after processing.
 *
 * @param request Pointer to the AsyncWebServerRequest object.
 * @param data Pointer to the current chunk of body data.
 * @param len Length of the current data chunk.
 * @param index Starting index of the current data chunk in the total body.
 * @param total Total expected length of the request body.
 */
void ApiRoutes::handleSchedulePostPutBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {

    String* buffer = nullptr; // Pointer to the buffer stored in request context

    // --- Buffer Allocation and Data Appending ---
    if (index == 0) { // First chunk
         SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - START for %s %s (Total: %d)\n", request->methodToString(), request->url().c_str(), total);
         if (total > 10240) { // Example limit: 10KB
             SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - Body too large: %d bytes. Aborting.\n", total);
             request->send(413, "text/plain", "Payload Too Large"); // Send 413 Payload Too Large
             // No buffer allocated yet, just return
             return;
         }
        buffer = new (std::nothrow) String(); // Allocate buffer safely
        if (buffer) {
            buffer->reserve(total); // Reserve space
            request->_tempObject = buffer; // Store pointer in request context
        } else {
            SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Failed to allocate buffer for request body!");
            request->send(500, "text/plain", "Internal Server Error: Failed to allocate buffer");
            return; // Stop processing
        }
    } else {
        buffer = (String*)request->_tempObject; // Retrieve buffer from context
    }

    if (!buffer) {
        SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Invalid buffer pointer retrieved.");
        // If buffer is null here, something went wrong earlier, but we might not be able to send a response if the request is already handled.
        return;
    } // Stop if buffer is invalid

    // Append data chunk to buffer
    if (buffer->length() + len <= 10240) { // Check limit again during append
         buffer->concat((char*)data, len);
    } else {
         SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Body buffer overflow during reception!");
         delete buffer; // Clean up allocated buffer
         request->_tempObject = nullptr; // Nullify context pointer
         request->send(413, "text/plain", "Payload Too Large during reception"); // Try to send error
         return; // Stop processing
    }
    // API_DEBUG_PRINTF("Body chunk: index=%d, len=%d, total=%d. Buffer size=%d\n", index, len, total, buffer->length()); // Verbose Debug

    // --- Process Request ONLY on the LAST chunk ---
    if (index + len == total) {
        SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - END. Final size: %d\n", buffer->length());
        SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - Received Body: %s\n", buffer->c_str()); // Log the full body

        // --- Start of processing logic ---
        SessionData* session = this->sessionManager.validateSession(request);
        if (!session) {
            SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Not authenticated.");
            request->send(401, "application/json", "{\"error\":\"Not authenticated\"}");
            delete buffer; request->_tempObject = nullptr; return;
        }
        if (session->userRole < MANAGER) {
            SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - Permission denied for user %s (role %d).\n", session->username.c_str(), session->userRole);
            request->send(403, "application/json", "{\"error\":\"Permission denied\"}");
            delete buffer; request->_tempObject = nullptr; return;
        }

        // Parse the buffered body
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, *buffer);

        // Clean up buffer *after* parsing, *before* returning in error cases below
        String bodyContent = *buffer; // Copy buffer content if needed for later logging
        delete buffer;
        request->_tempObject = nullptr;

        if (error) {
            SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - JSON Deserialization error: %s\n", error.c_str());
            SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - Invalid JSON received: %s\n", bodyContent.c_str());
            request->send(400, "application/json", "{\"error\":\"Invalid JSON body\"}");
            return;
        }

        JsonObject bodyJson = doc.as<JsonObject>();

        // --- Handle POST (Create) ---
        if (request->method() == HTTP_POST) {
            SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Handling POST (Create).");
            if (bodyJson["name"].isNull() || !bodyJson["name"].is<String>()) {
                 SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Missing or invalid 'name' field.");
                 request->send(400, "application/json", "{\"error\":\"Missing or invalid 'name' field in JSON body\"}");
            } else {
                String scheduleName = bodyJson["name"].as<String>();
                SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - Creating schedule with name: %s\n", scheduleName.c_str());
                if (scheduleName.isEmpty()) {
                    SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Schedule name cannot be empty.");
                    request->send(400, "application/json", "{\"error\":\"Schedule name cannot be empty\"}");
                } else {
                    Schedule newSchedule;
                    if (this->scheduleManager.createSchedule(scheduleName, newSchedule)) {
                        SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - Schedule object created with UID: %s\n", newSchedule.scheduleUID.c_str());
                        // Populate schedule from JSON
                        newSchedule.lightsOnTime = bodyJson["lightsOnTime"] | 0;
                        newSchedule.lightsOffTime = bodyJson["lightsOffTime"] | 0;

                        // *** PARSE EVENTS FROM RECEIVED JSON ***
                        SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Parsing events for new schedule...");
                        JsonArray apArray = bodyJson["autopilotWindows"];
                        if (!apArray.isNull()) { for (JsonObject apObj : apArray) { AutopilotWindow apw; apw.startTime = apObj["startTime"] | 0; apw.endTime = apObj["endTime"] | 0; apw.matricTension = apObj["matricTension"] | 0.0f; apw.doseVolume = apObj["doseVolume"] | 0; apw.settlingTime = apObj["settlingTime"] | 0; if (apw.isValid()) newSchedule.autopilotWindows.push_back(apw); else { SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Invalid AP Window data received during create.");} } }
                        JsonArray durArray = bodyJson["durationEvents"];
                        if (!durArray.isNull()) { for (JsonObject durObj : durArray) { DurationEvent de; de.startTime = durObj["startTime"] | 0; de.duration = durObj["duration"] | 0; de.endTime = de.startTime + (int)ceil(de.duration / 60.0); if (de.endTime > 1439) de.endTime = 1439; if (de.isValid()) newSchedule.durationEvents.push_back(de); else { SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Invalid Duration Event data received during create.");} } }
                        JsonArray volArray = bodyJson["volumeEvents"];
                        if (!volArray.isNull()) { for (JsonObject volObj : volArray) { VolumeEvent ve; ve.startTime = volObj["startTime"] | 0; ve.doseVolume = volObj["doseVolume"] | 0; if (ve.isValid()) newSchedule.volumeEvents.push_back(ve); else { SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Invalid Volume Event data received during create.");} } }

                        // Sort events (optional but good practice)
                        std::sort(newSchedule.autopilotWindows.begin(), newSchedule.autopilotWindows.end(), ScheduleManager::compareAutopilotWindows);
                        std::sort(newSchedule.durationEvents.begin(), newSchedule.durationEvents.end(), ScheduleManager::compareDurationEvents);
                        std::sort(newSchedule.volumeEvents.begin(), newSchedule.volumeEvents.end(), ScheduleManager::compareVolumeEvents);
                        // *** END EVENT PARSING ***

                        if (this->scheduleManager.saveSchedule(newSchedule)) {
                            SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - New schedule saved successfully: %s\n", newSchedule.scheduleUID.c_str());
                            JsonDocument responseDoc;
                            responseDoc["scheduleUID"] = newSchedule.scheduleUID;
                            responseDoc["scheduleName"] = newSchedule.scheduleName; // Return name as well
                            String jsonResponse;
                            serializeJson(responseDoc, jsonResponse);
                            AsyncWebServerResponse *response = request->beginResponse(201, "application/json", jsonResponse);
                            this->addSecurityHeaders(response);
                            request->send(response);
                        } else {
                            SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - Failed to save new schedule file for UID: %s\n", newSchedule.scheduleUID.c_str());
                            request->send(500, "application/json", "{\"error\":\"Failed to save new schedule file\"}");
                        }
                    } else {
                        SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Failed to create schedule object.");
                        request->send(500, "application/json", "{\"error\":\"Failed to create schedule object\"}");
                    }
                }
            }
        }
        // --- Handle PUT (Update) ---
        else if (request->method() == HTTP_PUT) {
            SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Handling PUT (Update).");
            if (!request->hasParam("uid")) {
                SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Missing UID parameter for PUT.");
                request->send(400, "application/json", "{\"error\":\"Missing schedule UID parameter for PUT\"}");
            } else {
                String uid = request->getParam("uid")->value();
                SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - Updating schedule UID: %s\n", uid.c_str());
                String resourceId = "schedule_" + uid;
                int persistentLockLevel = this->scheduleManager.getPersistentLockLevel(uid);

                if (persistentLockLevel == 1 || persistentLockLevel == 2) {
                    SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - Schedule %s is locked by template/cycle.\n", uid.c_str());
                    request->send(403, "application/json", "{\"error\":\"Schedule is locked by a template or active cycle and cannot be edited.\"}");
                } else if (persistentLockLevel < 0) {
                    SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - Schedule %s not found in index for PUT.\n", uid.c_str());
                    request->send(404, "application/json", "{\"error\":\"Schedule not found in index.\"}");
                } else {
                    // Acquire lock (Check if we already have it from the frontend's explicit lock call)
                    FileLock currentLockInfo;
                    bool hasImplicitLock = this->lockManager.getLockInfo(resourceId, currentLockInfo) && currentLockInfo.sessionId == session->sessionId;

                    if (!hasImplicitLock) {
                         SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - Attempting implicit lock acquire for PUT on %s by %s\n", uid.c_str(), session->username.c_str());
                         if (!this->lockManager.acquireLock(resourceId, EDITING_SCHEDULE, *session)) {
                             FileLock lockInfo; // Re-fetch lock info in case someone else got it
                             if (this->lockManager.getLockInfo(resourceId, lockInfo)) { SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - Schedule %s locked by %s.\n", uid.c_str(), lockInfo.username.c_str()); request->send(409, "application/json", "{\"error\":\"Schedule is currently being edited by " + lockInfo.username + "\"}"); }
                             else { SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Failed to acquire implicit editing lock."); request->send(500, "application/json", "{\"error\":\"Failed to acquire editing lock\"}"); }
                             return; // Exit if lock fails
                         }
                         SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Implicit lock acquired.");
                         hasImplicitLock = true; // Mark that we acquired it implicitly
                    } else {
                         SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - User %s already holds lock for %s.\n", session->username.c_str(), uid.c_str());
                    }


                    // Lock acquired (implicitly or explicitly), proceed with update
                    Schedule updatedSchedule;
                    // Load existing schedule to preserve UID and potentially other fields if needed
                    // Although the JS sends the full object, loading ensures consistency
                    if (!this->scheduleManager.loadSchedule(uid, updatedSchedule)) {
                         SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - Failed to load schedule %s for update after acquiring lock.\n", uid.c_str());
                         this->lockManager.releaseLock(resourceId, session->sessionId); // Release lock
                         request->send(500, "application/json", "{\"error\":\"Failed to load schedule for update\"}");
                         return;
                    }


                    updatedSchedule.scheduleName = bodyJson["scheduleName"] | updatedSchedule.scheduleName; // Use existing if not provided
                    updatedSchedule.lightsOnTime = bodyJson["lightsOnTime"] | updatedSchedule.lightsOnTime;
                    updatedSchedule.lightsOffTime = bodyJson["lightsOffTime"] | updatedSchedule.lightsOffTime;
                    // UID is already set from loadSchedule
                    updatedSchedule.autopilotWindows.clear();
                    updatedSchedule.durationEvents.clear();
                    updatedSchedule.volumeEvents.clear();

                    // Parse arrays...
                    JsonArray apArray = bodyJson["autopilotWindows"];
                    if (!apArray.isNull()) { for (JsonObject apObj : apArray) { AutopilotWindow apw; apw.startTime = apObj["startTime"] | 0; apw.endTime = apObj["endTime"] | 0; apw.matricTension = apObj["matricTension"] | 0.0f; apw.doseVolume = apObj["doseVolume"] | 0; apw.settlingTime = apObj["settlingTime"] | 0; if (apw.isValid()) updatedSchedule.autopilotWindows.push_back(apw); else { SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Invalid AP Window data received.");} } }
                    JsonArray durArray = bodyJson["durationEvents"];
                    if (!durArray.isNull()) { for (JsonObject durObj : durArray) { DurationEvent de; de.startTime = durObj["startTime"] | 0; de.duration = durObj["duration"] | 0; de.endTime = de.startTime + (int)ceil(de.duration / 60.0); if (de.endTime > 1439) de.endTime = 1439; if (de.isValid()) updatedSchedule.durationEvents.push_back(de); else { SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Invalid Duration Event data received.");} } }
                    JsonArray volArray = bodyJson["volumeEvents"];
                    if (!volArray.isNull()) { for (JsonObject volObj : volArray) { VolumeEvent ve; ve.startTime = volObj["startTime"] | 0; ve.doseVolume = volObj["doseVolume"] | 0; if (ve.isValid()) updatedSchedule.volumeEvents.push_back(ve); else { SCH_API_DEBUG_PRINTLN("API: handleSchedulePostPutBody - Invalid Volume Event data received.");} } }

                    // Sort
                    std::sort(updatedSchedule.autopilotWindows.begin(), updatedSchedule.autopilotWindows.end(), ScheduleManager::compareAutopilotWindows);
                    std::sort(updatedSchedule.durationEvents.begin(), updatedSchedule.durationEvents.end(), ScheduleManager::compareDurationEvents);
                    std::sort(updatedSchedule.volumeEvents.begin(), updatedSchedule.volumeEvents.end(), ScheduleManager::compareVolumeEvents);

                    // TODO: Phase 6 Validation here

                    // Save
                    if (this->scheduleManager.saveSchedule(updatedSchedule)) {
                        SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - Schedule %s updated successfully.\n", uid.c_str());
                        // Keep lock? The JS releases it on success. Let's keep it consistent and NOT release here.
                        // this->lockManager.releaseLock(resourceId, session->sessionId);
                        request->send(200, "application/json", "{\"message\":\"Schedule updated successfully\"}");
                    } else {
                        SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - Failed to save updated schedule %s.\n", uid.c_str());
                        // Don't release lock on failure, user might want to retry
                        // this->lockManager.releaseLock(resourceId, session->sessionId);
                        request->send(500, "application/json", "{\"error\":\"Failed to save updated schedule\"}");
                    }
                    // } // End lock acquired block <-- This was incorrect, lock handling is inside now
                } // End persistent lock check block
            } // End has UID check block
        } // End PUT block
        else {
             SCH_API_DEBUG_PRINTF("API: handleSchedulePostPutBody - Method %s not allowed.\n", request->methodToString());
             request->send(405, "application/json", "{\"error\":\"Method not allowed for this body handler\"}");
        }

        // Buffer cleanup is now done earlier after parsing

    } // End if last chunk
}


// --- New Handler for POST /api/schedule/lock ---
/**
 * @brief Handles POST requests to the /api/schedule/lock endpoint.
 *
 * Attempts to acquire an exclusive edit lock on a specific schedule, identified
 * by the 'uid' query parameter, for the currently authenticated user.
 * Requires an authenticated session with MANAGER or ADMIN role.
 * Checks for persistent locks (template/cycle) before attempting to acquire the lock.
 * Returns 200 OK if the lock is acquired successfully.
 * Returns 401 if not authenticated, 403 if permission denied (role or persistent lock),
 * 400 if 'uid' is missing, 404 if schedule not found, 409 if already locked by
 * another user, 500 if lock acquisition fails for other reasons.
 *
 * @param request Pointer to the AsyncWebServerRequest object. Expects 'uid' query parameter.
 */
void ApiRoutes::handleScheduleLockPost(AsyncWebServerRequest *request) {
    SCH_API_DEBUG_PRINTLN("API: handleScheduleLockPost request received.");

    SessionData* session = this->sessionManager.validateSession(request);
    if (!session) { SCH_API_DEBUG_PRINTLN("API: handleScheduleLockPost - Not authenticated."); request->send(401, "application/json", "{\"error\":\"Not authenticated\"}"); return; }
    if (session->userRole < MANAGER) { SCH_API_DEBUG_PRINTF("API: handleScheduleLockPost - Permission denied for user %s (role %d).\n", session->username.c_str(), session->userRole); request->send(403, "application/json", "{\"error\":\"Permission denied\"}"); return; }
    if (!request->hasParam("uid")) { SCH_API_DEBUG_PRINTLN("API: handleScheduleLockPost - Missing UID parameter."); request->send(400, "application/json", "{\"error\":\"Missing schedule UID parameter\"}"); return; }

    String uid = request->getParam("uid")->value();
    String resourceId = "schedule_" + uid;
    SCH_API_DEBUG_PRINTF("API: handleScheduleLockPost - Action for UID: %s by User: %s\n", uid.c_str(), session->username.c_str());

    int persistentLockLevel = this->scheduleManager.getPersistentLockLevel(uid);
    if (persistentLockLevel == 1 || persistentLockLevel == 2) { SCH_API_DEBUG_PRINTF("API: handleScheduleLockPost - Schedule %s locked by template/cycle.\n", uid.c_str()); request->send(403, "application/json", "{\"error\":\"Schedule is locked by a template or active cycle and cannot be edited.\"}"); return; }
    if (persistentLockLevel < 0) { SCH_API_DEBUG_PRINTF("API: handleScheduleLockPost - Schedule %s not found in index for lock.\n", uid.c_str()); request->send(404, "application/json", "{\"error\":\"Schedule not found in index.\"}"); return; }

    if (this->lockManager.acquireLock(resourceId, EDITING_SCHEDULE, *session)) {
        SCH_API_DEBUG_PRINTLN("API: handleScheduleLockPost - Lock acquired successfully.");
        request->send(200, "application/json", "{\"message\":\"Lock acquired successfully\"}");
    } else {
        FileLock lockInfo;
        if (this->lockManager.getLockInfo(resourceId, lockInfo)) { SCH_API_DEBUG_PRINTF("API: handleScheduleLockPost - Schedule %s already locked by %s.\n", uid.c_str(), lockInfo.username.c_str()); request->send(409, "application/json", "{\"error\":\"Schedule is currently being edited by " + lockInfo.username + "\"}"); }
        else { SCH_API_DEBUG_PRINTLN("API: handleScheduleLockPost - Failed to acquire lock (unknown reason)."); request->send(500, "application/json", "{\"error\":\"Failed to acquire editing lock\"}"); }
    }
}

// --- New Handler for DELETE /api/schedule/lock ---
/**
 * @brief Handles DELETE requests to the /api/schedule/lock endpoint.
 *
 * Attempts to release an exclusive edit lock on a specific schedule, identified
 * by the 'uid' query parameter, that is held by the currently authenticated user.
 * Requires an authenticated session with MANAGER or ADMIN role.
 * Returns 200 OK if the lock is released successfully.
 * Returns 401 if not authenticated, 403 if permission denied (role) or if the lock
 * is held by another user, 400 if 'uid' is missing or if the lock was not found.
 *
 * @param request Pointer to the AsyncWebServerRequest object. Expects 'uid' query parameter.
 */
void ApiRoutes::handleScheduleLockDelete(AsyncWebServerRequest *request) {
    SCH_API_DEBUG_PRINTLN("API: handleScheduleLockDelete request received.");

    SessionData* session = this->sessionManager.validateSession(request);
    if (!session) { SCH_API_DEBUG_PRINTLN("API: handleScheduleLockDelete - Not authenticated."); request->send(401, "application/json", "{\"error\":\"Not authenticated\"}"); return; }
    if (session->userRole < MANAGER) { SCH_API_DEBUG_PRINTF("API: handleScheduleLockDelete - Permission denied for user %s (role %d).\n", session->username.c_str(), session->userRole); request->send(403, "application/json", "{\"error\":\"Permission denied\"}"); return; }
    if (!request->hasParam("uid")) { SCH_API_DEBUG_PRINTLN("API: handleScheduleLockDelete - Missing UID parameter."); request->send(400, "application/json", "{\"error\":\"Missing schedule UID parameter\"}"); return; }

    String uid = request->getParam("uid")->value();
    String resourceId = "schedule_" + uid;
    SCH_API_DEBUG_PRINTF("API: handleScheduleLockDelete - Action for UID: %s by User: %s\n", uid.c_str(), session->username.c_str());

     if (this->lockManager.releaseLock(resourceId, session->sessionId)) {
         SCH_API_DEBUG_PRINTLN("API: handleScheduleLockDelete - Lock released successfully.");
         request->send(200, "application/json", "{\"message\":\"Lock released successfully\"}");
    } else {
        // Check if lock exists at all or is held by someone else
        FileLock lockInfo;
        if (this->lockManager.getLockInfo(resourceId, lockInfo)) {
             SCH_API_DEBUG_PRINTF("API: handleScheduleLockDelete - Failed to release lock (held by %s, not %s).\n", lockInfo.username.c_str(), session->username.c_str());
             request->send(403, "application/json", "{\"error\":\"Failed to release lock (held by another user)\"}"); // Forbidden
        } else {
             SCH_API_DEBUG_PRINTLN("API: handleScheduleLockDelete - Failed to release lock (not found).");
             request->send(400, "application/json", "{\"error\":\"Failed to release lock (lock not found)\"}"); // Bad request
        }
    }
}


// Method to register all API routes
/**
 * @brief Registers all API routes with the provided AsyncWebServer instance.
 *
 * Sets up handlers for authentication (/api/login, /api/logout), user information
 * (/api/user), and schedule management (/api/schedules, /api/schedule, /api/schedule/lock).
 * Uses lambdas to capture the 'this' pointer, allowing member functions to be used
 * as request handlers. Also registers the static file server for the web interface
 * located in the "/www/" directory within the LittleFS filesystem.
 *
 * @param server Reference to the AsyncWebServer object to register routes on.
 */
void ApiRoutes::registerRoutes(AsyncWebServer& server) {
    API_DEBUG_PRINTLN("Registering API routes...");

    // API endpoints - Use lambdas to capture 'this' and call member functions
    server.on("/api/login", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleLogin(request);
    });
    server.on("/api/logout", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleLogout(request);
    });
    server.on("/api/user", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleGetUserInfo(request);
    });

    // Schedule API Routes
    server.on("/api/schedules", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleGetSchedules(request); // List all
    });
    server.on("/api/schedule", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleGetSchedule(request); // Get one by ?uid=...
    });

    // *** REORDERED: Register /api/schedule/lock FIRST using standard handlers ***
    server.on("/api/schedule/lock", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleScheduleLockPost(request);
    });
     server.on("/api/schedule/lock", HTTP_DELETE, [this](AsyncWebServerRequest *request) {
        this->handleScheduleLockDelete(request);
    });

    // *** Now register /api/schedule routes ***
    // Register POST/PUT for /api/schedule with the body handler using lambda
    server.on("/api/schedule", HTTP_POST | HTTP_PUT,
        [](AsyncWebServerRequest *request){ /* No-op main handler */ SCH_API_DEBUG_PRINTLN("API: /api/schedule POST/PUT request received (initial handler)."); }, // Use SCH_API_DEBUG
        NULL, // onUpload handler (not needed)
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            // The actual work is done in the body handler member function
            this->handleSchedulePostPutBody(request, data, len, index, total);
        }
    );

    server.on("/api/schedule", HTTP_DELETE, [this](AsyncWebServerRequest *request) {
        this->handleDeleteSchedule(request); // Delete one by ?uid=...
    });

    // Serve static files from /www directory - Keep this registration logic here for now
    // It was part of the original registerRoutes function.
    API_DEBUG_PRINTLN("Registering static file serving for /www");
    server.serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html");

    API_DEBUG_PRINTLN("API routes registration complete.");
}