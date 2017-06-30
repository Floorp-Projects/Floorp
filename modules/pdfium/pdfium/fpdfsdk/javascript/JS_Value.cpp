// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/JS_Value.h"

#include <time.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "fpdfsdk/javascript/Document.h"
#include "fpdfsdk/javascript/JS_Define.h"
#include "fpdfsdk/javascript/JS_Object.h"

namespace {

const uint32_t g_nan[2] = {0, 0x7FF80000};

double GetNan() {
  return *(double*)g_nan;
}

double
MakeDate(int year, int mon, int day, int hour, int min, int sec, int ms) {
  return JS_MakeDate(JS_MakeDay(year, mon, day),
                     JS_MakeTime(hour, min, sec, ms));
}

}  // namespace

CJS_Value::CJS_Value(CJS_Runtime* pRuntime) {}

CJS_Value::CJS_Value(CJS_Runtime* pRuntime, v8::Local<v8::Value> pValue)
    : m_pValue(pValue) {}

CJS_Value::CJS_Value(CJS_Runtime* pRuntime, const int& iValue)
    : m_pValue(pRuntime->NewNumber(iValue)) {}

CJS_Value::CJS_Value(CJS_Runtime* pRuntime, const bool& bValue)
    : m_pValue(pRuntime->NewBoolean(bValue)) {}

CJS_Value::CJS_Value(CJS_Runtime* pRuntime, const float& fValue)
    : m_pValue(pRuntime->NewNumber(fValue)) {}

CJS_Value::CJS_Value(CJS_Runtime* pRuntime, const double& dValue)
    : m_pValue(pRuntime->NewNumber(dValue)) {}

CJS_Value::CJS_Value(CJS_Runtime* pRuntime, CJS_Object* pObj) {
  if (pObj)
    m_pValue = pObj->ToV8Object();
}

CJS_Value::CJS_Value(CJS_Runtime* pRuntime, const FX_WCHAR* pWstr)
    : m_pValue(pRuntime->NewString(pWstr)) {}

CJS_Value::CJS_Value(CJS_Runtime* pRuntime, const FX_CHAR* pStr)
    : m_pValue(pRuntime->NewString(CFX_WideString::FromLocal(pStr).c_str())) {}

CJS_Value::CJS_Value(CJS_Runtime* pRuntime, const CJS_Array& array)
    : m_pValue(array.ToV8Array(pRuntime)) {}

CJS_Value::CJS_Value(CJS_Runtime* pRuntime, const CJS_Date& date)
    : m_pValue(date.ToV8Date(pRuntime)) {}

CJS_Value::~CJS_Value() {}

CJS_Value::CJS_Value(const CJS_Value& other) = default;

void CJS_Value::Attach(v8::Local<v8::Value> pValue) {
  m_pValue = pValue;
}

void CJS_Value::Detach() {
  m_pValue = v8::Local<v8::Value>();
}

int CJS_Value::ToInt(CJS_Runtime* pRuntime) const {
  return pRuntime->ToInt32(m_pValue);
}

bool CJS_Value::ToBool(CJS_Runtime* pRuntime) const {
  return pRuntime->ToBoolean(m_pValue);
}

double CJS_Value::ToDouble(CJS_Runtime* pRuntime) const {
  return pRuntime->ToDouble(m_pValue);
}

float CJS_Value::ToFloat(CJS_Runtime* pRuntime) const {
  return (float)ToDouble(pRuntime);
}

CJS_Object* CJS_Value::ToCJSObject(CJS_Runtime* pRuntime) const {
  v8::Local<v8::Object> pObj = pRuntime->ToObject(m_pValue);
  return static_cast<CJS_Object*>(pRuntime->GetObjectPrivate(pObj));
}

v8::Local<v8::Object> CJS_Value::ToV8Object(CJS_Runtime* pRuntime) const {
  return pRuntime->ToObject(m_pValue);
}

CFX_WideString CJS_Value::ToCFXWideString(CJS_Runtime* pRuntime) const {
  return pRuntime->ToWideString(m_pValue);
}

