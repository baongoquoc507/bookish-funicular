#pragma once
#include "../global.hpp"
#include "../display_utils.h"
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <LittleFS.h>

#define RF_SCK  4
#define RF_MISO 5
#define RF_MOSI 6

static bool rf_ready = false;

bool RF_InitModule(float mhz, bool tx)
{
    if (!rf_ready) {
        ELECHOUSE_cc1101.setSpiPin(RF_SCK, RF_MISO, RF_MOSI, CC1101_CS);
        ELECHOUSE_cc1101.setGDO0(CC1101_GDO0);
        rf_ready = true;
    }
    ELECHOUSE_cc1101.SpiStrobe(0x30); // SRES
    delayMicroseconds(100);
    ELECHOUSE_cc1101.Init();
    byte ver = ELECHOUSE_cc1101.SpiReadStatus(CC1101_VERSION);
    if (ver == 0x00 || ver == 0xFF) return false;
    ELECHOUSE_cc1101.setMHZ(mhz);
    ELECHOUSE_cc1101.setModulation(2); // OOK/ASK
    if (tx) {
        ELECHOUSE_cc1101.setSyncMode(0);
        ELECHOUSE_cc1101.setCrc(0);
        ELECHOUSE_cc1101.setPktFormat(3); // async TX
        ELECHOUSE_cc1101.SetTx();
        pinMode(CC1101_GDO0, OUTPUT);
        digitalWrite(CC1101_GDO0, LOW);
    } else {
        ELECHOUSE_cc1101.setPktFormat(0);
        ELECHOUSE_cc1101.SetRx();
        pinMode(CC1101_GDO0, INPUT);
    }
    return true;
}

void RF_DeinitModule()
{
    ELECHOUSE_cc1101.SpiStrobe(0x36); // SIDLE
    pinMode(CC1101_GDO0, OUTPUT);
    digitalWrite(CC1101_GDO0, LOW);
}

#define RAW_MAX 600
static int rawTimings[RAW_MAX];

int RF_CaptureRaw(int* timings, int maxn, uint32_t timeoutMs)
{
    unsigned long start = millis();
    while (digitalRead(CC1101_GDO0) == LOW) {
        if (millis() - start > timeoutMs) return 0;
        delay(1);
    }
    int n = 0, level = HIGH;
    unsigned long t0 = micros(), lastEdge = t0;
    while (n < maxn) {
        int lv = digitalRead(CC1101_GDO0);
        if (lv != level) {
            unsigned long now = micros();
            timings[n++] = (int)(now - t0);
            t0 = now; lastEdge = now; level = lv;
        }
        if (n > 0 && micros() - lastEdge > 10000) break;
    }
    return n;
}

void RF_SendRawTimings(const int* timings, int n, int repeat)
{
    for (int r = 0; r < repeat; r++) {
        int level = LOW;
        for (int i = 0; i < n; i++) {
            level = !level;
            digitalWrite(CC1101_GDO0, level);
            delayMicroseconds(timings[i] > 0 ? timings[i] : 1);
        }
        digitalWrite(CC1101_GDO0, LOW);
        delayMicroseconds(8000);
    }
}

