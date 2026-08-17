#include "SessionController.h"

bool SessionController::open(const String &caseId, const String &examinerId) {
  if (_state == SessionState::Open) return false;
  _caseId = caseId;
  _examinerId = examinerId;
  _state = SessionState::Open;
  return true;
}

bool SessionController::close() {
  if (_state == SessionState::Closed) return false;
  _state = SessionState::Closed;
  return true;
}