CFX_ByteString CJS_Value::ToCFXByteString(CJS_Runtime* pRuntime) const {
  return CFX_ByteString::FromUnicode(ToCFXWideString(pRuntime));
}

v8::Local<v8::Value> CJS_Value::ToV8Value(CJS_Runtime* pRuntime) const {
  return m_pValue;
}

v8::Local<v8::Array> CJS_Value::ToV8Array(CJS_Runtime* pRuntime) const {
  return pRuntime->ToArray(m_pValue);
}

void CJS_Value::SetNull(CJS_Runtime* pRuntime) {
  m_pValue = pRuntime->NewNull();
}

void CJS_Value::MaybeCoerceToNumber(CJS_Runtime* pRuntime) {
  bool bAllowNaN = false;
  if (GetType() == VT_string) {
    CFX_ByteString bstr = ToCFXByteString(pRuntime);
    if (bstr.GetLength() == 0)
      return;
    if (bstr == "NaN")
      bAllowNaN = true;
  }
  v8::Isolate* pIsolate = pRuntime->GetIsolate();
  v8::TryCatch try_catch(pIsolate);
  v8::MaybeLocal<v8::Number> maybeNum =
      m_pValue->ToNumber(pIsolate->GetCurrentContext());
  if (maybeNum.IsEmpty())
    return;
  v8::Local<v8::Number> num = maybeNum.ToLocalChecked();
  if (std::isnan(num->Value()) && !bAllowNaN)
    return;
  m_pValue = num;
}

// static
CJS_Value::Type CJS_Value::GetValueType(v8::Local<v8::Value> value) {
  if (value.IsEmpty())
    return VT_unknown;
  if (value->IsString())
    return VT_string;
  if (value->IsNumber())
    return VT_number;
  if (value->IsBoolean())
    return VT_boolean;
  if (value->IsDate())
    return VT_date;
  if (value->IsObject())
    return VT_object;
  if (value->IsNull())
    return VT_null;
  if (value->IsUndefined())
    return VT_undefined;
  return VT_unknown;
}

bool CJS_Value::IsArrayObject() const {
  return !m_pValue.IsEmpty() && m_pValue->IsArray();
}

bool CJS_Value::IsDateObject() const {
  return !m_pValue.IsEmpty() && m_pValue->IsDate();
}

bool CJS_Value::ConvertToArray(CJS_Runtime* pRuntime, CJS_Array& array) const {
  if (!IsArrayObject())
    return false;
  array.Attach(pRuntime->ToArray(m_pValue));
  return true;
}

bool CJS_Value::ConvertToDate(CJS_Runtime* pRuntime, CJS_Date& date) const {
  if (!IsDateObject())
    return false;
  v8::Local<v8::Value> mutable_value = m_pValue;
  date.Attach(mutable_value.As<v8::Date>());
  return true;
}

CJS_PropValue::CJS_PropValue(CJS_Runtime* pRuntime)
    : m_bIsSetting(0), m_Value(pRuntime), m_pJSRuntime(pRuntime) {}

CJS_PropValue::CJS_PropValue(CJS_Runtime* pRuntime, const CJS_Value& value)
    : m_bIsSetting(0), m_Value(value), m_pJSRuntime(pRuntime) {}

CJS_PropValue::~CJS_PropValue() {}

void CJS_PropValue::operator<<(int iValue) {
  ASSERT(!m_bIsSetting);
  m_Value = CJS_Value(m_pJSRuntime, iValue);
}

void CJS_PropValue::operator>>(int& iValue) const {
  ASSERT(m_bIsSetting);
  iValue = m_Value.ToInt(m_pJSRuntime);
}

void CJS_PropValue::operator<<(bool bValue) {
  ASSERT(!m_bIsSetting);
  m_Value = CJS_Value(m_pJSRuntime, bValue);
}

void CJS_PropValue::operator>>(bool& bValue) const {
  ASSERT(m_bIsSetting);
  bValue = m_Value.ToBool(m_pJSRuntime);
}

