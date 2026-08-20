#include <Arduino.h>
#include <SD.h>

#include "SessionController.h"
#include "ChainLog.h"
#include "TimeSource.h"
#ifdef LAB_MODE
#include "LabNetwork.h"
#endif

// Firmware-Grundgeruest (Roadmap Schritt 1): Sitzungssteuerung, Hash-Chain,
// SD-Schreibpfad. Eingabe aktuell ueber die serielle Konsole als Platzhalter
// fuer den spaeteren USB-Host-Barcode-Scanner (siehe README, Hardware).
//
// LAB_MODE (env:teensy41_lab) ergaenzt Ethernet, NTP-Client und einen
// rein lesenden Status-Webserver -- nur fuer Werkbank/Kalibrierung,
// siehe README, "Labor-Modus".

SessionController session;
ChainLog chainLog;
TimeSource timeSource;
#ifdef LAB_MODE
LabNetwork labNetwork;
#endif

void printHelp() {
  Serial.println(F("Witness Bridge -- Firmware-Grundgeruest"));
  Serial.println(F("Befehle:"));
  Serial.println(F("  OPEN <case_id> <examiner_id>   Sitzung eroeffnen"));
  Serial.println(F("  HASH <sha256> <artefakt>       Ergebnis-Hash erfassen"));
  Serial.println(F("  NOTE <text>                    Freitext-Ereignis anhaengen"));
  Serial.println(F("  CLOSE                          Sitzung beenden"));
  Serial.println(F("  STATUS                         aktuellen Zustand anzeigen"));
  Serial.println(F("  LIST                           Fall-Log-Dateien auf der SD-Karte auflisten"));
  Serial.println(F("  DUMP <case_id>                 Log-Datei eines Falls ausgeben"));
#ifdef LAB_MODE
  Serial.println(F("  NTPSYNC <server_ip>            RTC per NTP stellen (Labor-Modus)"));
  Serial.println(F("  NETINFO                        Ethernet-Status/IP anzeigen (Labor-Modus)"));
#endif
  Serial.println(F("  HELP                           diese Uebersicht"));
}

