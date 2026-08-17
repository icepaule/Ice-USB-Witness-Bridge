#include "TimeSource.h"
#include <TimeLib.h>

static time_t getTeensyRtcTime() {
  return Teensy3Clock.get();
}

void TimeSource::begin() {
  setSyncProvider(getTeensyRtcTime);
}

String TimeSource::nowIso8601Utc() const {
  time_t t = now();
  char buf[24];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
           year(t), month(t), day(t), hour(t), minute(t), second(t));
  return String(buf);
}

const char *TimeSource::sourceLabel() const {
  // TODO (Roadmap Schritt 2): "gps_pps", sobald GPS/PPS verbaut und verifiziert ist.
  if (_ntpSynced) return "ntp_lab";
  return "rtc_local";
}

void TimeSource::setEpoch(unsigned long epochUtc) {
  Teensy3Clock.set(epochUtc);
  setTime((time_t)epochUtc);
  _ntpSynced = true;
}