void CJS_PropValue::operator<<(double dValue) {
  ASSERT(!m_bIsSetting);
  m_Value = CJS_Value(m_pJSRuntime, dValue);
}

void CJS_PropValue::operator>>(double& dValue) const {
  ASSERT(m_bIsSetting);
  dValue = m_Value.ToDouble(m_pJSRuntime);
}

void CJS_PropValue::operator<<(CJS_Object* pObj) {
  ASSERT(!m_bIsSetting);
  m_Value = CJS_Value(m_pJSRuntime, pObj);
}

void CJS_PropValue::operator>>(CJS_Object*& ppObj) const {
  ASSERT(m_bIsSetting);
  ppObj = m_Value.ToCJSObject(m_pJSRuntime);
}

void CJS_PropValue::operator<<(CJS_Document* pJsDoc) {
  ASSERT(!m_bIsSetting);
  m_Value = CJS_Value(m_pJSRuntime, pJsDoc);
}

void CJS_PropValue::operator>>(CJS_Document*& ppJsDoc) const {
  ASSERT(m_bIsSetting);
  ppJsDoc = static_cast<CJS_Document*>(m_Value.ToCJSObject(m_pJSRuntime));
}

void CJS_PropValue::operator<<(v8::Local<v8::Object> pObj) {
  ASSERT(!m_bIsSetting);
  m_Value = CJS_Value(m_pJSRuntime, pObj);
}

void CJS_PropValue::operator>>(v8::Local<v8::Object>& ppObj) const {
  ASSERT(m_bIsSetting);
  ppObj = m_Value.ToV8Object(m_pJSRuntime);
}

void CJS_PropValue::operator<<(CFX_ByteString str) {
  ASSERT(!m_bIsSetting);
  m_Value = CJS_Value(m_pJSRuntime, str.c_str());
}

void CJS_PropValue::operator>>(CFX_ByteString& str) const {
  ASSERT(m_bIsSetting);
  str = m_Value.ToCFXByteString(m_pJSRuntime);
}

void CJS_PropValue::operator<<(const FX_WCHAR* str) {
  ASSERT(!m_bIsSetting);
  m_Value = CJS_Value(m_pJSRuntime, str);
}

void CJS_PropValue::operator>>(CFX_WideString& wide_string) const {
  ASSERT(m_bIsSetting);
  wide_string = m_Value.ToCFXWideString(m_pJSRuntime);
}

void CJS_PropValue::operator<<(CFX_WideString wide_string) {
  ASSERT(!m_bIsSetting);
  m_Value = CJS_Value(m_pJSRuntime, wide_string.c_str());
}

void CJS_PropValue::operator>>(CJS_Array& array) const {
  ASSERT(m_bIsSetting);
  m_Value.ConvertToArray(m_pJSRuntime, array);
}

void CJS_PropValue::operator<<(CJS_Array& array) {
  ASSERT(!m_bIsSetting);
  m_Value = CJS_Value(m_pJSRuntime, array.ToV8Array(m_pJSRuntime));
}

void CJS_PropValue::operator>>(CJS_Date& date) const {
  ASSERT(m_bIsSetting);
  m_Value.ConvertToDate(m_pJSRuntime, date);
}

void CJS_PropValue::operator<<(CJS_Date& date) {
  ASSERT(!m_bIsSetting);
  m_Value = CJS_Value(m_pJSRuntime, date);
}

CJS_Array::CJS_Array() {}

CJS_Array::CJS_Array(const CJS_Array& other) = default;

CJS_Array::~CJS_Array() {}

void CJS_Array::Attach(v8::Local<v8::Array> pArray) {
  m_pArray = pArray;
}

void CJS_Array::GetElement(CJS_Runtime* pRuntime,
                           unsigned index,
                           CJS_Value& value) const {
  if (!m_pArray.IsEmpty())
    value.Attach(pRuntime->GetArrayElement(m_pArray, index));
}

void CJS_Array::SetElement(CJS_Runtime* pRuntime,
                           unsigned index,
                           const CJS_Value& value) {
  if (m_pArray.IsEmpty())
    m_pArray = pRuntime->NewArray();

  pRuntime->PutArrayElement(m_pArray, index, value.ToV8Value(pRuntime));
}

