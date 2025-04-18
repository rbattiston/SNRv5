# ESP32 Secure Web Application Plan

This plan outlines the steps to build a secure web application on the ESP32, incorporating user authentication, session management, file locking, HTTPS, and other security best practices using `ESPAsyncWebServer`, `ArduinoJSON7`, and `LittleFS`.

**Final Configuration:**
*   **Sessions:** Stored in Memory (Fast, lost on reboot)
*   **Locks:** Stored in LittleFS (Persistent, involves file I/O)

---

## Phase 1: Core Setup & Filesystem Structure

1.  **Project Initialization:**
    *   Ensure PlatformIO project is set up (`platformio.ini` configured for `esp32dev`, `arduino`, `littlefs`).
    *   Add necessary libraries to `platformio.ini`:
        *   `ESPAsyncWebServer` (& its dependency `AsyncTCP`)
        *   `ArduinoJson` (v7+)
        *   A library for SHA-256 (e.g., the built-in `mbedtls` functions or a dedicated Arduino library).
2.  **Filesystem Initialization:**
    *   Implement code in `setup()` to initialize LittleFS.
    *   Create the necessary directory structure on first boot if it doesn't exist:
        *   `/users/`: Stores user account files (e.g., `owner.json`, `manager1.json`).
        *   `/schedules/`: Stores daily schedule definition files (e.g., `schedule_veg.json`).
        *   `/cycles/templates/`: Stores cycle template files (e.g., `template_flower.json`).
        *   `/cycles/active/`: Stores active cycle state/configuration (e.g., `relay1_active.json`).
        *   `/certs/`: Stores HTTPS certificate (`cert.pem`) and private key (`key.pem`).
        *   `/locks/`: Stores active file lock information file (`active_locks.json`).
3.  **Configuration Management:**
    *   Define structures or classes for storing configuration data (WiFi credentials, system settings).
    *   Implement functions to load/save configuration from/to a file (e.g., `/config.json`).

---

## Phase 2: User Authentication & Roles

1.  **User Data Structure:**
    *   Define a structure/class for user accounts (`UserAccount`) containing:
        *   Username
        *   Hashed Password (SHA-256)
        *   Salt
        *   Role (`owner`, `manager`, `viewer`)
        *   User ID (unique identifier)
    *   Store each user's data in a separate JSON file within `/users/` (e.g., `/users/john_doe.json`).
2.  **Password Hashing:**
    *   Implement functions for:
        *   Generating a cryptographically secure random salt.
        *   Hashing a password with a given salt using SHA-256.
        *   Verifying a provided password against the stored hash and salt.
3.  **User Management Functions:**
    *   Create functions to:
        *   Load user data from LittleFS.
        *   Find a user by username.
        *   Save user data to LittleFS.
        *   (Owner only) Create, modify, delete users.
4.  **Login Endpoint (`/login`):**
    *   Create a web server endpoint (POST) to handle login requests.
    *   Accept username and password.
    *   Find the user. If found, verify the password using the hashing function.
    *   Implement rate limiting (e.g., track failed attempts per IP or username in memory, block after N failures for a period).
    *   If login is successful, proceed to Session Creation (Phase 3).
    *   Return appropriate success or error responses (avoiding information leakage).
5.  **Logout Endpoint (`/logout`):**
    *   Create a web server endpoint (POST or GET) to handle logout.
    *   Identify the session (from cookie).
    *   Invalidate the session (Phase 3).
    *   Clear the session cookie.
    *   Release any locks held by the session (Phase 4).

---

## Phase 3: Session Management (In-Memory)

1.  **Session Data Structure:**
    *   Define a structure/class for session data (`SessionData`) containing:
        *   Session ID (high-entropy random string)
        *   User ID
        *   User Role
        *   Creation Timestamp
        *   Last Heartbeat Timestamp
        *   Client Fingerprint (e.g., hash of User-Agent + IP address)
