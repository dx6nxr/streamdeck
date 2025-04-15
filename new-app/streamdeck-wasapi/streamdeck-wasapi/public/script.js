document.addEventListener('DOMContentLoaded', () => {
    // --- State & Configuration ---
    // Initialize with defaults FIRST to ensure they exist
    let config = {
        numContainers: 4,
        numDropdowns: 6,
        theme: 'light',
        designSystem: 'default',
        groups: {},
        settings: {},
        group_names: {}
    };
    let currentBindings = []; // Initialize as empty array
    let isRecordingKey = false;
    let capturedKeys = null;
    let initialGroupCount = config.numContainers; // Will be updated after loading prefs
    let initialDropdownCount = config.numDropdowns;

    // Controller state
    let controllerState = {
        sliders: [],
        buttons: [],
        connected: false,
        port: null
    };

    // COM ports state
    let comPorts = [];
    let selectedComPort = "";

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
        fetchApps: 'http://localhost:8080/api/get-apps',     // POST or GET
        getControllerState: 'http://localhost:8080/api/get-controller-state', // GET
        getComPorts: 'http://localhost:8080/api/get-com-ports', // GET
        setComPort: 'http://localhost:8080/api/set-com-port'  // POST
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

    // Controller state elements
    const slidersContainer = document.getElementById('sliders-container');
    const buttonsContainer = document.getElementById('buttons-container');

    // --- Drag & Drop State ---
    let draggedApp = null;

    // --- Initialization ---
    async function initializeApp() {
        showLoadingState("Initializing...");

        // 1. Initialize state with defaults guaranteed to exist
        config = { groups: {}, settings: {}, numContainers: 4, theme: 'light', group_names: {} };
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
                loadBindsFromServer(),   // Will try to overwrite currentBindings
                loadComPorts()          // Load available COM ports
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

        // 6. Get initial controller state
        try {
            await fetchControllerState();
        } catch (error) {
            console.error("Error fetching controller state:", error);
        }

        // 7. Start controller state update interval
        startControllerStateUpdates();

        hideLoadingState();
        console.log("App Initialization attempt complete.");
    }

    // Build the main UI structure
    function initializeUI() {
        console.log(`Initializing UI: ${config.numContainers} groups, Theme: ${config.theme}, Design System: ${config.designSystem}`);
        applyTheme(); // Ensure design system and theme are applied to the DOM
        createGroupContainers();
        createDropdowns();
        updateDropdownOptions();
        addDragDropListeners(availableAppsListDiv);
        updateBindingsList(); // Update list in modal (it's hidden initially)

        // Initialize controller state display
        updateControllerDisplay();
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

        // Add COM port refresh button listener
        const refreshComPortsBtn = document.getElementById('refresh-com-ports-btn');
        if (refreshComPortsBtn) refreshComPortsBtn.addEventListener('click', handleRefreshComPorts); else console.error("#refresh-com-ports-btn not found");

        // Add controller toggle button listener
        const toggleControllerBtn = document.getElementById('toggle-controller-btn');
        if (toggleControllerBtn) {
            toggleControllerBtn.addEventListener('click', toggleControllerContent);

            // Check for saved collapsed state
            const isCollapsed = localStorage.getItem('controllerCollapsed') === 'true';
            if (isCollapsed) {
                toggleControllerContent();
            }
        } else {
            console.error("#toggle-controller-btn not found");
        }

        document.addEventListener('keydown', handleGlobalKeyDown, true);
        console.log("Event listeners added.");
    }

    // Toggle controller content visibility
    function toggleControllerContent() {
        const toggleBtn = document.getElementById('toggle-controller-btn');
        const controllerContent = document.getElementById('controller-content');
        const controllerSection = document.getElementById('controller-section');

        if (!toggleBtn || !controllerContent || !controllerSection) {
            console.error("Toggle button, controller content, or controller section not found");
            return;
        }

        const isCollapsed = controllerContent.classList.toggle('collapsed');
        toggleBtn.classList.toggle('collapsed', isCollapsed);
        controllerSection.classList.toggle('collapsed', isCollapsed);

        // Update button text/icon
        toggleBtn.textContent = isCollapsed ? '◀' : '▼';
        toggleBtn.title = isCollapsed ? 'Expand Controller' : 'Collapse Controller';

        // Save state to localStorage
        localStorage.setItem('controllerCollapsed', isCollapsed.toString());
    }

    // --- Loading State Feedback ---
    function showLoadingState(message = "Loading...") { console.log(message); jsonOutputPre.textContent = message; jsonOutputPre.style.color = ''; }
    function hideLoadingState() { console.log("Loading complete."); jsonOutputPre.textContent = "Status: Ready"; jsonOutputPre.style.color = ''; }

    // --- Preferences (LocalStorage) ---
    function loadPreferencesFromStorage() {
        const savedTheme = localStorage.getItem('appTheme');
        const savedNumGroups = localStorage.getItem('numGroups');
        const savedNumDropdowns = localStorage.getItem('numDropdowns');
        const savedDesignSystem = localStorage.getItem('designSystem');

        config.theme = savedTheme || 'light';
        config.numContainers = parseInt(savedNumGroups, 10) || config.numContainers; // Use initial default if not saved
        config.numDropdowns = parseInt(savedNumDropdowns, 10) || config.numDropdowns;
        config.designSystem = savedDesignSystem || 'default';

        // Validate ranges
        config.numContainers = Math.max(1, Math.min(10, config.numContainers));
        config.numDropdowns = Math.max(0, Math.min(20, config.numDropdowns));

        console.log(`Preferences loaded: Theme=${config.theme}, Groups=${config.numContainers}, Dropdowns=${config.numDropdowns}, DesignSystem=${config.designSystem}`);
    }

    function savePreferencesToStorage() {
        localStorage.setItem('appTheme', config.theme);
        localStorage.setItem('numGroups', config.numContainers.toString());
        localStorage.setItem('numDropdowns', config.numDropdowns.toString());
        localStorage.setItem('designSystem', config.designSystem);
        console.log(`Preferences saved: Theme=${config.theme}, Groups=${config.numContainers}, Dropdowns=${config.numDropdowns}, DesignSystem=${config.designSystem}`);
    }

    function applyTheme() {
        // Apply dark theme if needed
        if (config.theme === 'dark') {
            bodyElement.classList.add('dark-theme');
        } else {
            bodyElement.classList.remove('dark-theme');
        }

        // Apply design system
        bodyElement.classList.remove('material-you');
        bodyElement.classList.remove('windows-11');
        if (config.designSystem === 'material-you') {
            bodyElement.classList.add('material-you');
        }
        else if (config.designSystem === 'windows-11') {
            bodyElement.classList.add('windows-11');
        }

        // Update checkboxes/selects in settings if they exist
        if (darkThemeCheckbox) darkThemeCheckbox.checked = (config.theme === 'dark');

        const designSystemSelect = document.getElementById('setting-design-system');
        if (designSystemSelect) designSystemSelect.value = config.designSystem;
    }

    // --- Persistence (Backend Interaction) ---
    async function loadConfigFromServer() {
        console.log("Loading config from server...");
        try {
            const response = await fetch(API_URLS.loadConfig);
            if (!response.ok) {
                if (response.status === 404) {
                    console.warn("config.json not found on server, using defaults.");
                    /* Defaults already set */
                } else {
                    throw new Error(`HTTP error ${response.status}`);
                }
            }
            else {
                const loadedData = await response.json();
                config.groups = (typeof loadedData.groups === 'object' && loadedData.groups !== null) ? loadedData.groups : {};
                config.settings = (typeof loadedData.settings === 'object' && loadedData.settings !== null) ? loadedData.settings : {};
                // Load group_names from the server response
                config.group_names = (typeof loadedData.group_names === 'object' && loadedData.group_names !== null) ? loadedData.group_names : {};
                // Add other fields that might be in config
                if (loadedData.numContainers) config.numContainers = loadedData.numContainers;
                if (loadedData.numDropdowns) config.numDropdowns = loadedData.numDropdowns;
                if (loadedData.theme) config.theme = loadedData.theme;
                if (loadedData.designSystem) config.designSystem = loadedData.designSystem;
                console.log("Config loaded and applied:", config);
            }
        } catch (error) {
            console.error("Error loading config:", error);
            showError("Failed to load configuration. Using defaults.");
            /* Defaults already set */
        }
    }
    const saveConfigToServer = debounce(async () => {
        console.log("Saving config to server...");
        showLoadingState("Saving...");

        try {
            // Make sure group_names is initialized if it doesn't exist
            if (!config.group_names) {
                config.group_names = {};
                // Create default display names for any groups without custom names
                Object.keys(config.groups).forEach(groupKey => {
                    if (!config.group_names[groupKey]) {
                        config.group_names[groupKey] = groupKey;
                    }
                });
            }

            // Ensure design system is set to a valid value
            if (!config.designSystem) {
                config.designSystem = 'default';
            }

            // Clean up any group_names entries for non-existent groups
            Object.keys(config.group_names).forEach(groupKey => {
                if (!config.groups || !config.groups[groupKey]) {
                    delete config.group_names[groupKey];
                }
            });

            console.log("Config to save:", config);

            const response = await fetch(API_URLS.saveConfig, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(config)
            });

            if (!response.ok) {
                throw new Error(`HTTP error ${response.status}`);
            }

            const data = await response.json();
            console.log("Config saved successfully:", data);
            hideLoadingState();
        } catch (error) {
            console.error("Error saving config:", error);
            showError("Failed to save configuration.");
        }
    }, 500);
    function updateCurrentConfigFromUI() {
        if (!config) return;

        const newGroups = {};
        groupBoxesWrapperDiv.querySelectorAll('.group-box').forEach(box => {
            const titleElement = box.querySelector('h3');
            if (!titleElement) return;

            const groupName = titleElement.textContent;
            if (!groupName) return;

            const appsInGroup = [];
            box.querySelectorAll('.app-box').forEach(appBox => {
                if (appBox.closest('.group-box') === box && appBox.dataset.appName) {
                    appsInGroup.push(appBox.dataset.appName);
                }
            });

            // Only add groups with valid names
            if (groupName.trim() !== '') {
                newGroups[groupName] = appsInGroup;
            }
        });

        config.groups = newGroups;

        // Update settings from dropdowns
        const newSettings = {};
        dropdownGridDiv.querySelectorAll('select').forEach(select => {
            if (select.dataset.settingId) {
                newSettings[select.dataset.settingId] = select.value;
            }
        });

        config.settings = newSettings;

        // For debugging - show the current config in the status area
        console.log("Updated config:", config);
    }
    async function loadBindsFromServer() {
        console.log("Loading bindings from server...");
        try {
            const response = await fetch(API_URLS.loadBinds);
            if (!response.ok) { if (response.status === 404) { console.warn("binds.json not found on server, starting empty."); currentBindings = []; } else { throw new Error(`HTTP error ${response.status}`); } }
            else { const loadedData = await response.json(); currentBindings = Array.isArray(loadedData) ? loadedData : []; console.log("Bindings loaded:", currentBindings); } // Assign directly
        } catch (error) { console.error("Error loading bindings:", error); showError("Failed to load key bindings."); currentBindings = []; } // Ensure default on error
    }
    async function saveBindsToServer() { console.log("Saving bindings to server..."); try { const response = await fetch(API_URLS.saveBinds, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(currentBindings) }); if (!response.ok) throw new Error(`HTTP error ${response.status}`); console.log("Bindings saved."); } catch (error) { console.error("Error saving bindings:", error); showError("Failed to save key bindings."); } }

    // --- COM Port Management ---
    async function loadComPorts() {
        try {
            const response = await fetch(API_URLS.getComPorts);
            if (!response.ok) {
                throw new Error(`HTTP error ${response.status}`);
            }
            const data = await response.json();
            comPorts = data.ports || [];
            selectedComPort = data.selected || "";
            console.log("COM ports loaded:", comPorts);
            console.log("Selected COM port:", selectedComPort);
        } catch (error) {
            console.error("Error loading COM ports:", error);
            showError("Failed to load COM ports");
        }
    }

    async function setComPort(port) {
        try {
            const response = await fetch(API_URLS.setComPort, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ port: port })
            });

            if (!response.ok) {
                throw new Error(`HTTP error ${response.status}`);
            }

            const data = await response.json();
            console.log("COM port set:", data.message);
            selectedComPort = port;
        } catch (error) {
            console.error("Error setting COM port:", error);
            showError("Failed to set COM port");
        }
    }

    function refreshComPortsList() {
        const comPortSelect = document.getElementById('setting-com-port');
        if (!comPortSelect) return;

        // Remember the currently selected value
        const currentValue = comPortSelect.value;

        // Clear the select options
        comPortSelect.innerHTML = '';

        // Add option for each port
        comPorts.forEach(port => {
            const option = document.createElement('option');
            option.value = port;
            option.textContent = port;
            comPortSelect.appendChild(option);
        });

        // Select the current port if it exists, otherwise select the first port
        if (currentValue && comPorts.includes(currentValue)) {
            comPortSelect.value = currentValue;
        } else if (selectedComPort && comPorts.includes(selectedComPort)) {
            comPortSelect.value = selectedComPort;
        } else if (comPorts.length > 0) {
            comPortSelect.value = comPorts[0];
        }
    }

    async function handleRefreshComPorts() {
        await loadComPorts();
        refreshComPortsList();
    }

    // --- Settings Modal Logic ---
    function openSettingsModal() {
        // Ensure modal elements exist
        if (!settingsModal || !numGroupsInput || !darkThemeCheckbox || !keyComboInput || !keyActionInput || !numDropdownsInput) {
            showError("Cannot open settings: Modal elements not found.");
            return;
        }
        console.log("Opening settings modal...");

        // Load current preferences into modal
        numGroupsInput.value = config.numContainers;
        numDropdownsInput.value = config.numDropdowns;
        darkThemeCheckbox.checked = (config.theme === 'dark');

        // Set design system selection
        const designSystemSelect = document.getElementById('setting-design-system');
        if (designSystemSelect) {
            designSystemSelect.value = config.designSystem || 'default';
        }

        // Store initial counts to detect changes on Apply
        initialGroupCount = config.numContainers;
        initialDropdownCount = config.numDropdowns;

        // Hide warnings initially
        if (groupWarningText) groupWarningText.style.display = 'none';
        const dropdownWarning = document.querySelector('.dropdown-warning');
        if (dropdownWarning) dropdownWarning.style.display = 'none';

        // Update COM ports list
        refreshComPortsList();

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
        const newNumDropdowns = parseInt(numDropdownsInput.value, 10);
        const newTheme = darkThemeCheckbox.checked ? 'dark' : 'light';

        // Get design system value
        const designSystemSelect = document.getElementById('setting-design-system');
        const newDesignSystem = designSystemSelect ? designSystemSelect.value : 'default';

        // Get the COM port selection
        const comPortSelect = document.getElementById('setting-com-port');
        if (comPortSelect && comPortSelect.value && comPortSelect.value !== selectedComPort) {
            setComPort(comPortSelect.value);
        }

        // Validate and Update Config State
        const validatedNumGroups = Math.max(1, Math.min(10, newNumGroups || config.numContainers));
        const validatedNumDropdowns = Math.max(0, Math.min(20, newNumDropdowns || config.numDropdowns));
        const groupsChanged = validatedNumGroups !== config.numContainers;
        const dropdownsChanged = validatedNumDropdowns !== config.numDropdowns;
        const themeChanged = newTheme !== config.theme;
        const designSystemChanged = newDesignSystem !== config.designSystem;

        config.numContainers = validatedNumGroups;
        config.numDropdowns = validatedNumDropdowns;
        config.theme = newTheme;
        config.designSystem = newDesignSystem;

        // Apply Changes & Save Preferences
        if (themeChanged || designSystemChanged) applyTheme();
        savePreferencesToStorage();

        // Rebuild UI sections ONLY if their counts changed
        let configNeedsSave = true; // Always save config when applying settings
        if (groupsChanged) {
            console.warn("Group count changed, rebuilding groups and resetting placements.");
            config.groups = {}; // Reset group data in config state
            createGroupContainers(); // Rebuild group UI
            fetchApplicationsFromServer(); // Reset available apps list
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
        }

        // Save config.json immediately
        if (configNeedsSave) {
            console.log("Saving theme and other changes to server");
            if (saveConfigToServer.flush) {
                saveConfigToServer.flush();
            } else {
                saveConfigToServer();
            }
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
    function createGroupContainers() {
        groupBoxesWrapperDiv.innerHTML = '';
        console.log(`Creating ${config.numContainers} group containers.`);

        // Debug output the current config.group_names
        console.log("Current group_names in config:", config.group_names);

        const existingGroupNames = Object.keys(config.groups || {});
        console.log("Existing group keys:", existingGroupNames);

        let groupNamesToUse = [];
        const numToCreate = config.numContainers;

        // Use existing group keys 
        groupNamesToUse = existingGroupNames.slice(0, numToCreate);

        // Add new group keys if needed
        for (let i = groupNamesToUse.length + 1; i <= numToCreate; i++) {
            let defaultName = `Group ${i}`;
            while (groupNamesToUse.includes(defaultName) || (config.groups && config.groups.hasOwnProperty(defaultName))) {
                defaultName = `Group ${i}_${Math.random().toString(36).substr(2, 3)}`;
            }
            groupNamesToUse.push(defaultName);
            if (!config.groups[defaultName]) config.groups[defaultName] = [];

            // Initialize default display name if not exists
            if (!config.group_names || !config.group_names[defaultName]) {
                console.log(`Creating default display name for new group ${defaultName}`);
                if (!config.group_names) config.group_names = {};
                config.group_names[defaultName] = defaultName;
            }
        }

        // Clean up any removed groups
        Object.keys(config.groups).forEach(name => {
            if (!groupNamesToUse.includes(name)) {
                console.log(`Removing unused group ${name} from config`);
                delete config.groups[name];
                if (config.group_names && config.group_names[name]) {
                    delete config.group_names[name];
                }
            }
        });

        console.log("Final group keys to use:", groupNamesToUse);

        // Create the group containers
        groupNamesToUse.forEach((groupName, index) => {
            const container = document.createElement('div');
            container.classList.add('group-box');
            container.dataset.groupId = `group-container-${index + 1}`;
            container.dataset.groupKey = groupName; // Store the original key for reference

            // Use the custom display name if available, otherwise use the group key
            let displayName = groupName;
            if (config.group_names && config.group_names[groupName]) {
                displayName = config.group_names[groupName];
                console.log(`Using custom name for ${groupName}: "${displayName}"`);
            } else {
                console.log(`No custom name found for ${groupName}, using key as name`);
            }

            const title = document.createElement('h3');
            title.textContent = displayName;
            title.dataset.groupName = groupName; // Store the internal group name for data management
            title.dataset.displayName = displayName; // Store the display name separately
            title.setAttribute('title', 'Double-click to rename');
            title.addEventListener('dblclick', handleRenameGroupStart);
            container.appendChild(title);

            const appsInGroup = config.groups[groupName] || [];
            displayApplications(appsInGroup, false, container);
            addDragDropListeners(container);

            groupBoxesWrapperDiv.appendChild(container);
        });
    }
    function handleRenameGroupStart(event) {
        const h3 = event.target;
        const container = h3.closest('.group-box');
        if (!container || container.querySelector('input.group-title-input')) return;

        const currentKey = h3.dataset.groupName; // Internal group key
        const currentDisplayName = h3.textContent; // Display name shown to user

        const input = document.createElement('input');
        input.type = 'text';
        input.value = currentDisplayName;
        input.classList.add('group-title-input');
        input.dataset.originalName = currentDisplayName;
        input.dataset.groupKey = currentKey; // Store the internal key

        h3.replaceWith(input);
        input.focus();
        input.select();

        input.addEventListener('blur', handleRenameGroupEnd);
        input.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') input.blur();
            else if (e.key === 'Escape') revertRename(input, currentDisplayName, currentKey);
        });
    }
    function handleRenameGroupEnd(event) {
        const input = event.target;
        const newDisplayName = input.value.trim();
        const oldDisplayName = input.dataset.originalName;
        const groupKey = input.dataset.groupKey; // Internal group key

        let finalDisplayName = oldDisplayName;
        let nameChanged = false;

        if (!newDisplayName) {
            console.log("Rename cancelled (empty), reverting.");
        } else if (newDisplayName === oldDisplayName) {
            console.log("Name unchanged.");
        } else {
            // Check if this display name is already used
            const isNameDuplicate = Object.values(config.group_names || {}).some(
                name => name.toLowerCase() === newDisplayName.toLowerCase() &&
                    config.group_names[groupKey] !== newDisplayName
            );

            if (isNameDuplicate) {
                showError(`Group name "${newDisplayName}" is already used.`);
            } else {
                console.log(`Renaming group display name from "${oldDisplayName}" to "${newDisplayName}"`);

                // Just update the display name in group_names
                if (!config.group_names) config.group_names = {};
                config.group_names[groupKey] = newDisplayName;

                finalDisplayName = newDisplayName;
                nameChanged = true;
            }
        }

        revertRename(input, finalDisplayName, groupKey);

        if (nameChanged) {
            saveConfigToServer();
        }
    }
    function revertRename(inputElement, nameToShow, groupKey) {
        const h3 = document.createElement('h3');
        h3.textContent = nameToShow;
        h3.dataset.groupName = groupKey; // Store the internal group key
        h3.dataset.displayName = nameToShow; // Store the display name
        h3.setAttribute('title', 'Double-click to rename');
        h3.addEventListener('dblclick', handleRenameGroupStart);
        inputElement.replaceWith(h3);
    }
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
    function displayApplications(appNames, isTestData = false, targetContainer) {
        if (!targetContainer) {
            console.error("Target container missing");
            return;
        }

        // Clear the available apps list if this is the main app list
        if (targetContainer === availableAppsListDiv) {
            targetContainer.innerHTML = '';
        }

        if (!appNames || appNames.length === 0) {
            if (targetContainer === availableAppsListDiv) {
                targetContainer.innerHTML = '<p>No apps found.</p>';
            }
            return;
        }

        console.log(`Displaying ${appNames.length} apps in ${targetContainer.id || 'group'}`);

        // If we're populating the available apps list, filter out apps already in groups
        let filteredAppNames = appNames;
        if (targetContainer === availableAppsListDiv) {
            // Get a list of all apps currently assigned to any group
            const appsInGroups = new Set();
            for (const groupName in config.groups) {
                config.groups[groupName].forEach(app => appsInGroups.add(app));
            }

            // Filter out apps that are already in groups
            filteredAppNames = appNames.filter(app => !appsInGroups.has(app));

            if (filteredAppNames.length === 0) {
                targetContainer.innerHTML = '<p>All apps are assigned to groups.</p>';
                return;
            }
        }

        // Create app boxes for the (possibly filtered) list
        filteredAppNames.forEach(appName => {
            // Skip if this app is already displayed in this container
            if (targetContainer.querySelector(`.app-box[data-app-name="${appName}"]`)) return;

            const appBox = document.createElement('div');
            appBox.classList.add('app-box');
            appBox.textContent = appName;
            appBox.draggable = true;
            appBox.dataset.appName = appName;
            appBox.addEventListener('dragstart', handleDragStart);
            appBox.addEventListener('dragend', handleDragEnd);

            targetContainer.appendChild(appBox);
        });
    }

    // --- Drag and Drop Logic ---
    function handleDragStart(event) { draggedApp = event.target; event.dataTransfer.setData('text/plain', event.target.dataset.appName); event.dataTransfer.effectAllowed = 'move'; setTimeout(() => { if (draggedApp) draggedApp.classList.add('dragging'); }, 0); }
    function handleDragEnd(event) { if (draggedApp) draggedApp.classList.remove('dragging'); draggedApp = null; document.querySelectorAll('.drag-over').forEach(el => el.classList.remove('drag-over')); }
    function handleDragEnter(event) { const dt = event.currentTarget; if ((dt.classList.contains('group-box') || dt.id === 'app-list') && draggedApp && dt !== draggedApp && dt !== draggedApp.parentNode) { document.querySelectorAll('.drag-over').forEach(el => el.classList.remove('drag-over')); dt.classList.add('drag-over'); } }
    function handleDragOver(event) { const dt = event.currentTarget; if ((dt.classList.contains('group-box') || dt.id === 'app-list') && draggedApp) { event.preventDefault(); event.dataTransfer.dropEffect = 'move'; if (!dt.classList.contains('drag-over') && dt !== draggedApp.parentNode) { document.querySelectorAll('.drag-over').forEach(el => el.classList.remove('drag-over')); dt.classList.add('drag-over'); } } else { event.dataTransfer.dropEffect = 'none'; } }
    function handleDragLeave(event) { const dt = event.currentTarget; if (!dt.contains(event.relatedTarget) && dt.classList.contains('drag-over')) dt.classList.remove('drag-over'); }
    function handleDrop(event) {
        event.preventDefault();
        event.stopPropagation();

        const dropZone = event.currentTarget;
        dropZone.classList.remove('drag-over');

        if (!draggedApp) return;

        const appName = draggedApp.dataset.appName;
        const sourceContainer = draggedApp.parentElement;
        const sourceGroupHeader = sourceContainer.querySelector('h3');
        const targetGroupHeader = dropZone.querySelector('h3');

        // Moving from app list to a group
        if (sourceContainer.id === 'app-list' && dropZone.classList.contains('group-box')) {
            const targetGroupName = targetGroupHeader.dataset.groupName;
            console.log(`Moving ${appName} to group ${targetGroupName}`);

            // Add to the target group's data structure
            if (!config.groups[targetGroupName].includes(appName)) {
                config.groups[targetGroupName].push(appName);

                // Add a visual representation to the target
                displayApplications([appName], false, dropZone);

                // Remove from available apps list (will happen on refresh)
                draggedApp.remove();

                // Save changes to server
                saveConfigToServer();

                // Refresh available apps list to hide this app
                fetchApplicationsFromServer();
            }
        }
        // Moving from a group to app list
        else if (sourceContainer.classList.contains('group-box') && dropZone.id === 'app-list') {
            const sourceGroupName = sourceGroupHeader.dataset.groupName;
            console.log(`Removing ${appName} from group ${sourceGroupName}`);

            // Remove from the source group's data structure
            config.groups[sourceGroupName] = config.groups[sourceGroupName].filter(app => app !== appName);

            // Remove from the UI
            draggedApp.remove();

            // Add to available apps list
            displayApplications([appName], false, availableAppsListDiv);

            // Save changes to server
            saveConfigToServer();
        }
        // Moving from one group to another
        else if (sourceContainer.classList.contains('group-box') && dropZone.classList.contains('group-box') && sourceContainer !== dropZone) {
            const sourceGroupName = sourceGroupHeader.dataset.groupName;
            const targetGroupName = targetGroupHeader.dataset.groupName;
            console.log(`Moving ${appName} from group ${sourceGroupName} to ${targetGroupName}`);

            // Remove from source group's data structure
            config.groups[sourceGroupName] = config.groups[sourceGroupName].filter(app => app !== appName);

            // Add to target group's data structure
            if (!config.groups[targetGroupName].includes(appName)) {
                config.groups[targetGroupName].push(appName);

                // Add to target group in UI
                displayApplications([appName], false, dropZone);

                // Remove from source group in UI
                draggedApp.remove();

                // Save changes to server
                saveConfigToServer();
            }
        }
    }
    function addDragDropListeners(element) { if (!element) return; element.addEventListener('dragenter', handleDragEnter); element.addEventListener('dragover', handleDragOver); element.addEventListener('dragleave', handleDragLeave); element.addEventListener('drop', handleDrop); }

    // --- Utility: Show Error Messages ---
    function showError(message) {
        console.error("UI Error:", message);
        if (!jsonOutputPre) return;
        const oldText = jsonOutputPre.textContent;
        const oldColor = jsonOutputPre.style.color;
        jsonOutputPre.textContent = `ERROR: ${message}`;
        jsonOutputPre.style.color = 'red';
        setTimeout(() => {
            if (jsonOutputPre.textContent === `ERROR: ${message}`) {
                jsonOutputPre.textContent = oldText;
                jsonOutputPre.style.color = oldColor;
            }
        }, 5000);
    }

    // --- Controller State Management ---
    async function fetchControllerState() {
        try {
            const response = await fetch(API_URLS.getControllerState);
            if (!response.ok) {
                throw new Error(`HTTP error ${response.status}`);
            }
            const data = await response.json();
            controllerState = data;
            updateControllerDisplay();
            console.log("Controller state updated:", controllerState);
        } catch (error) {
            console.error("Error fetching controller state:", error);
            showError("Failed to get controller state");
        }
    }

    function startControllerStateUpdates() {
        // Update controller state every 500ms
        setInterval(fetchControllerState, 500);
    }

    function updateControllerDisplay() {
        if (!slidersContainer || !buttonsContainer) {
            console.error("Controller containers not found");
            return;
        }

        // Clear existing content
        slidersContainer.innerHTML = '';
        buttonsContainer.innerHTML = '';

        // Get connection status
        const isConnected = controllerState.connected === true;
        const connectionStatus = document.getElementById('connection-status');

        if (connectionStatus) {
            connectionStatus.textContent = isConnected
                ? `Connected (${controllerState.port})`
                : `Not connected${controllerState.port ? ' - Check ' + controllerState.port : ' - Select a port in settings'}`;

            connectionStatus.className = isConnected ? 'status-connected' : 'status-disconnected';
        }

        // If not connected, show message and return
        if (!isConnected) {
            slidersContainer.innerHTML = '<p class="no-connection-warning">Arduino not connected. Please select a COM port in settings.</p>';
            buttonsContainer.innerHTML = '<p class="no-connection-warning">Arduino not connected. Please select a COM port in settings.</p>';
            return;
        }

        // Create slider items
        if (controllerState.sliders && controllerState.sliders.length > 0) {
            controllerState.sliders.forEach((value, index) => {
                const sliderItem = document.createElement('div');
                sliderItem.classList.add('slider-item');

                const label = document.createElement('div');
                label.classList.add('slider-label');
                label.textContent = `Slider ${index + 1}`;

                const valueDisplay = document.createElement('div');
                valueDisplay.classList.add('slider-value');
                valueDisplay.textContent = value;

                const bar = document.createElement('div');
                bar.classList.add('slider-bar');

                const fill = document.createElement('div');
                fill.classList.add('slider-fill');
                // Set width based on slider value (0-1023)
                const percentage = (value / 1023) * 100;
                fill.style.width = `${percentage}%`;

                bar.appendChild(fill);

                sliderItem.appendChild(label);
                sliderItem.appendChild(bar);
                sliderItem.appendChild(valueDisplay);

                slidersContainer.appendChild(sliderItem);
            });
        } else {
            slidersContainer.innerHTML = '<p>No slider data available</p>';
        }

        // Create button items
        if (controllerState.buttons && controllerState.buttons.length > 0) {
            controllerState.buttons.forEach((state, index) => {
                const buttonItem = document.createElement('div');
                buttonItem.classList.add('button-item');

                const label = document.createElement('div');
                label.classList.add('button-label');
                label.textContent = `Button ${index + 1}`;

                const indicator = document.createElement('div');
                indicator.classList.add('button-indicator');
                if (state === 1) {
                    indicator.classList.add('active');
                }

                const valueDisplay = document.createElement('div');
                valueDisplay.classList.add('button-value');
                valueDisplay.textContent = state === 1 ? 'ON' : 'OFF';

                buttonItem.appendChild(label);
                buttonItem.appendChild(indicator);
                buttonItem.appendChild(valueDisplay);

                buttonsContainer.appendChild(buttonItem);
            });
        } else {
            buttonsContainer.innerHTML = '<p>No button data available</p>';
        }
    }

    // --- Start the Application ---
    initializeApp();

}); // End DOMContentLoaded