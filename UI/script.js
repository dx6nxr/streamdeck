document.addEventListener('DOMContentLoaded', () => {
    // --- State & Configuration ---
    // Initialize with defaults FIRST to ensure they exist
    let config = {
        numContainers: 4,
        numDropdowns: 6,
        theme: 'light',
        groups: {},
        settings: {}
    };
    let currentBindings = []; // Initialize as empty array
    let isRecordingKey = false;
    let capturedKeys = null;
    let initialGroupCount = config.numContainers; // Will be updated after loading prefs
    let initialDropdownCount = config.numDropdowns;

    // Debounce function
    function debounce(func, wait) {
        let timeout;
        return function executedFunction(...args) {
            const later = () => {
                clearTimeout(timeout);
                func(...args);
            };
            clearTimeout(timeout);
            timeout = setTimeout(later, wait);
        };
    }

    // --- Backend API URLs ---
    // IMPORTANT: Replace these with your actual C++ backend server URLs!
    const API_URLS = {
        loadConfig: 'http://localhost:8080/api/load-config', // GET
        saveConfig: 'http://localhost:8080/api/save-config', // POST
        loadBinds: 'http://localhost:8080/api/load-binds',   // GET
        saveBinds: 'http://localhost:8080/api/save-binds',   // POST
        fetchApps: 'http://localhost:8080/api/get-apps'      // POST or GET
    };

    // --- DOM Element References --- (Ensure these IDs match your HTML)
    const bodyElement = document.body;
    const availableAppsListDiv = document.getElementById('app-list');
    const groupBoxesWrapperDiv = document.getElementById('group-boxes-wrapper');
    const dropdownGridDiv = document.getElementById('dropdown-grid');
    const fetchAppsBtn = document.getElementById('fetch-apps-btn');
    const settingsBtn = document.getElementById('settings-btn');
    const jsonOutputPre = document.getElementById('json-output');
    const settingsModal = document.getElementById('settings-modal');
    const closeModalBtn = document.getElementById('modal-close-btn');
    const numGroupsInput = document.getElementById('setting-num-groups');
    const darkThemeCheckbox = document.getElementById('setting-dark-theme');
    const groupWarningText = document.querySelector('.settings-warning.group-warning');
    const keyComboInput = document.getElementById('setting-key-combo');
    const recordKeyBtn = document.getElementById('record-key-btn');
    const keyActionInput = document.getElementById('setting-key-action');
    const addBindingBtn = document.getElementById('add-binding-btn');
    const bindingsListUl = document.getElementById('bindings-list');
    const applySettingsBtn = document.getElementById('settings-apply-btn');
    const numDropdownsInput = document.getElementById('setting-num-dropdowns');

    // --- Drag & Drop State ---
    let draggedApp = null;

    // --- Initialization ---
    async function initializeApp() {
        showLoadingState("Initializing...");

        // 1. Initialize state with defaults guaranteed to exist
        config = { groups: {}, settings: {}, numContainers: 4, theme: 'light' };
        currentBindings = [];

        // 2. Load preferences which might override some defaults
        loadPreferencesFromStorage();
        applyTheme();
        initialGroupCount = config.numContainers; // Set initial count based on prefs/defaults

        // 3. Attempt to load data from backend, letting catch blocks overwrite defaults if successful
        try {
            console.log("Attempting to load config and bindings...");
            await Promise.all([
                loadConfigFromServer(), // Will try to overwrite config.groups/settings
                loadBindsFromServer()   // Will try to overwrite currentBindings
            ]);
            console.log("Finished loading config and bindings attempts.");
        } catch (error) {
            console.error("Error during initial data loading phase (Promise.all):", error);
            // Errors inside load functions are handled there, this catches issues with Promise.all itself
        }

        // 4. Always initialize UI structure and add listeners using the current state (loaded or default)
        try {
            initializeUI();     // Build UI elements
            addEventListeners(); // Add interactivity AFTER elements exist
        } catch (uiError) {
            console.error("CRITICAL ERROR initializing UI or adding listeners:", uiError);
            showError("Critical UI initialization failed. Check console.");
            return; // Stop if basic UI/listeners fail
        }

        // 5. Attempt to fetch initial app list
        try {
            await fetchApplicationsFromServer();
        } catch (error) {
            console.error("Error fetching initial applications:", error);
            // Error display handled within fetchApplicationsFromServer's catch
        }

        hideLoadingState();
        console.log("App Initialization attempt complete.");
    }

    // Build the main UI structure
    function initializeUI() {
        console.log(`Initializing UI: ${config.numContainers} groups, Theme: ${config.theme}`);
        createGroupContainers();
        createDropdowns();
        updateDropdownOptions();
        addDragDropListeners(availableAppsListDiv);
        updateBindingsList(); // Update list in modal (it's hidden initially)
    }

    // Add event listeners
    function addEventListeners() {
        console.log("Adding event listeners...");
        // Add checks to prevent errors if elements somehow aren't found
        if (fetchAppsBtn) fetchAppsBtn.addEventListener('click', fetchApplicationsFromServer); else console.error("#fetch-apps-btn not found");
        if (settingsBtn) settingsBtn.addEventListener('click', openSettingsModal); else console.error("#settings-btn not found");
        if (closeModalBtn) closeModalBtn.addEventListener('click', closeSettingsModal); else console.error("#modal-close-btn not found");
        if (settingsModal) settingsModal.addEventListener('click', (event) => { if (event.target === settingsModal) closeSettingsModal(); }); else console.error("#settings-modal not found");
        if (applySettingsBtn) applySettingsBtn.addEventListener('click', handleApplySettings); else console.error("#settings-apply-btn not found");
        if (recordKeyBtn) recordKeyBtn.addEventListener('click', toggleKeyRecording); else console.error("#record-key-btn not found");
        if (addBindingBtn) addBindingBtn.addEventListener('click', handleAddBinding); else console.error("#add-binding-btn not found");
        document.addEventListener('keydown', handleGlobalKeyDown, true);
        console.log("Event listeners added.");
    }

    // --- Loading State Feedback ---
    function showLoadingState(message = "Loading...") { console.log(message); jsonOutputPre.textContent = message; jsonOutputPre.style.color = ''; }
    function hideLoadingState() { console.log("Loading complete."); jsonOutputPre.textContent = "Status: Ready"; jsonOutputPre.style.color = ''; }

    // --- Preferences (LocalStorage) ---
    function loadPreferencesFromStorage() {
        const savedTheme = localStorage.getItem('appTheme');
        const savedNumGroups = localStorage.getItem('numGroups');
        const savedNumDropdowns = localStorage.getItem('numDropdowns'); // <<< ADD loading for dropdowns

        config.theme = savedTheme || 'light';
        config.numContainers = parseInt(savedNumGroups, 10) || config.numContainers; // Use initial default if not saved
        config.numDropdowns = parseInt(savedNumDropdowns, 10) || config.numDropdowns; // <<< ADD parsing for dropdowns

        // Validate ranges
        config.numContainers = Math.max(1, Math.min(10, config.numContainers));
        config.numDropdowns = Math.max(0, Math.min(20, config.numDropdowns)); // <<< ADD validation for dropdowns

        console.log(`Preferences loaded: Theme=${config.theme}, Groups=${config.numContainers}, Dropdowns=${config.numDropdowns}`);
    }

    function savePreferencesToStorage() {
        localStorage.setItem('appTheme', config.theme);
        localStorage.setItem('numGroups', config.numContainers.toString());
        localStorage.setItem('numDropdowns', config.numDropdowns.toString()); // <<< ADD saving for dropdowns
        console.log(`Preferences saved: Theme=${config.theme}, Groups=${config.numContainers}, Dropdowns=${config.numDropdowns}`);
    }

    function applyTheme() { if (config.theme === 'dark') bodyElement.classList.add('dark-theme'); else bodyElement.classList.remove('dark-theme'); if (darkThemeCheckbox) darkThemeCheckbox.checked = (config.theme === 'dark'); }

    // --- Persistence (Backend Interaction) ---
    async function loadConfigFromServer() {
        console.log("Loading config from server...");
        try {
            const response = await fetch(API_URLS.loadConfig);
            if (!response.ok) { if (response.status === 404) { console.warn("config.json not found on server, using defaults."); /* Defaults already set */ } else { throw new Error(`HTTP error ${response.status}`); } }
            else { const loadedData = await response.json(); config.groups = (typeof loadedData.groups === 'object' && loadedData.groups !== null) ? loadedData.groups : {}; config.settings = (typeof loadedData.settings === 'object' && loadedData.settings !== null) ? loadedData.settings : {}; console.log("Config loaded and applied:", config); }
        } catch (error) { console.error("Error loading config:", error); showError("Failed to load configuration. Using defaults."); /* Defaults already set */ }
    }
    const saveConfigToServer = debounce(async () => { if (!currentConfig) { console.warn("Save config skipped: config not ready."); return; } console.log("Auto-saving config to server..."); updateCurrentConfigFromUI(); try { const response = await fetch(API_URLS.saveConfig, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(currentConfig) }); if (!response.ok) throw new Error(`HTTP error ${response.status}`); console.log("Config saved."); jsonOutputPre.textContent = `Config saved at ${new Date().toLocaleTimeString()}`; } catch (error) { console.error("Error saving config:", error); showError("Failed to auto-save configuration."); } }, 1500);
    function updateCurrentConfigFromUI() { if (!config) return; const newGroups = {}; groupBoxesWrapperDiv.querySelectorAll('.group-box').forEach(box => { const titleElement = box.querySelector('h3'); const groupName = titleElement ? titleElement.textContent : `Group_${box.dataset.groupId}`; const appsInGroup = []; box.querySelectorAll('.app-box').forEach(appBox => { if (appBox.closest('.group-box') === box) appsInGroup.push(appBox.dataset.appName); }); newGroups[groupName] = appsInGroup; }); config.groups = newGroups; const newSettings = {}; dropdownGridDiv.querySelectorAll('select').forEach(select => { newSettings[select.dataset.settingId] = select.value; }); config.settings = newSettings; jsonOutputPre.textContent = JSON.stringify({ config: config, bindings: currentBindings }, null, 2); } // Update reference to global config
    async function loadBindsFromServer() {
        console.log("Loading bindings from server...");
        try {
            const response = await fetch(API_URLS.loadBinds);
            if (!response.ok) { if (response.status === 404) { console.warn("binds.json not found on server, starting empty."); currentBindings = []; } else { throw new Error(`HTTP error ${response.status}`); } }
            else { const loadedData = await response.json(); currentBindings = Array.isArray(loadedData) ? loadedData : []; console.log("Bindings loaded:", currentBindings); } // Assign directly
        } catch (error) { console.error("Error loading bindings:", error); showError("Failed to load key bindings."); currentBindings = []; } // Ensure default on error
    }
    async function saveBindsToServer() { console.log("Saving bindings to server..."); try { const response = await fetch(API_URLS.saveBinds, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(currentBindings) }); if (!response.ok) throw new Error(`HTTP error ${response.status}`); console.log("Bindings saved."); } catch (error) { console.error("Error saving bindings:", error); showError("Failed to save key bindings."); } }

    // --- Settings Modal Logic ---
    function openSettingsModal() {
        // Ensure modal elements exist
        if (!settingsModal || !numGroupsInput || !darkThemeCheckbox || !keyComboInput || !keyActionInput || !numDropdownsInput) { // <<< ADD check for numDropdownsInput
            showError("Cannot open settings: Modal elements not found.");
            return;
        }
        console.log("Opening settings modal...");

        // Load current preferences into modal
        numGroupsInput.value = config.numContainers;
        numDropdownsInput.value = config.numDropdowns; // <<< POPULATE dropdown input
        darkThemeCheckbox.checked = (config.theme === 'dark');

        // Store initial counts to detect changes on Apply
        initialGroupCount = config.numContainers;
        initialDropdownCount = config.numDropdowns; // <<< STORE initial dropdown count

        // Hide warnings initially
        if (groupWarningText) groupWarningText.style.display = 'none';
        const dropdownWarning = document.querySelector('.dropdown-warning');
        if (dropdownWarning) dropdownWarning.style.display = 'none';

        updateBindingsList(); // Populate bindings list

        // Clear binding input fields
        keyComboInput.value = ''; keyActionInput.value = ''; capturedKeys = null;
        stopKeyRecording();

        settingsModal.style.display = 'flex';
    }

    function closeSettingsModal() { stopKeyRecording(); if (settingsModal) settingsModal.style.display = 'none'; }
    function handleApplySettings() {
        console.log("Applying general settings...");
        // Read values from modal
        const newNumGroups = parseInt(numGroupsInput.value, 10);
        const newNumDropdowns = parseInt(numDropdownsInput.value, 10); // <<< READ dropdown count
        const newTheme = darkThemeCheckbox.checked ? 'dark' : 'light';

        // Validate and Update Config State
        const validatedNumGroups = Math.max(1, Math.min(10, newNumGroups || config.numContainers));
        const validatedNumDropdowns = Math.max(0, Math.min(20, newNumDropdowns || config.numDropdowns)); // <<< VALIDATE dropdown count
        const groupsChanged = validatedNumGroups !== config.numContainers;
        const dropdownsChanged = validatedNumDropdowns !== config.numDropdowns; // <<< CHECK if dropdown count changed
        const themeChanged = newTheme !== config.theme;

        config.numContainers = validatedNumGroups;
        config.numDropdowns = validatedNumDropdowns; // <<< UPDATE config state
        config.theme = newTheme;

        // Apply Changes & Save Preferences
        if (themeChanged) applyTheme();
        savePreferencesToStorage(); // Save theme, group count, AND dropdown count

        // Rebuild UI sections ONLY if their counts changed
        let configNeedsSave = false;
        if (groupsChanged) {
            console.warn("Group count changed, rebuilding groups and resetting placements.");
            config.groups = {}; // Reset group data in config state
            createGroupContainers(); // Rebuild group UI
            fetchApplicationsFromServer(); // Reset available apps list
            configNeedsSave = true; // Mark config for saving
        }
        if (dropdownsChanged) {
            console.warn("Dropdown count changed, rebuilding action slots.");
            // We need to potentially prune settings for removed dropdowns
            const newSettings = {};
            for (let i = 1; i <= config.numDropdowns; i++) {
                const key = `setting-${i}`;
                if (config.settings[key] !== undefined) { // Keep existing selection if slot still exists
                    newSettings[key] = config.settings[key];
                } else {
                    newSettings[key] = ""; // Initialize new slots as empty
                }
            }
            config.settings = newSettings; // Update config state with potentially pruned settings

            createDropdowns(); // Rebuild dropdown structure
            updateDropdownOptions(); // Repopulate options and apply selections
            configNeedsSave = true; // Mark config for saving
        }

        // Save config.json immediately if structure changed
        if (configNeedsSave) {
            if (saveConfigToServer.flush) saveConfigToServer.flush(); else saveConfigToServer();
        }

        closeSettingsModal();
        console.log("General settings applied.");
    }

    // --- Key Binding Functions ---
    function handleAddBinding() { const combo = keyComboInput.value; const action = keyActionInput.value.trim(); if (!capturedKeys || !combo || combo.includes("Recording")) { showError("Record shortcut first."); return; } if (!action) { showError("Enter action name."); return; } if (currentBindings.some(b => b.action.toLowerCase() === action.toLowerCase())) { showError(`Action name "${action}" exists.`); return; } if (currentBindings.some(b => b.combo === combo)) { showError(`Shortcut "${combo}" exists.`); return; } const newBinding = { id: `bind-${Date.now()}-${Math.random().toString(36).substr(2, 5)}`, combo: combo, action: action, }; currentBindings.push(newBinding); updateBindingsList(); saveBindsToServer(); updateDropdownOptions(); keyComboInput.value = ''; keyActionInput.value = ''; capturedKeys = null; stopKeyRecording(); }
    function handleDeleteBinding(bindingIdToDelete) { console.log(`Deleting binding: ${bindingIdToDelete}`); const bindingToDelete = currentBindings.find(b => b.id === bindingIdToDelete); if (!bindingToDelete) return; currentBindings = currentBindings.filter(binding => binding.id !== bindingIdToDelete); updateBindingsList(); saveBindsToServer(); let configChanged = false; Object.keys(config.settings).forEach(key => { if (config.settings[key] === bindingIdToDelete) { config.settings[key] = ""; configChanged = true; } }); updateDropdownOptions(); if (configChanged) saveConfigToServer(); }
    function updateBindingsList() { if (!bindingsListUl) return; bindingsListUl.innerHTML = ''; if (!currentBindings || currentBindings.length === 0) { bindingsListUl.innerHTML = '<li>No bindings defined.</li>'; return; } currentBindings.forEach(binding => { const li = document.createElement('li'); li.dataset.bindingId = binding.id; const textSpan = document.createElement('span'); textSpan.classList.add('binding-text'); const comboSpan = document.createElement('span'); comboSpan.classList.add('binding-combo'); comboSpan.textContent = binding.combo; textSpan.appendChild(comboSpan); textSpan.append(" : "); const actionSpan = document.createElement('span'); actionSpan.classList.add('binding-action'); actionSpan.textContent = binding.action; textSpan.appendChild(actionSpan); li.appendChild(textSpan); const deleteBtn = document.createElement('button'); deleteBtn.textContent = 'Delete'; deleteBtn.classList.add('delete-btn'); deleteBtn.addEventListener('click', () => handleDeleteBinding(binding.id)); li.appendChild(deleteBtn); bindingsListUl.appendChild(li); }); }

    // --- Key Recording Logic ---
    function toggleKeyRecording() { if (!recordKeyBtn || !keyComboInput) return; if (isRecordingKey) stopKeyRecording(); else startKeyRecording(); }
    function startKeyRecording() { isRecordingKey = true; capturedKeys = null; keyComboInput.value = "Recording..."; recordKeyBtn.textContent = "Stop"; recordKeyBtn.classList.add('is-recording'); console.log("Key recording started."); }
    function stopKeyRecording() { isRecordingKey = false; if (!recordKeyBtn || !keyComboInput) return; recordKeyBtn.textContent = "Record"; recordKeyBtn.classList.remove('is-recording'); if (capturedKeys) { const formatted = formatKeyCombo(capturedKeys); keyComboInput.value = formatted || ''; if (!formatted) capturedKeys = null; } else { keyComboInput.value = ''; keyComboInput.placeholder = "Click 'Record' and press keys"; } console.log("Key recording stopped."); }
    function formatKeyCombo(ev) { if (!ev || !ev.key) return null; const parts = []; if (ev.ctrlKey) parts.push('Ctrl'); if (ev.altKey) parts.push('Alt'); if (ev.shiftKey) parts.push('Shift'); if (ev.metaKey) parts.push('Meta'); const key = ev.key.trim(); if (key && !['Control', 'Alt', 'Shift', 'Meta', ''].includes(key)) { if (key.length === 1 && key !== ' ') parts.push(key.toUpperCase()); else if (key === ' ') parts.push('Space'); else parts.push(key); } else if (parts.length === 0) return null; return parts.join('+'); }
    function handleGlobalKeyDown(event) { if (isRecordingKey && settingsModal && settingsModal.style.display !== 'none') { if (event.ctrlKey || event.altKey || event.metaKey || event.key.startsWith('F') || ['Tab', 'Escape', 'Enter', 'Space', 'Arrow'].some(k => event.key.startsWith(k))) { event.preventDefault(); event.stopPropagation(); } if (['Control', 'Shift', 'Alt', 'Meta'].includes(event.key)) { if (keyComboInput) keyComboInput.value = formatKeyCombo(event) + "+..." || "Recording..."; return; } capturedKeys = { ctrlKey: event.ctrlKey, altKey: event.altKey, shiftKey: event.shiftKey, metaKey: event.metaKey, key: event.key, code: event.code }; stopKeyRecording(); return false; } else { const pressedCombo = formatKeyCombo(event); if (pressedCombo) { const matchedBinding = currentBindings.find(b => b.combo === pressedCombo); if (matchedBinding) { console.log(`ACTION TRIGGERED: ${matchedBinding.action} (from combo: ${pressedCombo})`); event.preventDefault(); /* TODO: Execute action */ } } } }

    // --- UI Building Functions (Groups, Dropdowns) ---
    function createGroupContainers() { groupBoxesWrapperDiv.innerHTML = ''; console.log(`Creating ${config.numContainers} group containers.`); const existingGroupNames = Object.keys(config.groups || {}); let groupNamesToUse = []; const numToCreate = config.numContainers; groupNamesToUse = existingGroupNames.slice(0, numToCreate); for (let i = groupNamesToUse.length + 1; i <= numToCreate; i++) { let defaultName = `Group ${i}`; while (groupNamesToUse.includes(defaultName) || (config.groups && config.groups.hasOwnProperty(defaultName))) { defaultName = `Group ${i}_${Math.random().toString(36).substr(2, 3)}`; } groupNamesToUse.push(defaultName); if (!config.groups[defaultName]) config.groups[defaultName] = []; } Object.keys(config.groups).forEach(name => { if (!groupNamesToUse.includes(name)) delete config.groups[name]; }); groupNamesToUse.forEach((groupName, index) => { const container = document.createElement('div'); container.classList.add('group-box'); container.dataset.groupId = `group-container-${index + 1}`; const title = document.createElement('h3'); title.textContent = groupName; title.dataset.groupName = groupName; title.setAttribute('title', 'Double-click to rename'); title.addEventListener('dblclick', handleRenameGroupStart); container.appendChild(title); const appsInGroup = config.groups[groupName] || []; displayApplications(appsInGroup, false, container); addDragDropListeners(container); groupBoxesWrapperDiv.appendChild(container); }); }
    function handleRenameGroupStart(event) { const h3 = event.target; const container = h3.closest('.group-box'); if (!container || container.querySelector('input.group-title-input')) return; const currentName = h3.textContent; const input = document.createElement('input'); input.type = 'text'; input.value = currentName; input.classList.add('group-title-input'); input.dataset.originalName = currentName; h3.replaceWith(input); input.focus(); input.select(); input.addEventListener('blur', handleRenameGroupEnd); input.addEventListener('keydown', (e) => { if (e.key === 'Enter') input.blur(); else if (e.key === 'Escape') revertRename(input, currentName); }); }
    function handleRenameGroupEnd(event) { const input = event.target; const newName = input.value.trim(); const oldName = input.dataset.originalName; let finalName = oldName; let nameChanged = false; if (!newName) { console.log("Rename cancelled (empty), reverting."); } else if (newName === oldName) { console.log("Name unchanged."); } else if (config.groups.hasOwnProperty(newName)) { showError(`Group name "${newName}" exists.`); } else { console.log(`Renaming group "${oldName}" to "${newName}"`); config.groups[newName] = config.groups[oldName] || []; delete config.groups[oldName]; finalName = newName; nameChanged = true; } revertRename(input, finalName); if (nameChanged) saveConfigToServer(); }
    function revertRename(inputElement, nameToShow) { const h3 = document.createElement('h3'); h3.textContent = nameToShow; h3.dataset.groupName = nameToShow; h3.setAttribute('title', 'Double-click to rename'); h3.addEventListener('dblclick', handleRenameGroupStart); inputElement.replaceWith(h3); }
    function createDropdowns() {
        dropdownGridDiv.innerHTML = '';
        const numDropdownsToCreate = config.numDropdowns; // <<< Use config value
        // Adjust grid columns dynamically (simple example: always 2 columns)
        const gridColumns = 2; // Or calculate based on numDropdownsToCreate if desired
        dropdownGridDiv.style.gridTemplateColumns = `repeat(${gridColumns}, 1fr)`;
        console.log(`Creating ${numDropdownsToCreate} dropdowns.`);

        for (let i = 1; i <= numDropdownsToCreate; i++) {
            const settingKey = `setting-${i}`;
            const container = document.createElement('div');
            container.classList.add('dropdown-container');

            const label = document.createElement('label');
            label.setAttribute('for', `dropdown-${i}`);
            label.textContent = `Action Slot ${i}:`;
            container.appendChild(label);

            const select = document.createElement('select');
            select.id = `dropdown-${i}`;
            select.dataset.settingId = settingKey;
            select.innerHTML = `<option value="">-- Select Action --</option>`; // Default placeholder

            // Add change listener for auto-saving config.json
            select.addEventListener('change', saveConfigToServer);

            container.appendChild(select);
            dropdownGridDiv.appendChild(container);
        }
        // Options are populated by updateDropdownOptions right after this runs
    }
    function updateDropdownOptions() { console.log("Updating dropdown options..."); const selects = dropdownGridDiv.querySelectorAll('select'); const savedSelections = config.settings || {}; selects.forEach(select => { const settingKey = select.dataset.settingId; const previouslySelectedId = savedSelections[settingKey] || ""; const currentSelectedValue = select.value; select.innerHTML = `<option value="">-- Select Action --</option>`; currentBindings.forEach(binding => { const option = document.createElement('option'); option.value = binding.id; option.textContent = `${binding.action} (${binding.combo})`; select.appendChild(option); }); if (previouslySelectedId && currentBindings.some(b => b.id === previouslySelectedId)) select.value = previouslySelectedId; else if (currentSelectedValue && currentBindings.some(b => b.id === currentSelectedValue)) select.value = currentSelectedValue; else select.value = ""; }); console.log("Dropdown options updated."); }

    // --- Application Loading & Display ---
    async function fetchApplicationsFromServer() { console.log(`Workspaceing apps from ${API_URLS.fetchApps}...`); availableAppsListDiv.innerHTML = '<p><i>Loading apps...</i></p>'; try { const response = await fetch(API_URLS.fetchApps, { method: 'POST' }); if (!response.ok) throw new Error(`HTTP error ${response.status}`); const appNames = await response.json(); if (!Array.isArray(appNames)) throw new Error("Invalid app list format"); console.log("Fetched apps:", appNames); displayApplications(appNames, false, availableAppsListDiv); } catch (error) { console.error('Error fetching apps:', error); availableAppsListDiv.innerHTML = `<p style="color: red;">Error loading apps.</p>`; } }
    function displayApplications(appNames, isTestData = false, targetContainer) { if (!targetContainer) { console.error("Target container missing"); return; } if (targetContainer === availableAppsListDiv) targetContainer.innerHTML = ''; if (!appNames || appNames.length === 0) { if (targetContainer === availableAppsListDiv) targetContainer.innerHTML = '<p>No apps found.</p>'; return; } console.log(`Displaying ${appNames.length} apps in ${targetContainer.id || 'group'}`); appNames.forEach(appName => { if (targetContainer.querySelector(`.app-box[data-app-name="${appName}"]`)) return; const appBox = document.createElement('div'); appBox.classList.add('app-box'); appBox.textContent = appName; appBox.draggable = true; appBox.dataset.appName = appName; appBox.addEventListener('dragstart', handleDragStart); appBox.addEventListener('dragend', handleDragEnd); targetContainer.appendChild(appBox); }); }

    // --- Drag and Drop Logic ---
    function handleDragStart(event) { draggedApp = event.target; event.dataTransfer.setData('text/plain', event.target.dataset.appName); event.dataTransfer.effectAllowed = 'move'; setTimeout(() => { if (draggedApp) draggedApp.classList.add('dragging'); }, 0); }
    function handleDragEnd(event) { if (draggedApp) draggedApp.classList.remove('dragging'); draggedApp = null; document.querySelectorAll('.drag-over').forEach(el => el.classList.remove('drag-over')); }
    function handleDragEnter(event) { const dt = event.currentTarget; if ((dt.classList.contains('group-box') || dt.id === 'app-list') && draggedApp && dt !== draggedApp && dt !== draggedApp.parentNode) { document.querySelectorAll('.drag-over').forEach(el => el.classList.remove('drag-over')); dt.classList.add('drag-over'); } }
    function handleDragOver(event) { const dt = event.currentTarget; if ((dt.classList.contains('group-box') || dt.id === 'app-list') && draggedApp) { event.preventDefault(); event.dataTransfer.dropEffect = 'move'; if (!dt.classList.contains('drag-over') && dt !== draggedApp.parentNode) { document.querySelectorAll('.drag-over').forEach(el => el.classList.remove('drag-over')); dt.classList.add('drag-over'); } } else { event.dataTransfer.dropEffect = 'none'; } }
    function handleDragLeave(event) { const dt = event.currentTarget; if (!dt.contains(event.relatedTarget) && dt.classList.contains('drag-over')) dt.classList.remove('drag-over'); }
    function handleDrop(event) { event.preventDefault(); const dropTarget = event.currentTarget; dropTarget.classList.remove('drag-over'); if (draggedApp && (dropTarget.classList.contains('group-box') || dropTarget.id === 'app-list')) { const originalParent = draggedApp.parentNode; if (originalParent !== dropTarget) { const targetName = dropTarget.id === 'app-list' ? 'Available Apps' : dropTarget.querySelector('h3')?.textContent; console.log(`Dropped ${draggedApp.dataset.appName} onto ${targetName}`); if (dropTarget.classList.contains('group-box')) { const title = dropTarget.querySelector('h3'); dropTarget.appendChild(draggedApp); if (title) dropTarget.insertBefore(title, dropTarget.firstChild); } else { dropTarget.appendChild(draggedApp); } saveConfigToServer(); } } } // Trigger save
    function addDragDropListeners(element) { if (!element) return; element.addEventListener('dragenter', handleDragEnter); element.addEventListener('dragover', handleDragOver); element.addEventListener('dragleave', handleDragLeave); element.addEventListener('drop', handleDrop); }

    // --- Utility: Show Error Messages ---
    function showError(message) { console.error("UI Error:", message); if (!jsonOutputPre) return; const oldText = jsonOutputPre.textContent; const oldColor = jsonOutputPre.style.color; jsonOutputPre.textContent = `ERROR: ${message}`; jsonOutputPre.style.color = 'red'; setTimeout(() => { if (jsonOutputPre.textContent === `ERROR: ${message}`) { jsonOutputPre.textContent = oldText; jsonOutputPre.style.color = oldColor; } }, 5000); }

    // --- Start the Application ---
    initializeApp();

}); // End DOMContentLoaded