int CJS_Array::GetLength(CJS_Runtime* pRuntime) const {
  if (m_pArray.IsEmpty())
    return 0;
  return pRuntime->GetArrayLength(m_pArray);
}

v8::Local<v8::Array> CJS_Array::ToV8Array(CJS_Runtime* pRuntime) const {
  if (m_pArray.IsEmpty())
    m_pArray = pRuntime->NewArray();

  return m_pArray;
}

CJS_Date::CJS_Date() {}

CJS_Date::CJS_Date(CJS_Runtime* pRuntime, double dMsecTime)
    : m_pDate(pRuntime->NewDate(dMsecTime)) {}

CJS_Date::CJS_Date(CJS_Runtime* pRuntime,
                   int year,
                   int mon,
                   int day,
                   int hour,
                   int min,
                   int sec)
    : m_pDate(pRuntime->NewDate(MakeDate(year, mon, day, hour, min, sec, 0))) {}

CJS_Date::~CJS_Date() {}

bool CJS_Date::IsValidDate(CJS_Runtime* pRuntime) const {
  return !m_pDate.IsEmpty() && !JS_PortIsNan(pRuntime->ToDouble(m_pDate));
}

void CJS_Date::Attach(v8::Local<v8::Date> pDate) {
  m_pDate = pDate;
}

int CJS_Date::GetYear(CJS_Runtime* pRuntime) const {
  if (!IsValidDate(pRuntime))
    return 0;

  return JS_GetYearFromTime(JS_LocalTime(pRuntime->ToDouble(m_pDate)));
}

void CJS_Date::SetYear(CJS_Runtime* pRuntime, int iYear) {
  m_pDate = pRuntime->NewDate(
      MakeDate(iYear, GetMonth(pRuntime), GetDay(pRuntime), GetHours(pRuntime),
               GetMinutes(pRuntime), GetSeconds(pRuntime), 0));
}

int CJS_Date::GetMonth(CJS_Runtime* pRuntime) const {
  if (!IsValidDate(pRuntime))
    return 0;

  return JS_GetMonthFromTime(JS_LocalTime(pRuntime->ToDouble(m_pDate)));
}

void CJS_Date::SetMonth(CJS_Runtime* pRuntime, int iMonth) {
  m_pDate = pRuntime->NewDate(
      MakeDate(GetYear(pRuntime), iMonth, GetDay(pRuntime), GetHours(pRuntime),
               GetMinutes(pRuntime), GetSeconds(pRuntime), 0));
}

int CJS_Date::GetDay(CJS_Runtime* pRuntime) const {
  if (!IsValidDate(pRuntime))
    return 0;

  return JS_GetDayFromTime(JS_LocalTime(pRuntime->ToDouble(m_pDate)));
}

void CJS_Date::SetDay(CJS_Runtime* pRuntime, int iDay) {
  m_pDate = pRuntime->NewDate(
      MakeDate(GetYear(pRuntime), GetMonth(pRuntime), iDay, GetHours(pRuntime),
               GetMinutes(pRuntime), GetSeconds(pRuntime), 0));
}

int CJS_Date::GetHours(CJS_Runtime* pRuntime) const {
  if (!IsValidDate(pRuntime))
    return 0;

  return JS_GetHourFromTime(JS_LocalTime(pRuntime->ToDouble(m_pDate)));
}

void CJS_Date::SetHours(CJS_Runtime* pRuntime, int iHours) {
  m_pDate = pRuntime->NewDate(
      MakeDate(GetYear(pRuntime), GetMonth(pRuntime), GetDay(pRuntime), iHours,
               GetMinutes(pRuntime), GetSeconds(pRuntime), 0));
}

int CJS_Date::GetMinutes(CJS_Runtime* pRuntime) const {
  if (!IsValidDate(pRuntime))
    return 0;

  return JS_GetMinFromTime(JS_LocalTime(pRuntime->ToDouble(m_pDate)));
}