2.  **Session Storage:**
    *   Implement a session manager (class or set of functions).
    *   Store active sessions:
        *   **Option A (Selected):** In-memory `std::map<String, SessionData>`. Simpler, faster access, but session data is lost on device reboot.
3.  **Session Creation (on successful login):**
    *   Generate a secure, random Session ID.
    *   Create a `SessionData` object with user details, timestamps, and fingerprint.
    *   **Store** the `SessionData` object in the in-memory map, keyed by the Session ID.
    *   Set a secure session cookie on the client's browser:
        *   Name: e.g., `session_id`
        *   Value: The Session ID
        *   Flags: `HttpOnly`, `Secure` (requires HTTPS), `SameSite=Strict`
        *   Expiry: Optional (can rely on server-side expiry)
4.  **Session Validation (Request Middleware/Filter):**
    *   Implement a function/filter that runs before protected endpoints.
    *   Extract the Session ID from the request cookie.
    *   **Look up** the Session ID in the in-memory session map.
    *   If found:
        *   Retrieve the `SessionData` object.
        *   Verify the client fingerprint against the request's User-Agent/IP.
        *   Check for expiry (current time - last heartbeat > 15 minutes).
        *   If valid:
            *   Update the last heartbeat timestamp in the `SessionData` object within the map.
            *   Allow the request to proceed. Attach user/role info to the request object.
        *   If invalid (fingerprint mismatch, expired):
            *   **Remove** the session entry from the in-memory map.
            *   Clear the session cookie on the client.
            *   Deny access.
    *   If not found in the map:
        *   Clear the session cookie on the client.
        *   Deny access.
5.  **Heartbeat Mechanism:**
    *   Create a simple endpoint (e.g., `/heartbeat`) that authenticated clients can periodically ping (e.g., every 5 minutes via JavaScript).
    *   This endpoint performs the Session Validation steps using the in-memory map (lookup, check validity, update heartbeat timestamp in the map) and returns success if the session is valid.
6.  **Session Cleanup:**
    *   Implement a periodic task (e.g., using `Ticker` or in the main loop).
    *   This task will iterate through the in-memory session map.
    *   For each session, check the `lastHeartbeatTimestamp` against the expiry time (15 minutes).
    *   If a session is found to be expired, **remove** the entry from the map.
    *   Crucially, when removing an expired session, trigger the lock release process (Phase 4) for any locks associated with that Session ID.

---

## Phase 4: File Locking (Persistent in LittleFS)

1.  **Lock Data Structure:**
    *   Define a structure/class for lock information (`FileLock`) containing:
        *   Resource ID (e.g., schedule filename, template filename)
        *   Lock Type (`editing`, `template_editing`)
        *   Session ID of the lock holder
        *   Timestamp of lock acquisition
2.  **Lock Storage:**
    *   Implement a lock manager.
    *   Store active locks:
        *   **Option B (Selected):** In LittleFS. Use a single JSON file (e.g., `/locks/active_locks.json`) containing an array or object of all active locks. This provides persistence across device restarts but requires reading/writing the whole file for any lock change.
3.  **Lock Acquisition:**
    *   Before allowing an edit operation (e.g., on `/edit-schedule` endpoint):
        *   Check if the resource (schedule/template) is part of an *active* cycle. If yes, deny edit.
        *   **Read** the `/locks/active_locks.json` file.
        *   Check if the resource is already locked by *another* session within the loaded data. If yes, deny edit (return conflict error).
        *   If the resource is lockable (and not locked by another), create a `FileLock` entry, add it to the lock data, and **write** the updated data back to `/locks/active_locks.json`.
4.  **Lock Release:**
    *   Locks are released when:
        *   The user explicitly finishes editing (e.g., saves changes, cancels). Requires reading, modifying, and writing `/locks/active_locks.json`.
        *   The user logs out. Requires reading, modifying, and writing `/locks/active_locks.json`.
        *   The user's session expires. Requires reading, modifying, and writing `/locks/active_locks.json`.
        *   A lock timeout occurs (optional). Implement via periodic cleanup task that reads, checks timestamps, modifies, and writes `/locks/active_locks.json`.
