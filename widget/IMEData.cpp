/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IMEData.h"
#include <sstream>

namespace mozilla {

namespace widget {

std::ostream& operator<<(std::ostream& aStream,
                         const IMEState::Enabled& aEnabled) {
  switch (aEnabled) {
    case IMEState::DISABLED:
      aStream << "DISABLED";
      break;
    case IMEState::ENABLED:
      aStream << "ENABLED";
      break;
    case IMEState::PASSWORD:
      aStream << "PASSWORD";
      break;
    case IMEState::PLUGIN:
      aStream << "PLUGIN";
      break;
    default:
      aStream << "illegal value";
      break;
  }
  return aStream;
}

std::ostream& operator<<(std::ostream& aStream, const IMEState::Open& aOpen) {
  switch (aOpen) {
    case IMEState::DONT_CHANGE_OPEN_STATE:
      aStream << "DONT_CHANGE_OPEN_STATE";
      break;
    case IMEState::OPEN:
      aStream << "OPEN";
      break;
    case IMEState::CLOSED:
      aStream << "CLOSED";
      break;
    default:
      aStream << "illegal value";
      break;
  }
  return aStream;
}

std::ostream& operator<<(std::ostream& aStream,
                         const InputContextAction::Cause& aCause) {
  switch (aCause) {
    case InputContextAction::CAUSE_UNKNOWN:
      aStream << "CAUSE_UNKNOWN";
      break;
    case InputContextAction::CAUSE_UNKNOWN_CHROME:
      aStream << "CAUSE_UNKNOWN_CHROME";
      break;
    case InputContextAction::CAUSE_KEY:
      aStream << "CAUSE_KEY";
      break;
    case InputContextAction::CAUSE_MOUSE:
      aStream << "CAUSE_MOUSE";
      break;
    case InputContextAction::CAUSE_TOUCH:
      aStream << "CAUSE_TOUCH";
      break;
    case InputContextAction::CAUSE_LONGPRESS:
      aStream << "CAUSE_LONGPRESS";
      break;
    case InputContextAction::CAUSE_UNKNOWN_DURING_NON_KEYBOARD_INPUT:
      aStream << "CAUSE_UNKNOWN_DURING_NON_KEYBOARD_INPUT";
      break;
    case InputContextAction::CAUSE_UNKNOWN_DURING_KEYBOARD_INPUT:
      aStream << "CAUSE_UNKNOWN_DURING_KEYBOARD_INPUT";
      break;
    default:
      aStream << "illegal value";
      break;
  }
  return aStream;
}

std::ostream& operator<<(std::ostream& aStream,
                         const InputContextAction::FocusChange& aFocusChange) {
  switch (aFocusChange) {
    case InputContextAction::FOCUS_NOT_CHANGED:
      aStream << "FOCUS_NOT_CHANGED";
      break;
    case InputContextAction::GOT_FOCUS:
      aStream << "GOT_FOCUS";
      break;
    case InputContextAction::LOST_FOCUS:
      aStream << "LOST_FOCUS";
      break;
    case InputContextAction::MENU_GOT_PSEUDO_FOCUS:
      aStream << "MENU_GOT_PSEUDO_FOCUS";
      break;
    case InputContextAction::MENU_LOST_PSEUDO_FOCUS:
      aStream << "MENU_LOST_PSEUDO_FOCUS";
      break;
    case InputContextAction::WIDGET_CREATED:
      aStream << "WIDGET_CREATED";
      break;
    default:
      aStream << "illegal value";
      break;
  }
  return aStream;
}

}  // namespace widget
}  // namespace mozilla
