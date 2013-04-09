/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TimeStamp_windows_h
#define mozilla_TimeStamp_windows_h

namespace mozilla {

class TimeStamp;

class TimeStampValue
{
  friend struct IPC::ParamTraits<mozilla::TimeStampValue>;
  friend class TimeStamp;
  friend void StartupTimelineRecordExternal(int, uint64_t);

  // Both QPC and GTC are kept in [mt] units.
  uint64_t mGTC;
  uint64_t mQPC;
  bool mHasQPC;
  bool mIsNull;

  TimeStampValue(uint64_t GTC, uint64_t QPC, bool hasQPC);

  bool CheckQPC(int64_t aDuration, const TimeStampValue &aOther) const;

public:
  struct _SomethingVeryRandomHere;
  TimeStampValue(_SomethingVeryRandomHere* nullValue);

  uint64_t operator-(const TimeStampValue &aOther) const;

  TimeStampValue operator+(const int64_t aOther) const
    { return TimeStampValue(mGTC + aOther, mQPC + aOther, mHasQPC); }
  TimeStampValue operator-(const int64_t aOther) const
    { return TimeStampValue(mGTC - aOther, mQPC - aOther, mHasQPC); }
  TimeStampValue& operator+=(const int64_t aOther);
  TimeStampValue& operator-=(const int64_t aOther);

  bool operator<(const TimeStampValue &aOther) const
    { return int64_t(*this - aOther) < 0; }
  bool operator>(const TimeStampValue &aOther) const
    { return int64_t(*this - aOther) > 0; }
  bool operator<=(const TimeStampValue &aOther) const
    { return int64_t(*this - aOther) <= 0; }
  bool operator>=(const TimeStampValue &aOther) const
    { return int64_t(*this - aOther) >= 0; }
  bool operator==(const TimeStampValue &aOther) const
    { return int64_t(*this - aOther) == 0; }
  bool operator!=(const TimeStampValue &aOther) const
    { return int64_t(*this - aOther) != 0; }
};

}

#endif /* mozilla_TimeStamp_h */
