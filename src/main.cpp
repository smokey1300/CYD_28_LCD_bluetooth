/*  Rui Santos & Sara Santos - Random Nerd Tutorials
    THIS EXAMPLE WAS TESTED WITH THE FOLLOWING HARDWARE:
    1) ESP32-2432S028R 2.8 inch 240×320 also known as the Cheap Yellow Display (CYD): https://makeradvisor.com/tools/cyd-cheap-yellow-display-esp32-2432s028r/
      SET UP INSTRUCTIONS: https://RandomNerdTutorials.com/cyd-lvgl/
    2) REGULAR ESP32 Dev Board + 2.8 inch 240x320 TFT Display: https://makeradvisor.com/tools/2-8-inch-ili9341-tft-240x320/ and https://makeradvisor.com/tools/esp32-dev-board-wi-fi-bluetooth/
      SET UP INSTRUCTIONS: https://RandomNerdTutorials.com/esp32-tft-lvgl/
    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
    The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
    **** Working 4 button code with good formatting and Auto connect feature added by W.J. Lam - Jan 2026    
*/

#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// BLE Libraries
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEAddress.h>
#include <BLEClient.h>
#include <BLERemoteCharacteristic.h>

// NVS for storing MAC addresses
#include <Preferences.h>

// Touchscreen pins
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

int x, y, z;

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

// Global variables
lv_obj_t * main_screen;
lv_obj_t * bluetooth_screen;
lv_obj_t * stored_devices_screen;  // ADDED: Stored devices screen
lv_obj_t * status_indicator;  // ADDED: Status indicator circle

// BLE Variables
BLEScan* pBLEScan;
BLEClient* pClient = nullptr;
BLEAddress* serverAddress = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
bool isScanning = false;
bool isConnected = false;
String connectedDeviceName = "";
String connectedDeviceAddress = "";

// Device storage
struct BLEDeviceInfo {
  String name;
  String address;
  int rssi;
  bool isTarget1;
  bool isTarget2;
};

std::vector<BLEDeviceInfo> bleDevices;
int selectedDeviceIdx = -1;

// Stored devices (Target1 and Target2)
BLEDeviceInfo storedDevices[2];  // Index 0: Target1, Index 1: Target2

// UI elements
lv_obj_t* deviceList;
lv_obj_t* selectedDeviceLabel;
lv_obj_t* connectionStatusLabel;
lv_obj_t* target1StatusLabel;  // ADDED: Target1 status label
lv_obj_t* target2StatusLabel;  // ADDED: Target2 status label

// Your BLE Service and Characteristic UUIDs
#define SERVICE_UUID        "0000FFE0-0000-1000-8000-00805F9B34FB"
#define CHARACTERISTIC_UUID "0000FFE1-0000-1000-8000-00805F9B34FB"

// Default MAC addresses (used if nothing is stored in NVS)
#define DEFAULT_TARGET_DEVICE_ADDRESS "B4:52:A9:B0:0F:BB"
#define DEFAULT_TARGET2_DEVICE_ADDRESS "00:00:00:00:00:00"

// NVS Preferences
Preferences preferences;
#define NVS_NAMESPACE "ble_storage"
#define TARGET1_MAC_KEY "target1_mac"
#define TARGET2_MAC_KEY "target2_mac"
#define AUTOCONNECT_ENABLED_KEY "auto_connect"  // CHANGED: Shortened key name

// Current MAC addresses from NVS
String storedTarget1MAC = "";
String storedTarget2MAC = "";

// Auto-connect state
bool autoConnectEnabled = true;  // ADDED: Default to enabled

// Forward function declarations
void bleStartScan();
bool bleConnectToDevice(int deviceIndex);
void bleDisconnect();
void bleSendData(const String& data);
void bleSendHexString(const String& hexString);
bool bleAutoConnectDirect();  // ADDED THIS
bool bleAutoConnectTarget2();  // ADDED THIS: Direct connect to Target2
void updateStatusIndicator();  // ADDED: Function to update status indicator
void updateStoredDevicesScreen();  // ADDED: Update stored devices screen
void log_print(lv_log_level_t level, const char * buf);
void touchscreen_read(lv_indev_t * indev, lv_indev_data_t * data);
void loadStoredMACs();  // ADDED: Load MACs from NVS
void saveTarget1MAC(const String& mac);  // ADDED: Save Target1 MAC to NVS
void saveTarget2MAC(const String& mac);  // ADDED: Save Target2 MAC to NVS
void loadAutoConnectState();  // ADDED: Load auto-connect state from NVS
void saveAutoConnectState(bool enabled);  // ADDED: Save auto-connect state to NVS

// EVENT HANDLER DECLARATIONS - ADDED THIS
static void event_handler_btnSet(lv_event_t * e);
static void event_handler_btnBack(lv_event_t * e);
static void event_handler_btnBackStored(lv_event_t * e);  // ADDED: Back from stored devices
static void event_handler_btnScan(lv_event_t * e);
static void event_handler_btnConnect(lv_event_t * e);
static void event_handler_btnDisconnect(lv_event_t * e);
static void event_handler_deviceList(lv_event_t * e);  // ADDED THIS LINE
static void event_handler_btn1(lv_event_t * e);
static void event_handler_btn2(lv_event_t * e);
static void event_handler_btn3(lv_event_t * e);
static void event_handler_btn4(lv_event_t * e);
static void event_handler_btnTarget1(lv_event_t * e);  // ADDED: Store as Target1
static void event_handler_btnTarget2(lv_event_t * e);  // ADDED: Store as Target2
static void event_handler_btnStoredDevices(lv_event_t * e);  // ADDED: For Stored Devices button
static void event_handler_btnConnectTarget1(lv_event_t * e);  // ADDED: Connect to Target1
static void event_handler_btnConnectTarget2(lv_event_t * e);  // ADDED: Connect to Target2
static void event_handler_autoConnectCheckbox(lv_event_t * e);  // ADDED: For auto-connect checkbox

