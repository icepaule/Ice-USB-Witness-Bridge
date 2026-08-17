#pragma once
#include <Arduino.h>

// Zeitquelle fuer das Log. Aktuell: gepufferte RTC des Teensy 4.1.
// TODO (Roadmap Schritt 2): GPS/PPS-Diszipliniertheit ergaenzen und
// sourceLabel() erst dann auf "gps_pps" umstellen, wenn die Zeit
// tatsaechlich extern verifiziert ist -- nicht vorher, sonst behauptet
// das Log eine Vertrauensstufe, die es (noch) nicht einloest.
class TimeSource {
public:
  void begin();
  String nowIso8601Utc() const;
  const char *sourceLabel() const;
};
