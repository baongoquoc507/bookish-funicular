#pragma once
#include "../global.hpp"
#include "../display_utils.h"
#include "utils.hpp"
#include <WiFi.h>
#include "esp_wifi.h"

static volatile uint32_t pkt_count = 0;
static void IRAM_ATTR pkt_sniffer(void* buf, wifi_promiscuous_pkt_type_t type) { pkt_count++; }

void WiFi_PacketMonitor()
{
    WiFi_Init();
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    uint8_t chan = 1;
    uint32_t history[21] = {0};

    esp_wifi_set_promiscuous_rx_cb(&pkt_sniffer);
    wifi_promiscuous_filter_t filt = { .filter_mask = WIFI_PROMIS_FILTER_MASK_ALL };
    esp_wifi_set_promiscuous_filter(&filt);
    esp_wifi_set_channel(chan, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(true);

    HaltTillRelease(BUTTON_CENTER);
    unsigned long lastSample = 0;
    while (!ReadButton(BUTTON_CENTER)) {
        if (ReadButton(BUTTON_LEFT))  { chan = chan > 1  ? chan - 1 : 13; esp_wifi_set_channel(chan, WIFI_SECOND_CHAN_NONE); delay(180); }
        if (ReadButton(BUTTON_RIGHT)) { chan = chan < 13 ? chan + 1 : 1;  esp_wifi_set_channel(chan, WIFI_SECOND_CHAN_NONE); delay(180); }

        if (millis() - lastSample > 200) {
            lastSample = millis();
            uint32_t c = pkt_count; pkt_count = 0;
            for (int i = 0; i < 20; i++) history[i] = history[i + 1];
            history[20] = c;

            uint32_t maxv = 1;
            for (int i = 0; i < 21; i++) if (history[i] > maxv) maxv = history[i];

            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(0, 0); display.print("PKTS CH:"); display.print(chan);
            display.setCursor(90, 0); display.print(c);
            for (int i = 0; i < 21; i++) {
                int h = map(history[i], 0, maxv, 0, 50);
                display.fillRect(i * 6, 63 - h, 5, h, WHITE);
            }
            display.display();
        }
        delay(10);
    }
    HaltTillRelease(BUTTON_CENTER);
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(NULL);
    WiFi.mode(WIFI_OFF);
}
