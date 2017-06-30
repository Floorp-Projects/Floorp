// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/cpdfsdk_datetime.h"

#include "core/fxcrt/fx_ext.h"

namespace {

int GetTimeZoneInSeconds(int8_t tzhour, uint8_t tzminute) {
  return (int)tzhour * 3600 + (int)tzminute * (tzhour >= 0 ? 60 : -60);
}

bool IsLeapYear(int16_t year) {
  return ((year % 400 == 0) || ((year % 4 == 0) && (year % 100 != 0)));
}

uint16_t GetYearDays(int16_t year) {
  return (IsLeapYear(year) ? 366 : 365);
}

uint8_t GetMonthDays(int16_t year, uint8_t month) {
  uint8_t mDays;
  switch (month) {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
      mDays = 31;
      break;

    case 4:
    case 6:
    case 9:
    case 11:
      mDays = 30;
      break;

    case 2:
      if (IsLeapYear(year))
        mDays = 29;
      else
        mDays = 28;
      break;

    default:
      mDays = 0;
      break;
  }

  return mDays;
}

}  // namespace

CPDFSDK_DateTime::CPDFSDK_DateTime() {
  ResetDateTime();
}

CPDFSDK_DateTime::CPDFSDK_DateTime(const CFX_ByteString& dtStr) {
  ResetDateTime();
  FromPDFDateTimeString(dtStr);
}

CPDFSDK_DateTime::CPDFSDK_DateTime(const CPDFSDK_DateTime& that)
    : m_year(that.m_year),
      m_month(that.m_month),
      m_day(that.m_day),
      m_hour(that.m_hour),
      m_minute(that.m_minute),
      m_second(that.m_second),
      m_tzHour(that.m_tzHour),
      m_tzMinute(that.m_tzMinute) {}

CPDFSDK_DateTime::CPDFSDK_DateTime(const FX_SYSTEMTIME& st) {
  tzset();

  m_year = static_cast<int16_t>(st.wYear);
  m_month = static_cast<uint8_t>(st.wMonth);
  m_day = static_cast<uint8_t>(st.wDay);
  m_hour = static_cast<uint8_t>(st.wHour);
  m_minute = static_cast<uint8_t>(st.wMinute);
  m_second = static_cast<uint8_t>(st.wSecond);
}

void CPDFSDK_DateTime::ResetDateTime() {
  tzset();

  time_t curTime;
  time(&curTime);

  struct tm* newtime = localtime(&curTime);
  m_year = newtime->tm_year + 1900;
  m_month = newtime->tm_mon + 1;
  m_day = newtime->tm_mday;
  m_hour = newtime->tm_hour;
  m_minute = newtime->tm_min;
  m_second = newtime->tm_sec;
}

bool CPDFSDK_DateTime::operator==(const CPDFSDK_DateTime& that) const {
  return m_year == that.m_year && m_month == that.m_month &&
         m_day == that.m_day && m_hour == that.m_hour &&
         m_minute == that.m_minute && m_second == that.m_second &&
         m_tzHour == that.m_tzHour && m_tzMinute == that.m_tzMinute;
}

bool CPDFSDK_DateTime::operator!=(const CPDFSDK_DateTime& datetime) const {
  return !(*this == datetime);
}

time_t CPDFSDK_DateTime::ToTime_t() const {
  struct tm newtime;

  newtime.tm_year = m_year - 1900;
  newtime.tm_mon = m_month - 1;
  newtime.tm_mday = m_day;
  newtime.tm_hour = m_hour;
  newtime.tm_min = m_minute;
  newtime.tm_sec = m_second;

  return mktime(&newtime);
}

