#ifndef API_ROUTES_H
#define API_ROUTES_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "UserManager.h"
#include "SessionManager.h"
#include "ScheduleManager.h"
#include "LockManager.h"
#include "DebugConfig.h" // Include for API_DEBUG macros

/**
 * @class ApiRoutes
 * @brief Manages the registration and handling of all API endpoints for the web server.
 *
 * This class encapsulates the logic for different API routes, such as login, logout,
 * user info retrieval, and schedule management (CRUD operations, locking). It interacts
 * with various manager classes (UserManager, SessionManager, ScheduleManager, LockManager)
 * to perform its tasks.
 */
class ApiRoutes {
public:
    // Constructor takes references to the manager instances
    /**
     * @brief Constructs an ApiRoutes object.
     * @param userMgr Reference to the UserManager instance.
     * @param sessionMgr Reference to the SessionManager instance.
     * @param scheduleMgr Reference to the ScheduleManager instance.
     * @param lockMgr Reference to the LockManager instance.
     * @param https Boolean indicating if HTTPS is enabled (used for setting secure cookie flags).
     */
    ApiRoutes(UserManager& userMgr, SessionManager& sessionMgr, ScheduleManager& scheduleMgr, LockManager& lockMgr, bool https);

    // Method to register all API routes
    /**
     * @brief Registers all defined API routes with the provided AsyncWebServer instance.
     * @param server Reference to the AsyncWebServer object.
     */
    void registerRoutes(AsyncWebServer& server);

private:
    // References to manager instances
    UserManager& userManager;         ///< Reference to the user management service.
    SessionManager& sessionManager;   ///< Reference to the session management service.
    ScheduleManager& scheduleManager; ///< Reference to the schedule management service.
    LockManager& lockManager;         ///< Reference to the resource lock management service.
    bool httpsEnabled;                ///< Flag indicating if HTTPS is enabled (for secure cookies).

    // --- Private Helper Functions ---
    /**
     * @brief Adds common security headers to an HTTP response if HTTPS is enabled.
     * @param response Pointer to the AsyncWebServerResponse object.
     */
    void addSecurityHeaders(AsyncWebServerResponse *response);

    // --- Private Request Handlers ---
    // These will be bound to the server routes
    /** @brief Handles POST requests to /api/login for user authentication. */
    void handleLogin(AsyncWebServerRequest *request);
    /** @brief Handles POST requests to /api/logout to invalidate the user session. */
    void handleLogout(AsyncWebServerRequest *request);
    /** @brief Handles GET requests to /api/user to retrieve current user information. */
    void handleGetUserInfo(AsyncWebServerRequest *request);

    // Schedule API Handlers
    /** @brief Handles GET requests to /api/schedules to list all available schedules. */
    void handleGetSchedules(AsyncWebServerRequest *request);
    /** @brief Handles GET requests to /api/schedule?uid=... to retrieve a specific schedule. */
    void handleGetSchedule(AsyncWebServerRequest *request);
    /** @brief Handles DELETE requests to /api/schedule?uid=... to delete a specific schedule. */
    void handleDeleteSchedule(AsyncWebServerRequest *request);
    /**
     * @brief Handles the body content for POST and PUT requests to /api/schedule.
     *        Used for creating (POST) or updating (PUT) a schedule.
     * @param request Pointer to the request object.
     * @param data Pointer to the data chunk.
     * @param len Length of the data chunk.
     * @param index Starting index of the chunk.
     * @param total Total size of the body.
     */
    void handleSchedulePostPutBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    // Removed handleScheduleLockBody, replaced with specific method handlers:
    /** @brief Handles POST requests to /api/schedule/lock?uid=... to acquire an edit lock. */
    void handleScheduleLockPost(AsyncWebServerRequest *request);   // Handler for POST /api/schedule/lock
    /** @brief Handles DELETE requests to /api/schedule/lock?uid=... to release an edit lock. */
    void handleScheduleLockDelete(AsyncWebServerRequest *request); // Handler for DELETE /api/schedule/lock

    // Add other handlers if needed...
};

#endif // API_ROUTES_H