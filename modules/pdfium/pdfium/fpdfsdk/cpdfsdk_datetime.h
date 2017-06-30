// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_CPDFSDK_DATETIME_H_
#define FPDFSDK_CPDFSDK_DATETIME_H_

#if _FX_OS_ == _FX_ANDROID_
#include <time.h>
#else
#include <ctime>
#endif

#include "fpdfsdk/cfx_systemhandler.h"

class CPDFSDK_DateTime {
 public:
  CPDFSDK_DateTime();
  explicit CPDFSDK_DateTime(const CFX_ByteString& dtStr);
  explicit CPDFSDK_DateTime(const FX_SYSTEMTIME& st);
  CPDFSDK_DateTime(const CPDFSDK_DateTime& datetime);

  bool operator==(const CPDFSDK_DateTime& datetime) const;
  bool operator!=(const CPDFSDK_DateTime& datetime) const;

  CPDFSDK_DateTime& FromPDFDateTimeString(const CFX_ByteString& dtStr);
  CFX_ByteString ToCommonDateTimeString();
  CFX_ByteString ToPDFDateTimeString();
  void ToSystemTime(FX_SYSTEMTIME& st);
  time_t ToTime_t() const;
  CPDFSDK_DateTime ToGMT() const;
  CPDFSDK_DateTime& AddDays(short days);
  CPDFSDK_DateTime& AddSeconds(int seconds);
  void ResetDateTime();

 private:
  int16_t m_year;
  uint8_t m_month;
  uint8_t m_day;
  uint8_t m_hour;
  uint8_t m_minute;
  uint8_t m_second;
  int8_t m_tzHour;
  uint8_t m_tzMinute;
};

#endif  // FPDFSDK_CPDFSDK_DATETIME_H_
