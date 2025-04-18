# Step 6: Frontend UI Implementation - Detailed Plan (Alpine.js, Responsive, Complete Examples)

## 1. Goal

To implement a visually refined, responsive, and robust web UI for the SNRv5 Irrigation Controller, providing full access to all system features via the HTTP API defined in Step 5. The UI must support authentication, role-based access, configuration locking, CRUD for all major entities, and real-time feedback, using Alpine.js for reactivity and state management.

---

## 2. Prerequisites

- Steps 1-5 finalized and implemented (data structures, backend managers, API endpoints, error formats).
- API endpoints available and tested (see Cycles_Step_5.md).
- Alpine.js available via CDN.
- Existing HTML/CSS/JS structure in `/data/www/` (index.html, dashboard.html, schedule.html, style.css, etc.).
- User roles and session management functional.
- LockManager and lock endpoints operational.

---

## 3. Deliverables

- Refactored and expanded HTML pages:
  - `index.html` (Login)
  - `dashboard.html` (System overview)
  - `schedule.html` (Tabbed: Schedule Templates, Cycle Templates, Active Cycles)
  - `io_config.html` (Output/Input Point Configuration)
  - `modbus_profiles.html` (Modbus Profile Management)
- Per-page JS (or Alpine.js components) for all dynamic UI.
- Inline notification system for errors/success.
- Responsive CSS for desktop and mobile.
- Example-rich documentation within this file.

---

## 4. UI/UX Structure & Component Breakdown

### 4.1. index.html (Login)

- **Components:**
  - Login form (username, password)
  - Inline error/success messages
- **Logic:**
  - On submit, POST to `/api/v1/session/login`
  - On success, redirect to dashboard
  - On error, show inline alert

**Example HTML/Alpine.js:**
```html
<form x-data="loginForm()" @submit.prevent="submit">
  <input x-model="username" required>
  <input type="password" x-model="password" required>
  <button type="submit" :disabled="loading">Login</button>
  <div x-show="error" class="alert alert-error" x-text="error"></div>
</form>
<script>
function loginForm() {
  return {
    username: '', password: '', error: '', loading: false,
    async submit() {
      this.loading = true;
      const res = await fetch('/api/v1/session/login', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({username: this.username, password: this.password})
      });
      const data = await res.json();
      if (data.success) {
        window.location.href = '/dashboard.html';
      } else {
        this.error = data.error?.message || 'Login failed';
      }
      this.loading = false;
    }
  }
}
</script>
```

---

### 4.2. dashboard.html

- **Components:**
  - Header (user info, logout)
  - Sidebar navigation
  - Cards: Active Cycles, Schedules, System Status
  - Recent Schedules list
  - Inline notifications
- **Logic:**
  - On load, fetch user info, cycles, system status
  - Show/hide controls based on user role
  - Logout clears session and redirects to login

**Example: Fetching and displaying system status**
```js
async function fetchStatus() {
  const res = await fetch('/api/v1/system/status');
  const data = await res.json();
  if (data.success) {
    // update Alpine.js state
  } else {
    // show inline error
  }
}
```

---

### 4.3. schedule.html (Tabbed UI)

- **Tabs:**
  - Schedule Templates
  - Cycle Templates
  - Active Cycles

#### 4.3.1. Schedule Templates Tab

- **Components:**
  - List of templates (GET `/api/v1/schedules/templates`)
  - Create/Edit form (name, events, autopilot windows)
  - Delete button (with confirmation)
- **Logic:**
  - Only Admin with lock can create/edit/delete
  - Inline validation and error display

**Example: Creating a new template**
```js
async function createTemplate(template) {
  const res = await fetch('/api/v1/schedules/templates', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(template)
  });
  const data = await res.json();
  if (data.success) {
    // refresh list, show success
  } else {
    // show data.error.message
  }
}
```

#### 4.3.2. Cycle Templates Tab

- **Components:**
  - List of cycle templates (GET `/api/v1/cycles/templates`)
  - Create/Edit form (sequence of schedule template UIDs)
  - Delete button

#### 4.3.3. Active Cycles Tab

- **Components:**
  - List of active cycles (GET `/api/v1/cycles/active`)
  - Create new active cycle (from template or manual)
  - Edit active cycle (name, associations, sequence)
  - Activate/Deactivate, Delete buttons
  - "Edit Schedule Instance" modal (GET/PUT `/api/v1/cycles/instances/{id}`)
- **Logic:**
  - Only Admin with lock can create/edit/delete
  - Operator can activate/deactivate
  - Inline status indicators

---

### 4.4. io_config.html