void handleCommand(const String &line) {
  int sp = line.indexOf(' ');
  String cmd = sp == -1 ? line : line.substring(0, sp);
  String rest = sp == -1 ? "" : line.substring(sp + 1);
  cmd.toUpperCase();

  if (cmd == "OPEN") {
    int sp2 = rest.indexOf(' ');
    if (sp2 == -1) {
      Serial.println(F("Nutzung: OPEN <case_id> <examiner_id>"));
      return;
    }
    if (session.isOpen()) {
      Serial.println(F("[FEHLER] Sitzung bereits offen -- erst CLOSE."));
      return;
    }
    String caseId = rest.substring(0, sp2);
    String examinerId = rest.substring(sp2 + 1);
    examinerId.trim();

    session.open(caseId, examinerId);
    chainLog.begin(caseId);
    if (!chainLog.append("session_open", "{}", timeSource.nowIso8601Utc(),
                          timeSource.sourceLabel(), caseId, examinerId)) {
      session.close();
      Serial.println(F("[FEHLER] Konnte session_open nicht auf SD schreiben -- Sitzung nicht eroeffnet."));
      return;
    }
    Serial.print(F("[OK] Sitzung eroeffnet: "));
    Serial.println(caseId);

  } else if (cmd == "HASH") {
    if (!session.isOpen()) {
      Serial.println(F("[FEHLER] Keine offene Sitzung."));
      return;
    }
    int sp2 = rest.indexOf(' ');
    String hash = sp2 == -1 ? rest : rest.substring(0, sp2);
    String artifact = sp2 == -1 ? "" : rest.substring(sp2 + 1);
    String detail = "{\"sha256\":\"" + hash + "\",\"artifact\":\"" + artifact + "\"}";
    if (!chainLog.append("hash_ingest", detail, timeSource.nowIso8601Utc(),
                          timeSource.sourceLabel(), session.caseId(), session.examinerId())) {
      Serial.println(F("[FEHLER] SD-Schreibvorgang fehlgeschlagen -- Hash NICHT gesichert."));
      return;
    }
    Serial.println(F("[OK] Hash erfasst."));

  } else if (cmd == "NOTE") {
    if (!session.isOpen()) {
      Serial.println(F("[FEHLER] Keine offene Sitzung."));
      return;
    }
    String escaped = rest;
    escaped.replace("\"", "'");
    String detail = "{\"text\":\"" + escaped + "\"}";
    if (!chainLog.append("examiner_note", detail, timeSource.nowIso8601Utc(),
                          timeSource.sourceLabel(), session.caseId(), session.examinerId())) {
      Serial.println(F("[FEHLER] SD-Schreibvorgang fehlgeschlagen -- Notiz NICHT gesichert."));
      return;
    }
    Serial.println(F("[OK] Notiz angehaengt."));

  } else if (cmd == "CLOSE") {
    if (!session.isOpen()) {
      Serial.println(F("[FEHLER] Keine offene Sitzung."));
      return;
    }
    bool ok = chainLog.append("session_close", "{}", timeSource.nowIso8601Utc(),
                               timeSource.sourceLabel(), session.caseId(), session.examinerId());
    session.close();
    if (!ok) {
      Serial.println(F("[FEHLER] session_close nicht auf SD geschrieben -- Kette unvollstaendig! Sitzungszustand trotzdem zurueckgesetzt."));
      return;
    }
    Serial.println(F("[OK] Sitzung geschlossen."));

  } else if (cmd == "STATUS") {
    Serial.print(F("Sitzung: "));
    Serial.println(session.isOpen() ? "offen" : "geschlossen");
    if (session.isOpen()) {
      Serial.print(F("  Fall: "));
      Serial.println(session.caseId());
      Serial.print(F("  Pruefer: "));
      Serial.println(session.examinerId());
      Serial.print(F("  Sequenz: "));
      Serial.println(chainLog.sequence());
      Serial.print(F("  Letzter Hash: "));
      Serial.println(chainLog.lastHashHex());
    }

  } else if (cmd == "LIST") {
    File dir = SD.open("/cases");
    if (!dir) {
      Serial.println(F("[FEHLER] Verzeichnis /cases nicht vorhanden."));
      return;
    }
    File entry = dir.openNextFile();
    bool any = false;
    while (entry) {
      Serial.print(F("  "));
      Serial.print(entry.name());
      Serial.print(F("  ("));
      Serial.print(entry.size());
      Serial.println(F(" Bytes)"));
      any = true;
      entry.close();
      entry = dir.openNextFile();
    }
    dir.close();
    if (!any) Serial.println(F("  (leer)"));

  } else if (cmd == "DUMP") {
    String caseId = rest;
    caseId.trim();
    if (caseId.length() == 0) {
      Serial.println(F("Nutzung: DUMP <case_id>"));
      return;
    }
    String path = "/cases/" + caseId + ".jsonl";
    File f = SD.open(path.c_str(), FILE_READ);
    if (!f) {
      Serial.print(F("[FEHLER] Nicht gefunden: "));
      Serial.println(path);
      return;
    }
    while (f.available()) {
      Serial.write(f.read());
    }
    f.close();

#ifdef LAB_MODE
  } else if (cmd == "NTPSYNC") {
    String serverIp = rest;
    serverIp.trim();
    if (serverIp.length() == 0) {
      Serial.println(F("Nutzung: NTPSYNC <server_ip>"));
      return;
    }
    labNetwork.ntpSync(serverIp.c_str(), timeSource);

  } else if (cmd == "NETINFO") {
    Serial.print(F("Link: "));
    Serial.println(labNetwork.isLinked() ? "verbunden" : "getrennt");
    Serial.print(F("IP: "));
    Serial.println(labNetwork.ipAddress());
#endif

  } else if (cmd == "HELP") {
    printHelp();

  } else {
    Serial.println(F("Unbekannter Befehl. HELP fuer Uebersicht."));
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }

  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println(F("[FEHLER] SD-Karte nicht gefunden. Log-Funktion deaktiviert."));
  }

  timeSource.begin();

#ifdef LAB_MODE
  Serial.println(F("[Labor-Modus] Firmware-Variante -- NICHT fuer echte Sitzungen verwenden."));
  labNetwork.begin(timeSource);
#endif

  printHelp();
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      handleCommand(line);
    }
  }

#ifdef LAB_MODE
  labNetwork.poll(session, chainLog, timeSource);
#endif
}
