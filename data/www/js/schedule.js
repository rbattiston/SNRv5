/**
 * @file schedule.js
 * @brief Manages the schedule creation, editing, viewing, and interaction logic on the schedule management page.
 *
 * Handles fetching the list of schedules, displaying selected schedule details, managing edit locks,
 * adding different types of events (Autopilot, Duration, Volume), validating inputs, saving changes,
 * deleting schedules, copying schedules, and updating the UI state (read-only vs. edit mode).
 */
const DEBUG_SCHEDULE = true; // Set to false to disable schedule-related console logs

document.addEventListener('DOMContentLoaded', function() {
    // --- DOM Elements ---
    const scheduleSelect = document.getElementById('scheduleSelect');
    const createNewBtn = document.getElementById('createNewBtn');
    const editSelectedBtn = document.getElementById('editSelectedBtn');
    const deleteSelectedBtn = document.getElementById('deleteSelectedBtn'); // New
    const copySelectedBtn = document.getElementById('copySelectedBtn');   // New
    const editSection = document.getElementById('editSection');
    const editSectionTitle = document.getElementById('editSectionTitle');
    const scheduleNameInput = document.getElementById('scheduleName');
    const scheduleUIDInput = document.getElementById('scheduleUID');
    const lightsOnInput = document.getElementById('lightsOnTime');
    const lightsOffInput = document.getElementById('lightsOffTime');
    const statusMessage = document.getElementById('statusMessage');
    const eventAddStatus = document.getElementById('eventAddStatus');
    const visualizationPre = document.querySelector('#visualization pre');
    const saveScheduleBtn = document.getElementById('saveScheduleBtn');
    const cancelEditBtn = document.getElementById('cancelEditBtn');
    const addEventBtn = document.getElementById('addEventBtn');
    const tabsContainer = document.querySelector('.tabs');
    const tabContents = document.querySelectorAll('.tab-content');
    const tabButtons = document.querySelectorAll('.tab-button');
    const visualizationContainer = document.getElementById('visualizationContainer'); // New
    const timelineDiv = document.getElementById('timeline'); // New
    const eventGridDiv = document.getElementById('eventGrid'); // New
    const textVisualizationDiv = document.getElementById('textVisualization'); // New

    // Event Input Fields
    const apStartTimeInput = document.getElementById('apStartTime');
    const apEndTimeInput = document.getElementById('apEndTime');
    const apMatricTensionInput = document.getElementById('apMatricTension');
    const apDoseVolumeInput = document.getElementById('apDoseVolume');
    const apSettlingTimeInput = document.getElementById('apSettlingTime');
    const durStartTimeInput = document.getElementById('durStartTime');
    const durDurationInput = document.getElementById('durDuration');
    const durRepetitionsInput = document.getElementById('durRepetitions');
    const durIntervalInput = document.getElementById('durInterval');
    const durIntervalValueSpan = document.getElementById('durIntervalValue');
    const volStartTimeInput = document.getElementById('volStartTime');
    const volDoseVolumeInput = document.getElementById('volDoseVolume');
    const volRepetitionsInput = document.getElementById('volRepetitions');
    const volIntervalInput = document.getElementById('volInterval');
    const volIntervalValueSpan = document.getElementById('volIntervalValue');


    // --- State Variables ---
    let currentSchedule = null; // Holds the schedule data being viewed/edited/copied
    let allSchedulesData = []; // Store fetched list with lock status
    let isCreatingNew = false; // True if creating new or copying
    let currentEditingUID = null; // UID of the schedule being viewed/edited (null if new/copy)
    let hasEditLock = false; // True if we hold the editing lock from the backend
    let isEditingMode = false; // True if the form is currently editable (vs. viewing)

    // --- Helper Functions ---
    /**
     * @function showStatus
     * @brief Displays a status message (success or error) in the main status area.
     * @param {string} message - The message text to display.
     * @param {boolean} [isError=true] - If true, styles the message as an error; otherwise, as success.
     */
    function showStatus(message, isError = true) {
        if (!statusMessage) return;
        statusMessage.textContent = message;
        statusMessage.className = isError ? 'error-message' : 'success-message';
        statusMessage.style.display = 'block';
        setTimeout(() => { statusMessage.style.display = 'none'; }, 5000);
    }

     /**
      * @function showEventStatus
      * @brief Displays a status message specifically for event adding operations.
      * @param {string} message - The message text to display.
      * @param {boolean} [isError=true] - If true, styles the message as an error; otherwise, as success.
      */
     function showEventStatus(message, isError = true) {
        if (!eventAddStatus) return;
        eventAddStatus.textContent = message;
        eventAddStatus.className = isError ? 'error-message' : 'success-message';
        eventAddStatus.style.display = 'block';
         setTimeout(() => { eventAddStatus.style.display = 'none'; }, 4000);
    }

    /**
     * @function minutesToHHMM
     * @brief Converts total minutes since midnight (0-1439) to HH:MM format string.
     * @param {number} minutes - The number of minutes since midnight.
     * @returns {string} The time in HH:MM format (e.g., "06:30", "14:05"). Returns "00:00" for invalid input.
     */
    function minutesToHHMM(minutes) {
        if (minutes === null || minutes === undefined || minutes < 0 || minutes > 1439) {
            return "00:00"; // Default or error representation
        }
        const hours = Math.floor(minutes / 60);
        const mins = minutes % 60;
        return `${String(hours).padStart(2, '0')}:${String(mins).padStart(2, '0')}`;
    }

    /**
     * @function hhmmToMinutes
     * @brief Converts an HH:MM format string to total minutes since midnight.
     * @param {string} hhmm - The time string in HH:MM format.
     * @returns {number} The total minutes since midnight (0-1439). Returns 0 for invalid input.
     */
    function hhmmToMinutes(hhmm) {
        if (!hhmm || typeof hhmm !== 'string' || !hhmm.includes(':')) {
            return 0; // Default or error
        }
        const parts = hhmm.split(':');
        const hours = parseInt(parts[0], 10);
        const mins = parseInt(parts[1], 10);
        if (isNaN(hours) || isNaN(mins) || hours < 0 || hours > 23 || mins < 0 || mins > 59) {
            return 0; // Invalid format
        }
        return hours * 60 + mins;
    }


    // --- Timeline Creation ---
    /**
     * @function createTimelineLabels
     * @brief Creates the hour and half-hour labels for the visualization timeline.
     */
    function createTimelineLabels() {
        if (!timelineDiv) return;
        timelineDiv.innerHTML = ''; // Clear existing labels

        for (let hour = 0; hour <= 24; hour++) {
            // Full hour mark
            const label = document.createElement('div');
            label.classList.add('timeline-label');
            label.textContent = `${hour}`; // Display hour number
            const leftPercent = (hour / 24) * 100;
            label.style.left = `${leftPercent}%`;
            timelineDiv.appendChild(label);

            // Half-hour mark (except for 24:00)
            if (hour < 24) {
                const halfLabel = document.createElement('div');
                halfLabel.classList.add('timeline-label');
                halfLabel.textContent = '|'; // Simple marker for half hours
                halfLabel.style.fontSize = '0.7em';
                const halfLeftPercent = ((hour + 0.5) / 24) * 100;
                halfLabel.style.left = `${halfLeftPercent}%`;
                timelineDiv.appendChild(halfLabel);
            }
        }
         if (DEBUG_SCHEDULE) console.log('[createTimelineLabels] Timeline labels generated.');
    }

    // --- Graphical Visualization ---
    /**
     * @function updateVisualization
     * @brief Updates the graphical schedule visualization based on the `currentSchedule` data.
     *        Places each Duration/Volume event on its own row, sorted chronologically.
     */
    function updateVisualization() {
        if (DEBUG_SCHEDULE) console.log('[updateVisualization] Updating graphical visualization (row per event)...');
        if (!eventGridDiv || !visualizationContainer) {
            if (DEBUG_SCHEDULE) console.log('[updateVisualization] Error: eventGridDiv or visualizationContainer element not found.');
            return;
        }
        eventGridDiv.innerHTML = ''; // Clear previous events

        if (!currentSchedule) {
            if (DEBUG_SCHEDULE) console.log('[updateVisualization] No currentSchedule data. Grid remains empty.');
            visualizationContainer.style.height = '80px'; // Reset to min height
            return;
        }

        const totalMinutes = 24 * 60; // 1440
        const barHeight = 18; // Must match CSS .event-bar height
        const verticalGap = 0; // Explicitly 0 for no space between lines
        const labelMargin = 5; // Space between bar and label in pixels

        // --- Render Autopilot Windows (Background) ---
        (currentSchedule.autopilotWindows || []).forEach(ap => {
            const startPercent = (ap.startTime / totalMinutes) * 100;
            const endPercent = (ap.endTime / totalMinutes) * 100;
            const widthPercent = Math.max(0, endPercent - startPercent);

            if (widthPercent > 0) {
                const apBar = document.createElement('div');
                // Removed duplicate declaration below
                apBar.classList.add('event-bar', 'event-autopilot');
                apBar.style.left = `${startPercent}%`;
                apBar.style.width = `${widthPercent}%`;
                // Ensure all parameters are in the title for hover
                apBar.title = `AutoPilot Window\nStart: ${minutesToHHMM(ap.startTime)}\nEnd: ${minutesToHHMM(ap.endTime)}\nMatric Tension: ${ap.matricTension} kPa\nDose Volume: ${ap.doseVolume} mL\nSettling Time: ${ap.settlingTime} min`;
                apBar.textContent = `AP (${minutesToHHMM(ap.startTime)} - ${minutesToHHMM(ap.endTime)})`; // Show time range on bar
                eventGridDiv.appendChild(apBar);
            }
        });

        // --- Prepare and Sort Duration/Volume Events for Row Layout ---
        let rowEvents = [];
        (currentSchedule.durationEvents || []).forEach(dur => {
            rowEvents.push({
                type: 'duration',
                startTime: dur.startTime,
                duration: dur.duration,
                elementClass: 'event-duration',
                // Updated label text format
                labelText: `${minutesToHHMM(dur.startTime)} (${dur.duration}s)`,
                titleText: `Duration Event\nStart: ${minutesToHHMM(dur.startTime)}\nDuration: ${dur.duration}s`
            });
        });
        (currentSchedule.volumeEvents || []).forEach(vol => {
            rowEvents.push({
                type: 'volume',
                startTime: vol.startTime,
                doseVolume: vol.doseVolume,
                elementClass: 'event-volume',
                // Updated label text format
                labelText: `${minutesToHHMM(vol.startTime)} (${vol.doseVolume}mL)`,
                titleText: `Volume Event\nStart: ${minutesToHHMM(vol.startTime)}\nDose: ${vol.doseVolume}mL`
            });
        });

        // Sort primarily by start time
        rowEvents.sort((a, b) => a.startTime - b.startTime);

        // --- Render Events Row by Row ---
        let currentTop = 0; // Start position for the first row
        rowEvents.forEach((event, index) => {
            const startPercent = (event.startTime / totalMinutes) * 100;
            let widthPercent;

            if (event.type === 'duration') {
                 // Calculate width based on duration in seconds -> minutes
                 const durationMinutes = Math.max(1, Math.ceil(event.duration / 60)); // Min 1 min visual width
                 const endMinutes = event.startTime + durationMinutes;
                 const endPercent = (Math.min(endMinutes, totalMinutes) / totalMinutes) * 100; // Cap end time
                 widthPercent = Math.max(0, endPercent - startPercent);
            } else { // Volume
                 // Fixed small width for volume events
                 const minWidthPercent = (1 / totalMinutes) * 100 * 1.5; // Approx 1.5 min width visually
                 widthPercent = minWidthPercent;
            }

            if (widthPercent > 0) {
                const topPosition = index * (barHeight + verticalGap); // Each event gets its own row index

                // Create Bar
                const eventBar = document.createElement('div');
                eventBar.classList.add('event-bar', event.elementClass);
                eventBar.style.left = `${startPercent}%`;
                eventBar.style.width = `${widthPercent}%`;
                eventBar.style.top = `${topPosition}px`;
                eventBar.title = event.titleText;
                eventGridDiv.appendChild(eventBar);

                // Create Label
                const eventLabel = document.createElement('div');
                eventLabel.classList.add('event-label');
                eventLabel.textContent = event.labelText;
                eventLabel.style.top = `${topPosition}px`;
                // Position label immediately after the bar
                const labelLeftPercent = startPercent + widthPercent;
                eventLabel.style.left = `calc(${labelLeftPercent}% + ${labelMargin}px)`;
                eventGridDiv.appendChild(eventLabel);

                currentTop = topPosition + barHeight + verticalGap; // Update position for next potential row

            } else if (DEBUG_SCHEDULE) {
                 console.warn(`[updateVisualization] Skipping zero-width ${event.type} event:`, event);
            }
        });

        // --- Adjust Container Height ---
        const requiredHeight = currentTop; // Height is determined by the bottom of the last event
        const minHeight = 80; // From CSS min-height
        visualizationContainer.style.height = `${Math.max(minHeight, requiredHeight)}px`;

        if (DEBUG_SCHEDULE) console.log(`[updateVisualization] Graphical visualization updated (row per event). Total rows: ${rowEvents.length}. Container height: ${visualizationContainer.style.height}`);
    }


    // --- Text Visualization & Deletion ---
    /**
     * @function updateTextVisualization
     * @brief Updates the text-based event list with delete links.
     */
    function updateTextVisualization() {
        if (DEBUG_SCHEDULE) console.log('[updateTextVisualization] Updating text visualization...');
        if (!textVisualizationDiv) {
            if (DEBUG_SCHEDULE) console.log('[updateTextVisualization] Error: textVisualizationDiv element not found.');
            return;
        }
        textVisualizationDiv.innerHTML = ''; // Clear previous list

        if (!currentSchedule || 
            (!currentSchedule.autopilotWindows?.length && 
             !currentSchedule.durationEvents?.length && 
             !currentSchedule.volumeEvents?.length)) {
            textVisualizationDiv.innerHTML = '<p>No events added yet.</p>';
            return;
        }

        const createEventLine = (text, type, index) => {
            const lineDiv = document.createElement('div');
            lineDiv.classList.add('text-event-line');

            const textSpan = document.createElement('span');
            textSpan.textContent = text;
            lineDiv.appendChild(textSpan);

            // Add delete link only in edit mode
            if (isEditingMode) {
                const deleteLink = document.createElement('span');
                deleteLink.textContent = 'Delete';
                deleteLink.classList.add('delete-event-link');
                deleteLink.dataset.eventType = type;
                deleteLink.dataset.eventIndex = index; // Use index for simplicity
                lineDiv.appendChild(deleteLink);
            }
            return lineDiv;
        };

        // Autopilot Windows
        (currentSchedule.autopilotWindows || []).forEach((ap, index) => {
            const startText = `${minutesToHHMM(ap.startTime)}: AutoPilot Window START (Tension: ${ap.matricTension}kPa, Dose: ${ap.doseVolume}mL, Settle: ${ap.settlingTime}min)`;
            const endText = `${minutesToHHMM(ap.endTime)}: AutoPilot Window END`;
            // Add delete link only to the START line for the whole window
            textVisualizationDiv.appendChild(createEventLine(startText, 'autopilot', index)); 
            textVisualizationDiv.appendChild(createEventLine(endText, 'autopilot_end', index)); // Use different type to prevent delete link
        });

        // Duration Events
        (currentSchedule.durationEvents || []).forEach((dur, index) => {
            const text = `${minutesToHHMM(dur.startTime)}: Duration Event (${dur.duration}s)`;
            textVisualizationDiv.appendChild(createEventLine(text, 'duration', index));
        });

        // Volume Events
        (currentSchedule.volumeEvents || []).forEach((vol, index) => {
            const text = `${minutesToHHMM(vol.startTime)}: Volume Event (${vol.doseVolume}mL)`;
            textVisualizationDiv.appendChild(createEventLine(text, 'volume', index));
        });
         if (DEBUG_SCHEDULE) console.log('[updateTextVisualization] Text visualization updated.');
    }

    /**
     * @function handleDeleteTextEvent
     * @brief Handles clicks on the delete links in the text visualization.
     * @param {Event} e - The click event object.
     */
    function handleDeleteTextEvent(e) {
        if (!e.target.classList.contains('delete-event-link')) return; // Only act on delete links
        if (!isEditingMode) return; // Should not happen if links aren't shown, but safety check

        const eventType = e.target.dataset.eventType;
        const eventIndex = parseInt(e.target.dataset.eventIndex, 10);

        if (DEBUG_SCHEDULE) console.log(`[handleDeleteTextEvent] Clicked delete for type: ${eventType}, index: ${eventIndex}`);

        if (isNaN(eventIndex) || !currentSchedule) return;

        let eventDescription = '';
        let eventArray = null;

        switch (eventType) {
            case 'autopilot':
                eventArray = currentSchedule.autopilotWindows;
                if (eventArray && eventArray[eventIndex]) {
                    eventDescription = `AutoPilot Window (${minutesToHHMM(eventArray[eventIndex].startTime)} - ${minutesToHHMM(eventArray[eventIndex].endTime)})`;
                }
                break;
            case 'duration':
                eventArray = currentSchedule.durationEvents;
                 if (eventArray && eventArray[eventIndex]) {
                    eventDescription = `Duration Event at ${minutesToHHMM(eventArray[eventIndex].startTime)} (${eventArray[eventIndex].duration}s)`;
                }
                break;
            case 'volume':
                eventArray = currentSchedule.volumeEvents;
                 if (eventArray && eventArray[eventIndex]) {
                    eventDescription = `Volume Event at ${minutesToHHMM(eventArray[eventIndex].startTime)} (${eventArray[eventIndex].doseVolume}mL)`;
                }
                break;
            default:
                 if (DEBUG_SCHEDULE) console.warn(`[handleDeleteTextEvent] Unknown event type: ${eventType}`);
                return;
        }

        if (!eventArray || eventIndex < 0 || eventIndex >= eventArray.length) {
             if (DEBUG_SCHEDULE) console.error(`[handleDeleteTextEvent] Invalid index ${eventIndex} for event type ${eventType}`);
            return;
        }

        if (confirm(`Are you sure you want to delete this event?
${eventDescription}`)) {
            eventArray.splice(eventIndex, 1); // Remove the event from the array
            if (DEBUG_SCHEDULE) console.log(`[handleDeleteTextEvent] Event removed. Updating visualizations.`);
            updateVisualization(); // Update graphical view
            updateTextVisualization(); // Update text list
            showEventStatus('Event deleted.', false);
        } else {
             if (DEBUG_SCHEDULE) console.log(`[handleDeleteTextEvent] Deletion cancelled by user.`);
        }
    }


    // --- API Interaction ---
    /**
     * @function fetchSchedules
     * @brief Fetches the list of available schedules from the `/api/schedules` endpoint.
     *
     * Populates the `scheduleSelect` dropdown with the fetched schedule UIDs and their lock status.
     * Stores the full fetched data (including lock info) in the `allSchedulesData` state variable.
     * Handles fetch errors and displays a status message.
     * @async
     */
    async function fetchSchedules() {
        try {
            const response = await fetch('/api/schedules');
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            allSchedulesData = await response.json(); // Store the fetched list
            if (DEBUG_SCHEDULE) console.log('[fetchSchedules] Fetched schedules:', allSchedulesData);

            // Clear existing options except the placeholder
            scheduleSelect.innerHTML = '<option value="">-- Select a Schedule --</option>';

            allSchedulesData.forEach(sch => {
                const option = document.createElement('option');
                option.value = sch.uid;
                let lockText = '';
                // Determine lock status text based on persistent lock field
                if (sch.locked === 1) lockText = ' (Locked: Template)';
                else if (sch.locked === 2) lockText = ' (Locked: Active Cycle)';
                else if (sch.lockedBy) lockText = ` (Editing: ${sch.lockedBy})`; // Show editing lock if present
                option.textContent = `${sch.uid}${lockText}`;
                scheduleSelect.appendChild(option);
            });
        } catch (error) {
            console.error('Error fetching schedules:', error);
            showStatus('Failed to load schedule list.');
            allSchedulesData = []; // Clear local cache on error
        }
    }

    /**
     * @function acquireLock
     * @brief Attempts to acquire an editing lock for a specific schedule UID via the API.
     * @param {string} uid - The UID of the schedule to lock.
     * @returns {Promise<boolean>} True if the lock was acquired successfully, false otherwise.
     * @async
     */
    async function acquireLock(uid) {
        if (DEBUG_SCHEDULE) console.log(`[acquireLock] Attempting for UID: ${uid}`);
        if (!uid) return false;
        try {
            const response = await fetch(`/api/schedule/lock?uid=${encodeURIComponent(uid)}`, { method: 'POST' });
            if (response.ok) {
                if (DEBUG_SCHEDULE) console.log(`[acquireLock] Success for UID: ${uid}`);
                hasEditLock = true;
                return true;
            } else {
                let errorMsg = `HTTP error ${response.status}`;
                try {
                    const errorData = await response.json();
                    errorMsg = errorData.error || response.statusText || errorMsg;
                } catch (e) { /* Ignore JSON parse error if body is not JSON */ }
                showStatus(`Failed to lock schedule: ${errorMsg}`, true);
                hasEditLock = false;
                return false;
            }
        } catch (error) {
            if (DEBUG_SCHEDULE) console.error('[acquireLock] Fetch/JSON Parse Error:', error);
            console.error('Error acquiring lock:', error); // Keep original error log
            showStatus('Network error trying to lock schedule.', true);
            hasEditLock = false;
            return false;
        }
    }

    /**
     * @function releaseLock
     * @brief Releases the editing lock for a specific schedule UID via the API.
     *
     * Only attempts release if `hasEditLock` is true for the given UID.
     * Updates the `hasEditLock` state variable regardless of API call success/failure.
     * @param {string} uid - The UID of the schedule whose lock should be released.
     * @async
     */
    async function releaseLock(uid) {
        if (DEBUG_SCHEDULE) console.log(`[releaseLock] Attempting for UID: ${uid}, hasLock: ${hasEditLock}`);
        if (!uid || !hasEditLock) return; // Only release if we think we have it
        try {
            const response = await fetch(`/api/schedule/lock?uid=${encodeURIComponent(uid)}`, { method: 'DELETE' });
            if (!response.ok) {
                 console.warn(`Failed to release lock for ${uid}, status: ${response.status}`);
            } else {
                 if (DEBUG_SCHEDULE) console.log(`Lock released for ${uid}`);
            }
        } catch (error) {
            if (DEBUG_SCHEDULE) console.error('[releaseLock] Fetch Error:', error);
            console.error('Error releasing lock:', error); // Keep original error log
        } finally {
            hasEditLock = false; // Assume released even on error
        }
    }

    // --- Form Read-only State Management ---
    /**
     * @function setFormReadOnly
     * @brief Sets the edit form elements to be read-only or editable.
     *
     * Toggles the `disabled` property on input fields and buttons within the edit section.
     * Manages the enabled/disabled state of the top-level action buttons (Edit, Delete, Copy)
     * based on whether the form is read-only, if a schedule is selected, and the schedule's
     * persistent lock status (template/cycle lock).
     * @param {boolean} isReadOnly - True to make the form read-only, false to make it editable.
     */
    function setFormReadOnly(isReadOnly) {
        if (DEBUG_SCHEDULE) console.log(`[setFormReadOnly] Setting read-only: ${isReadOnly}`);
        isEditingMode = !isReadOnly; // Update state tracker

        if (isReadOnly) {
            editSection.classList.add('readonly-mode');
        } else {
            editSection.classList.remove('readonly-mode');
        }

        // Toggle 'disabled' property for form input elements
        const elementsToToggle = [
            scheduleNameInput, lightsOnInput, lightsOffInput,
            apStartTimeInput, apEndTimeInput, apMatricTensionInput, apDoseVolumeInput, apSettlingTimeInput,
            durStartTimeInput, durDurationInput, durRepetitionsInput, durIntervalInput,
            volStartTimeInput, volDoseVolumeInput, volRepetitionsInput, volIntervalInput,
            addEventBtn, saveScheduleBtn
        ];

        elementsToToggle.forEach(el => {
            if (el) { // Check if element exists
                 el.disabled = isReadOnly;
            }
        });

         // --- Button State Logic ---
         let isTemplateLocked = false;
         let isActiveCycleLocked = false;
         const selectedScheduleData = allSchedulesData.find(s => s.uid === currentEditingUID);

         if (selectedScheduleData) {
             isTemplateLocked = selectedScheduleData.locked === 1;
             isActiveCycleLocked = selectedScheduleData.locked === 2;
         }

         const canPerformActions = !isCreatingNew && currentEditingUID && !isEditingMode; // Viewing existing, not editing

         if (editSelectedBtn) {
             editSelectedBtn.disabled = !canPerformActions || isActiveCycleLocked;
             if (DEBUG_SCHEDULE) console.log(`[setFormReadOnly] editSelectedBtn.disabled set to: ${editSelectedBtn.disabled} (canPerformActions: ${canPerformActions}, isActiveCycleLocked: ${isActiveCycleLocked})`);
         }
         if (deleteSelectedBtn) {
             deleteSelectedBtn.disabled = !canPerformActions || isTemplateLocked || isActiveCycleLocked;
              if (DEBUG_SCHEDULE) console.log(`[setFormReadOnly] deleteSelectedBtn.disabled set to: ${deleteSelectedBtn.disabled} (canPerformActions: ${canPerformActions}, isTemplateLocked: ${isTemplateLocked}, isActiveCycleLocked: ${isActiveCycleLocked})`);
         }
         if (copySelectedBtn) {
             copySelectedBtn.disabled = !canPerformActions; // Can copy any existing schedule
              if (DEBUG_SCHEDULE) console.log(`[setFormReadOnly] copySelectedBtn.disabled set to: ${copySelectedBtn.disabled} (canPerformActions: ${canPerformActions})`);
         }

         // Save button is enabled only when editing
         if (saveScheduleBtn) saveScheduleBtn.disabled = isReadOnly;
         // Add Event button is enabled only when editing
         if (addEventBtn) addEventBtn.disabled = isReadOnly;
         // Cancel button is always enabled when the section is visible
         if (cancelEditBtn) cancelEditBtn.disabled = false;

         // Ensure UID field remains readonly regardless of mode
         if (scheduleUIDInput) scheduleUIDInput.readOnly = true;
    }


    // --- Event Handlers ---
    /**
     * @function handleScheduleSelect
     * @brief Handles the change event on the schedule selection dropdown.
     *
     * Fetches the details of the selected schedule, populates the form,
     * displays the edit section in read-only mode. Releases any existing edit lock
     * before fetching the new schedule. Hides the edit section if the placeholder is selected.
     * @async
     */
    async function handleScheduleSelect() {
        const selectedUID = scheduleSelect.value;
        if (DEBUG_SCHEDULE) console.log(`[handleScheduleSelect] Selected UID: ${selectedUID}`);

        // If we were editing (had lock), release it before viewing another
        if (hasEditLock) {
            if (DEBUG_SCHEDULE) console.log(`[handleScheduleSelect] Releasing lock for previous UID: ${currentEditingUID} due to new selection.`);
            await releaseLock(currentEditingUID); // Release lock silently
            hasEditLock = false; // Ensure lock state is reset
        }

        // If user selects "-- Select a Schedule --"
        if (!selectedUID) {
            hideEditSection(); // Hide section, disable edit button etc.
            currentEditingUID = null;
            return;
        }

        // If selecting the same schedule already being viewed/edited, do nothing
        if (selectedUID === currentEditingUID) {
             if (DEBUG_SCHEDULE) console.log(`[handleScheduleSelect] Same UID selected (${selectedUID}), no change.`);
             return;
        }

        currentEditingUID = selectedUID;
        isCreatingNew = false;
        // --- LOCK IS NOT ACQUIRED ON SELECT ---

        // Fetch schedule data for display
        if (DEBUG_SCHEDULE) console.log(`[handleScheduleSelect] Fetching data for UID: ${currentEditingUID}`);
        try {
            const response = await fetch(`/api/schedule?uid=${encodeURIComponent(currentEditingUID)}`);
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            currentSchedule = await response.json();
            if (DEBUG_SCHEDULE) console.log('[handleScheduleSelect] Fetched schedule data:', currentSchedule); // Log fetched data

            populateEditForm();     // Populate form fields AND update visualization
            showEditSection();      // Show the section
            setFormReadOnly(true);  // Set form to read-only state initially, this also sets button states

        } catch (error) {
            if (DEBUG_SCHEDULE) console.error('[handleScheduleSelect] Fetch Error:', error);
            console.error('Error fetching schedule details:', error); // Keep original
            showStatus(`Failed to load schedule ${currentEditingUID}.`);
            hideEditSection(); // Hide section on error
            currentEditingUID = null;
        }
    }

    /**
     * @function handleEditSelected
     * @brief Handles the click event for the "Edit Selected" button.
     *
     * Attempts to acquire an edit lock for the currently selected schedule.
     * If successful, sets the form to editable mode. Displays status messages
     * based on lock acquisition success or failure.
     * @async
     */
    async function handleEditSelected() {
        if (!currentEditingUID || isCreatingNew) return;
        if (DEBUG_SCHEDULE) console.log(`[handleEditSelected] Attempting lock for UID: ${currentEditingUID}`);

        if (await acquireLock(currentEditingUID)) {
            if (DEBUG_SCHEDULE) console.log(`[handleEditSelected] Lock acquired. Enabling edit mode.`);
            setFormReadOnly(false); // Make form editable, disable Edit/Delete/Copy
            editSectionTitle.textContent = `Edit Schedule: ${currentSchedule?.scheduleName || currentEditingUID}`; // Update title
        } else {
            if (DEBUG_SCHEDULE) console.log(`[handleEditSelected] Lock failed. Cannot enable edit mode.`);
            // Status message shown by acquireLock
        }
    }

    /**
     * @function handleDeleteSelected
     * @brief Handles the click event for the "Delete Selected" button.
     *
     * Confirms the deletion with the user. Checks persistent lock status.
     * Sends a DELETE request to the API for the currently selected schedule.
     * Refreshes the schedule list and hides the edit section on success.
     * Displays status messages.
     * @async
     */
    async function handleDeleteSelected() {
        if (!currentEditingUID || isCreatingNew || isEditingMode) return;
        if (DEBUG_SCHEDULE) console.log(`[handleDeleteSelected] Attempting delete for UID: ${currentEditingUID}`);

        // Check persistent lock status again just in case
        const selectedScheduleData = allSchedulesData.find(s => s.uid === currentEditingUID);
        if (!selectedScheduleData || selectedScheduleData.locked === 1 || selectedScheduleData.locked === 2) {
            showStatus('Cannot delete locked schedule (Template or Active Cycle).', true);
            return;
        }

        if (confirm(`Are you sure you want to delete schedule "${currentSchedule?.scheduleName || currentEditingUID}"? This cannot be undone.`)) {
            try {
                const response = await fetch(`/api/schedule?uid=${encodeURIComponent(currentEditingUID)}`, { method: 'DELETE' });
                if (response.ok) {
                    showStatus('Schedule deleted successfully.', false);
                    hideEditSection(); // Hide section and reset state
                    fetchSchedules(); // Refresh the dropdown list
                } else {
                    let errorMsg = `HTTP error ${response.status}`;
                    try {
                        const errorData = await response.json();
                        errorMsg = errorData.error || response.statusText || errorMsg;
                    } catch (e) { /* Ignore JSON parse error */ }
                    showStatus(`Failed to delete schedule: ${errorMsg}`, true);
                }
            } catch (error) {
                 if (DEBUG_SCHEDULE) console.error('[handleDeleteSelected] Fetch Error:', error);
                 console.error('Error deleting schedule:', error);
                 showStatus('Network error trying to delete schedule.', true);
            }
        } else {
             if (DEBUG_SCHEDULE) console.log('[handleDeleteSelected] Deletion cancelled by user.');
        }
    }

    /**
     * @function handleCopySelected
     * @brief Handles the click event for the "Copy Selected" button.
     *
     * Creates a deep copy of the `currentSchedule` data, modifies the name and UID
     * to indicate it's a copy, releases any existing edit lock, and sets the form
     * state to "creating new" (editable) populated with the copied data.
     */
    function handleCopySelected() {
        if (!currentEditingUID || isCreatingNew || isEditingMode || !currentSchedule) return;
        if (DEBUG_SCHEDULE) console.log(`[handleCopySelected] Copying schedule UID: ${currentEditingUID}`);

        // If we were editing (had lock), release it before copying
        if (hasEditLock) {
            if (DEBUG_SCHEDULE) console.log(`[handleCopySelected] Releasing lock for previous UID: ${currentEditingUID} before copying.`);
            // Don't await, just fire and forget release? Or await? Let's await.
            releaseLock(currentEditingUID); // Release lock silently
            hasEditLock = false; // Ensure lock state is reset
        }

        // Deep copy the current schedule data
        // Using JSON stringify/parse for a simple deep copy
        const copiedSchedule = JSON.parse(JSON.stringify(currentSchedule));

        // Modify for copy state
        copiedSchedule.scheduleName = `Copy of ${copiedSchedule.scheduleName}`;
        copiedSchedule.scheduleUID = "(Copy)"; // Placeholder

        // Set state for creation
        isCreatingNew = true;
        currentEditingUID = null; // No UID for the copy yet
        hasEditLock = false;
        currentSchedule = copiedSchedule; // Use the copied data

        populateEditForm();     // Populate form with copied data
        showEditSection(true);  // Show as "Create" (or maybe "Edit Copy"?)
        setFormReadOnly(false); // Make form editable
        editSectionTitle.textContent = `Edit Copy: ${currentSchedule.scheduleName}`; // Update title

        if (DEBUG_SCHEDULE) console.log('[handleCopySelected] Copied schedule data:', currentSchedule);
    }


    /**
     * @function handleCreateNew
     * @brief Handles the click event for the "Create New" button.
     *
     * Resets the form state, initializes an empty `currentSchedule` object with defaults,
     * shows the edit section, and makes the form editable. Releases any existing edit lock first.
     */
    function handleCreateNew() {
        // If we were editing/viewing something else, release lock first
        handleCancelEdit();

        isCreatingNew = true;
        currentEditingUID = null; // No UID for new schedule yet
        hasEditLock = false; // No lock needed for creation initially
        currentSchedule = { // Initialize empty schedule object with defaults
            scheduleName: "",
            lightsOnTime: hhmmToMinutes("06:00"), // Default lights on
            lightsOffTime: hhmmToMinutes("22:00"), // Default lights off
            scheduleUID: "(New)", // Placeholder UID
            autopilotWindows: [],
            durationEvents: [],
            volumeEvents: []
        };
        populateEditForm();     // Populate form fields AND update visualization
        showEditSection(true);  // Show as "Create"
        setFormReadOnly(false); // Make form editable for new schedule
    }

    /**
     * @function handleAddEvent
     * @brief Handles the click event for the "Add Event" button.
     *
     * Determines the active event type tab (Autopilot, Duration, Volume).
     * Reads the input values for that event type.
     * Performs client-side validation (using placeholder functions for now).
     * Handles repetitions for Duration and Volume events.
     * If valid, adds the new event(s) to the `currentSchedule` object's corresponding array,
     * sorts the array, updates the visualization, and shows a success message.
     * Shows an error message if validation fails or data processing errors occur.
     */
    function handleAddEvent() {
        // Determine active tab
        const activeTab = document.querySelector('.tab-button.active').dataset.tab;
        let newEvent = null;
        let validationResult = false;

        if (!currentSchedule) {
             showEventStatus('Cannot add event: No schedule loaded.', true);
             return;
        }
        // Ensure event arrays exist
        currentSchedule.autopilotWindows = currentSchedule.autopilotWindows || [];
        currentSchedule.durationEvents = currentSchedule.durationEvents || [];
        currentSchedule.volumeEvents = currentSchedule.volumeEvents || [];


        try {
            if (activeTab === 'autopilotTab') {
                newEvent = {
                    startTime: hhmmToMinutes(apStartTimeInput.value),
                    endTime: hhmmToMinutes(apEndTimeInput.value),
                    matricTension: parseFloat(apMatricTensionInput.value),
                    doseVolume: parseInt(apDoseVolumeInput.value, 10),
                    settlingTime: parseInt(apSettlingTimeInput.value, 10)
                };
                validationResult = validateAutopilotEvent(newEvent);
                if(validationResult) {
                    currentSchedule.autopilotWindows.push(newEvent);
                    currentSchedule.autopilotWindows.sort((a,b) => a.startTime - b.startTime);
                }

            } else if (activeTab === 'durationTab') {
                const startTime = hhmmToMinutes(durStartTimeInput.value);
                const duration = parseInt(durDurationInput.value, 10);
                const repetitions = parseInt(durRepetitionsInput.value, 10);
                const interval = parseInt(durIntervalInput.value, 10);
                let eventsToAdd = [];

                for (let i = 0; i <= repetitions; i++) {
                    const eventStartTime = startTime + (i * interval);
                    const eventEndTime = eventStartTime + Math.ceil(duration / 60.0);
                     if (eventStartTime > 1439 || eventEndTime > 1439) {
                         showEventStatus(`Event repetition ${i+1} exceeds 23:59.`, true);
                         validationResult = false;
                         break; // Stop adding if time exceeds limit
                     }
                    newEvent = {
                        startTime: eventStartTime,
                        duration: duration,
                        endTime: Math.min(eventEndTime, 1439) // Cap end time
                    };
                    validationResult = validateDurationEvent(newEvent);
                    if(!validationResult) break;
                    eventsToAdd.push(newEvent);
                }
                 if(validationResult) { // Only add if all repetitions were valid
                    currentSchedule.durationEvents.push(...eventsToAdd);
                    currentSchedule.durationEvents.sort((a,b) => a.startTime - b.startTime);
                }


            } else if (activeTab === 'volumeTab') {
                 const startTime = hhmmToMinutes(volStartTimeInput.value);
                 const doseVolume = parseInt(volDoseVolumeInput.value, 10);
                 const repetitions = parseInt(volRepetitionsInput.value, 10);
                 const interval = parseInt(volIntervalInput.value, 10);
                 let eventsToAdd = [];

                 for (let i = 0; i <= repetitions; i++) {
                     const eventStartTime = startTime + (i * interval);
                      if (eventStartTime > 1439) {
                         showEventStatus(`Event repetition ${i+1} exceeds 23:59.`, true);
                         validationResult = false;
                         break; // Stop adding if time exceeds limit
                     }
                     newEvent = {
                         startTime: eventStartTime,
                         doseVolume: doseVolume
                     };
                     validationResult = validateVolumeEvent(newEvent);
                     if(!validationResult) break;
                     eventsToAdd.push(newEvent);
                 }
                  if(validationResult) { // Only add if all repetitions were valid
                    currentSchedule.volumeEvents.push(...eventsToAdd);
                    currentSchedule.volumeEvents.sort((a,b) => a.startTime - b.startTime);
                }
            }

            if (validationResult) {
                updateVisualization();
        updateTextVisualization(); // Update text list
                showEventStatus('Event added successfully.', false);
                // Optionally clear input fields
            } else {
                 // Error message shown by validation functions
            }
        } catch (e) {
            console.error("Error adding event:", e);
            showEventStatus('Error processing event data.', true);
        }
    }

    // --- Placeholder Validation Functions (Implement based on C++ logic) ---
    /**
     * @function validateAutopilotEvent
     * @brief Performs client-side validation for a new Autopilot event.
     *        Checks time validity and overlap with existing Autopilot windows.
     * @param {object} event - The Autopilot event object to validate.
     * @returns {boolean} True if the event is valid, false otherwise (shows status message on error).
     */
    function validateAutopilotEvent(event) {
        if (!currentSchedule || !currentSchedule.autopilotWindows) return false; // Safety check
        if (event.endTime <= event.startTime) {
            showEventStatus('AutoPilot end time must be after start time.', true); return false;
        }
        // Add overlap checks against currentSchedule.autopilotWindows
        for (const existing of currentSchedule.autopilotWindows) {
             if (event.startTime == existing.startTime) { showEventStatus('Overlap: Starts at same time as existing AP window.', true); return false; }
             if (event.endTime == existing.endTime) { showEventStatus('Overlap: Ends at same time as existing AP window.', true); return false; }
             if (event.startTime < existing.startTime && event.endTime > existing.endTime) { showEventStatus('Overlap: Envelops existing AP window.', true); return false; }
             if (event.startTime > existing.startTime && event.startTime < existing.endTime) { showEventStatus('Overlap: Starts within existing AP window.', true); return false; }
             if (event.endTime > existing.startTime && event.endTime < existing.endTime) { showEventStatus('Overlap: Ends within existing AP window.', true); return false; }
        }
        return true; // Placeholder
    }
    /**
     * @function validateDurationEvent
     * @brief Performs client-side validation for a new Duration event.
     *        Checks event limits and overlaps with existing Duration and Volume events.
     * @param {object} event - The Duration event object to validate.
     * @returns {boolean} True if the event is valid, false otherwise (shows status message on error).
     */
    function validateDurationEvent(event) {
         if (!currentSchedule || !currentSchedule.durationEvents || !currentSchedule.volumeEvents) return false; // Safety check
         if ((currentSchedule.durationEvents.length + currentSchedule.volumeEvents.length + 1) > 100) {
             showEventStatus('Cannot add event: Maximum total Duration/Volume events (100) reached.', true); return false;
         }
         // Add overlap checks against currentSchedule.durationEvents and currentSchedule.volumeEvents
         for (const existing of currentSchedule.durationEvents) {
             if (event.startTime == existing.startTime) { showEventStatus('Overlap: Starts at same time as existing Duration event.', true); return false; }
             if (event.startTime > existing.startTime && event.startTime < existing.endTime) { showEventStatus('Overlap: Starts within existing Duration event.', true); return false; }
             if (event.duration > 0 && event.endTime > existing.startTime && event.endTime < existing.endTime) { showEventStatus('Overlap: Ends within existing Duration event.', true); return false; }
             if (event.duration > 0 && event.startTime < existing.startTime && event.endTime > existing.endTime) { showEventStatus('Overlap: Envelops existing Duration event.', true); return false; }
         }
          for (const existing of currentSchedule.volumeEvents) {
             if (event.startTime == existing.startTime) { showEventStatus('Overlap: Starts at same time as existing Volume event.', true); return false; }
         }
        return true; // Placeholder
    }
     /**
      * @function validateVolumeEvent
      * @brief Performs client-side validation for a new Volume event.
      *        Checks event limits and overlaps with existing Volume and Duration events.
      * @param {object} event - The Volume event object to validate.
      * @returns {boolean} True if the event is valid, false otherwise (shows status message on error).
      */
     function validateVolumeEvent(event) {
         if (!currentSchedule || !currentSchedule.durationEvents || !currentSchedule.volumeEvents) return false; // Safety check
         if ((currentSchedule.durationEvents.length + currentSchedule.volumeEvents.length + 1) > 100) {
             showEventStatus('Cannot add event: Maximum total Duration/Volume events (100) reached.', true); return false;
         }
         // Add overlap checks against currentSchedule.durationEvents and currentSchedule.volumeEvents
         for (const existing of currentSchedule.volumeEvents) {
             if (event.startTime == existing.startTime) { showEventStatus('Overlap: Starts at same time as existing Volume event.', true); return false; }
         }
          for (const existing of currentSchedule.durationEvents) {
             if (event.startTime == existing.startTime) { showEventStatus('Overlap: Starts at same time as existing Duration event.', true); return false; }
             if (event.startTime > existing.startTime && event.startTime < existing.endTime) { showEventStatus('Overlap: Starts within existing Duration event.', true); return false; }
         }
        return true; // Placeholder
    }
    // --- End Validation Placeholders ---


    /**
     * @function handleSaveSchedule
     * @brief Handles the click event for the "Save Schedule" button.
     *
     * Updates the `currentSchedule` object with the latest form values.
     * Performs basic validation (e.g., non-empty name).
     * Acquires lock if editing an existing schedule and lock not already held.
     * Sends a POST (for new/copy) or PUT (for existing) request to the API with the schedule data.
     * Releases the lock on successful PUT. Refreshes the schedule list and hides the edit section on success.
     * Displays status messages.
     * @async
     */
    async function handleSaveSchedule() {
        if (DEBUG_SCHEDULE) console.log(`[handleSaveSchedule] Start. isCreatingNew: ${isCreatingNew}, currentEditingUID: ${currentEditingUID}, hasEditLock: ${hasEditLock}`);
        if (!currentSchedule) return;

        // Update schedule object from form fields
        currentSchedule.scheduleName = scheduleNameInput.value;
        currentSchedule.lightsOnTime = hhmmToMinutes(lightsOnInput.value);
        currentSchedule.lightsOffTime = hhmmToMinutes(lightsOffInput.value);

        // Basic validation
        if (!currentSchedule.scheduleName) {
            showStatus('Schedule Name cannot be empty.', true);
            return;
        }
        // Prevent saving with placeholder UID
        if (isCreatingNew && currentSchedule.scheduleUID === "(Copy)") {
             currentSchedule.scheduleUID = ""; // Ensure UID is empty for backend generation
        }


        // --- Acquire lock BEFORE saving an existing schedule (if not already held) ---
        if (!isCreatingNew && !hasEditLock) {
             if (DEBUG_SCHEDULE) console.log(`[handleSaveSchedule] Attempting to acquire lock for existing UID: ${currentEditingUID} before saving.`);
             if (!(await acquireLock(currentEditingUID))) {
                 showStatus('Failed to acquire edit lock before saving. Another user might be editing, or the schedule is locked.', true);
                 return; // Stop save if lock fails
             }
             // Lock acquired successfully if we reach here
             if (DEBUG_SCHEDULE) console.log(`[handleSaveSchedule] Lock acquired successfully for save.`);
        } else if (!isCreatingNew && hasEditLock) {
             if (DEBUG_SCHEDULE) console.log(`[handleSaveSchedule] Already have lock for UID: ${currentEditingUID}. Proceeding with save.`);
        } else if (isCreatingNew) {
             if (DEBUG_SCHEDULE) console.log(`[handleSaveSchedule] Creating new schedule (or saving copy), no lock needed before POST.`);
        }
        // --- End lock acquisition ---


        const url = isCreatingNew ? '/api/schedule' : `/api/schedule?uid=${encodeURIComponent(currentEditingUID)}`;
        const method = isCreatingNew ? 'POST' : 'PUT';

        // Construct payload
        const payload = {
            scheduleName: currentSchedule.scheduleName,
            lightsOnTime: currentSchedule.lightsOnTime,
            lightsOffTime: currentSchedule.lightsOffTime,
            // Send UID only for PUT, backend uses it from query param
            scheduleUID: isCreatingNew ? "" : currentEditingUID,
            autopilotWindows: currentSchedule.autopilotWindows || [],
            durationEvents: currentSchedule.durationEvents || [],
            volumeEvents: currentSchedule.volumeEvents || []
        };
        // If creating, send name in payload for backend parsing
        if (isCreatingNew) {
            payload.name = currentSchedule.scheduleName;
        }

        // --- Add JS Debugging ---
        if (DEBUG_SCHEDULE) {
            console.log("--- Saving Schedule ---");
            console.log("Method:", method);
            console.log("URL:", url);
            console.log("Payload Object:", payload); // Log the object before stringify
            const bodyString = JSON.stringify(payload);
            console.log("Payload String:", bodyString);
            console.log("-----------------------");
        }
        // --- End JS Debugging ---

        try {
            const response = await fetch(url, {
                method: method,
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload) // Use the stringified version
            });

            if (response.ok) {
                // Try to parse JSON, might be empty on 200 OK for PUT
                let resultData = {};
                try {
                    resultData = await response.json();
                } catch (e) {
                    if (DEBUG_SCHEDULE) console.log("[handleSaveSchedule] Response OK but no JSON body (likely PUT).");
                }

                if (isCreatingNew && resultData.scheduleUID) {
                    // currentEditingUID = resultData.scheduleUID; // Don't set this here, let fetchSchedules update
                    if (DEBUG_SCHEDULE) console.log(`[handleSaveSchedule] New schedule created with UID: ${resultData.scheduleUID}`);
                }

                showStatus(`Schedule ${isCreatingNew ? 'created' : 'updated'} successfully.`, false);
                // Release lock only if we were editing an existing schedule
                if (!isCreatingNew && hasEditLock) {
                    await releaseLock(currentEditingUID);
                }
                hideEditSection(); // Hide section and reset state
                fetchSchedules(); // Refresh the dropdown list
            } else {
                let errorMsg = `HTTP error ${response.status}`;
                 try {
                    const errorData = await response.json();
                    errorMsg = errorData.error || response.statusText || errorMsg;
                } catch (e) { /* Ignore JSON parse error if body is not JSON */ }
                showStatus(`Failed to save schedule: ${errorMsg}`, true);
                // Keep lock if save failed? Yes, allow user to retry or cancel.
            }
        } catch (error) {
            if (DEBUG_SCHEDULE) console.error('[handleSaveSchedule] Fetch/Save Error:', error);
            console.error('Fetch Error in handleSaveSchedule:', error); // Keep original
            showStatus('Network error saving schedule.', true);
            // Do NOT release lock here if save failed, allow retry/cancel
        }
    }

    /**
     * @function handleCancelEdit
     * @brief Handles the click event for the "Cancel" button.
     *
     * Releases the edit lock if it was held for the current schedule.
     * Hides the edit section and resets the UI state.
     * @async
     */
    async function handleCancelEdit() {
        if (DEBUG_SCHEDULE) console.log(`[handleCancelEdit] Called. Current UID: ${currentEditingUID}, Has Lock: ${hasEditLock}`);
        // Release lock only if we were actually editing (had the lock)
        if (currentEditingUID && hasEditLock) {
            if (DEBUG_SCHEDULE) console.log(`[handleCancelEdit] Releasing lock for UID: ${currentEditingUID}`);
            await releaseLock(currentEditingUID); // Release lock
        }
        // Always hide the section and reset state regardless of lock
        hideEditSection(); // Hides section, resets state vars, disables buttons
    }

    // --- UI Update Functions ---
    /**
     * @function populateEditForm
     * @brief Populates the form fields with data from the `currentSchedule` object.
     *        Also updates the schedule visualization.
     */
    function populateEditForm() {
        if (!currentSchedule) return;
        if (DEBUG_SCHEDULE) console.log('[populateEditForm] Populating form with data:', currentSchedule);

        scheduleNameInput.value = currentSchedule.scheduleName || "";
        scheduleUIDInput.value = currentSchedule.scheduleUID || (isCreatingNew ? "(New)" : "");
        lightsOnInput.value = minutesToHHMM(currentSchedule.lightsOnTime);
        lightsOffInput.value = minutesToHHMM(currentSchedule.lightsOffTime);

        // ** Crucial: Call updateVisualization AFTER basic fields are set **
        updateVisualization();
        updateTextVisualization(); // Also update text list

        // Clear event add status
        if(eventAddStatus) eventAddStatus.style.display = 'none';
    }

    /**
     * @function showEditSection
     * @brief Makes the schedule edit/view section visible and updates its title.
     * @param {boolean} [isNew=false] - If true, sets the title for creating a new schedule.
     */
    function showEditSection(isNew = false) {
        // Update title based on whether creating or viewing/editing
        if (isNew) {
             editSectionTitle.textContent = 'Create New Schedule';
        } else {
             // Title changes when edit mode is entered via handleEditSelected
             editSectionTitle.textContent = `View Schedule: ${currentSchedule?.scheduleName || currentEditingUID}`;
        }
        editSection.style.display = 'block';
        // Scroll to edit section?
        // editSection.scrollIntoView({ behavior: 'smooth' });
    }

    /**
     * @function hideEditSection
     * @brief Hides the schedule edit/view section and resets related state variables and UI elements.
     */
    function hideEditSection() {
        if (DEBUG_SCHEDULE) console.log(`[hideEditSection] Hiding edit section.`);
        editSection.style.display = 'none';
        editSection.classList.remove('readonly-mode'); // Ensure readonly class is removed
        currentSchedule = null;
        isCreatingNew = false;
        isEditingMode = false; // Reset editing mode flag
        currentEditingUID = null; // Clear the UID being worked on
        hasEditLock = false; // Reset lock status flag
        if (editSelectedBtn) editSelectedBtn.disabled = true; // Disable buttons
        if (deleteSelectedBtn) deleteSelectedBtn.disabled = true;
        if (copySelectedBtn) copySelectedBtn.disabled = true;
        if (scheduleSelect) scheduleSelect.value = ""; // Reset dropdown selection
        if (visualizationPre) visualizationPre.textContent = 'No schedule data loaded.'; // Clear visualization
        if (textVisualizationDiv) textVisualizationDiv.innerHTML = '<p>No events added yet.</p>'; // Clear text visualization
    }
    createTimelineLabels(); // Draw the timeline initially

    // --- Slider Value Display ---
     /**
      * @function setupSliderDisplay
      * @brief Sets up a slider input to update a corresponding display element dynamically.
      * @param {string} sliderId - The ID of the slider input element.
      * @param {string} displayId - The ID of the span/element to display the value.
      * @param {string} unit - The unit string to append to the value (e.g., "min", "s").
      */
     function setupSliderDisplay(sliderId, displayId, unit) {
        const slider = document.getElementById(sliderId);
        const display = document.getElementById(displayId);
        if (slider && display) {
            // Initial display update
            display.textContent = `${slider.value} ${unit}`;
            // Update display on slider input
            slider.addEventListener('input', () => {
                display.textContent = `${slider.value} ${unit}`;
            });
        }
    }

    // --- Event Listener Setup ---
    if (scheduleSelect) scheduleSelect.addEventListener('change', handleScheduleSelect);
    if (createNewBtn) createNewBtn.addEventListener('click', handleCreateNew);
    if (editSelectedBtn) editSelectedBtn.addEventListener('click', handleEditSelected);
    if (deleteSelectedBtn) deleteSelectedBtn.addEventListener('click', handleDeleteSelected); // New
    if (copySelectedBtn) copySelectedBtn.addEventListener('click', handleCopySelected);     // New
    if (addEventBtn) addEventBtn.addEventListener('click', handleAddEvent);
    if (saveScheduleBtn) saveScheduleBtn.addEventListener('click', handleSaveSchedule);
    if (cancelEditBtn) cancelEditBtn.addEventListener('click', handleCancelEdit);
    if (tabsContainer) tabsContainer.addEventListener('click', (e) => {
        if (e.target.classList.contains('tab-button')) {
            // Remove active class from all buttons and content
            tabButtons.forEach(btn => btn.classList.remove('active'));
            tabContents.forEach(content => content.classList.remove('active'));

            // Add active class to the clicked button and corresponding content
            const tabId = e.target.dataset.tab;
            e.target.classList.add('active');
            const activeContent = document.getElementById(tabId);
            if (activeContent) activeContent.classList.add('active');
        }
    });
    // Text visualization delete listener (using event delegation)
    if (textVisualizationDiv) textVisualizationDiv.addEventListener('click', handleDeleteTextEvent);
    // Slider value displays
    setupSliderDisplay('durInterval', 'durIntervalValue', 'min');
    setupSliderDisplay('volInterval', 'volIntervalValue', 'min');


    // --- Initial Load ---
    fetchSchedules();
    hideEditSection(); // Ensure edit section is hidden and state is reset initially

});