// Logging
void log_print(lv_log_level_t level, const char * buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

// Touchscreen
void touchscreen_read(lv_indev_t * indev, lv_indev_data_t * data) {
  if(touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
    y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
    z = p.z;

    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// Update status indicator function
void updateStatusIndicator() {
  if (status_indicator) {
    if (isConnected && pClient && pClient->isConnected()) {
      // Connected - Use BLUE (0xFF0000) 
      lv_obj_set_style_bg_color(status_indicator, lv_color_hex(0xFF0000), LV_PART_MAIN);
    } else {
      // Not connected - Use Red (0x00FF00)
      lv_obj_set_style_bg_color(status_indicator, lv_color_hex(0x00FF00), LV_PART_MAIN);
    }
    lv_obj_set_style_bg_opa(status_indicator, LV_OPA_COVER, LV_PART_MAIN);
  }
}

// ADDED: Load stored MACs from NVS
void loadStoredMACs() {
  preferences.begin(NVS_NAMESPACE, false);
  storedTarget1MAC = preferences.getString(TARGET1_MAC_KEY, DEFAULT_TARGET_DEVICE_ADDRESS);
  storedTarget2MAC = preferences.getString(TARGET2_MAC_KEY, DEFAULT_TARGET2_DEVICE_ADDRESS);
  preferences.end();
  
  Serial.println("=== Loaded Stored MACs from NVS ===");
  Serial.println("Target1 MAC: " + storedTarget1MAC);
  Serial.println("Target2 MAC: " + storedTarget2MAC);
}

// ADDED: Save Target1 MAC to NVS
void saveTarget1MAC(const String& mac) {
  preferences.begin(NVS_NAMESPACE, false);
  preferences.putString(TARGET1_MAC_KEY, mac);
  preferences.end();
  
  storedTarget1MAC = mac;
  Serial.println("Saved Target1 MAC to NVS: " + mac);
}

// ADDED: Save Target2 MAC to NVS
void saveTarget2MAC(const String& mac) {
  preferences.begin(NVS_NAMESPACE, false);
  preferences.putString(TARGET2_MAC_KEY, mac);
  preferences.end();
  
  storedTarget2MAC = mac;
  Serial.println("Saved Target2 MAC to NVS: " + mac);
}

// ADDED: Load auto-connect state from NVS
void loadAutoConnectState() {
  preferences.begin(NVS_NAMESPACE, false);
  autoConnectEnabled = preferences.getBool(AUTOCONNECT_ENABLED_KEY, true); // Default to enabled
  preferences.end();
  
  Serial.println("Auto-connect enabled: " + String(autoConnectEnabled ? "YES" : "NO"));
}

// ADDED: Save auto-connect state to NVS
void saveAutoConnectState(bool enabled) {
  preferences.begin(NVS_NAMESPACE, false);
  preferences.putBool(AUTOCONNECT_ENABLED_KEY, enabled);
  preferences.end();
  
  autoConnectEnabled = enabled;
  Serial.println("Saved auto-connect state: " + String(enabled ? "ENABLED" : "DISABLED"));
}

// ADDED: Function to update stored devices screen
void updateStoredDevicesScreen() {
  if (!stored_devices_screen) return;
  
  // Update Target1 status
  if (target1StatusLabel) {
    String statusText = "Target1: ";
    if (isConnected && connectedDeviceAddress == storedTarget1MAC) {
      statusText += "CONNECTED";
      lv_obj_set_style_text_color(target1StatusLabel, lv_color_hex(0x00FF00), LV_PART_MAIN);
    } else {
      statusText += "DISCONNECTED";
      lv_obj_set_style_text_color(target1StatusLabel, lv_color_hex(0xFF0000), LV_PART_MAIN);
    }
    lv_label_set_text(target1StatusLabel, statusText.c_str());
  }
  
  // Update Target2 status
  if (target2StatusLabel) {
    String statusText = "Target2: ";
    if (isConnected && connectedDeviceAddress == storedTarget2MAC) {
      statusText += "CONNECTED";
      lv_obj_set_style_text_color(target2StatusLabel, lv_color_hex(0x00FF00), LV_PART_MAIN);
    } else if (storedTarget2MAC == "00:00:00:00:00:00") {
      statusText += "MAC NOT SET";
      lv_obj_set_style_text_color(target2StatusLabel, lv_color_hex(0xFFA500), LV_PART_MAIN);
    } else {
      statusText += "DISCONNECTED";
      lv_obj_set_style_text_color(target2StatusLabel, lv_color_hex(0xFF0000), LV_PART_MAIN);
    }
    lv_label_set_text(target2StatusLabel, statusText.c_str());
  }
}

// BLE Callback for discovered devices (updated to just log, not store)
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // Just log found devices, but don't store them here anymore
    String deviceName = advertisedDevice.getName().c_str();
    if (deviceName.length() == 0) {
      deviceName = "Unknown Device";
    }
    
    String deviceAddress = advertisedDevice.getAddress().toString().c_str();
    int rssi = advertisedDevice.getRSSI();
    
    Serial.printf("BLE Scanning: %s - %s (%d dB)\n", 
                  deviceName.c_str(), deviceAddress.c_str(), rssi);
  }
};

// Initialize stored devices
void initStoredDevices() {
  // Target1
  storedDevices[0].name = "MY TARGET DEVICE";
  storedDevices[0].address = storedTarget1MAC;
  storedDevices[0].rssi = -60;
  storedDevices[0].isTarget1 = true;
  storedDevices[0].isTarget2 = false;
  
  // Target2 (if not placeholder)
  storedDevices[1].name = "TARGET2 DEVICE";
  storedDevices[1].address = storedTarget2MAC;
  storedDevices[1].rssi = -60;
  storedDevices[1].isTarget1 = false;
  storedDevices[1].isTarget2 = (storedTarget2MAC != "00:00:00:00:00:00");
}

// NEW: Direct auto-connect function for Target1
bool bleAutoConnectDirect() {
  Serial.println("=== Attempting Direct Auto-Connect to Target Device ===");
  
  // Check if Target1 address is stored
  if (storedTarget1MAC.length() == 0 || storedTarget1MAC == "00:00:00:00:00:00") {
    Serial.println("ERROR: Target1 MAC address is not stored!");
    Serial.println("Please select a device and tap 'Target1' to store it.");
    
    if (connectionStatusLabel) {
      lv_label_set_text(connectionStatusLabel, "Status: Target1 MAC not set");
    }
    return false;
  }
  
  // Disconnect if already connected
  if (isConnected) {
    bleDisconnect();
    delay(500);
  }
  
  // Create BLE client
  pClient = BLEDevice::createClient();
  serverAddress = new BLEAddress(storedTarget1MAC.c_str());
  
  Serial.print("Direct connection to: ");
  Serial.println(storedTarget1MAC);
  
  // Connect to BLE server
  if (pClient->connect(*serverAddress)) {
    Serial.println("Connected to BLE server!");
    
    // Get the service
    BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr) {
      Serial.println("Failed to find service UUID");
      bleDisconnect();
      updateStatusIndicator();  // UPDATE status indicator
      updateStoredDevicesScreen();  // UPDATE stored devices screen
      return false;
    }
    
    // Get the characteristic
    pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.println("Failed to find characteristic UUID");
      bleDisconnect();
      updateStatusIndicator();  // UPDATE status indicator
      updateStoredDevicesScreen();  // UPDATE stored devices screen
      return false;
    }
    
    isConnected = true;
    connectedDeviceName = "MY TARGET DEVICE";
    connectedDeviceAddress = storedTarget1MAC;
    
    if (connectionStatusLabel) {
      lv_label_set_text(connectionStatusLabel, "Status: Auto-Connected");
    }
    
    // UPDATE status indicator and stored devices screen
    updateStatusIndicator();
    updateStoredDevicesScreen();
    
    // Send connection confirmation
    delay(100);
    bleSendData("CONNECTED");
    
    Serial.println("=== BLE Auto-Connection Successful ===");
    return true;
  } else {
    Serial.println("Failed to auto-connect to BLE server");
    delete pClient;
    pClient = nullptr;
    delete serverAddress;
    serverAddress = nullptr;
    updateStatusIndicator();  // UPDATE status indicator
    updateStoredDevicesScreen();  // UPDATE stored devices screen
    return false;
  }
}

