#pragma once
#include "../global.hpp"
#include "../menu.hpp"
#include "../display_utils.h"
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <NimBLEServer.h>
#include <NimBLEAdvertising.h>

static const uint8_t hidReportMapKeyboard[] = {
  0x05,0x01,0x09,0x06,0xA1,0x01,0x85,0x01,0x05,0x07,0x19,0xE0,0x29,0xE7,
  0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x08,0x81,0x02,0x95,0x01,0x75,0x08,0x81,0x01,
  0x95,0x05,0x75,0x01,0x05,0x08,0x19,0x01,0x29,0x05,0x91,0x02,
  0x95,0x01,0x75,0x03,0x91,0x01,0x95,0x06,0x75,0x08,0x15,0x00,0x25,0x65,0x05,0x07,0x19,0x00,0x29,0x65,0x81,0x00,0xC0
};

struct BleHidState {
    NimBLEHIDDevice* hid = nullptr;
    NimBLECharacteristic* input = nullptr;
    volatile bool connected = false;
};
static BleHidState gBle;

class HidServerCB : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* s, NimBLEConnInfo& ci) override {
        gBle.connected = true;
        NimBLEDevice::stopAdvertising();
    }
    void onDisconnect(NimBLEServer* s, NimBLEConnInfo& ci) override {
        gBle.connected = false;
        s->startAdvertising();
    }
};

void BleHid_Start(const char* name, const uint8_t* map, size_t mapLen, uint8_t reportId)
{
    NimBLEDevice::init(name);
    NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND);
    NimBLEServer* server = NimBLEDevice::createServer();
    server->setCallbacks(new HidServerCB());
    gBle.hid = new NimBLEHIDDevice(server);
    gBle.hid->setReportMap((uint8_t*)map, mapLen);
    gBle.hid->startServices();
    gBle.input = gBle.hid->inputReport(reportId);
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->setAppearance(HID_KEYBOARD);
    adv->addServiceUUID(gBle.hid->hidService()->getUUID());
    adv->start();
}

void BleHid_Stop()
{
    NimBLEDevice::deinit(true);
    gBle.connected = false;
    gBle.hid = nullptr;
    gBle.input = nullptr;
}

void BleHid_WaitConnect(const char* line2)
{
    while (!gBle.connected) {
        if (ReadButton(BUTTON_CENTER)) return; // cho phep huy
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);  display.print("BLE HID");
        display.setCursor(0, 16); display.print(line2);
        display.setCursor(0, 32); display.print("Waiting connect...");
        display.setCursor(0, 56); display.print("CENTER: cancel");
        display.display();
        delay(300);
    }
}

void BleKey_Send(uint8_t mod, uint8_t key)
{
    if (!gBle.connected || !gBle.input) return;
    uint8_t down[8] = {mod, 0, key, 0, 0, 0, 0, 0};
    gBle.input->setValue(down, 8); gBle.input->notify(); delay(12);
    uint8_t up[8] = {0};
    gBle.input->setValue(up, 8);   gBle.input->notify(); delay(12);
}

uint8_t AsciiToKey(char c, uint8_t& mod)
{
    mod = 0;
    if (c >= 'a' && c <= 'z') return 0x04 + (c - 'a');
    if (c >= 'A' && c <= 'Z') { mod = 2; return 0x04 + (c - 'A'); }
    if (c >= '1' && c <= '9') return 0x1E + (c - '1');
    if (c == '0') return 0x27;
    if (c == ' ')  return 0x2C;
    if (c == '\n') return 0x28;
    if (c == '.')  return 0x37;
    if (c == ',')  return 0x36;
    if (c == '/')  return 0x38;
    if (c == '-')  return 0x2D;
    if (c == '@')  { mod = 2; return 0x1F; }
    if (c == ':')  { mod = 2; return 0x33; }
    if (c == '!')  { mod = 2; return 0x1E; }
    if (c == '?')  { mod = 2; return 0x38; }
    return 0;
}

void BleType_Text(const String& text)
{
    for (unsigned i = 0; i < text.length(); i++) {
        uint8_t mod; uint8_t k = AsciiToKey(text[i], mod);
        if (k) BleKey_Send(mod, k);
    }
}

