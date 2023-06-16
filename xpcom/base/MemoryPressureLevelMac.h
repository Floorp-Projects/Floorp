/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MemoryPressureLevelMac_h
#define mozilla_MemoryPressureLevelMac_h

namespace mozilla {

#if defined(XP_DARWIN)
// An internal representation of the Mac memory-pressure level constants.
class MacMemoryPressureLevel {
 public:
  // Order enum values so that higher integer values represent higher
  // memory pressure levels allowing comparison operators to be used.
  enum class Value {
    eUnset,
    eUnexpected,
    eNormal,
    eWarning,
    eCritical,
  };

  MacMemoryPressureLevel() : mValue(Value::eUnset) {}
  MOZ_IMPLICIT MacMemoryPressureLevel(Value aValue) : mValue(aValue) {}

  bool operator==(const Value& aRhsValue) const { return mValue == aRhsValue; }
  bool operator==(const MacMemoryPressureLevel& aRhs) const {
    return mValue == aRhs.mValue;
  }

  // Implement '<' and derive the other comparators from it.
  bool operator<(const MacMemoryPressureLevel& aRhs) const {
    return mValue < aRhs.mValue;
  }
  bool operator>(const MacMemoryPressureLevel& aRhs) const {
    return aRhs < *this;
  }
  bool operator<=(const MacMemoryPressureLevel& aRhs) const {
    return !(aRhs < *this);
  }
  bool operator>=(const MacMemoryPressureLevel& aRhs) const {
    return !(*this < aRhs);
  }

  Value GetValue() { return mValue; }
  bool IsNormal() { return mValue == Value::eNormal; }
  bool IsUnsetOrNormal() { return IsNormal() || (mValue == Value::eUnset); }
  bool IsWarningOrAbove() {
    return (mValue == Value::eWarning) || (mValue == Value::eCritical);
  }

  const char* ToString() {
    switch (mValue) {
      case Value::eUnset:
        return "Unset";
      case Value::eUnexpected:
        return "Unexpected";
      case Value::eNormal:
        return "Normal";
      case Value::eWarning:
        return "Warning";
      case Value::eCritical:
        return "Critical";
    }
  }

 private:
  Value mValue;
};
#endif

}  // namespace mozilla

#endif  // mozilla_MemoryPressureLevelMac_h
