#pragma once
#include <Arduino.h>

// Haengt jedes Ereignis als eigene JSON-Zeile an eine fallbezogene Log-Datei
// auf der SD-Karte an und verkettet die Eintraege per SHA-256 (Hash-Chain),
// wie im Log-Format des Projekt-READMEs beschrieben.
//
// TODO (Roadmap Schritt 3): entry_hash zusaetzlich per ECDSA P-256 im
// Secure Element (ATECC608A) signieren. Bis dahin bleibt sig_ecdsa_p256
// bewusst null, statt eine Signatur vorzutaeuschen, die es nicht gibt.
class ChainLog {
public:
  bool begin(const String &caseId);

  bool append(const String &eventType,
              const String &detailJson,
              const String &tsUtc,
              const String &timeSourceLabel,
              const String &caseId,
              const String &examinerId);

  uint32_t sequence() const { return _seq; }
  const String &lastHashHex() const { return _prevHash; }

private:
  String _logPath;
  String _prevHash;
  uint32_t _seq = 0;

  static String zeroHash();
  static String sha256Hex(const String &data);
};