void BleKeyboard_Menu()
{
    Menu m;
    bool running = true;
    const char* scripts[] = {
        "Hello from Pocket Puter!\n",
        "I can type on your phone :-)\n",
        "This is a BLE keyboard demo\n"
    };
    for (int i = 0; i < 3; i++) {
        m.AddItem(MenuItem("Script " + String(i + 1), [i, scripts]() {
            BleHid_Start("PocketPuter KB", hidReportMapKeyboard, sizeof(hidReportMapKeyboard), 1);
            BleHid_WaitConnect("Keyboard mode");
            if (gBle.connected) BleType_Text(scripts[i]);
            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(0, 0);  display.print(gBle.connected ? "Sent script!" : "Cancelled");
            display.setCursor(0, 20); display.print("CENTER: stop BLE");
            display.display();
            HaltTillRelease(BUTTON_CENTER);
            while (!ReadButton(BUTTON_CENTER)) delay(10);
            HaltTillRelease(BUTTON_CENTER);
            BleHid_Stop();
        }));
    }
    m.AddItem(MenuItem("[Back]", [&](){ running = false; }));
    m.Revive();
    while (running) { m.HandleButtons(); m.Render(); }
}

void BleMouse_Run()
{
    static const uint8_t combo[] = {
      0x05,0x01,0x09,0x06,0xA1,0x01,0x85,0x01,0x05,0x07,0x19,0xE0,0x29,0xE7,
      0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x08,0x81,0x02,0x95,0x01,0x75,0x08,0x81,0x01,
      0x95,0x05,0x75,0x01,0x05,0x08,0x19,0x01,0x29,0x05,0x91,0x02,
      0x95,0x01,0x75,0x03,0x91,0x01,0x95,0x06,0x75,0x08,0x15,0x00,0x25,0x65,0x05,0x07,0x19,0x00,0x29,0x65,0x81,0x00,0xC0,
      0x05,0x01,0x09,0x02,0xA1,0x01,0x85,0x02,0x09,0x01,0xA1,0x00,
      0x05,0x09,0x19,0x01,0x29,0x03,0x15,0x00,0x25,0x01,0x95,0x03,0x75,0x01,0x81,0x02,
      0x95,0x01,0x75,0x05,0x81,0x01,
      0x05,0x01,0x09,0x30,0x09,0x31,0x09,0x38,0x15,0x81,0x25,0x7F,
      0x75,0x08,0x95,0x03,0x81,0x06,0xC0,0xC0
    };
    BleHid_Start("PocketPuter Mouse", combo, sizeof(combo), 2);
    BleHid_WaitConnect("Mouse mode");
    if (!gBle.connected) { BleHid_Stop(); return; }

    const int8_t dx8[8] = {10, 7, 0, -7, -10, -7, 0, 7};
    const int8_t dy8[8] = {0, 7, 10, 7, 0, -7, -10, -7};
    int dir = 0;
    bool clicking = false;
    unsigned long pressStart = 0;

    HaltTillRelease(BUTTON_CENTER);
    while (true) {
        if (ReadButton(BUTTON_LEFT))  { dir = (dir + 7) % 8; delay(180); }
        if (ReadButton(BUTTON_RIGHT)) { dir = (dir + 1) % 8; delay(180); }
        if (ReadButton(BUTTON_CENTER)) {
            if (pressStart == 0) pressStart = millis();
            if (millis() - pressStart > 800) break; // giu CENTER 0.8s = thoat
            clicking = true;
        } else { pressStart = 0; clicking = false; }

        if (gBle.connected && gBle.input) {
            uint8_t rep[3] = { (uint8_t)(clicking ? 1 : 0), (uint8_t)dx8[dir], (uint8_t)dy8[dir] };
            gBle.input->setValue(rep, 3);
            gBle.input->notify();
        }
        delay(25);

        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);  display.print(gBle.connected ? "BLE MOUSE" : "DISCONNECTED");
        display.setCursor(0, 20); display.print("LEFT/RIGHT: huong");
        display.setCursor(0, 32); display.print("CENTER: click");
        display.setCursor(0, 44); display.print("Giu CENTER: thoat");
        display.display();
    }
    HaltTillRelease(BUTTON_CENTER);
    BleHid_Stop();
}
