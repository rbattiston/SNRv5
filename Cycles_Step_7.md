# Step 7: Integration & Testing - Detailed Plan (with Cypress, AUnit, and Postman)

## 1. Goal

To ensure that all components of the SNRv5 Irrigation Controller system—including backend managers, API, and frontend UI—work together seamlessly, reliably, and securely. This step covers end-to-end integration, comprehensive testing (manual and automated), validation of all workflows, error handling, concurrency/locking, and persistence.

---

## 2. Prerequisites

- Steps 1-6 fully implemented and reviewed.
- All API endpoints operational and documented.
- Frontend UI pages (index.html, dashboard.html, schedule.html, io_config.html, modbus_profiles.html) deployed and accessible.
- Test user accounts with both Admin and Operator roles.
- Test data for I/O points, schedules, cycles, and templates present in the system.
- Node.js and npm installed (for Cypress).
- PlatformIO installed (for AUnit).
- Postman installed (for API testing).

---

## 3. Deliverables

- Integration test plan and test cases (manual and automated).
- Test scripts for API and UI (Cypress, Postman).
- Unit/integration test code for backend (AUnit).
- Bug/issue tracking log.
- Final validation report with pass/fail status for all test cases.
- Recommendations for improvements or fixes.

---

## 4. Testing Frameworks Overview

### 4.1. Cypress (Frontend End-to-End Testing)

- **Setup:**  
  - Install: `npm install cypress --save-dev`
  - Launch: `npx cypress open`
- **Use Cases:**  
  - Automate UI workflows (login, CRUD, lock handling, role-based UI).
  - Simulate user interactions and verify UI updates.
- **Example Test:**
```js
// cypress/integration/login_spec.js
describe('Login Flow', () => {
  it('logs in as admin and sees dashboard', () => {
    cy.visit('/index.html');
    cy.get('input[name="username"]').type('admin');
    cy.get('input[name="password"]').type('password');
    cy.get('button[type="submit"]').click();
    cy.url().should('include', '/dashboard.html');
    cy.contains('SNR System');
  });
});
```

---

### 4.2. AUnit (Backend Unit/Integration Testing for ESP32/Arduino)

- **Setup:**  
  - Install via PlatformIO: `pio lib install "AUnit"`
  - Create test files in `/test/` directory.
- **Use Cases:**  
  - Test backend logic (CycleManager, OutputPointManager, LockManager, etc.).
  - Validate state transitions, event processing, and persistence.
- **Example Test:**
```cpp
#include <AUnit.h>
#include <ArduinoJson.h>

test(CycleManager_StateTransition) {
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, "{\"state\":\"DRAFT\"}");
  // Simulate state transition
  doc["state"] = "ACTIVE";
  assertEqual(String(doc["state"]), "ACTIVE");
}
```

---

### 4.3. Postman (API Contract and Workflow Testing)