// NEW: Direct auto-connect function for Target2
bool bleAutoConnectTarget2() {
  Serial.println("=== Attempting Direct Auto-Connect to Target2 Device ===");
  
  // Check if Target2 address is still placeholder
  if (storedTarget2MAC == "00:00:00:00:00:00") {
    Serial.println("ERROR: Target2 MAC address is still placeholder!");
    Serial.println("Please select a device and tap 'Target2' to store it.");
    
    if (connectionStatusLabel) {
      lv_label_set_text(connectionStatusLabel, "Status: Target2 MAC not set");
    }
    updateStoredDevicesScreen();  // UPDATE stored devices screen
    return false;
  }
  
  // Disconnect if already connected
  if (isConnected) {
    bleDisconnect();
    delay(500);
  }
  
  // Create BLE client
  pClient = BLEDevice::createClient();
  serverAddress = new BLEAddress(storedTarget2MAC.c_str());
  
  Serial.print("Direct connection to Target2: ");
  Serial.println(storedTarget2MAC);
  
  // Connect to BLE server
  if (pClient->connect(*serverAddress)) {
    Serial.println("Connected to Target2 BLE server!");
    
    // Get the service
    BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr) {
      Serial.println("Failed to find service UUID on Target2");
      bleDisconnect();
      updateStatusIndicator();  // UPDATE status indicator
      updateStoredDevicesScreen();  // UPDATE stored devices screen
      return false;
    }
    
    // Get the characteristic
    pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.println("Failed to find characteristic UUID on Target2");
      bleDisconnect();
      updateStatusIndicator();  // UPDATE status indicator
      updateStoredDevicesScreen();  // UPDATE stored devices screen
      return false;
    }
    
    isConnected = true;
    connectedDeviceName = "TARGET2 DEVICE";
    connectedDeviceAddress = storedTarget2MAC;
    
    if (connectionStatusLabel) {
      lv_label_set_text(connectionStatusLabel, "Status: Connected to Target2");
    }
    
    // UPDATE status indicator and stored devices screen
    updateStatusIndicator();
    updateStoredDevicesScreen();
    
    // Send connection confirmation
    delay(100);
    bleSendData("CONNECTED_TO_TARGET2");
    
    Serial.println("=== Target2 BLE Auto-Connection Successful ===");
    return true;
  } else {
    Serial.println("Failed to auto-connect to Target2 BLE server");
    delete pClient;
    pClient = nullptr;
    delete serverAddress;
    serverAddress = nullptr;
    updateStatusIndicator();  // UPDATE status indicator
    updateStoredDevicesScreen();  // UPDATE stored devices screen
    return false;
  }
}

// BLE Functions
void bleStartScan() {
  if (isScanning) return;
  
  Serial.println("=== Starting BLE Scan ===");
  isScanning = true;
  
  bleDevices.clear();
  selectedDeviceIdx = -1;
  
  if (deviceList) {
    lv_obj_clean(deviceList);
    lv_list_add_text(deviceList, "Scanning...");
  }
  
  if (selectedDeviceLabel) {
    lv_label_set_text(selectedDeviceLabel, "Selected: None");
  }
  
  // Start BLE scan for 5 seconds
  BLEScanResults foundDevices = pBLEScan->start(5, false);
  Serial.print("BLE scan found: ");
  Serial.print(foundDevices.getCount());
  Serial.println(" devices");
  
  // Update UI with found devices
  if (deviceList) {
    lv_obj_clean(deviceList);
    lv_list_add_text(deviceList, "Found Devices:");
    
    if (foundDevices.getCount() > 0) {
      // First, add all found devices
      for (int i = 0; i < foundDevices.getCount(); i++) {
        BLEAdvertisedDevice device = foundDevices.getDevice(i);
        String deviceName = device.getName().c_str();
        if (deviceName.length() == 0) {
          deviceName = "Unknown Device";
        }
        
        String deviceAddress = device.getAddress().toString().c_str();
        int rssi = device.getRSSI();
        
        BLEDeviceInfo newDevice;
        newDevice.name = deviceName;
        newDevice.address = deviceAddress;
        newDevice.rssi = rssi;
        newDevice.isTarget1 = (deviceAddress == storedTarget1MAC);
        newDevice.isTarget2 = (deviceAddress == storedTarget2MAC);
        bleDevices.push_back(newDevice);
        
        Serial.printf("BLE Found: %s - %s (%d dB)\n", 
                      deviceName.c_str(), deviceAddress.c_str(), rssi);
      }
      
      // Now add devices to the list
      for (size_t i = 0; i < bleDevices.size(); i++) {
        String displayText = bleDevices[i].name;
        
        // Mark as Target1 or Target2 if they match stored MACs
        if (bleDevices[i].isTarget1) {
          displayText = ">> " + displayText + " (Target1)";
        } else if (bleDevices[i].isTarget2) {
          displayText = ">> " + displayText + " (Target2)";
        }
        
        // Add RSSI if available
        if (bleDevices[i].rssi != 0) {
          displayText += " (" + String(bleDevices[i].rssi) + "dB)";
        }
        
        // Truncate if too long
        if (displayText.length() > 30) {
          displayText = displayText.substring(0, 27) + "...";
        }
        
        lv_obj_t* btn = lv_list_add_btn(deviceList, LV_SYMBOL_BLUETOOTH, displayText.c_str());
        // Store device index in button user data - FIXED
        lv_obj_set_user_data(btn, (void*)(uintptr_t)i);
        
        // CRITICAL FIX: Add event handler to the button itself
        lv_obj_add_event_cb(btn, event_handler_deviceList, LV_EVENT_CLICKED, NULL);
        
        Serial.printf("Added device %d to list: %s\n", i, displayText.c_str());
      }
    } else {
      lv_list_add_text(deviceList, "No devices found");
    }
  }
  
  pBLEScan->clearResults();
  isScanning = false;
  Serial.printf("=== Scan Complete: %d devices ===\n", bleDevices.size());
}