void CJS_Date::SetMinutes(CJS_Runtime* pRuntime, int minutes) {
  m_pDate = pRuntime->NewDate(MakeDate(GetYear(pRuntime), GetMonth(pRuntime),
                                       GetDay(pRuntime), GetHours(pRuntime),
                                       minutes, GetSeconds(pRuntime), 0));
}

int CJS_Date::GetSeconds(CJS_Runtime* pRuntime) const {
  if (!IsValidDate(pRuntime))
    return 0;

  return JS_GetSecFromTime(JS_LocalTime(pRuntime->ToDouble(m_pDate)));
}

void CJS_Date::SetSeconds(CJS_Runtime* pRuntime, int seconds) {
  m_pDate = pRuntime->NewDate(MakeDate(GetYear(pRuntime), GetMonth(pRuntime),
                                       GetDay(pRuntime), GetHours(pRuntime),
                                       GetMinutes(pRuntime), seconds, 0));
}

double CJS_Date::ToDouble(CJS_Runtime* pRuntime) const {
  return !m_pDate.IsEmpty() ? pRuntime->ToDouble(m_pDate) : 0.0;
}

CFX_WideString CJS_Date::ToString(CJS_Runtime* pRuntime) const {
  return !m_pDate.IsEmpty() ? pRuntime->ToWideString(m_pDate)
                            : CFX_WideString();
}

v8::Local<v8::Date> CJS_Date::ToV8Date(CJS_Runtime* pRuntime) const {
  return m_pDate;
}

double _getLocalTZA() {
  if (!FSDK_IsSandBoxPolicyEnabled(FPDF_POLICY_MACHINETIME_ACCESS))
    return 0;
  time_t t = 0;
  time(&t);
  localtime(&t);
#if _MSC_VER >= 1900
  // In gcc and in Visual Studio prior to VS 2015 'timezone' is a global
  // variable declared in time.h. That variable was deprecated and in VS 2015
  // is removed, with _get_timezone replacing it.
  long timezone = 0;
  _get_timezone(&timezone);
#endif
  return (double)(-(timezone * 1000));
}

int _getDaylightSavingTA(double d) {
  if (!FSDK_IsSandBoxPolicyEnabled(FPDF_POLICY_MACHINETIME_ACCESS))
    return 0;
  time_t t = (time_t)(d / 1000);
  struct tm* tmp = localtime(&t);
  if (!tmp)
    return 0;
  if (tmp->tm_isdst > 0)
    // One hour.
    return (int)60 * 60 * 1000;
  return 0;
}

double _Mod(double x, double y) {
  double r = fmod(x, y);
  if (r < 0)
    r += y;
  return r;
}

int _isfinite(double v) {
#if _MSC_VER
  return ::_finite(v);
#else
  return std::fabs(v) < std::numeric_limits<double>::max();
#endif
}

double _toInteger(double n) {
  return (n >= 0) ? FXSYS_floor(n) : -FXSYS_floor(-n);
}

bool _isLeapYear(int year) {
  return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 != 0));
}

int _DayFromYear(int y) {
  return (int)(365 * (y - 1970.0) + FXSYS_floor((y - 1969.0) / 4) -
               FXSYS_floor((y - 1901.0) / 100) +
               FXSYS_floor((y - 1601.0) / 400));
}

double _TimeFromYear(int y) {
  return 86400000.0 * _DayFromYear(y);
}

static const uint16_t daysMonth[12] = {0,   31,  59,  90,  120, 151,
                                       181, 212, 243, 273, 304, 334};
static const uint16_t leapDaysMonth[12] = {0,   31,  60,  91,  121, 152,
                                           182, 213, 244, 274, 305, 335};

double _TimeFromYearMonth(int y, int m) {
  const uint16_t* pMonth = _isLeapYear(y) ? leapDaysMonth : daysMonth;
  return _TimeFromYear(y) + ((double)pMonth[m]) * 86400000;
}

int _Day(double t) {
  return (int)FXSYS_floor(t / 86400000);
}

