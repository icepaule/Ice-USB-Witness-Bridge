#include "ChainLog.h"
#include <SD.h>
#include <ArduinoJson.h>
#include <SHA256.h>

String ChainLog::zeroHash() {
  String s;
  s.reserve(64);
  for (int i = 0; i < 64; i++) s += '0';
  return s;
}

String ChainLog::sha256Hex(const String &data) {
  SHA256 sha;
  sha.reset();
  sha.update((const uint8_t *)data.c_str(), data.length());
  uint8_t digest[32];
  sha.finalize(digest, sizeof(digest));

  String hex;
  hex.reserve(64);
  const char *hexChars = "0123456789abcdef";
  for (int i = 0; i < 32; i++) {
    hex += hexChars[(digest[i] >> 4) & 0x0F];
    hex += hexChars[digest[i] & 0x0F];
  }
  return hex;
}

bool ChainLog::begin(const String &caseId) {
  if (!SD.exists("/cases")) {
    SD.mkdir("/cases");
  }
  _logPath = "/cases/" + caseId + ".jsonl";
  _seq = 0;
  _prevHash = zeroHash();
  return true;
}

bool ChainLog::append(const String &eventType,
                       const String &detailJson,
                       const String &tsUtc,
                       const String &timeSourceLabel,
                       const String &caseId,
                       const String &examinerId) {
  if (_logPath.length() == 0) return false;

  // Kanonische Form fuer die Verkettung -- muss mit der Formel im
  // README (Abschnitt "Log-Format") uebereinstimmen.
  String canonical = _prevHash + "|" + String(_seq) + "|" + tsUtc + "|" +
                      caseId + "|" + examinerId + "|" + eventType + "|" + detailJson;
  String entryHash = sha256Hex(canonical);

  StaticJsonDocument<768> doc;
  doc["seq"] = _seq;
  doc["ts_utc"] = tsUtc;
  doc["time_source"] = timeSourceLabel;
  doc["case_id"] = caseId;
  doc["examiner_id"] = examinerId;
  doc["event"] = eventType;

  StaticJsonDocument<384> detailDoc;
  if (deserializeJson(detailDoc, detailJson) == DeserializationError::Ok) {
    doc["detail"] = detailDoc.as<JsonVariant>();
  } else {
    doc["detail"] = detailJson;
  }

  doc["prev_hash"] = _prevHash;
  doc["entry_hash"] = entryHash;
  doc["sig_ecdsa_p256"] = (const char *)nullptr; // siehe TODO in ChainLog.h

  File f = SD.open(_logPath.c_str(), FILE_WRITE);
  if (!f) return false;
  serializeJson(doc, f);
  f.println();
  f.close();

  _prevHash = entryHash;
  _seq++;
  return true;
}