bool bleConnectToDevice(int deviceIndex) {
  Serial.printf("=== Connecting to device index: %d ===\n", deviceIndex);
  
  if (deviceIndex < 0 || deviceIndex >= bleDevices.size()) {
    Serial.printf("ERROR: Invalid device index %d (list has %d devices)\n", 
                  deviceIndex, bleDevices.size());
    return false;
  }
  
  BLEDeviceInfo device = bleDevices[deviceIndex];
  Serial.printf("Connecting to: %s (%s)\n", 
                device.name.c_str(), device.address.c_str());
  
  if (connectionStatusLabel) {
    lv_label_set_text(connectionStatusLabel, "Status: Connecting...");
  }
  
  // Disconnect if already connected
  if (isConnected) {
    bleDisconnect();
    delay(500);
  }
  
  // Create BLE client
  pClient = BLEDevice::createClient();
  serverAddress = new BLEAddress(device.address.c_str());
  
  Serial.print("Attempting BLE connection to: ");
  Serial.println(device.address.c_str());
  
  // Connect to BLE server
  if (pClient->connect(*serverAddress)) {
    Serial.println("Connected to BLE server!");
    
    // Get the service
    BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr) {
      Serial.println("Failed to find service UUID: " + String(SERVICE_UUID));
      bleDisconnect();
      updateStatusIndicator();  // UPDATE status indicator
      updateStoredDevicesScreen();  // UPDATE stored devices screen
      return false;
    }
    
    Serial.println("Found BLE service!");
    
    // Get the characteristic
    pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.println("Failed to find characteristic UUID: " + String(CHARACTERISTIC_UUID));
      bleDisconnect();
      updateStatusIndicator();  // UPDATE status indicator
      updateStoredDevicesScreen();  // UPDATE stored devices screen
      return false;
    }
    
    Serial.println("Found BLE characteristic!");
    
    // Check if we can write to it
    if (pRemoteCharacteristic->canWrite()) {
      Serial.println("Characteristic supports write operations");
    } else {
      Serial.println("Warning: Characteristic may not support write");
    }
    
    isConnected = true;
    connectedDeviceName = device.name;
    connectedDeviceAddress = device.address;
    
    if (connectionStatusLabel) {
      lv_label_set_text(connectionStatusLabel, ("Status: Connected to " + device.name).c_str());
    }
    
    // UPDATE status indicator and stored devices screen
    updateStatusIndicator();
    updateStoredDevicesScreen();
    
    // Send connection confirmation
    delay(100);
    bleSendData("CONNECTED");
    
    Serial.println("=== BLE Connection Successful ===");
    return true;
  } else {
    Serial.println("Failed to connect to BLE server");
    
    if (connectionStatusLabel) {
      lv_label_set_text(connectionStatusLabel, "Status: Connection failed");
    }
    
    delete pClient;
    pClient = nullptr;
    delete serverAddress;
    serverAddress = nullptr;
    
    updateStatusIndicator();  // UPDATE status indicator
    updateStoredDevicesScreen();  // UPDATE stored devices screen
    
    return false;
  }
}

void bleDisconnect() {
  if (isConnected || pClient != nullptr) {
    Serial.println("Disconnecting from BLE...");
    
    if (isConnected) {
      bleSendData("DISCONNECT");
    }
    
    if (pClient && pClient->isConnected()) {
      pClient->disconnect();
    }
    
    if (pClient) {
      delete pClient;
      pClient = nullptr;
    }
    
    if (serverAddress) {
      delete serverAddress;
      serverAddress = nullptr;
    }
    
    pRemoteCharacteristic = nullptr;
    isConnected = false;
    connectedDeviceName = "";
    connectedDeviceAddress = "";
    
    if (connectionStatusLabel) {
      lv_label_set_text(connectionStatusLabel, "Status: Disconnected");
    }
    
    // UPDATE status indicator and stored devices screen
    updateStatusIndicator();
    updateStoredDevicesScreen();
    
    Serial.println("BLE Disconnected");
  }
}

void bleSendData(const String& data) {
  if (isConnected && pClient && pClient->isConnected() && pRemoteCharacteristic) {
    if (pRemoteCharacteristic->canWrite()) {
      pRemoteCharacteristic->writeValue(data.c_str(), data.length());
      Serial.println("BLE Sent: " + data);
    } else {
      // Try to write anyway (some characteristics don't report canWrite correctly)
      pRemoteCharacteristic->writeValue(data.c_str(), data.length());
      Serial.println("BLE Sent (force write): " + data);
    }
  } else {
    Serial.println("Cannot send: Not connected to BLE");
  }
}

// NEW FUNCTION: Send hex string as raw bytes
void bleSendHexString(const String& hexString) {
  if (isConnected && pClient && pClient->isConnected() && pRemoteCharacteristic) {
    // Convert hex string to bytes
    int length = hexString.length();
    if (length % 2 != 0) {
      Serial.println("ERROR: Hex string must have even number of characters");
      return;
    }
    
    int byteCount = length / 2;
    uint8_t* bytes = new uint8_t[byteCount];
    
    for (int i = 0; i < byteCount; i++) {
      String byteString = hexString.substring(i * 2, i * 2 + 2);
      bytes[i] = strtoul(byteString.c_str(), NULL, 16);
    }
    
    // Send the bytes
    if (pRemoteCharacteristic->canWrite()) {
      pRemoteCharacteristic->writeValue(bytes, byteCount, false);
      Serial.print("BLE Sent hex bytes: ");
      for (int i = 0; i < byteCount; i++) {
        Serial.printf("%02X ", bytes[i]);
      }
      Serial.println();
    } else {
      // Try to write anyway
      pRemoteCharacteristic->writeValue(bytes, byteCount, false);
      Serial.print("BLE Sent hex bytes (force write): ");
      for (int i = 0; i < byteCount; i++) {
        Serial.printf("%02X ", bytes[i]);
      }
      Serial.println();
    }
    
    delete[] bytes;
  } else {
    Serial.println("Cannot send hex: Not connected to BLE");
  }
}

// Callbacks
static void event_handler_btnSet(lv_event_t * e) {
  if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_screen_load(bluetooth_screen);
  }
}

static void event_handler_btnBack(lv_event_t * e) {
  if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_screen_load(main_screen);
  }
}

// ADDED: Back from stored devices screen - CORRECTED to go to Bluetooth screen
static void event_handler_btnBackStored(lv_event_t * e) {
  if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_screen_load(bluetooth_screen);  // CORRECTED: Goes to Bluetooth screen, not main screen
  }
}

static void event_handler_btnScan(lv_event_t * e) {
  if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
    Serial.println("=== Scan Button Clicked ===");
    bleStartScan();
  }
}

static void event_handler_btnConnect(lv_event_t * e) {
  if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
    Serial.printf("=== Connect Button Clicked ===\n");
    Serial.printf("Selected device index: %d\n", selectedDeviceIdx);
    
    if (selectedDeviceIdx >= 0 && selectedDeviceIdx < bleDevices.size()) {
      bleConnectToDevice(selectedDeviceIdx);
    } else {
      Serial.println("ERROR: Please select a device from the list first!");
      if (connectionStatusLabel) {
        lv_label_set_text(connectionStatusLabel, "Status: Select device first");
      }
    }
  }
}

static void event_handler_btnDisconnect(lv_event_t * e) {
  if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
    Serial.println("=== Disconnect Button Clicked ===");
    bleDisconnect();
  }
}