5.  **Integration with Session Management:**
    *   Session logout and expiry cleanup (from the in-memory session map) must trigger the lock release process (which involves reading/modifying/writing the lock file in LittleFS).

---

## Phase 5: HTTPS & Security Headers

1.  **Certificate Handling:**
    *   In `setup()`:
        *   Check if `/certs/cert.pem` and `/certs/key.pem` exist in LittleFS.
        *   If they exist, load them.
        *   If they *don't* exist, implement logic to generate a self-signed certificate and private key (using mbedTLS functions available in ESP-IDF/Arduino) and save them to `/certs/`. *Note: On-device generation can be slow and complex.*
        *   Provide clear instructions/placeholders for users to upload their own externally generated certs later.
2.  **HTTPS Server Setup:**
    *   Instantiate `ESPAsyncWebServer` using the loaded certificate and private key.
    *   Start the server on port 443.
3.  **HTTP to HTTPS Redirection:**
    *   Optionally, start a standard HTTP server (`AsyncWebServer`) on port 80.
    *   Configure this server to respond to all requests with a 301 or 302 redirect to the `https://` equivalent URL.
4.  **Security Headers:**
    *   Add middleware or modify response handlers to include security headers in all HTTPS responses:
        *   `Strict-Transport-Security (HSTS)`: `max-age=31536000; includeSubDomains` (start with a shorter max-age during testing).
        *   `Content-Security-Policy (CSP)`: Define a strict policy (e.g., `default-src 'self'; script-src 'self' 'nonce-XYZ'; style-src 'self'; img-src 'self';`). Requires careful implementation, especially with JavaScript. Use nonces for inline scripts if necessary.
        *   `X-Content-Type-Options`: `nosniff`
        *   `X-Frame-Options`: `DENY` or `SAMEORIGIN`
        *   `Referrer-Policy`: `no-referrer` or `strict-origin-when-cross-origin`
        *   `Permissions-Policy`: Restrict features (e.g., `geolocation=(), microphone=()`)

---

## Phase 6: Web Interface & API Endpoints

1.  **Static Files:**
    *   Serve HTML, CSS, and JavaScript files from LittleFS (e.g., from a `/www/` directory).
    *   Ensure static file handlers do not require authentication for basic assets but protect sensitive pages.
2.  **API Endpoints:**
    *   Define RESTful API endpoints for all required actions (login, logout, heartbeat, view schedules, edit schedules, manage cycles, view status, user management, etc.).
    *   Apply the session validation middleware to all protected endpoints.
    *   Implement authorization checks within each endpoint handler based on the user's role (retrieved during session validation). Use the detailed permission list provided.
    *   Implement file lock acquisition/release logic within relevant endpoints (e.g., `/start-edit-schedule`, `/save-schedule`, `/cancel-edit-schedule`).
    *   Use `ArduinoJson` for parsing incoming JSON request bodies and serializing JSON responses. Handle potential JSON parsing errors gracefully.
3.  **Frontend JavaScript:**
    *   Implement JavaScript to:
        *   Handle login/logout forms.
        *   Make API calls to fetch data and perform actions.
        *   Send periodic heartbeats.
        *   Handle session expiry (e.g., redirect to login).
        *   Manage UI elements based on user permissions.
        *   Handle file lock notifications (e.g., "This schedule is currently being edited by another user").

---

## Phase 7: WiFi Management

1.  **Modes:** Implement logic to start the ESP32 in:
    *   **Station (STA) Mode:** Connects to an existing WiFi network. Requires storing SSID/Password securely (e.g., in `/config.json`).
    *   **Access Point (AP) Mode:** Creates its own WiFi network. Useful for initial configuration. Define a default AP SSID/Password.
    *   **AP+STA Mode:** Operates in both modes simultaneously.
2.  **Configuration Interface:** Provide a simple web page (possibly served in AP mode initially) for users to configure WiFi credentials for STA mode.

---

## Phase 8: Resource Management & Error Handling