CPDFSDK_DateTime& CPDFSDK_DateTime::FromPDFDateTimeString(
    const CFX_ByteString& dtStr) {
  int strLength = dtStr.GetLength();
  if (strLength <= 0)
    return *this;

  int i = 0;
  while (i < strLength && !std::isdigit(dtStr[i]))
    ++i;

  if (i >= strLength)
    return *this;

  int j = 0;
  int k = 0;
  FX_CHAR ch;
  while (i < strLength && j < 4) {
    ch = dtStr[i];
    k = k * 10 + FXSYS_toDecimalDigit(ch);
    j++;
    if (!std::isdigit(ch))
      break;
    i++;
  }
  m_year = static_cast<int16_t>(k);
  if (i >= strLength || j < 4)
    return *this;

  j = 0;
  k = 0;
  while (i < strLength && j < 2) {
    ch = dtStr[i];
    k = k * 10 + FXSYS_toDecimalDigit(ch);
    j++;
    if (!std::isdigit(ch))
      break;
    i++;
  }
  m_month = static_cast<uint8_t>(k);
  if (i >= strLength || j < 2)
    return *this;

  j = 0;
  k = 0;
  while (i < strLength && j < 2) {
    ch = dtStr[i];
    k = k * 10 + FXSYS_toDecimalDigit(ch);
    j++;
    if (!std::isdigit(ch))
      break;
    i++;
  }
  m_day = static_cast<uint8_t>(k);
  if (i >= strLength || j < 2)
    return *this;

  j = 0;
  k = 0;
  while (i < strLength && j < 2) {
    ch = dtStr[i];
    k = k * 10 + FXSYS_toDecimalDigit(ch);
    j++;
    if (!std::isdigit(ch))
      break;
    i++;
  }
  m_hour = static_cast<uint8_t>(k);
  if (i >= strLength || j < 2)
    return *this;

  j = 0;
  k = 0;
  while (i < strLength && j < 2) {
    ch = dtStr[i];
    k = k * 10 + FXSYS_toDecimalDigit(ch);
    j++;
    if (!std::isdigit(ch))
      break;
    i++;
  }
  m_minute = static_cast<uint8_t>(k);
  if (i >= strLength || j < 2)
    return *this;

  j = 0;
  k = 0;
  while (i < strLength && j < 2) {
    ch = dtStr[i];
    k = k * 10 + FXSYS_toDecimalDigit(ch);
    j++;
    if (!std::isdigit(ch))
      break;
    i++;
  }
  m_second = static_cast<uint8_t>(k);
  if (i >= strLength || j < 2)
    return *this;

  ch = dtStr[i++];
  if (ch != '-' && ch != '+')
    return *this;
  if (ch == '-')
    m_tzHour = -1;
  else
    m_tzHour = 1;
  j = 0;
  k = 0;
  while (i < strLength && j < 2) {
    ch = dtStr[i];
    k = k * 10 + FXSYS_toDecimalDigit(ch);
    j++;
    if (!std::isdigit(ch))
      break;
    i++;
  }
  m_tzHour *= static_cast<int8_t>(k);
  if (i >= strLength || j < 2)
    return *this;

  if (dtStr[i++] != '\'')
    return *this;
  j = 0;
  k = 0;
  while (i < strLength && j < 2) {
    ch = dtStr[i];
    k = k * 10 + FXSYS_toDecimalDigit(ch);
    j++;
    if (!std::isdigit(ch))
      break;
    i++;
  }
  m_tzMinute = static_cast<uint8_t>(k);
  return *this;
}

CFX_ByteString CPDFSDK_DateTime::ToCommonDateTimeString() {
  CFX_ByteString str1;
  str1.Format("%04d-%02u-%02u %02u:%02u:%02u ", m_year, m_month, m_day, m_hour,
              m_minute, m_second);
  if (m_tzHour < 0)
    str1 += "-";
  else
    str1 += "+";
  CFX_ByteString str2;
  str2.Format("%02d:%02u", std::abs(static_cast<int>(m_tzHour)), m_tzMinute);
  return str1 + str2;
}

