#include "WiFi.h"
#include "wifiFunctions.h"
#include "oledFunc.h"
#include "ledFunc.h"
#include "secrets.h"
#include <lwip/inet.h>   // for INADDR_NONE (ESP32)
#include <time.h>
#include <WiFiClientSecure.h>

#define TEST_DHCP_ONLY 0   // set to 0 to restore DHCP->static reconnect

// external variables
extern String CONN_STATUS;
extern WiFiServer server;

static bool serverStarted = false;

bool wifiLinkUp() {
  return WiFi.status() == WL_CONNECTED &&
         WiFi.localIP() != IPAddress(0,0,0,0) &&
         WiFi.dnsIP()  != IPAddress(0,0,0,0);
}

bool timeValid() {
  return time(nullptr) > 1577836800; // 2020-01-01
}

bool syncTime(uint32_t timeoutMs = 15000) {
  Serial.println("Syncing time (NTP)...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    time_t now = time(nullptr);
    if (now > 1700000000) {
      Serial.print("Valid Epoch time: ");
      Serial.println((long)now);
      return true;
    }
    Serial.print(".");
    delay(500);
  }

  Serial.println("\nNTP sync FAILED");
  Serial.print("Invalid Epoch time: ");
  Serial.println((long)time(nullptr));
  return false;
}

void scanWifi() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Skipping WiFi scan: already connected");
        return;
    }

    displayPopupScreen("SCANNING WIFI","Finding Networks");
    updatePopupScreen();
    Serial.println("Starting WiFi scan...");

    int startTime = micros();
    int netCount = WiFi.scanNetworks();
    
    Serial.println("Scan complete");
    if (netCount == 0) {
        Serial.println("NO networks found");
    } else {
        Serial.print(netCount);
        Serial.println(" Networks found");
        for (int i = 0; i < netCount; ++i) {
            // Print SSID and RSSI for each network found
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
            delay(10);
        }
    }
    Serial.println("");
    Serial.println("Scan time: "+String(micros() - startTime));
}

bool waitForWifiStable(uint32_t stableMs, uint32_t timeoutMs) {
  Serial.println("Waiting for WiFi to stabilize...");

  uint32_t tStart = millis();
  uint32_t stableStart = 0;

  while (millis() - tStart < timeoutMs) {
    if (wifiLinkUp() && timeValid()) {
      if (stableStart == 0) stableStart = millis();
      if (millis() - stableStart >= stableMs) {
        Serial.println("WiFi stable.");
        return true;
      }
    } else {
      stableStart = 0; // reset stability window
    }
    delay(50);
  }

  Serial.println("WiFi NOT stable (timeout)");
  return false;
}


void connectFirebaseWifi() {
  displayPopupScreen("CONNECTING...", "Network: " + String(WIFI_SSID));
  updatePopupScreen();

  bool ok = connectDhcpThenStaticHost(WIFI_SSID, WIFI_PSW);

  if (ok) {
    CONN_STATUS = "CONN";
    Serial.println("WiFi connected with static host 186");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Keep your time sync here (don’t return early if you want server/UI to keep working)
    if (!syncTime()) {
      Serial.println("NTP failed; skipping Firebase (TLS may fail).");
    }

  } else {
    CONN_STATUS = "NOT CONNECTED";
    Serial.println("WiFi connect FAILED");
  }

  oledMain(MAIN_TIMEOUT_SEC);
  ledOff();
}

