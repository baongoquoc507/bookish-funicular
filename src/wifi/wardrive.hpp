#pragma once
#include "../global.hpp"
#include "../display_utils.h"
#include "utils.hpp"
#include <WiFi.h>
#include <LittleFS.h>

void WiFi_Wardrive()
{
    WiFi_Init();
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    File log = LittleFS.open("/wardrive.txt", FILE_APPEND);
    if (!log) { Display_PrintCentered("Cannot open log"); display.display(); delay(1500); return; }
    log.println("=== Wardrive session ===");
    int total = 0;

    HaltTillRelease(BUTTON_CENTER);
    while (!ReadButton(BUTTON_CENTER)) {
        int n = WiFi.scanNetworks(false, true);
        for (int i = 0; i < n; i++) {
            String line = WiFi.SSID(i) + "," + WiFi.BSSIDstr(i) + "," +
                          String(WiFi.channel(i)) + "," + WiFi.RSSI(i);
            log.println(line);
        }
        if (n > 0) total += n;
        log.flush();

        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);  display.print("WARDRIVING");
        display.setCursor(0, 20); display.print("This scan: "); display.print(n);
        display.setCursor(0, 32); display.print("Total: "); display.print(total);
        display.setCursor(0, 56); display.print("CENTER: stop");
        display.display();
        delay(3000);
    }
    HaltTillRelease(BUTTON_CENTER);
    log.close();
}