CFX_ByteString CPDFSDK_DateTime::ToPDFDateTimeString() {
  CFX_ByteString dtStr;
  char tempStr[32];
  memset(tempStr, 0, sizeof(tempStr));
  FXSYS_snprintf(tempStr, sizeof(tempStr) - 1, "D:%04d%02u%02u%02u%02u%02u",
                 m_year, m_month, m_day, m_hour, m_minute, m_second);
  dtStr = CFX_ByteString(tempStr);
  if (m_tzHour < 0)
    dtStr += CFX_ByteString("-");
  else
    dtStr += CFX_ByteString("+");
  memset(tempStr, 0, sizeof(tempStr));
  FXSYS_snprintf(tempStr, sizeof(tempStr) - 1, "%02d'%02u'",
                 std::abs(static_cast<int>(m_tzHour)), m_tzMinute);
  dtStr += CFX_ByteString(tempStr);
  return dtStr;
}

void CPDFSDK_DateTime::ToSystemTime(FX_SYSTEMTIME& st) {
  time_t t = this->ToTime_t();
  struct tm* pTime = localtime(&t);

  if (!pTime)
    return;

  st.wYear = static_cast<uint16_t>(pTime->tm_year) + 1900;
  st.wMonth = static_cast<uint16_t>(pTime->tm_mon) + 1;
  st.wDay = static_cast<uint16_t>(pTime->tm_mday);
  st.wDayOfWeek = static_cast<uint16_t>(pTime->tm_wday);
  st.wHour = static_cast<uint16_t>(pTime->tm_hour);
  st.wMinute = static_cast<uint16_t>(pTime->tm_min);
  st.wSecond = static_cast<uint16_t>(pTime->tm_sec);
  st.wMilliseconds = 0;
}

CPDFSDK_DateTime CPDFSDK_DateTime::ToGMT() const {
  CPDFSDK_DateTime new_dt = *this;
  new_dt.AddSeconds(-GetTimeZoneInSeconds(new_dt.m_tzHour, new_dt.m_tzMinute));
  new_dt.m_tzHour = 0;
  new_dt.m_tzMinute = 0;
  return new_dt;
}

CPDFSDK_DateTime& CPDFSDK_DateTime::AddDays(short days) {
  if (days == 0)
    return *this;

  int16_t y = m_year;
  uint8_t m = m_month;
  uint8_t d = m_day;

  int ldays = days;
  if (ldays > 0) {
    int16_t yy = y;
    if ((static_cast<uint16_t>(m) * 100 + d) > 300)
      yy++;
    int ydays = GetYearDays(yy);
    int mdays;
    while (ldays >= ydays) {
      y++;
      ldays -= ydays;
      yy++;
      mdays = GetMonthDays(y, m);
      if (d > mdays) {
        m++;
        d -= mdays;
      }
      ydays = GetYearDays(yy);
    }
    mdays = GetMonthDays(y, m) - d + 1;
    while (ldays >= mdays) {
      ldays -= mdays;
      m++;
      d = 1;
      mdays = GetMonthDays(y, m);
    }
    d += ldays;
  } else {
    ldays *= -1;
    int16_t yy = y;
    if ((static_cast<uint16_t>(m) * 100 + d) < 300)
      yy--;
    int ydays = GetYearDays(yy);
    while (ldays >= ydays) {
      y--;
      ldays -= ydays;
      yy--;
      int mdays = GetMonthDays(y, m);
      if (d > mdays) {
        m++;
        d -= mdays;
      }
      ydays = GetYearDays(yy);
    }
    while (ldays >= d) {
      ldays -= d;
      m--;
      d = GetMonthDays(y, m);
    }
    d -= ldays;
  }

  m_year = y;
  m_month = m;
  m_day = d;

  return *this;
}

CPDFSDK_DateTime& CPDFSDK_DateTime::AddSeconds(int seconds) {
  if (seconds == 0)
    return *this;

  int n;
  int days;

  n = m_hour * 3600 + m_minute * 60 + m_second + seconds;
  if (n < 0) {
    days = (n - 86399) / 86400;
    n -= days * 86400;
  } else {
    days = n / 86400;
    n %= 86400;
  }
  m_hour = static_cast<uint8_t>(n / 3600);
  m_hour %= 24;
  n %= 3600;
  m_minute = static_cast<uint8_t>(n / 60);
  m_second = static_cast<uint8_t>(n % 60);
  if (days != 0)
    AddDays(days);

  return *this;
}