- **Setup:**  
  - Download and install Postman (https://www.postman.com/downloads/)
  - Import or create collections for SNRv5 API endpoints.
- **Use Cases:**  
  - Manually test all API endpoints (GET/POST/PUT/DELETE).
  - Automate API test collections for regression testing.
  - Validate request/response formats, error handling, and authorization.
- **Example Test:**
  - POST `/api/v1/outputs` with/without lock, check for correct error codes and messages.
  - Use Postman scripts to assert on response fields.

---

## 5. Integration & Testing Areas

### 5.1. End-to-End Workflow Testing (Cypress + Postman)

#### 5.1.1. Example Workflow: Full Cycle Creation and Execution

**Steps:**
1. Login as Admin (Cypress).
2. Acquire configuration lock (Cypress/Postman).
3. Define new Output and Input Points (Cypress/Postman).
4. Create a Schedule Template (Cypress/Postman).
5. Create a Cycle Template referencing the Schedule Template (Cypress/Postman).
6. Create a new Active Cycle from the Cycle Template (Cypress/Postman).
7. Associate Output/Input Points with the Active Cycle (Cypress/Postman).
8. Activate the cycle (Cypress/Postman).
9. Monitor cycle execution and input readings (Cypress/Postman).
10. Edit a Schedule Instance within the Active Cycle (Cypress/Postman).
11. Deactivate and delete the cycle (Cypress/Postman).
12. Release the configuration lock (Cypress/Postman).

---

### 5.2. API Contract Validation (Postman)

- Verify all frontend API calls match backend definitions (routes, methods, request/response formats).
- Test all endpoints for:
  - Success responses (200/201)
  - Structured error responses (see Step 5)
  - Authorization and role enforcement
  - Lock enforcement (403 errors when not held)
- Use Postman scripts for automated regression.

---

### 5.3. Backend Logic Testing (AUnit)

- Unit and integration tests for:
  - CycleManager: state transitions, event processing, Autopilot logic, reboot recovery
  - OutputPointManager/InputPointManager: command queueing, input polling, persistence
  - LockManager: lock acquisition, expiry, refresh, release
- Simulate edge cases (e.g., sensor failure, reboot during active cycle).

---

### 5.4. Frontend Logic & UI Testing (Cypress)

- Test all UI flows:
  - Login/logout, session expiry
  - Role-based UI (Admin vs. Operator)
  - Lock acquisition, timer, expiry, and refresh
  - CRUD for all entities (outputs, inputs, schedules, cycles, templates, modbus profiles)
  - Inline error/success notifications
  - Responsive layout (desktop/mobile)
- Use Cypress for automated browser-based tests.

---

### 5.5. Error Handling & Edge Case Testing (All Frameworks)

- Test all defined error conditions:
  - Validation errors (missing/invalid fields)
  - Auth errors (unauthenticated, insufficient permissions)
  - Lock errors (not held, expired, held by other)
  - Not found, conflict, internal errors
- Test UI and API responses for each.

---

### 5.6. Concurrency & Locking (Cypress + Postman)

- Simulate multiple users attempting to acquire/use the lock.
- Test lock timeout, expiry, and manual release.
- Ensure only one session can modify configuration at a time.

---

### 5.7. Persistence & Reboot Recovery (AUnit + Manual)

- Verify all data is saved to and loaded from LittleFS across reboots.
- Test that active cycles, output/input definitions, and templates persist.
- Test reboot recovery logic for lighting and other outputs.

---

### 5.8. (Optional) Performance Testing

- Assess UI responsiveness and API call latency under typical and heavy load.
- Use browser dev tools and network analysis.

---

## 6. Reporting & Issue Tracking

- Log all test results, issues, and bugs.
- Track status of fixes and retests.
- Prepare a final validation report.

---

## 7. Diagrams

### 7.1. Integration Test Flow

```mermaid
sequenceDiagram
    participant Admin as Admin User
    participant UI as Frontend UI
    participant API as HTTP API
    participant Backend as Backend Managers
    participant FS as LittleFS

    Admin->>UI: Login (Cypress)
    UI->>API: POST /api/v1/session/login (Postman)
    API->>Backend: Validate credentials (AUnit)
    Backend-->>API: Session token
    API-->>UI: Success

    Admin->>UI: Acquire Lock (Cypress/Postman)
    UI->>API: POST /api/v1/system/lock (Postman)
    API->>Backend: LockManager.acquire() (AUnit)
    Backend-->>API: Lock granted
    API-->>UI: Success

    Admin->>UI: Create Output/Input/Schedule/Cycle (Cypress/Postman)
    UI->>API: POST /api/v1/... (Postman)
    API->>Backend: Manager.create() (AUnit)
    Backend->>FS: Save to LittleFS
    FS-->>Backend: OK
    Backend-->>API: Success
    API-->>UI: Success

    Admin->>UI: Activate Cycle (Cypress/Postman)
    UI->>API: POST /api/v1/cycles/active/{id}/activate (Postman)
    API->>Backend: CycleManager.activate() (AUnit)
    Backend-->>API: Success
    API-->>UI: Success

    Admin->>UI: Monitor, Edit, Deactivate, Delete, Release Lock (Cypress/Postman)
    ...
```

---

## 8. Notes

- All backend JSON handling must use ArduinoJson v7 functions.
- All error responses must follow the structured format defined in Step 5.
- All test scripts and pseudocode are illustrative and should be adapted to the actual implementation and test framework used.
- Cypress, AUnit, and Postman are all beginner-friendly and have extensive documentation for setup and usage.