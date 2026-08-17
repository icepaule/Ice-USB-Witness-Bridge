#include <Arduino.h>
#include <SD.h>

#include "SessionController.h"
#include "ChainLog.h"
#include "TimeSource.h"

// Firmware-Grundgeruest (Roadmap Schritt 1): Sitzungssteuerung, Hash-Chain,
// SD-Schreibpfad. Eingabe aktuell ueber die serielle Konsole als Platzhalter
// fuer den spaeteren USB-Host-Barcode-Scanner (siehe README, Hardware).

SessionController session;
ChainLog chainLog;
TimeSource timeSource;

void printHelp() {
  Serial.println(F("Witness Bridge -- Firmware-Grundgeruest"));
  Serial.println(F("Befehle:"));
  Serial.println(F("  OPEN <case_id> <examiner_id>   Sitzung eroeffnen"));
  Serial.println(F("  HASH <sha256> <artefakt>       Ergebnis-Hash erfassen"));
  Serial.println(F("  NOTE <text>                    Freitext-Ereignis anhaengen"));
  Serial.println(F("  CLOSE                          Sitzung beenden"));
  Serial.println(F("  STATUS                         aktuellen Zustand anzeigen"));
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
    chainLog.append("session_open", "{}", timeSource.nowIso8601Utc(),
                     timeSource.sourceLabel(), caseId, examinerId);
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
    chainLog.append("hash_ingest", detail, timeSource.nowIso8601Utc(),
                     timeSource.sourceLabel(), session.caseId(), session.examinerId());
    Serial.println(F("[OK] Hash erfasst."));

  } else if (cmd == "NOTE") {
    if (!session.isOpen()) {
      Serial.println(F("[FEHLER] Keine offene Sitzung."));
      return;
    }
    String escaped = rest;
    escaped.replace("\"", "'");
    String detail = "{\"text\":\"" + escaped + "\"}";
    chainLog.append("examiner_note", detail, timeSource.nowIso8601Utc(),
                     timeSource.sourceLabel(), session.caseId(), session.examinerId());
    Serial.println(F("[OK] Notiz angehaengt."));

  } else if (cmd == "CLOSE") {
    if (!session.isOpen()) {
      Serial.println(F("[FEHLER] Keine offene Sitzung."));
      return;
    }
    chainLog.append("session_close", "{}", timeSource.nowIso8601Utc(),
                     timeSource.sourceLabel(), session.caseId(), session.examinerId());
    session.close();
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
}
