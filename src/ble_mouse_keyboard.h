#ifndef BLE_MOUSE_KEYBOARD_H
#define BLE_MOUSE_KEYBOARD_H

#include <BLECharacteristic.h>
#include <BLEHIDDevice.h>
#include <BLEServer.h>
#include <string>

class BleMouseKeyboard : public BLEServerCallbacks {
public:
    BleMouseKeyboard(std::string deviceName, std::string deviceManufacturer, uint8_t batteryLevel = 100);
    void begin();
    bool isConnected() const;
    void move(signed char x, signed char y, signed char wheel = 0, signed char hWheel = 0);
    void typeText(const char* text);
    void onConnect(BLEServer* server) override;
    void onConnect(BLEServer* server, esp_ble_gatts_cb_param_t* param) override;
    void onDisconnect(BLEServer* server) override;
    void onDisconnect(BLEServer* server, esp_ble_gatts_cb_param_t* param) override;

private:
    std::string deviceName;
    std::string deviceManufacturer;
    uint8_t batteryLevel;
    uint8_t buttons = 0;
    bool connected = false;
    BLEHIDDevice* hid = nullptr;
    BLECharacteristic* inputMouse = nullptr;
    BLECharacteristic* inputKeyboard = nullptr;
};

#endif