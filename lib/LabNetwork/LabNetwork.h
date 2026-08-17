#pragma once
#ifdef LAB_MODE

#include <Arduino.h>
#include "SessionController.h"
#include "ChainLog.h"
#include "TimeSource.h"

// Labor-Modus: bringt den nativen Ethernet-Port hoch, synchronisiert die RTC
// per NTP und stellt einen minimalen, rein lesenden Status-Webserver bereit
// (nur GET, keine schreibenden Endpunkte -- die Log-Dateien selbst werden
// ausschliesslich ueber ChainLog/die serielle Konsole geschrieben).
//
// NUR fuer Werkbank/Kalibrierung. Diese Firmware-Variante nie fuer echte
// Sitzungen flashen -- siehe README, Abschnitt "Labor-Modus".
class LabNetwork {
public:
  void begin();
  void poll(SessionController &session, ChainLog &chainLog, TimeSource &timeSource);
  bool ntpSync(const char *serverIp, TimeSource &timeSource);
  bool isLinked() const;
  String ipAddress() const;
};

#endif // LAB_MODE
