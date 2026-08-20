#include "LabNetwork.h"
#ifdef LAB_MODE

#include <QNEthernet.h>
#include <SD.h>
#include <ArduinoJson.h>

using namespace qindesign::network;

static EthernetServer httpServer(80);
static EthernetUDP ntpUdp;

static String ipToString(const IPAddress &ip) {
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

void LabNetwork::begin(TimeSource &timeSource) {
  Serial.println(F("[Labor-Modus] Starte Ethernet (DHCP)..."));
  Ethernet.begin();

  unsigned long start = millis();
  while (Ethernet.localIP() == INADDR_NONE && millis() - start < 15000) {
    delay(200);
  }

  if (Ethernet.localIP() == INADDR_NONE) {
    Serial.println(F("[Labor-Modus][FEHLER] Keine IP per DHCP erhalten."));
    return;
  }

  Serial.print(F("[Labor-Modus] IP: "));
  Serial.println(ipAddress());

  httpServer.begin();
  ntpUdp.begin(2390);

  // Seriell-freier Betrieb: RTC automatisch gegen das Gateway abgleichen.
  // Ergebnis (Erfolg/Fehlschlag) ist ueber GET /status (Feld "time_source")
  // nachvollziehbar, auch ohne USB-Verbindung.
  String gw = gatewayIp();
  Serial.print(F("[Labor-Modus] Auto-NTP-Abgleich gegen Gateway "));
  Serial.println(gw);
  if (!ntpSync(gw.c_str(), timeSource)) {
    Serial.println(F("[Labor-Modus][FEHLER] Auto-NTP-Abgleich fehlgeschlagen -- RTC bleibt unkalibriert (rtc_local)."));
  }
}

bool LabNetwork::isLinked() const {
  return Ethernet.linkState();
}

String LabNetwork::ipAddress() const {
  return ipToString(Ethernet.localIP());
}

String LabNetwork::gatewayIp() const {
  return ipToString(Ethernet.gatewayIP());
}

// Minimaler SNTP-Client (RFC 4330): sendet ein NTP-Request-Paket und liest
// die Antwort aus. Kein Abgleich mehrerer Server, keine Drift-Korrektur --
// fuer die einmalige RTC-Kalibrierung auf der Werkbank ausreichend.
bool LabNetwork::ntpSync(const char *serverIp, TimeSource &timeSource) {
  IPAddress server;
  if (!server.fromString(serverIp)) {
    Serial.println(F("[Labor-Modus][FEHLER] Ungueltige NTP-Server-IP."));
    return false;
  }

  uint8_t packet[48] = {0};
  packet[0] = 0b00100011; // LI=0, VN=4, Mode=3 (Client)

  ntpUdp.beginPacket(server, 123);
  ntpUdp.write(packet, sizeof(packet));
  ntpUdp.endPacket();

  unsigned long start = millis();
  int size = 0;
  while (millis() - start < 3000) {
    size = ntpUdp.parsePacket();
    if (size >= 48) break;
    delay(10);
  }
  if (size < 48) {
    Serial.println(F("[Labor-Modus][FEHLER] Keine NTP-Antwort erhalten."));
    return false;
  }

  ntpUdp.read(packet, 48);
  uint32_t secsSince1900 =
      ((uint32_t)packet[40] << 24) | ((uint32_t)packet[41] << 16) |
      ((uint32_t)packet[42] << 8) | (uint32_t)packet[43];

  const uint32_t seventyYears = 2208988800UL; // Differenz 1900 -> 1970
  if (secsSince1900 < seventyYears) {
    Serial.println(F("[Labor-Modus][FEHLER] Unplausible NTP-Antwort."));
    return false;
  }

  unsigned long epochUtc = secsSince1900 - seventyYears;
  timeSource.setEpoch(epochUtc);
  Serial.print(F("[Labor-Modus] RTC per NTP gestellt: "));
  Serial.println(timeSource.nowIso8601Utc());
  return true;
}

static void sendStatusJson(EthernetClient &client, SessionController &session,
                            ChainLog &chainLog, TimeSource &timeSource) {
  StaticJsonDocument<384> doc;
  doc["mode"] = "lab";
  doc["link"] = Ethernet.linkState();
  doc["ip"] = ipToString(Ethernet.localIP());
  doc["gateway"] = ipToString(Ethernet.gatewayIP());
  doc["session_open"] = session.isOpen();
  if (session.isOpen()) {
    doc["case_id"] = session.caseId();
    doc["examiner_id"] = session.examinerId();
    doc["sequence"] = chainLog.sequence();
    doc["last_hash"] = chainLog.lastHashHex();
  }
  doc["time_source"] = timeSource.sourceLabel();
  doc["now_utc"] = timeSource.nowIso8601Utc();

  client.println(F("HTTP/1.0 200 OK"));
  client.println(F("Content-Type: application/json"));
  client.println(F("Connection: close"));
  client.println();
  serializeJson(doc, client);
  client.println();
}

static void sendCaseList(EthernetClient &client) {
  client.println(F("HTTP/1.0 200 OK"));
  client.println(F("Content-Type: text/html; charset=utf-8"));
  client.println(F("Connection: close"));
  client.println();
  client.println(F("<h1>Witness Bridge -- Labor-Modus</h1>"));
  client.println(F("<p><a href=\"/status\">/status</a></p><ul>"));

  File dir = SD.open("/cases");
  if (dir) {
    File entry = dir.openNextFile();
    while (entry) {
      String name = entry.name();
      client.print(F("<li><a href=\"/cases/"));
      client.print(name);
      client.print(F("\">"));
      client.print(name);
      client.println(F("</a></li>"));
      entry.close();
      entry = dir.openNextFile();
    }
    dir.close();
  }
  client.println(F("</ul>"));
}

static void sendCaseFile(EthernetClient &client, const String &name) {
  String path = "/cases/" + name;
  File f = SD.open(path.c_str(), FILE_READ);
  if (!f) {
    client.println(F("HTTP/1.0 404 Not Found"));
    client.println(F("Connection: close"));
    client.println();
    return;
  }
  client.println(F("HTTP/1.0 200 OK"));
  client.println(F("Content-Type: text/plain; charset=utf-8"));
  client.println(F("Connection: close"));
  client.println();
  while (f.available()) {
    client.write(f.read());
  }
  f.close();
}

void LabNetwork::poll(SessionController &session, ChainLog &chainLog, TimeSource &timeSource) {
  EthernetClient client = httpServer.accept();
  if (!client) return;

  String requestLine = client.readStringUntil('\n');
  requestLine.trim();
  // Restliche Header verwerfen -- es wird nur GET unterstuetzt, es gibt
  // keine schreibenden Endpunkte und keine Auswertung von Request-Bodies.
  while (client.connected() && client.available()) {
    String h = client.readStringUntil('\n');
    if (h.length() <= 1) break;
  }

  int sp1 = requestLine.indexOf(' ');
  int sp2 = requestLine.indexOf(' ', sp1 + 1);
  String method = (sp1 == -1) ? "" : requestLine.substring(0, sp1);
  String path = (sp1 == -1 || sp2 == -1) ? "/" : requestLine.substring(sp1 + 1, sp2);

  if (method != "GET") {
    client.println(F("HTTP/1.0 405 Method Not Allowed"));
    client.println(F("Connection: close"));
    client.println();
  } else if (path == "/status") {
    sendStatusJson(client, session, chainLog, timeSource);
  } else if (path.startsWith("/cases/")) {
    sendCaseFile(client, path.substring(String("/cases/").length()));
  } else {
    sendCaseList(client);
  }

  client.flush();
  delay(1);
  client.stop();
}

#endif // LAB_MODE