- **Components:**
  - List of output points (GET `/api/v1/outputs`)
  - List of input points (GET `/api/v1/inputs`)
  - Edit forms for each (dynamic fields)
  - Save (POST/PUT), only Admin with lock
- **Logic:**
  - Show/hide fields based on type
  - Inline validation and error display

---

### 4.5. modbus_profiles.html

- **Components:**
  - List of profiles (GET `/api/v1/modbus_profiles`)
  - Upload new profile (file input)
  - Edit/view profile details
- **Logic:**
  - Only Admin with lock can upload/edit

---

### 4.6. Lock Management (All Config Pages)

- **Components:**
  - Lock status display (GET `/api/v1/system/lock`)
  - 30-min countdown timer (lockRemainingSeconds, polled every 10s)
  - Acquire, Refresh, Release buttons (POST/DELETE `/api/v1/system/lock`)
  - Visual warning when <60s remain
- **Logic:**
  - On expiry, auto-disable editing, discard unsaved changes, reload data, notify user

**Example: Lock timer with Alpine.js**
```html
<div x-data="lockTimer()" x-init="init()">
  <span x-text="remaining"></span> seconds left
  <button @click="refreshLock">Refresh Lock</button>
  <div x-show="warning" class="alert alert-warning">Lock expiring soon!</div>
</div>
<script>
function lockTimer() {
  return {
    remaining: 0, warning: false,
    async init() {
      setInterval(async () => {
        const res = await fetch('/api/v1/system/lock');
        const data = await res.json();
        this.remaining = data.lockRemainingSeconds;
        this.warning = this.remaining < 60;
      }, 10000);
    },
    async refreshLock() {
      await fetch('/api/v1/system/lock', {method: 'POST'});
      // update remaining
    }
  }
}
</script>
```

---

## 5. Error Handling & Notification Examples

- All API errors are displayed as inline alerts.
- Example error response:
```json
{
  "success": false,
  "error": {
    "code": "CONFIG_LOCKED_BY_OTHER",
    "message": "Configuration is locked by another session.",
    "details": { "holderSessionId": "...", "remainingSeconds": 123 }
  }
}
```
- UI displays: "Configuration is locked by another session."

---

## 6. User Interaction Flows

### 6.1. Editing a Schedule Template

1. User (Admin, lock held) clicks "Create Template".
2. Fills out form (name, events, autopilot).
3. Clicks "Save" → POST to `/api/v1/schedules/templates`.
4. On success, template appears in list; on error, inline alert.

### 6.2. Lock Expiry

1. User is editing a form.
2. Lock timer reaches 0.
3. UI disables editing, discards unsaved changes, reloads data, shows warning: "Lock expired, changes discarded."

---

## 7. Responsive & Role-Based UI

- Use CSS media queries for mobile/desktop.
- Alpine.js state for user role; hide/disable controls for Operator vs. Admin.

---

## 8. Diagrams

### 8.1. UI Structure & Data Flow

```mermaid
flowchart TD
    subgraph Pages
        A1[index.html (Login)]
        A2[dashboard.html]
        A3[schedule.html]
        A4[io_config.html]
        A5[modbus_profiles.html]
    end

    subgraph API
        B1[/api/v1/session]
        B2[/api/v1/system/lock]
        B3[/api/v1/outputs]
        B4[/api/v1/inputs]
        B5[/api/v1/schedules/templates]
        B6[/api/v1/cycles/templates]
        B7[/api/v1/cycles/active]
        B8[/api/v1/cycles/instances]
        B9[/api/v1/system/status]
        B10[/api/v1/modbus_profiles]
    end

    A1 -- login --> B1
    A2 -- fetch user, cycles, status --> B1 & B7 & B9
    A2 -- lock status --> B2
    A3 -- CRUD templates/cycles/instances --> B5 & B6 & B7 & B8
    A3 -- lock status --> B2
    A4 -- CRUD outputs/inputs --> B3 & B4
    A4 -- lock status --> B2
    A5 -- CRUD modbus profiles --> B10
    A5 -- lock status --> B2

    classDef page fill:#f9f,stroke:#333,stroke-width:1px;
    class A1,A2,A3,A4,A5 page;
```

---

## 9. Testing & Validation

- Manual and automated UI tests for all workflows.
- Test lock acquisition, expiry, and refresh.
- Test all CRUD operations for templates, cycles, I/O, and Modbus.
- Test error handling for all API endpoints.
- Test role-based access (Admin vs. Operator).
- Test responsiveness on desktop and mobile.

---

## 10. Notes on Alpine.js Feasibility

- Alpine.js is highly feasible: lightweight, easy to integrate, and supports all required UI reactivity.
- Can be incrementally adopted alongside existing JS.
- Well-suited for dynamic forms, modals, and stateful UI.

---