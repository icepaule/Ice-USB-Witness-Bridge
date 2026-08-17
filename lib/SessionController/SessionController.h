#pragma once
#include <Arduino.h>

enum class SessionState { Closed, Open };

class SessionController {
public:
  bool open(const String &caseId, const String &examinerId);
  bool close();
  bool isOpen() const { return _state == SessionState::Open; }
  const String &caseId() const { return _caseId; }
  const String &examinerId() const { return _examinerId; }

private:
  SessionState _state = SessionState::Closed;
  String _caseId;
  String _examinerId;
};