bool connectDhcpThenStaticHost(const char* ssid, const char* password) {
    int host = EDGE_HOST_ID;

  // If already connected with correct static IP, do nothing
  if (WiFi.status() == WL_CONNECTED) {
    IPAddress ip = WiFi.localIP();
    if (ip[3] == host) {
      return true;
    }
  }

  Serial.println("WiFi: DHCP -> static host reconnect");

  WiFi.disconnect(true);
  delay(250);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  // -----------------------------
  // Step 1: Connect using DHCP
  // -----------------------------
  WiFi.begin(ssid, password);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(200);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi DHCP connect failed");
    return false;
  }

  // Wait until DHCP options are populated
  t0 = millis();
  while ((WiFi.gatewayIP() == IPAddress(0,0,0,0) ||
          WiFi.gatewayIP() == IPAddress(255,255,255,255) ||
          WiFi.subnetMask() == IPAddress(0,0,0,0)) &&
         millis() - t0 < 5000) {
    delay(200);
  }

  IPAddress gw   = WiFi.gatewayIP();
  IPAddress mask = WiFi.subnetMask();
  IPAddress dns  = WiFi.dnsIP();

  if (gw == IPAddress(0,0,0,0) || mask == IPAddress(0,0,0,0)) {
    Serial.println("DHCP did not provide valid network info");
    return false;
  }

#if TEST_DHCP_ONLY
  Serial.println("TEST: staying on DHCP (skipping static reconnect)");
  return true;   // WiFi is connected via DHCP; caller can proceed to NTP/Firebase
#endif

  // -----------------------------
  // Step 2: Build static IP
  // -----------------------------
  IPAddress staticIP;

  // Common home LAN case (/24)
  if (mask == IPAddress(255,255,255,0)) {
    staticIP = IPAddress(gw[0], gw[1], gw[2], host);
  } else {
    // Fallback: derive network portion
    IPAddress net(gw[0] & mask[0],
                  gw[1] & mask[1],
                  gw[2] & mask[2],
                  gw[3] & mask[3]);

    staticIP = IPAddress(net[0], net[1], net[2], host);

    Serial.print("WARN: non-/24 subnet detected: ");
    Serial.println(mask);
  }

  Serial.print("DHCP IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Gateway: ");
  Serial.println(gw);
  Serial.print("Subnet: ");
  Serial.println(mask);
  Serial.print("DNS: ");
  Serial.println(dns);
  Serial.print("Static IP: ");
  Serial.println(staticIP);

  // -----------------------------
  // Step 3: Reconnect using static IP
  // -----------------------------
  WiFi.disconnect(true);
  delay(250);

  WiFi.config(staticIP, gw, mask, dns);
  WiFi.begin(ssid, password);

  t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(200);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Static reconnect failed");
    return false;
  }

  Serial.print("WiFi connected with static IP: ");
  Serial.println(WiFi.localIP());
  return (WiFi.localIP() == staticIP);
}

bool ensureServerStarted() {
  // If WiFi dropped, allow restart later
  if (serverStarted && WiFi.status() != WL_CONNECTED) {
    serverStarted = false;
  }

  // Only start once we have a real IP
  IPAddress ip = WiFi.localIP();
  bool hasIP = (ip != IPAddress(0,0,0,0) && ip != IPAddress(255,255,255,255));

  if (!serverStarted && WiFi.status() == WL_CONNECTED && hasIP) {
    server.begin();
    serverStarted = true;
    Serial.println("SERVER STARTED");
    Serial.print("Open: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/");
  }

  return serverStarted;
}


bool wifiOK() {
    if (WiFi.status() == WL_CONNECTED  &&
        WiFi.localIP() != IPAddress(0,0,0,0)) {
    CONN_STATUS = "CONN";
    return true;
    }

    CONN_STATUS = "OFF";
    return false;
}

bool dnsOK() {
  return WiFi.dnsIP() != IPAddress(0,0,0,0);
}

bool internetOK() {
  WiFiClient c;
  int timeout = 2000;
  c.setTimeout(timeout / 1000);
  return c.connect("1.1.1.1", 80);
}

bool internetOK443() {
  WiFiClientSecure c;
  int timeout = 3000;
  c.setInsecure();                 // reachability test only
  c.setTimeout(timeout / 1000);    // seconds
  bool ok = c.connect("www.google.com", 443);
  c.stop();
  return ok;
}