// Device list selection callback - FIXED
static void event_handler_deviceList(lv_event_t * e) {
  lv_event_code_t code = lv_event_get_code(e);
  
  if(code == LV_EVENT_CLICKED) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    
    // Get the device index from user data - FIXED
    uintptr_t index = (uintptr_t)lv_obj_get_user_data(btn);
    
    Serial.printf("Device list item clicked! Index from user data: %u\n", (unsigned int)index);
    
    if (index < bleDevices.size()) {
      selectedDeviceIdx = (int)index;
      BLEDeviceInfo& device = bleDevices[index];
      
      Serial.printf("SUCCESS: Selected device %u: %s (%s)\n", 
                    (unsigned int)index, device.name.c_str(), device.address.c_str());
      
      // Update selected device label
      if (selectedDeviceLabel) {
        String displayText = "Selected: " + device.name;
        if (device.isTarget1) {
          displayText += " [TARGET1]";
        } else if (device.isTarget2) {
          displayText += " [TARGET2]";
        }
        lv_label_set_text(selectedDeviceLabel, displayText.c_str());
      }
    } else {
      Serial.printf("ERROR: Index %u is invalid (list size: %d)\n", (unsigned int)index, bleDevices.size());
      selectedDeviceIdx = -1;
      if (selectedDeviceLabel) {
        lv_label_set_text(selectedDeviceLabel, "Selected: INVALID");
      }
    }
  }
}

// ADDED: Event handler for Store as Target1 button
static void event_handler_btnTarget1(lv_event_t * e) {
  if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
    Serial.println("=== Store as Target1 Button Clicked ===");
    
    if (selectedDeviceIdx >= 0 && selectedDeviceIdx < bleDevices.size()) {
      BLEDeviceInfo device = bleDevices[selectedDeviceIdx];
      saveTarget1MAC(device.address);
      
      Serial.println("Saved as Target1: " + device.address);
      
      // Update status label
      if (connectionStatusLabel) {
        lv_label_set_text(connectionStatusLabel, ("Stored as Target1: " + device.name).c_str());
      }
      
      // Update stored devices screen if it exists
      updateStoredDevicesScreen();
    } else {
      Serial.println("ERROR: Please select a device from the list first!");
      if (connectionStatusLabel) {
        lv_label_set_text(connectionStatusLabel, "Status: Select device first");
      }
    }
  }
}

// ADDED: Event handler for Store as Target2 button (renamed from just Target2)
static void event_handler_btnTarget2(lv_event_t * e) {
  if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
    Serial.println("=== Store as Target2 Button Clicked ===");
    
    if (selectedDeviceIdx >= 0 && selectedDeviceIdx < bleDevices.size()) {
      BLEDeviceInfo device = bleDevices[selectedDeviceIdx];
      saveTarget2MAC(device.address);
      
      Serial.println("Saved as Target2: " + device.address);
      
      // Update status label
      if (connectionStatusLabel) {
        lv_label_set_text(connectionStatusLabel, ("Stored as Target2: " + device.name).c_str());
      }
      
      // Update stored devices screen if it exists
      updateStoredDevicesScreen();
    } else {
      Serial.println("ERROR: Please select a device from the list first!");
      if (connectionStatusLabel) {
        lv_label_set_text(connectionStatusLabel, "Status: Select device first");
      }
    }
  }
}

// ADDED: Event handler for auto-connect checkbox
static void event_handler_autoConnectCheckbox(lv_event_t * e) {
  if(lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
    bool checked = lv_obj_has_state(obj, LV_STATE_CHECKED);
    
    Serial.println("Auto-connect checkbox changed: " + String(checked ? "ENABLED" : "DISABLED"));
    
    // Save the state to NVS
    saveAutoConnectState(checked);
    
    // Update status label
    if (connectionStatusLabel) {
      lv_label_set_text(connectionStatusLabel, 
                       checked ? "Status: Auto-connect ENABLED" : "Status: Auto-connect DISABLED");
    }
  }
}

static void event_handler_btn1(lv_event_t * e) {
  if(lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t * obj = (lv_obj_t*) lv_event_get_target(e);
    bool state = lv_obj_has_state(obj, LV_STATE_CHECKED);
    
    if (state) {
      // Relay 1 ON: A00101A2
      bleSendHexString("A00101A2");
      Serial.println("Relay1: ON (A00101A2)");
    } else {
      // Relay 1 OFF: A00100A1
      bleSendHexString("A00100A1");
      Serial.println("Relay1: OFF (A00100A1)");
    }
  }
}

static void event_handler_btn2(lv_event_t * e) {
  if(lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t * obj = (lv_obj_t*) lv_event_get_target(e);
    bool state = lv_obj_has_state(obj, LV_STATE_CHECKED);
    
    if (state) {
      // Relay 2 ON: A00201A3
      bleSendHexString("A00201A3");
      Serial.println("Relay2: ON (A00201A3)");
    } else {
      // Relay 2 OFF: A00200A2
      bleSendHexString("A00200A2");
      Serial.println("Relay2: OFF (A00200A2)");
    }
  }
}

static void event_handler_btn3(lv_event_t * e) {
  if(lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t * obj = (lv_obj_t*) lv_event_get_target(e);
    bool state = lv_obj_has_state(obj, LV_STATE_CHECKED);
    
    if (state) {
      // Relay 3 ON: A00301A4
      bleSendHexString("A00301A4");
      Serial.println("Relay3: ON (A00301A4)");
    } else {
      // Relay 3 OFF: A00300A3
      bleSendHexString("A00300A3");
      Serial.println("Relay3: OFF (A00300A3)");
    }
  }
}

static void event_handler_btn4(lv_event_t * e) {
  if(lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t * obj = (lv_obj_t*) lv_event_get_target(e);
    bool state = lv_obj_has_state(obj, LV_STATE_CHECKED);
    
    if (state) {
      // Relay 4 ON: A00401A5
      bleSendHexString("A00401A5");
      Serial.println("Relay4: ON (A00401A5)");
    } else {
      // Relay 4 OFF: A00400A4
      bleSendHexString("A00400A4");
      Serial.println("Relay4: OFF (A00400A4)");
    }
  }
}

// ADDED: Event handler for Stored Devices button
static void event_handler_btnStoredDevices(lv_event_t * e) {
  if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
    Serial.println("=== Stored Devices Button Clicked ===");
    lv_screen_load(stored_devices_screen);
    updateStoredDevicesScreen();  // Update the screen with current status
  }
}

// ADDED: Event handler for Connect to Target1 button
static void event_handler_btnConnectTarget1(lv_event_t * e) {
  if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
    Serial.println("=== Connect to Target1 Button Clicked ===");
    bleAutoConnectDirect();
  }
}

// ADDED: Event handler for Connect to Target2 button
static void event_handler_btnConnectTarget2(lv_event_t * e) {
  if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
    Serial.println("=== Connect to Target2 Button Clicked ===");
    bleAutoConnectTarget2();
  }
}