void RF_ReadRaw()
{
    if (!RF_InitModule(433.92f, false)) {
        Display_PrintCentered("CC1101 not found!"); display.display(); delay(1500); return;
    }
    display.clearDisplay();
    Display_PrintCentered("Waiting for\nRF signal...\nCENTER: cancel");
    display.display();
    HaltTillRelease(BUTTON_CENTER);

    int n = 0;
    while (true) {
        n = RF_CaptureRaw(rawTimings, RAW_MAX, 200);
        if (n > 8) break;
        if (ReadButton(BUTTON_CENTER)) { RF_DeinitModule(); HaltTillRelease(BUTTON_CENTER); return; }
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);  display.print("Captured "); display.print(n); display.print(" pulses");
    display.setCursor(0, 16); display.print("CENTER: TX test");
    display.setCursor(0, 28); display.print("LEFT: save file");
    display.setCursor(0, 56); display.print("RIGHT: exit");
    display.display();

    while (true) {
        if (ReadButton(BUTTON_CENTER)) {
            HaltTillRelease(BUTTON_CENTER);
            if (RF_InitModule(433.92f, true)) {
                RF_SendRawTimings(rawTimings, n, 5);
                RF_DeinitModule();
            }
        }
        if (ReadButton(BUTTON_LEFT)) {
            HaltTillRelease(BUTTON_LEFT);
            File f = LittleFS.open("/raw_signal.txt", FILE_WRITE);
            if (f) {
                for (int i = 0; i < n; i++) { f.print(rawTimings[i]); f.print(','); }
                f.close();
            }
        }
        if (ReadButton(BUTTON_RIGHT)) {
            RF_DeinitModule();
            HaltTillRelease(BUTTON_RIGHT);
            return;
        }
        delay(10);
    }
}

void RF_Analyzer()
{
    if (!RF_InitModule(433.92f, false)) {
        Display_PrintCentered("CC1101 not found!"); display.display(); delay(1500); return;
    }
    HaltTillRelease(BUTTON_CENTER);
    uint32_t hist[21] = {0};
    int idx = 0;
    while (!ReadButton(BUTTON_CENTER)) {
        float mhz = 300.0f + idx * (628.0f / 20.0f); // 300..928 MHz
        ELECHOUSE_cc1101.setMHZ(mhz);
        delay(6);
        int raw = ELECHOUSE_cc1101.SpiReadStatus(0x34); // CC1101_RSSI
        int rssiDbm = ((raw >= 128) ? raw - 256 : raw) / 2 - 74;
        int v = constrain(-rssiDbm - 30, 0, 60);
        hist[idx] = (hist[idx] * 3 + (uint32_t)v) / 4;
        idx = (idx + 1) % 21;

        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0); display.print("RF ANALYZER");
        uint32_t maxv = 1;
        for (int i = 0; i < 21; i++) if (hist[i] > maxv) maxv = hist[i];
        for (int i = 0; i < 21; i++) {
            int h = map(hist[i], 0, maxv, 0, 48);
            display.fillRect(i * 6, 63 - h, 5, h, WHITE);
        }
        display.display();
    }
    HaltTillRelease(BUTTON_CENTER);
    RF_DeinitModule();
}

void RF_BruteCame()
{
    if (!RF_InitModule(433.92f, true)) {
        Display_PrintCentered("CC1101 not found!"); display.display(); delay(1500); return;
    }
    HaltTillRelease(BUTTON_CENTER);
    for (uint16_t code = 0; code < 4096; code++) {
        if (ReadButton(BUTTON_CENTER)) break;
        digitalWrite(CC1101_GDO0, LOW);  delayMicroseconds(11520);
        digitalWrite(CC1101_GDO0, HIGH); delayMicroseconds(320);
        for (int b = 11; b >= 0; b--) {
            bool one = (code >> b) & 1;
            if (one) { digitalWrite(CC1101_GDO0, LOW); delayMicroseconds(640);
                       digitalWrite(CC1101_GDO0, HIGH); delayMicroseconds(320); }
            else     { digitalWrite(CC1101_GDO0, LOW); delayMicroseconds(320);
                       digitalWrite(CC1101_GDO0, HIGH); delayMicroseconds(640); }
        }
        digitalWrite(CC1101_GDO0, LOW);
        delay(25);

        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);  display.print("BRUTE CAME 433.92");
        display.setCursor(0, 20); display.print("Code: "); display.print(code);
        display.setCursor(0, 32); display.print("Progress: ");
        display.print((int)(code * 100 / 4096)); display.print("%");
        display.setCursor(0, 56); display.print("CENTER: stop");
        display.display();
    }
    HaltTillRelease(BUTTON_CENTER);
    RF_DeinitModule();
}
