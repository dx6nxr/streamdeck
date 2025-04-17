"use client";

import { useState, useEffect } from "react";
import { Button } from "@/components/ui/button";
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Slider } from "@/components/ui/slider";
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogTrigger, DialogClose } from "@/components/ui/dialog";
import { Separator } from "@/components/ui/separator";
import { Checkbox } from "@/components/ui/checkbox";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select";
import { AppBox } from "@/components/app-box";
import { GroupBox } from "@/components/group-box";
import { ControllerStatus } from "@/components/controller-status";

// Type definitions
interface Slider {
  id: number;
  value: number;
  label: string;
}

interface Button {
  id: number;
  pressed: boolean;
  label: string;
}

interface ControllerState {
  sliders: Slider[];
  buttons: Button[];
  connected: boolean;
  port: string | null;
}

interface Binding {
  combo: string;
  action: string;
}

interface Config {
  numContainers: number;
  numDropdowns: number;
  theme: string;
  designSystem: string;
  groups: Record<string, string[]>;
  settings: Record<string, any>;
  group_names: Record<string, string>;
}

// API URLs - Replace with your backend endpoints
const API_URLS = {
  loadConfig: 'http://localhost:8080/api/load-config',
  saveConfig: 'http://localhost:8080/api/save-config',
  loadBinds: 'http://localhost:8080/api/load-binds',
  saveBinds: 'http://localhost:8080/api/save-binds',
  fetchApps: 'http://localhost:8080/api/get-apps',
  getControllerState: 'http://localhost:8080/api/get-controller-state',
  getComPorts: 'http://localhost:8080/api/get-com-ports',
  setComPort: 'http://localhost:8080/api/set-com-port'
};