1.  **ArduinoJson:** Use `JsonDocument` efficiently. Prefer `JsonDocument` on the stack for smaller, temporary objects. Use dynamic `JsonDocument` for larger or longer-lived data, being mindful of heap fragmentation. Check allocation success.
2.  **Memory:** Monitor memory usage (`ESP.getFreeHeap()`). Avoid large global buffers. Release resources promptly.
3.  **Asynchronous Operations:** Leverage the async nature of `ESPAsyncWebServer`. Avoid long-blocking operations within request handlers.
4.  **Error Handling:** Implement robust error handling for file I/O, JSON parsing, network operations, etc. Log errors appropriately (e.g., to Serial) but avoid sending detailed internal error information to the client. Return generic error messages.

---

## Diagrams

### Component Interaction

```mermaid
graph TD
    Client[Client Browser] -- HTTPS --> ESP32[ESP32]
    ESP32 -- WiFi --> Router[WiFi Router/AP]
    ESP32 -- LittleFS --> Flash[Flash Memory]

    subgraph ESP32
        direction LR
        WebServer[ESPAsyncWebServer] -- Handles --> Requests[HTTP(S) Requests]
        Requests -- Auth/Session --> SessionMgr[Session Manager (Memory)]
        Requests -- Locking --> LockMgr[Lock Manager (FS)]
        Requests -- Filesystem Ops --> FSOps[Filesystem Operations]
        SessionMgr -- Stores/Retrieves --> SessionData[(Session Data - Memory)]
        LockMgr -- Reads/Writes --> LockFile[/locks/active_locks.json]
        FSOps -- Reads/Writes --> UserFiles[/users/]
        FSOps -- Reads/Writes --> ScheduleFiles[/schedules/]
        FSOps -- Reads/Writes --> CycleFiles[/cycles/]
        FSOps -- Reads/Writes --> CertFiles[/certs/]
        FSOps -- Reads/Writes --> ConfigFile[/config.json]
        AuthLogic[Authentication Logic] -- Uses --> Hashing[SHA-256 Hashing]
        AuthLogic -- Interacts --> UserFiles
        SessionMgr -- Interacts --> AuthLogic
        WebServer -- Uses --> CertFiles
    end

    Flash --> UserFiles
    Flash --> ScheduleFiles
    Flash --> CycleFiles
    Flash --> CertFiles
    Flash --> ConfigFile
    Flash --> LockFile
end
```

### Login & Session Flow

```mermaid
sequenceDiagram
    participant Client
    participant ESP32WebServer
    participant SessionManager (Memory)
    participant Filesystem

    Client->>+ESP32WebServer: POST /login (user, pass)
    ESP32WebServer->>+Filesystem: Read user data (user)
    Filesystem-->>-ESP32WebServer: User data (hash, salt)
    ESP32WebServer->>ESP32WebServer: Verify password(pass, hash, salt)
    alt Login Success
        ESP32WebServer->>+SessionManager (Memory): Create Session(userID, role)
        SessionManager (Memory)-->>-ESP32WebServer: sessionID
        ESP32WebServer-->>Client: 200 OK (Set-Cookie: session_id=...)
    else Login Failure
        ESP32WebServer-->>Client: 401 Unauthorized
    end

    Client->>+ESP32WebServer: GET /protected-resource (Cookie: session_id=...)
    ESP32WebServer->>+SessionManager (Memory): Validate Session(sessionID, fingerprint)
    alt Session Valid
        SessionManager (Memory)-->>-ESP32WebServer: OK (userID, role)
        ESP32WebServer->>ESP32WebServer: Check Role Permissions
        ESP32WebServer->>+Filesystem: Access resource
        Filesystem-->>-ESP32WebServer: Resource data
        ESP32WebServer-->>Client: 200 OK (Resource data)
    else Session Invalid/Expired
        SessionManager (Memory)-->>-ESP32WebServer: Invalid
        ESP32WebServer-->>Client: 401 Unauthorized / Redirect to Login
    end