int _YearFromTime(double t) {
  // estimate the time.
  int y = 1970 + static_cast<int>(t / (365.2425 * 86400000));
  if (_TimeFromYear(y) <= t) {
    while (_TimeFromYear(y + 1) <= t)
      y++;
  } else {
    while (_TimeFromYear(y) > t)
      y--;
  }
  return y;
}

int _DayWithinYear(double t) {
  int year = _YearFromTime(t);
  int day = _Day(t);
  return day - _DayFromYear(year);
}

int _MonthFromTime(double t) {
  int day = _DayWithinYear(t);
  int year = _YearFromTime(t);
  if (0 <= day && day < 31)
    return 0;
  if (31 <= day && day < 59 + _isLeapYear(year))
    return 1;
  if ((59 + _isLeapYear(year)) <= day && day < (90 + _isLeapYear(year)))
    return 2;
  if ((90 + _isLeapYear(year)) <= day && day < (120 + _isLeapYear(year)))
    return 3;
  if ((120 + _isLeapYear(year)) <= day && day < (151 + _isLeapYear(year)))
    return 4;
  if ((151 + _isLeapYear(year)) <= day && day < (181 + _isLeapYear(year)))
    return 5;
  if ((181 + _isLeapYear(year)) <= day && day < (212 + _isLeapYear(year)))
    return 6;
  if ((212 + _isLeapYear(year)) <= day && day < (243 + _isLeapYear(year)))
    return 7;
  if ((243 + _isLeapYear(year)) <= day && day < (273 + _isLeapYear(year)))
    return 8;
  if ((273 + _isLeapYear(year)) <= day && day < (304 + _isLeapYear(year)))
    return 9;
  if ((304 + _isLeapYear(year)) <= day && day < (334 + _isLeapYear(year)))
    return 10;
  if ((334 + _isLeapYear(year)) <= day && day < (365 + _isLeapYear(year)))
    return 11;

  return -1;
}

int _DateFromTime(double t) {
  int day = _DayWithinYear(t);
  int year = _YearFromTime(t);
  int leap = _isLeapYear(year);
  int month = _MonthFromTime(t);
  switch (month) {
    case 0:
      return day + 1;
    case 1:
      return day - 30;
    case 2:
      return day - 58 - leap;
    case 3:
      return day - 89 - leap;
    case 4:
      return day - 119 - leap;
    case 5:
      return day - 150 - leap;
    case 6:
      return day - 180 - leap;
    case 7:
      return day - 211 - leap;
    case 8:
      return day - 242 - leap;
    case 9:
      return day - 272 - leap;
    case 10:
      return day - 303 - leap;
    case 11:
      return day - 333 - leap;
    default:
      return 0;
  }
}

double JS_GetDateTime() {
  if (!FSDK_IsSandBoxPolicyEnabled(FPDF_POLICY_MACHINETIME_ACCESS))
    return 0;
  time_t t = time(nullptr);
  struct tm* pTm = localtime(&t);

  int year = pTm->tm_year + 1900;
  double t1 = _TimeFromYear(year);

  return t1 + pTm->tm_yday * 86400000.0 + pTm->tm_hour * 3600000.0 +
         pTm->tm_min * 60000.0 + pTm->tm_sec * 1000.0;
}

int JS_GetYearFromTime(double dt) {
  return _YearFromTime(dt);
}

int JS_GetMonthFromTime(double dt) {
  return _MonthFromTime(dt);
}

int JS_GetDayFromTime(double dt) {
  return _DateFromTime(dt);
}

int JS_GetHourFromTime(double dt) {
  return (int)_Mod(floor(dt / (60 * 60 * 1000)), 24);
}

int JS_GetMinFromTime(double dt) {
  return (int)_Mod(floor(dt / (60 * 1000)), 60);
}

int JS_GetSecFromTime(double dt) {
  return (int)_Mod(floor(dt / 1000), 60);
}