export default function Home() {
  // State management
  const [config, setConfig] = useState<Config>({
    numContainers: 4,
    numDropdowns: 6,
    theme: 'light',
    designSystem: 'default',
    groups: {},
    settings: {},
    group_names: {}
  });
  const [currentBindings, setCurrentBindings] = useState<Binding[]>([]);
  const [apps, setApps] = useState<string[]>([]);
  const [controllerState, setControllerState] = useState<ControllerState>({
    sliders: [],
    buttons: [],
    connected: false,
    port: null
  });
  const [comPorts, setComPorts] = useState<string[]>([]);
  const [selectedComPort, setSelectedComPort] = useState("");
  const [isSettingsOpen, setIsSettingsOpen] = useState(false);
  const [isRecordingKey, setIsRecordingKey] = useState(false);
  const [capturedKeys, setCapturedKeys] = useState<string | null>(null);
  const [jsonOutput, setJsonOutput] = useState("");
  const [draggedApp, setDraggedApp] = useState<string | null>(null);
  const [groupApps, setGroupApps] = useState<Record<string, string[]>>({});
  const [groupNames, setGroupNames] = useState<string[]>([]);

  // Initialize the app
  useEffect(() => {
    const initializeApp = async () => {
      // Load preferences and apply theme
      loadPreferencesFromStorage();
      
      try {
        // Load data from backend
        await Promise.all([
          loadConfigFromServer(),
          loadBindsFromServer(),
          loadComPorts()
        ]);
      } catch (error) {
        console.error("Error during initial data loading:", error);
      }
      
      // Fetch initial app list and controller state
      try {
        await fetchApplicationsFromServer();
        await fetchControllerState();
      } catch (error) {
        console.error("Error fetching initial data:", error);
      }
      
      // Start controller state update interval
      startControllerStateUpdates();
    };
    
    initializeApp();
  }, []);

  // Initialize group names when config changes
  useEffect(() => {
    // Initialize group names and group apps
    const initialGroupNames = Array.from({ length: config.numContainers }, (_, i) => {
      // Use existing name from config.group_names if it exists
      const groupKey = `Group ${i + 1}`;
      return config.group_names[groupKey] || groupKey;
    });
    
    setGroupNames(initialGroupNames);
    
    // Initialize group apps data structure
    const initialGroupApps: Record<string, string[]> = {};
    initialGroupNames.forEach((name, index) => {
      const groupKey = `Group ${index + 1}`;
      initialGroupApps[groupKey] = config.groups[groupKey] || [];
    });
    
    setGroupApps(initialGroupApps);
  }, [config.numContainers, config.group_names, config.groups]);

  // Add useEffect to update document class when theme changes
  useEffect(() => {
    if (config.theme === 'dark') {
      document.documentElement.classList.add('dark');
    } else {
      document.documentElement.classList.remove('dark');
    }
  }, [config.theme]);

  // Placeholder functions - to be implemented
  const loadPreferencesFromStorage = () => {
    const savedTheme = localStorage.getItem('appTheme');
    const savedNumGroups = localStorage.getItem('numGroups');
    const savedNumDropdowns = localStorage.getItem('numDropdowns');
    const savedDesignSystem = localStorage.getItem('designSystem');

    setConfig(prev => ({
      ...prev,
      theme: savedTheme || 'light',
      numContainers: parseInt(savedNumGroups || '4', 10),
      numDropdowns: parseInt(savedNumDropdowns || '6', 10),
      designSystem: savedDesignSystem || 'default'
    }));
  };
  
  const loadConfigFromServer = async () => {
    try {
      const response = await fetch(API_URLS.loadConfig);
      if (response.ok) {
        const loadedData = await response.json();
        setConfig(prev => ({
          ...prev,
          groups: loadedData.groups || {},
          settings: loadedData.settings || {},
          group_names: loadedData.group_names || {},
          numContainers: loadedData.numContainers || prev.numContainers,
          numDropdowns: loadedData.numDropdowns || prev.numDropdowns,
          theme: loadedData.theme || prev.theme,
          designSystem: loadedData.designSystem || prev.designSystem
        }));
      }
    } catch (error) {
      console.error("Error loading config:", error);
      setJsonOutput("Failed to load configuration. Using defaults.");
    }
  };
  
  const loadBindsFromServer = async () => {
    try {
      const response = await fetch(API_URLS.loadBinds);
      if (response.ok) {
        const binds = await response.json();
        setCurrentBindings(binds);
      }
    } catch (error) {
      console.error("Error loading bindings:", error);
      setJsonOutput("Failed to load bindings.");
    }
  };
  
  const loadComPorts = async () => {
    try {
      const response = await fetch(API_URLS.getComPorts);
      if (response.ok) {
        const ports = await response.json();
        setComPorts(ports);
        if (ports.length > 0 && !selectedComPort) {
          setSelectedComPort(ports[0]);
        }
      }
    } catch (error) {
      console.error("Error loading COM ports:", error);
      setJsonOutput("Failed to load COM ports.");
    }
  };
  
  const fetchApplicationsFromServer = async () => {
    try {
      const response = await fetch(API_URLS.fetchApps);
      if (response.ok) {
        const appList = await response.json();
        setApps(appList);
      } else {
        // For demonstration, use dummy data
        setApps(['Chrome', 'Spotify', 'Discord', 'Visual Studio', 'OBS']);
      }
    } catch (error) {
      console.error("Error fetching apps:", error);
      setJsonOutput("Failed to fetch applications.");
      // For demonstration, use dummy data
      setApps(['Chrome', 'Spotify', 'Discord', 'Visual Studio', 'OBS']);
    }
  };
  
  const fetchControllerState = async () => {
    try {
      const response = await fetch(API_URLS.getControllerState);
      if (response.ok) {
        const state = await response.json();
        setControllerState(state);
      } else {
        // For demonstration, use dummy data
        setControllerState({
          sliders: [
            { id: 1, value: 50, label: "Slider 1" },
            { id: 2, value: 75, label: "Slider 2" }
          ],
          buttons: [
            { id: 1, pressed: false, label: "Button 1" },
            { id: 2, pressed: true, label: "Button 2" }
          ],
          connected: true,
          port: "COM3"
        });
      }
    } catch (error) {
      console.error("Error fetching controller state:", error);
      // For demonstration, use dummy data
      setControllerState({
        sliders: [
          { id: 1, value: 50, label: "Slider 1" },
          { id: 2, value: 75, label: "Slider 2" }
        ],
        buttons: [
          { id: 1, pressed: false, label: "Button 1" },
          { id: 2, pressed: true, label: "Button 2" }
        ],
        connected: true,
        port: "COM3"
      });
    }
  };
  
  const startControllerStateUpdates = () => {
    // Update controller state every 500ms
    const interval = setInterval(fetchControllerState, 500);
    return () => clearInterval(interval);
  };

  // Drag and drop handling
  const handleAppDragStart = (appName: string) => {
    setDraggedApp(appName);
  };
  
  const handleAppDrop = (groupIndex: number, appName: string) => {
    const groupKey = `Group ${groupIndex + 1}`;
    
    // Update the group apps
    setGroupApps(prev => {
      const newGroupApps = { ...prev };
      if (!newGroupApps[groupKey]) {
        newGroupApps[groupKey] = [];
      }
      if (!newGroupApps[groupKey].includes(appName)) {
        newGroupApps[groupKey] = [...newGroupApps[groupKey], appName];
      }
      return newGroupApps;
    });
    
    // Update the config.groups for persistence
    setConfig(prev => {
      const newGroups = { ...prev.groups };
      if (!newGroups[groupKey]) {
        newGroups[groupKey] = [];
      }
      if (!newGroups[groupKey].includes(appName)) {
        newGroups[groupKey] = [...newGroups[groupKey], appName];
      }
      return { ...prev, groups: newGroups };
    });
    
    setJsonOutput(`Added "${appName}" to "${groupKey}"`);
  };
  
  const handleGroupRename = (groupIndex: number, newName: string) => {
    const groupKey = `Group ${groupIndex + 1}`;
    
    // Update the group names
    setGroupNames(prev => {
      const newNames = [...prev];
      newNames[groupIndex] = newName;
      return newNames;
    });
    
    // Update the config.group_names for persistence
    setConfig(prev => {
      const newGroupNames = { ...prev.group_names };
      newGroupNames[groupKey] = newName;
      return { ...prev, group_names: newGroupNames };
    });
    
    setJsonOutput(`Renamed group to "${newName}"`);
  };
  
  // Settings handlers
  const handleApplySettings = () => {
    // Save settings to local storage
    localStorage.setItem('appTheme', config.theme);
    localStorage.setItem('numGroups', config.numContainers.toString());
    localStorage.setItem('numDropdowns', config.numDropdowns.toString());
    localStorage.setItem('designSystem', config.designSystem);
    
    // Save config to server
    saveConfigToServer();
    
    setIsSettingsOpen(false);
  };
  
  const saveConfigToServer = async () => {
    try {
      const response = await fetch(API_URLS.saveConfig, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(config),
      });
      
      if (response.ok) {
        setJsonOutput("Configuration saved successfully.");
      } else {
        setJsonOutput("Failed to save configuration.");
      }
    } catch (error) {
      console.error("Error saving config:", error);
      setJsonOutput("Error saving configuration.");
    }
  };

  // Add a function to handle key recording
  const handleKeyRecording = () => {
    if (isRecordingKey) {
      // Stop recording
      setIsRecordingKey(false);
    } else {
      // Start recording
      setIsRecordingKey(true);
      setCapturedKeys("");
      
      // Add keydown event listener
      const handleKeyDown = (e: KeyboardEvent) => {
        e.preventDefault();
        
        // Ignore standalone modifier keys
        if (['Control', 'Alt', 'Shift', 'Meta'].includes(e.key)) {
          return;
        }
        
        // Build the key combination string
        let combo = "";
        if (e.ctrlKey) combo += "Ctrl+";
        if (e.altKey) combo += "Alt+";
        if (e.shiftKey) combo += "Shift+";
        if (e.metaKey) combo += "Meta+";
        
        // Add the main key
        combo += e.key.length === 1 ? e.key.toUpperCase() : e.key;
        
        // Update the captured keys
        setCapturedKeys(combo);
        
        // Stop recording after capturing a combination
        setIsRecordingKey(false);
        
        // Remove the event listener
        document.removeEventListener('keydown', handleKeyDown);
      };
      
      // Add the event listener
      document.addEventListener('keydown', handleKeyDown);
    }
  };

  // Add a function to add a new binding
  const addKeyBinding = () => {
    const keyCombo = document.getElementById('key-combo') as HTMLInputElement;
    const keyAction = document.getElementById('key-action') as HTMLInputElement;
    
    if (keyCombo.value && keyAction.value) {
      const newBinding: Binding = {
        combo: keyCombo.value,
        action: keyAction.value
      };
      
      setCurrentBindings([...currentBindings, newBinding]);
      setCapturedKeys("");
      
      // Optionally save to server
      saveBindingsToServer([...currentBindings, newBinding]);
      
      // Clear the input fields
      keyAction.value = "";
    } else {
      setJsonOutput("Please provide both shortcut and action name.");
    }
  };

  // Save bindings to server
  const saveBindingsToServer = async (bindings: Binding[]) => {
    try {
      const response = await fetch(API_URLS.saveBinds, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(bindings),
      });
      
      if (response.ok) {
        setJsonOutput("Binding saved successfully.");
      } else {
        setJsonOutput("Failed to save binding.");
      }
    } catch (error) {
      console.error("Error saving binding:", error);
      setJsonOutput("Error saving binding.");
    }
  };

  // Delete a binding
  const deleteBinding = (index: number) => {
    const updatedBindings = [...currentBindings];
    updatedBindings.splice(index, 1);
    setCurrentBindings(updatedBindings);
    
    // Optionally save to server
    saveBindingsToServer(updatedBindings);
    
    setJsonOutput("Binding deleted.");
  };

  // Render the UI
  return (
    <div className="min-h-screen bg-background">
      <div className="flex flex-col min-h-screen">
        <main className="flex flex-1 p-4 gap-4">
          {/* Available Apps Section */}
          <Card className="w-[200px] flex-shrink-0">
            <CardHeader>
              <CardTitle>Available Apps</CardTitle>
            </CardHeader>
            <CardContent className="flex flex-col gap-2">
              <Button 
                variant="outline" 
                className="w-full mb-4"
                onClick={fetchApplicationsFromServer}
              >
                Fetch Apps
              </Button>
              <div className="flex flex-col gap-2 min-h-[50px]">
                {apps.map((app, index) => (
                  <AppBox 
                    key={index}
                    appName={app}
                    onDragStart={handleAppDragStart}
                  />
                ))}
              </div>
            </CardContent>
          </Card>

          {/* Group Containers Section */}
          <Card className="flex-grow bg-muted/40">
            <CardHeader>
              <CardTitle>Groups (Double-click to rename)</CardTitle>
            </CardHeader>
            <CardContent className="flex gap-4 h-[calc(100%-60px)]">
              {groupNames.map((name, index) => (
                <GroupBox
                  key={index}
                  groupName={name}
                  groupIndex={index}
                  apps={groupApps[`Group ${index + 1}`] || []}
                  onDrop={handleAppDrop}
                  onRename={handleGroupRename}
                />
              ))}
            </CardContent>
          </Card>

          {/* Assign Actions Section */}
          <Card className="w-[280px] flex-shrink-0">
            <CardHeader>
              <CardTitle>Assign Actions</CardTitle>
            </CardHeader>
            <CardContent>
              <div className="grid gap-4">
                {Array.from({ length: config.numDropdowns }, (_, i) => (
                  <div key={i} className="space-y-2">
                    <Label htmlFor={`dropdown-${i}`}>Action {i + 1}</Label>
                    <Select>
                      <SelectTrigger id={`dropdown-${i}`}>
                        <SelectValue placeholder="Select action" />
                      </SelectTrigger>
                      <SelectContent>
                        <SelectItem value="action1">Action 1</SelectItem>
                        <SelectItem value="action2">Action 2</SelectItem>
                        <SelectItem value="action3">Action 3</SelectItem>
                      </SelectContent>
                    </Select>
                  </div>
                ))}
              </div>
            </CardContent>
          </Card>
        </main>

        {/* Controller Status Section */}
        <ControllerStatus
          connected={controllerState.connected}
          sliders={controllerState.sliders}
          buttons={controllerState.buttons}
        />

        {/* Bottom Controls */}
        <div className="border-t p-4 bg-muted/30 flex items-center gap-5">
          <Dialog open={isSettingsOpen} onOpenChange={setIsSettingsOpen}>
            <DialogTrigger asChild>
              <Button>⚙️ Settings</Button>
            </DialogTrigger>
            <DialogContent className="max-w-3xl max-h-[90vh] overflow-y-auto">
              <DialogHeader>
                <DialogTitle>Application Settings</DialogTitle>
              </DialogHeader>
              
              <div className="space-y-6">
                <div>
                  <h3 className="text-lg font-medium mb-4">General</h3>
                  <div className="space-y-4">
                    <div className="grid grid-cols-2 gap-4">
                      <div className="space-y-2">
                        <Label htmlFor="num-groups">Number of Groups (1-10):</Label>
                        <Input 
                          id="num-groups" 
                          type="number" 
                          min="1" 
                          max="10" 
                          value={config.numContainers}
                          onChange={(e) => setConfig(prev => ({ ...prev, numContainers: parseInt(e.target.value) || 4 }))}
                        />
                      </div>
                      <div className="space-y-2">
                        <Label htmlFor="num-dropdowns">Number of Action Slots (0-20):</Label>
                        <Input 
                          id="num-dropdowns" 
                          type="number" 
                          min="0" 
                          max="20" 
                          value={config.numDropdowns}
                          onChange={(e) => setConfig(prev => ({ ...prev, numDropdowns: parseInt(e.target.value) || 6 }))}
                        />
                      </div>
                    </div>
                    
                    <div className="flex items-center space-x-2">
                      <Checkbox 
                        id="dark-theme" 
                        checked={config.theme === 'dark'}
                        onCheckedChange={(checked) => setConfig(prev => ({ ...prev, theme: checked ? 'dark' : 'light' }))}
                      />
                      <Label htmlFor="dark-theme">Dark Theme</Label>
                    </div>
                    
                    <div className="space-y-2">
                      <Label htmlFor="design-system">Design System:</Label>
                      <Select 
                        value={config.designSystem}
                        onValueChange={(value) => setConfig(prev => ({ ...prev, designSystem: value }))}
                      >
                        <SelectTrigger id="design-system">
                          <SelectValue placeholder="Select design system" />
                        </SelectTrigger>
                        <SelectContent>
                          <SelectItem value="windows-11">Windows 11 Style</SelectItem>
                          <SelectItem value="material-you">Material You</SelectItem>
                        </SelectContent>
                      </Select>
                    </div>
                  </div>
                </div>
                
                <Separator />
                
                <div>
                  <h3 className="text-lg font-medium mb-4">Arduino Connection</h3>
                  <div className="space-y-4">
                    <div className="space-y-2">
                      <Label htmlFor="com-port">COM Port:</Label>
                      <div className="flex space-x-2">
                        <Select 
                          value={selectedComPort}
                          onValueChange={setSelectedComPort}
                        >
                          <SelectTrigger id="com-port" className="flex-1">
                            <SelectValue placeholder="Select COM port" />
                          </SelectTrigger>
                          <SelectContent>
                            {comPorts.map((port, index) => (
                              <SelectItem key={index} value={port}>{port}</SelectItem>
                            ))}
                          </SelectContent>
                        </Select>
                        <Button variant="outline" size="icon" onClick={loadComPorts}>↻</Button>
                      </div>
                    </div>
                    <p className="text-sm text-muted-foreground italic">
                      Select the COM port where your Arduino is connected. Click the refresh button to scan for new ports.
                    </p>
                  </div>
                </div>
                
                <Separator />
                
                <div>
                  <h3 className="text-lg font-medium mb-4">Key Shortcut Bindings</h3>
                  <div className="space-y-6">
                    <div className="space-y-4">
                      <h4 className="font-medium">Add New Binding</h4>
                      <div className="space-y-4">
                        <div className="flex items-center space-x-2">
                          <div className="space-y-2 flex-1">
                            <Label htmlFor="key-combo">Shortcut:</Label>
                            <div className="flex space-x-2">
                              <Input 
                                id="key-combo" 
                                readOnly 
                                placeholder="Click 'Record' and press keys" 
                                className="flex-1"
                                value={capturedKeys || ""}
                              />
                              <Button 
                                variant={isRecordingKey ? "destructive" : "secondary"}
                                onClick={handleKeyRecording}
                              >
                                {isRecordingKey ? "Stop" : "Record"}
                              </Button>
                            </div>
                          </div>
                        </div>
                        <div className="space-y-2">
                          <Label htmlFor="key-action">Action Name:</Label>
                          <Input 
                            id="key-action" 
                            placeholder="Unique name, e.g., open_settings" 
                          />
                        </div>
                        <Button onClick={addKeyBinding}>Add Binding</Button>
                      </div>
                    </div>
                    
                    <div className="space-y-2">
                      <h4 className="font-medium">Current Bindings</h4>
                      <ul className="border rounded-md divide-y">
                        {currentBindings.length > 0 ? (
                          currentBindings.map((binding, index) => (
                            <li key={index} className="flex justify-between items-center p-3">
                              <div>
                                <span className="font-mono bg-muted px-1 py-0.5 rounded">{binding.combo}</span>
                                <span className="ml-2 text-muted-foreground">{binding.action}</span>
                              </div>
                              <Button variant="ghost" size="sm" onClick={() => deleteBinding(index)}>×</Button>
                            </li>
                          ))
                        ) : (
                          <li className="p-3 text-center text-muted-foreground">No bindings defined</li>
                        )}
                      </ul>
                    </div>
                  </div>
                </div>
              </div>
              
              <div className="flex justify-end mt-6">
                <Button onClick={handleApplySettings}>Apply & Close</Button>
              </div>
            </DialogContent>
          </Dialog>
          
          <pre className="text-xs font-mono bg-muted p-2 rounded flex-1 h-10 overflow-hidden">
            {jsonOutput}
          </pre>
        </div>
      </div>
    </div>
  );
}
