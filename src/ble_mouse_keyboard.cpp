#include "ble_mouse_keyboard.h"

#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLESecurity.h>
#include <HIDTypes.h>
#include <cstring>

static const uint8_t HID_REPORT_DESCRIPTOR[] = {
    USAGE_PAGE(1), 0x01,
    USAGE(1), 0x02,
    COLLECTION(1), 0x01,
    REPORT_ID(1), 0x02,
    USAGE(1), 0x01,
    COLLECTION(1), 0x00,
    USAGE_PAGE(1), 0x09,
    USAGE_MINIMUM(1), 0x01,
    USAGE_MAXIMUM(1), 0x05,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x01,
    REPORT_SIZE(1), 0x01,
    REPORT_COUNT(1), 0x05,
    HIDINPUT(1), 0x02,
    REPORT_SIZE(1), 0x03,
    REPORT_COUNT(1), 0x01,
    HIDINPUT(1), 0x03,
    USAGE_PAGE(1), 0x01,
    USAGE(1), 0x30,
    USAGE(1), 0x31,
    USAGE(1), 0x38,
    LOGICAL_MINIMUM(1), 0x81,
    LOGICAL_MAXIMUM(1), 0x7f,
    REPORT_SIZE(1), 0x08,
    REPORT_COUNT(1), 0x03,
    HIDINPUT(1), 0x06,
    USAGE_PAGE(1), 0x0c,
    USAGE(2), 0x38, 0x02,
    LOGICAL_MINIMUM(1), 0x81,
    LOGICAL_MAXIMUM(1), 0x7f,
    REPORT_SIZE(1), 0x08,
    REPORT_COUNT(1), 0x01,
    HIDINPUT(1), 0x06,
    END_COLLECTION(0),
    END_COLLECTION(0),

    USAGE_PAGE(1), 0x01,
    USAGE(1), 0x06,
    COLLECTION(1), 0x01,
    REPORT_ID(1), 0x01,
    USAGE_PAGE(1), 0x07,
    USAGE_MINIMUM(1), 0xe0,
    USAGE_MAXIMUM(1), 0xe7,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x01,
    REPORT_SIZE(1), 0x01,
    REPORT_COUNT(1), 0x08,
    HIDINPUT(1), 0x02,
    REPORT_COUNT(1), 0x01,
    REPORT_SIZE(1), 0x08,
    HIDINPUT(1), 0x01,
    REPORT_COUNT(1), 0x06,
    REPORT_SIZE(1), 0x08,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x65,
    USAGE_PAGE(1), 0x07,
    USAGE_MINIMUM(1), 0x00,
    USAGE_MAXIMUM(1), 0x65,
    HIDINPUT(1), 0x00,
    END_COLLECTION(0)
};

static void logBleAuthentication(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    if (event == ESP_GAP_BLE_AUTH_CMPL_EVT) {
        const esp_ble_auth_cmpl_t& result = param->ble_security.auth_cmpl;
        Serial.printf("BLE authentication %s, reason=0x%02X\n",
                      result.success ? "succeeded" : "failed", result.fail_reason);
    }
}

static void logGattAccess(esp_gatts_cb_event_t event, esp_gatt_if_t,
                          esp_ble_gatts_cb_param_t* param) {
    if (event == ESP_GATTS_READ_EVT) {
        Serial.printf("GATT read handle=0x%04X offset=%u\n",
                      param->read.handle, param->read.offset);
    } else if (event == ESP_GATTS_WRITE_EVT) {
        Serial.printf("GATT write handle=0x%04X length=%u data=",
                      param->write.handle, param->write.len);
        for (uint16_t i = 0; i < param->write.len; ++i) {
            Serial.printf("%02X", param->write.value[i]);
        }
        Serial.println();
    }
}

BleMouseKeyboard::BleMouseKeyboard(std::string name, std::string manufacturer, uint8_t level)
    : deviceName(name), deviceManufacturer(manufacturer), batteryLevel(level) {}

