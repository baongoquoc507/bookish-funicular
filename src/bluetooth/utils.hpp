#pragma once

#include <NimBLEDevice.h>
#include <NimBLEUtils.h>
#include <NimBLEServer.h>
#include <NimBLEAdvertising.h>
#include <esp_arduino_version.h>

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define MAX_TX_POWER ESP_PWR_LVL_P21  // ESP32C3 ESP32C2 ESP32S3
#elif defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C6)
#define MAX_TX_POWER ESP_PWR_LVL_P20  // ESP32H2 ESP32C6
#else
#define MAX_TX_POWER ESP_PWR_LVL_P9   // Default
#endif

NimBLEAdvertising* advertising = nullptr;

void BLE_Setup()
{
    NimBLEDevice::init("");
    NimBLEDevice::setPower(MAX_TX_POWER);

    NimBLEServer* pServer = NimBLEDevice::createServer();
    advertising = pServer->getAdvertising();

    NimBLEAddress null_addr("fe:ed:c0:ff:ee:69", 1);
    advertising->setDeviceAddress(null_addr);
}
