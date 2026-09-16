#pragma once

#include "utils.hpp"
#include "../display_utils.h"
#include "../menu.hpp"
#include <NimBLEScan.h>
#include <NimBLEAdvertisedDevice.h>

static std::vector<String> g_found;
static NimBLEScan* pBLEScan = nullptr;

class MyAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
public:
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        String addr = advertisedDevice->getAddress().toString().c_str();
        String name = advertisedDevice->getName().c_str();
        int rssi = advertisedDevice->getRSSI();

        String entry;
        if (name.length() > 0) {
            entry = name + " [" + addr + "] RSSI:" + String(rssi);
        } else {
            entry = addr + " RSSI:" + String(rssi);
        }

        if (advertisedDevice->haveManufacturerData()) {
            std::string mfg = advertisedDevice->getManufacturerData();
            entry += " MFG:";
            for (size_t i = 0; i < mfg.length(); i++) {
                char buf[4];
                snprintf(buf, sizeof(buf), "%02X", static_cast<uint8_t>(mfg[i]));
                entry += buf;
            }
        }

        if (std::find(g_found.begin(), g_found.end(), entry) == g_found.end()) {
            g_found.push_back(entry);
        }
    }
};

void BLE_Scan()
{
    g_found.clear();

    display.clearDisplay();
    Display_PrintCentered("Scanning...");
    display.display();

    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), false);
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    while (!ReadButton(BUTTON_CENTER)) {
        g_found.clear();
        pBLEScan->start(3, false);
        display.clearDisplay();
        Display_PrintCentered("Scanning...\nFound: %d", static_cast<int>(g_found.size()));
        display.display();
        pBLEScan->clearResults();
        delay(10);
    }

    pBLEScan->stop();
    NimBLEDevice::deinit(true);

    Menu menu;
    for (auto& dev : g_found) {
        menu.AddItem(MenuItem(dev, [](){}));
    }
    menu.AddItem(MenuItem("[Exit]", [&](){ g_found.clear(); }));
    menu.Revive();
    menu.Render();

    HaltTillRelease(BUTTON_CENTER);

    bool running = true;
    while (running) {
        menu.HandleButtons();
        menu.Render();
        if (ReadButton(BUTTON_CENTER)) {
            HaltTillRelease(BUTTON_CENTER);
            running = false;
        }
        delay(10);
    }
}