void BleMouseKeyboard::begin() {
    BLEDevice::init(deviceName);
    BLEDevice::setCustomGapHandler(logBleAuthentication);
    BLEDevice::setCustomGattsHandler(logGattAccess);
    BLEServer* server = BLEDevice::createServer();
    server->setCallbacks(this);

    hid = new BLEHIDDevice(server);
    inputMouse = hid->inputReport(2);
    inputKeyboard = hid->inputReport(1);
    hid->manufacturer()->setValue(deviceManufacturer);
    hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
    hid->hidInfo(0x00, 0x02);
    hid->reportMap(const_cast<uint8_t*>(HID_REPORT_DESCRIPTOR), sizeof(HID_REPORT_DESCRIPTOR));
    hid->startServices();
    BLEDescriptor* mouseCccd = inputMouse->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    BLEDescriptor* mouseReference = inputMouse->getDescriptorByUUID(BLEUUID((uint16_t)0x2908));
    BLEDescriptor* keyboardCccd = inputKeyboard->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    BLEDescriptor* keyboardReference = inputKeyboard->getDescriptorByUUID(BLEUUID((uint16_t)0x2908));
    Serial.printf("HID mouse report=0x%04X CCCD=0x%04X ref=0x%04X\n",
                  inputMouse->getHandle(), mouseCccd->getHandle(), mouseReference->getHandle());
    Serial.printf("HID keyboard report=0x%04X CCCD=0x%04X ref=0x%04X\n",
                  inputKeyboard->getHandle(), keyboardCccd->getHandle(), keyboardReference->getHandle());
    hid->setBatteryLevel(batteryLevel);

    BLESecurity* security = new BLESecurity();
    security->setAuthenticationMode(ESP_LE_AUTH_BOND);
    security->setCapability(ESP_IO_CAP_NONE);
    security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    security->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    BLEAdvertising* advertising = server->getAdvertising();
    advertising->setAppearance(GENERIC_HID);
    advertising->addServiceUUID(hid->hidService()->getUUID());
    advertising->start();
}

bool BleMouseKeyboard::isConnected() const {
    return connected;
}

void BleMouseKeyboard::move(signed char x, signed char y, signed char wheel, signed char hWheel) {
    if (!connected || inputMouse == nullptr) return;

    uint8_t report[5] = {buttons, static_cast<uint8_t>(x), static_cast<uint8_t>(y),
                         static_cast<uint8_t>(wheel), static_cast<uint8_t>(hWheel)};
    inputMouse->setValue(report, sizeof(report));
    inputMouse->notify();
}

static bool keyForAscii(char character, uint8_t& modifiers, uint8_t& key) {
    modifiers = 0;
    if (character >= 'a' && character <= 'z') {
        key = 0x04 + character - 'a';
        return true;
    }
    if (character >= 'A' && character <= 'Z') {
        modifiers = 0x02;
        key = 0x04 + character - 'A';
        return true;
    }
    if (character >= '1' && character <= '9') {
        key = 0x1e + character - '1';
        return true;
    }
    if (character == '0') { key = 0x27; return true; }
    if (character == ' ') { key = 0x2c; return true; }
    if (character == '\n' || character == '\r') { key = 0x28; return true; }

    const char* shiftedDigits = "!@#$%^&*()";
    const char* plainSymbols = "-=[]\\;'`,./";
    const char* shiftedSymbols = "_+{}|:\"~<>?";
    if (const char* match = strchr(shiftedDigits, character)) {
        modifiers = 0x02;
        uint8_t digit = match - shiftedDigits;
        key = digit == 9 ? 0x27 : 0x1e + digit;
        return true;
    }
    if (const char* match = strchr(plainSymbols, character)) {
        const uint8_t usages[] = {0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38};
        key = usages[match - plainSymbols];
        return true;
    }
    if (const char* match = strchr(shiftedSymbols, character)) {
        const uint8_t usages[] = {0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38};
        modifiers = 0x02;
        key = usages[match - shiftedSymbols];
        return true;
    }
    return false;
}

void BleMouseKeyboard::typeText(const char* text) {
    if (!connected || inputKeyboard == nullptr || text == nullptr) return;

    for (const char* current = text; *current; ++current) {
        uint8_t modifiers;
        uint8_t key;
        if (!keyForAscii(*current, modifiers, key)) continue;

        uint8_t report[8] = {modifiers, 0, key, 0, 0, 0, 0, 0};
        inputKeyboard->setValue(report, sizeof(report));
        inputKeyboard->notify();
        delay(10);
        memset(report, 0, sizeof(report));
        inputKeyboard->setValue(report, sizeof(report));
        inputKeyboard->notify();
        delay(10);
    }
}

void BleMouseKeyboard::onConnect(BLEServer*) {
    connected = true;
}

void BleMouseKeyboard::onConnect(BLEServer*, esp_ble_gatts_cb_param_t*) {
    connected = true;
}

void BleMouseKeyboard::onDisconnect(BLEServer* server) {
    connected = false;
    server->startAdvertising();
}

void BleMouseKeyboard::onDisconnect(BLEServer* server, esp_ble_gatts_cb_param_t* param) {
    connected = false;
    server->startAdvertising();
    Serial.printf("BLE disconnected, reason=0x%02X\n", param->disconnect.reason);
}