double JS_DateParse(const CFX_WideString& str) {
  v8::Isolate* pIsolate = v8::Isolate::GetCurrent();
  v8::Isolate::Scope isolate_scope(pIsolate);
  v8::HandleScope scope(pIsolate);

  v8::Local<v8::Context> context = pIsolate->GetCurrentContext();

  // Use the built-in object method.
  v8::Local<v8::Value> v =
      context->Global()
          ->Get(context, v8::String::NewFromUtf8(pIsolate, "Date",
                                                 v8::NewStringType::kNormal)
                             .ToLocalChecked())
          .ToLocalChecked();
  if (v->IsObject()) {
    v8::Local<v8::Object> o = v->ToObject(context).ToLocalChecked();
    v = o->Get(context, v8::String::NewFromUtf8(pIsolate, "parse",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked())
            .ToLocalChecked();
    if (v->IsFunction()) {
      v8::Local<v8::Function> funC = v8::Local<v8::Function>::Cast(v);
      const int argc = 1;
      v8::Local<v8::Value> timeStr =
          CJS_Runtime::CurrentRuntimeFromIsolate(pIsolate)->NewString(
              str.AsStringC());
      v8::Local<v8::Value> argv[argc] = {timeStr};
      v = funC->Call(context, context->Global(), argc, argv).ToLocalChecked();
      if (v->IsNumber()) {
        double date = v->ToNumber(context).ToLocalChecked()->Value();
        if (!_isfinite(date))
          return date;
        return JS_LocalTime(date);
      }
    }
  }
  return 0;
}

double JS_MakeDay(int nYear, int nMonth, int nDate) {
  if (!_isfinite(nYear) || !_isfinite(nMonth) || !_isfinite(nDate))
    return GetNan();
  double y = _toInteger(nYear);
  double m = _toInteger(nMonth);
  double dt = _toInteger(nDate);
  double ym = y + FXSYS_floor((double)m / 12);
  double mn = _Mod(m, 12);

  double t = _TimeFromYearMonth((int)ym, (int)mn);

  if (_YearFromTime(t) != ym || _MonthFromTime(t) != mn ||
      _DateFromTime(t) != 1)
    return GetNan();
  return _Day(t) + dt - 1;
}

double JS_MakeTime(int nHour, int nMin, int nSec, int nMs) {
  if (!_isfinite(nHour) || !_isfinite(nMin) || !_isfinite(nSec) ||
      !_isfinite(nMs))
    return GetNan();

  double h = _toInteger(nHour);
  double m = _toInteger(nMin);
  double s = _toInteger(nSec);
  double milli = _toInteger(nMs);

  return h * 3600000 + m * 60000 + s * 1000 + milli;
}

double JS_MakeDate(double day, double time) {
  if (!_isfinite(day) || !_isfinite(time))
    return GetNan();

  return day * 86400000 + time;
}

bool JS_PortIsNan(double d) {
  return d != d;
}

double JS_LocalTime(double d) {
  return d + _getLocalTZA() + _getDaylightSavingTA(d);
}

std::vector<CJS_Value> JS_ExpandKeywordParams(
    CJS_Runtime* pRuntime,
    const std::vector<CJS_Value>& originals,
    size_t nKeywords,
    ...) {
  ASSERT(nKeywords);

  std::vector<CJS_Value> result(nKeywords, CJS_Value(pRuntime));
  size_t size = std::min(originals.size(), nKeywords);
  for (size_t i = 0; i < size; ++i)
    result[i] = originals[i];

  if (originals.size() != 1 || originals[0].GetType() != CJS_Value::VT_object ||
      originals[0].IsArrayObject()) {
    return result;
  }
  v8::Local<v8::Object> pObj = originals[0].ToV8Object(pRuntime);
  result[0] = CJS_Value(pRuntime);  // Make unknown.

  va_list ap;
  va_start(ap, nKeywords);
  for (size_t i = 0; i < nKeywords; ++i) {
    const wchar_t* property = va_arg(ap, const wchar_t*);
    v8::Local<v8::Value> v8Value = pRuntime->GetObjectProperty(pObj, property);
    if (!v8Value->IsUndefined())
      result[i] = CJS_Value(pRuntime, v8Value);
  }
  va_end(ap);
  return result;
}