// Screen creation - Stored Devices Screen
// Screen creation - Stored Devices Screen
void create_stored_devices_screen() {
  stored_devices_screen = lv_obj_create(NULL);
  lv_obj_set_size(stored_devices_screen, SCREEN_WIDTH, SCREEN_HEIGHT);
  
  // Set background color to BLACK
  lv_obj_set_style_bg_color(stored_devices_screen, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(stored_devices_screen, LV_OPA_COVER, LV_PART_MAIN);
  
  // Title
  lv_obj_t * title = lv_label_create(stored_devices_screen);
  lv_label_set_text(title, "Stored Devices");
  lv_obj_set_width(title, 300);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
  
  // Helper function to create blue buttons like main screen
  auto create_blue_button = [&](const char* label, lv_event_cb_t event_cb, lv_align_t align, int x_offset, int y_offset, int width = 80, int height = 35) -> lv_obj_t* {
    lv_obj_t * btn = lv_button_create(stored_devices_screen);
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn, align, x_offset, y_offset);
    lv_obj_set_size(btn, width, height);
    
    // Set blue background (0xFF0000 on your display)
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    
    // Set white text
    lv_obj_set_style_text_color(btn, lv_color_white(), LV_PART_MAIN);
    
    // Set font size 12
    lv_obj_set_style_text_font(btn, &lv_font_montserrat_12, LV_PART_MAIN);
    
    // Add border for better visibility
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    
    // Create label
    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_center(lbl);
    
    return btn;
  };
  
  // Helper function for navigation buttons (smaller blue buttons)
  auto create_nav_button = [&](const char* symbol, lv_event_cb_t event_cb, lv_align_t align, lv_obj_t* parent) -> lv_obj_t* {
    lv_obj_t * btn = lv_button_create(parent);
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_size(btn, 35, 35);
    lv_obj_align(btn, align, 0, 0);
    
    // Set blue background (0xFF0000 on your display)
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    
    // Set white text
    lv_obj_set_style_text_color(btn, lv_color_white(), LV_PART_MAIN);
    
    // Add border for better visibility
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    
    // Create label with symbol
    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text(lbl, symbol);
    lv_obj_center(lbl);
    
    return btn;
  };
  
  // Container for navigation buttons (75x35) at top right
  lv_obj_t * nav_container = lv_obj_create(stored_devices_screen);
  lv_obj_set_size(nav_container, 75, 35);
  lv_obj_align(nav_container, LV_ALIGN_TOP_RIGHT, -5, 10);  // Position at top right
  lv_obj_set_style_border_width(nav_container, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(nav_container, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_pad_all(nav_container, 0, LV_PART_MAIN);
  
  // Left button (Back to bluetooth screen) - 35x35 with blue style
  create_nav_button(LV_SYMBOL_LEFT, event_handler_btnBackStored, LV_ALIGN_LEFT_MID, nav_container);
  
  // Right button (Back to main screen) - 35x35 with blue style
  create_nav_button(LV_SYMBOL_RIGHT, event_handler_btnBack, LV_ALIGN_RIGHT_MID, nav_container);
  
  // Target1 Section - Adjusted Y positions since navigation container is at top
  lv_obj_t * target1Label = lv_label_create(stored_devices_screen);
  lv_label_set_text(target1Label, "Target1 Device:");
  lv_obj_set_width(target1Label, 200);
  lv_obj_align(target1Label, LV_ALIGN_TOP_LEFT, 10, 60);  // Moved down from 50 to 60
  lv_obj_set_style_text_font(target1Label, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(target1Label, lv_color_white(), LV_PART_MAIN);
  
  // Target1 MAC address
  lv_obj_t * target1MacLabel = lv_label_create(stored_devices_screen);
  lv_label_set_text(target1MacLabel, ("MAC: " + storedTarget1MAC).c_str());
  lv_obj_set_width(target1MacLabel, 220);
  lv_obj_align(target1MacLabel, LV_ALIGN_TOP_LEFT, 10, 85);  // Moved down from 75 to 85
  lv_obj_set_style_text_font(target1MacLabel, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_set_style_text_color(target1MacLabel, lv_color_white(), LV_PART_MAIN);
  
  // Target1 Status
  target1StatusLabel = lv_label_create(stored_devices_screen);
  lv_label_set_text(target1StatusLabel, "Target1: DISCONNECTED");
  lv_obj_set_width(target1StatusLabel, 220);
  lv_obj_align(target1StatusLabel, LV_ALIGN_TOP_LEFT, 10, 110);  // Moved down from 100 to 110
  lv_obj_set_style_text_font(target1StatusLabel, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_set_style_text_color(target1StatusLabel, lv_color_hex(0xFF0000), LV_PART_MAIN);
  
  // Target1 Connect Button - Blue style
  create_blue_button("Connect", event_handler_btnConnectTarget1, LV_ALIGN_TOP_RIGHT, -5, 60);
  
  // Target1 Disconnect Button - Blue style
  create_blue_button("Disconnect", event_handler_btnDisconnect, LV_ALIGN_TOP_RIGHT, -5, 100);
  
  // Separator line - Adjusted Y position
  lv_obj_t * line1 = lv_line_create(stored_devices_screen);
  static lv_point_precise_t line_points1[] = { {10, 150}, {230, 150} };  // Moved down from 140 to 150
  lv_line_set_points(line1, line_points1, 2);
  lv_obj_set_style_line_width(line1, 1, 0);
  lv_obj_set_style_line_color(line1, lv_color_hex(0x808080), 0);
  
  // Target2 Section - Adjusted Y positions
  lv_obj_t * target2Label = lv_label_create(stored_devices_screen);
  lv_label_set_text(target2Label, "Target2 Device:");
  lv_obj_set_width(target2Label, 200);
  lv_obj_align(target2Label, LV_ALIGN_TOP_LEFT, 10, 170);  // Moved down from 160 to 170
  lv_obj_set_style_text_font(target2Label, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(target2Label, lv_color_white(), LV_PART_MAIN);
  
  // Target2 MAC address
  lv_obj_t * target2MacLabel = lv_label_create(stored_devices_screen);
  String target2MacText = "MAC: " + storedTarget2MAC;
  if (storedTarget2MAC == "00:00:00:00:00:00") {
    target2MacText += " (PLACEHOLDER)";
  }
  lv_label_set_text(target2MacLabel, target2MacText.c_str());
  lv_obj_set_width(target2MacLabel, 220);
  lv_obj_align(target2MacLabel, LV_ALIGN_TOP_LEFT, 10, 195);  // Moved down from 185 to 195
  lv_obj_set_style_text_font(target2MacLabel, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_set_style_text_color(target2MacLabel, lv_color_white(), LV_PART_MAIN);
  
  // Target2 Status
  target2StatusLabel = lv_label_create(stored_devices_screen);
  lv_label_set_text(target2StatusLabel, "Target2: MAC NOT SET");
  lv_obj_set_width(target2StatusLabel, 220);
  lv_obj_align(target2StatusLabel, LV_ALIGN_TOP_LEFT, 10, 220);  // Moved down from 210 to 220
  lv_obj_set_style_text_font(target2StatusLabel, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_set_style_text_color(target2StatusLabel, lv_color_hex(0xFFA500), LV_PART_MAIN);
  
  // Target2 Connect Button - Blue style
  create_blue_button("Connect", event_handler_btnConnectTarget2, LV_ALIGN_TOP_RIGHT, -5, 165);
  
  // Target2 Disconnect Button - Blue style
  create_blue_button("Disconnect", event_handler_btnDisconnect, LV_ALIGN_TOP_RIGHT, -5, 205);
}

// Screen creation - Bluetooth Screen
// Screen creation - Bluetooth Screen
// Screen creation - Bluetooth Screen
void create_bluetooth_screen() {
  bluetooth_screen = lv_obj_create(NULL);
  lv_obj_set_size(bluetooth_screen, SCREEN_WIDTH, SCREEN_HEIGHT);
  
  // Set background color to BLACK
  lv_obj_set_style_bg_color(bluetooth_screen, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bluetooth_screen, LV_OPA_COVER, LV_PART_MAIN);
  
  // Title
  lv_obj_t * title = lv_label_create(bluetooth_screen);
  lv_label_set_text(title, "BLE 4.0 Client");
  lv_obj_set_width(title, 300);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
  
  // Device list - WIDE: 220px - Style with blue background and white text
  deviceList = lv_list_create(bluetooth_screen);
  lv_obj_set_size(deviceList, 220, 120);
  lv_obj_align(deviceList, LV_ALIGN_TOP_LEFT, 0, 40);
  
  // Style the list background to blue
  lv_obj_set_style_bg_color(deviceList, lv_color_hex(0xFF0000), LV_PART_MAIN); // Blue background
  lv_obj_set_style_bg_opa(deviceList, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(deviceList, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(deviceList, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_border_opa(deviceList, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(deviceList, 5, LV_PART_MAIN);
  
  // Style list text to white
  lv_obj_set_style_text_color(deviceList, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(deviceList, &lv_font_montserrat_12, LV_PART_MAIN);
  
  // Style scrollbar if needed
  lv_obj_set_style_bg_color(deviceList, lv_color_hex(0xFF6666), LV_PART_SCROLLBAR); // Lighter blue scrollbar
  lv_obj_set_style_bg_opa(deviceList, LV_OPA_COVER, LV_PART_SCROLLBAR);
  
  // Add initial text with white color
  lv_list_add_text(deviceList, "Devices will appear here");
  
  // Selected device label
  selectedDeviceLabel = lv_label_create(bluetooth_screen);
  lv_label_set_text(selectedDeviceLabel, "Selected: None");
  lv_obj_set_width(selectedDeviceLabel, 250);
  lv_obj_align(selectedDeviceLabel, LV_ALIGN_BOTTOM_MID, -30, -35);
  lv_obj_set_style_text_align(selectedDeviceLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(selectedDeviceLabel, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_set_style_text_color(selectedDeviceLabel, lv_color_white(), LV_PART_MAIN);
  
  // Connection status
  connectionStatusLabel = lv_label_create(bluetooth_screen);
  lv_label_set_text(connectionStatusLabel, "Status: Disconnected");
  lv_obj_set_width(connectionStatusLabel, 250);
  lv_obj_align(connectionStatusLabel, LV_ALIGN_BOTTOM_MID, -30, -10);
  lv_obj_set_style_text_font(connectionStatusLabel, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_set_style_text_align(connectionStatusLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(connectionStatusLabel, lv_color_white(), LV_PART_MAIN);
  
  // Helper function to create blue buttons like main screen
  auto create_blue_button = [&](const char* label, lv_event_cb_t event_cb, lv_align_t align, int x_offset, int y_offset, int width = 80, int height = 35) -> lv_obj_t* {
    lv_obj_t * btn = lv_button_create(bluetooth_screen);
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn, align, x_offset, y_offset);
    lv_obj_set_size(btn, width, height);
    
    // Set blue background (0xFF0000 on your display)
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    
    // Set white text
    lv_obj_set_style_text_color(btn, lv_color_white(), LV_PART_MAIN);
    
    // Set font size 12 (or 14 for larger if preferred)
    lv_obj_set_style_text_font(btn, &lv_font_montserrat_12, LV_PART_MAIN);
    
    // Add border for better visibility
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    
    // Create label
    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_center(lbl);
    
    return btn;
  };
  
  // Scan button - Blue style
  lv_obj_t * btnScan = create_blue_button("Scan", event_handler_btnScan, LV_ALIGN_TOP_RIGHT, -5, 5);
  
  // Target1 Store button - Blue style
  lv_obj_t * btnTarget1 = create_blue_button("Target1", event_handler_btnTarget1, LV_ALIGN_TOP_RIGHT, -5, 50);
  
  // Target2 Store button - Blue style
  lv_obj_t * btnTarget2 = create_blue_button("Target2", event_handler_btnTarget2, LV_ALIGN_TOP_RIGHT, -5, 95);
  
  // ADDED: Auto-connect checkbox and label
  // Container for checkbox and label
  lv_obj_t * autoConnectContainer = lv_obj_create(bluetooth_screen);
  lv_obj_set_size(autoConnectContainer, 220, 35);
  lv_obj_align(autoConnectContainer, LV_ALIGN_TOP_LEFT, 0, 165); // Below device list
  lv_obj_set_style_border_width(autoConnectContainer, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(autoConnectContainer, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_pad_all(autoConnectContainer, 0, LV_PART_MAIN);
  
  // Checkbox
  lv_obj_t * autoConnectCheckbox = lv_checkbox_create(autoConnectContainer);
  lv_checkbox_set_text(autoConnectCheckbox, "Auto-connect");
  lv_obj_add_event_cb(autoConnectCheckbox, event_handler_autoConnectCheckbox, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_align(autoConnectCheckbox, LV_ALIGN_LEFT_MID, 0, 0);
  
  // Set checkbox text color to white
  lv_obj_set_style_text_color(autoConnectCheckbox, lv_color_white(), LV_PART_MAIN);
  
  // Set initial state from NVS
  if (autoConnectEnabled) {
    lv_obj_add_state(autoConnectCheckbox, LV_STATE_CHECKED);
  }
  
  // Container for navigation buttons (75x35) at bottom right
  lv_obj_t * nav_container = lv_obj_create(bluetooth_screen);
  lv_obj_set_size(nav_container, 75, 35);
  lv_obj_align(nav_container, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
  lv_obj_set_style_border_width(nav_container, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(nav_container, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_pad_all(nav_container, 0, LV_PART_MAIN);
  
  // Helper function for navigation buttons (smaller blue buttons)
  auto create_nav_button = [&](const char* symbol, lv_event_cb_t event_cb, lv_align_t align) -> lv_obj_t* {
    lv_obj_t * btn = lv_button_create(nav_container);
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_size(btn, 35, 35);
    lv_obj_align(btn, align, 0, 0);
    
    // Set blue background (0xFF0000 on your display)
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    
    // Set white text
    lv_obj_set_style_text_color(btn, lv_color_white(), LV_PART_MAIN);
    
    // Add border for better visibility
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    
    // Create label with symbol
    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text(lbl, symbol);
    lv_obj_center(lbl);
    
    return btn;
  };
  
  // Left button (Back to main screen) - 35x35 with blue style
  create_nav_button(LV_SYMBOL_LEFT, event_handler_btnBack, LV_ALIGN_LEFT_MID);
  
  // Right button (Go to stored devices) - 35x35 with blue style
  create_nav_button(LV_SYMBOL_RIGHT, event_handler_btnStoredDevices, LV_ALIGN_RIGHT_MID);
}
// Screen creation - Main Screen
// Screen creation - Main Screen
void create_main_screen() {
  main_screen = lv_screen_active();
  
  // Set main screen background color to BLACK
  lv_obj_set_style_bg_color(main_screen, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(main_screen, LV_OPA_COVER, LV_PART_MAIN);
  
  // ADDED: Status indicator circle (20px diameter) at top right
  status_indicator = lv_obj_create(main_screen);
  lv_obj_set_size(status_indicator, 20, 20);  // 20px diameter
  
  // IMPORTANT: First set position, then apply styles
  lv_obj_align(status_indicator, LV_ALIGN_TOP_RIGHT, -10, 10);  // 10px from top and right edges
  
  // Apply circle styling
  lv_obj_set_style_radius(status_indicator, 10, LV_PART_MAIN);  // Half of 20px for perfect circle
  lv_obj_set_style_bg_color(status_indicator, lv_color_hex(0xFF0000), LV_PART_MAIN); // RED when not connected
  lv_obj_set_style_bg_opa(status_indicator, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(status_indicator, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(status_indicator, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_border_opa(status_indicator, LV_OPA_COVER, LV_PART_MAIN);
  
  // Remove any padding or default spacing
  lv_obj_set_style_pad_all(status_indicator, 0, LV_PART_MAIN);
  
  // Settings button - moved down slightly to make room for status indicator
  lv_obj_t * btnSet = lv_button_create(main_screen);
  lv_obj_add_event_cb(btnSet, event_handler_btnSet, LV_EVENT_CLICKED, NULL);
  lv_obj_align(btnSet, LV_ALIGN_TOP_MID, 0, 15);  // Changed from 10 to 15
  lv_obj_set_size(btnSet, 200, 40);
  // Set the text color to white
  lv_obj_set_style_text_color(btnSet, lv_color_white(), LV_PART_MAIN);
  // Set the background color to RED (0x00FF00 on your display)
  lv_obj_set_style_bg_opa(btnSet, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(btnSet, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
  
  // Add border to the title button (2px white border)
  lv_obj_set_style_border_width(btnSet, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(btnSet, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_border_opa(btnSet, LV_OPA_COVER, LV_PART_MAIN);
  
  lv_obj_t * lblSet = lv_label_create(btnSet);
  lv_label_set_text(lblSet, "POV BLE Controller");
  // Set font size 16 for the title button
  lv_obj_set_style_text_font(lblSet, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_center(lblSet);
  
  // Relay buttons
  const int btnWidth = 150;
  const int btnHeight = 70;
  
  // Helper function to create relay buttons with consistent styling
  auto create_relay_button = [&](const char* label, lv_event_cb_t event_cb, lv_align_t align, int x_offset, int y_offset) -> lv_obj_t* {
    lv_obj_t * btn = lv_button_create(main_screen);
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_align(btn, align, x_offset, y_offset);
    lv_obj_set_size(btn, btnWidth, btnHeight);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
    
    // Set blue background (0xFF0000 on your display)
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    
    // Set white text
    lv_obj_set_style_text_color(btn, lv_color_white(), LV_PART_MAIN);
    
    // Set pressed/checked state styling (lighter blue)
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x0000FF), LV_PART_MAIN | LV_STATE_CHECKED);
    
    // Add border for better visibility
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    
    // Create label with larger font for bolder appearance
    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    // Use larger font size 16 for bolder appearance
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_center(lbl);
    
    return btn;
  };
  
  // Button 1
  lv_obj_t * btn1 = create_relay_button("Relay1", event_handler_btn1, LV_ALIGN_CENTER, -80, -15);
  
  // Button 2
  lv_obj_t * btn2 = create_relay_button("Relay2", event_handler_btn2, LV_ALIGN_CENTER, 80, -15);
  
  // Button 3
  lv_obj_t * btn3 = create_relay_button("Relay3", event_handler_btn3, LV_ALIGN_CENTER, -80, 70);
  
  // Button 4
  lv_obj_t * btn4 = create_relay_button("Relay4", event_handler_btn4, LV_ALIGN_CENTER, 80, 70);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n==========================================");
  Serial.println("       POV BLE CONTROLLER STARTING");
  Serial.println("==========================================");
  
  // Load stored MACs from NVS - CORRECT PLACE
  loadStoredMACs();
  
  // Load auto-connect state from NVS
  loadAutoConnectState();
  
  // Initialize stored devices
  initStoredDevices();
  
  // Initialize BLE
  Serial.println("Initializing BLE Client...");
  BLEDevice::init("POV_BLE_Controller");
  
  // Create BLE scan
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  Serial.println("BLE initialized successfully");
  Serial.println("Device Name: POV_BLE_Controller");
  Serial.println("Stored Target1 MAC: " + storedTarget1MAC);
  Serial.println("Stored Target2 MAC: " + storedTarget2MAC);
  Serial.println("Auto-connect enabled: " + String(autoConnectEnabled ? "YES" : "NO"));
  Serial.println("Service UUID: " + String(SERVICE_UUID));
  Serial.println("Characteristic UUID: " + String(CHARACTERISTIC_UUID));
  
  // Initialize LVGL
  lv_init();
  lv_log_register_print_cb(log_print);
  
  // Initialize touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(2);
  
  // Initialize display
  lv_display_t * disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
  
  // Initialize input device
  lv_indev_t * indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchscreen_read);
  
  // Create screens
  create_main_screen();
  create_bluetooth_screen();
  create_stored_devices_screen();  // ADDED: Create stored devices screen
  
  // NEW: Try to auto-connect to target device (after UI is created)
  delay(1000); // Give BLE stack time to initialize
  if (autoConnectEnabled && storedTarget1MAC != "00:00:00:00:00:00") {
    bleAutoConnectDirect();
  }
  
  Serial.println("\nSetup Complete!");
  Serial.println("Instructions:");
  Serial.println("1. Go to Bluetooth screen (tap 'POV BLE Controller')");
  Serial.println("2. Tap 'Scan' to find BLE devices");
  Serial.println("3. Tap a device in the list to select it");
  Serial.println("4. Tap 'Target1' to store it as Target1 (for auto-connect)");
  Serial.println("5. Tap 'Target2' to store it as Target2");
  Serial.println("6. Use 'Auto-connect' checkbox to enable/disable auto-connect at boot");
  Serial.println("7. Tap 'Stored' to view and manage stored devices");
  Serial.println("8. Return to main screen and toggle relays");
  Serial.println("==========================================\n");
}

void loop() {
  lv_task_handler();
  lv_tick_inc(5);
  
  // Check BLE connection periodically
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 2000) {
    lastCheck = millis();
    
    if (isConnected && pClient && !pClient->isConnected()) {
      Serial.println("BLE connection lost!");
      bleDisconnect();
    }
  }
  
  delay(5);
}