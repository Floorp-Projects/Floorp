// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_JAVASCRIPT_JS_VALUE_H_
#define FPDFSDK_JAVASCRIPT_JS_VALUE_H_

#include <vector>

#include "core/fxcrt/fx_basic.h"
#include "fxjs/fxjs_v8.h"

class CJS_Array;
class CJS_Date;
class CJS_Document;
class CJS_Object;
class CJS_Runtime;

class CJS_Value {
 public:
  enum Type {
    VT_unknown,
    VT_string,
    VT_number,
    VT_boolean,
    VT_date,
    VT_object,
    VT_null,
    VT_undefined
  };

  explicit CJS_Value(CJS_Runtime* pRuntime);
  CJS_Value(CJS_Runtime* pRuntime, v8::Local<v8::Value> pValue);
  CJS_Value(CJS_Runtime* pRuntime, const int& iValue);
  CJS_Value(CJS_Runtime* pRuntime, const double& dValue);
  CJS_Value(CJS_Runtime* pRuntime, const float& fValue);
  CJS_Value(CJS_Runtime* pRuntime, const bool& bValue);
  CJS_Value(CJS_Runtime* pRuntime, CJS_Object* pObj);
  CJS_Value(CJS_Runtime* pRuntime, const FX_CHAR* pStr);
  CJS_Value(CJS_Runtime* pRuntime, const FX_WCHAR* pWstr);
  CJS_Value(CJS_Runtime* pRuntime, const CJS_Array& array);
  CJS_Value(CJS_Runtime* pRuntime, const CJS_Date& date);
  CJS_Value(CJS_Runtime* pRuntime, const CJS_Object* object);
  CJS_Value(const CJS_Value& other);

  ~CJS_Value();

  void SetNull(CJS_Runtime* pRuntime);
  void SetValue(const CJS_Value& other);
  void Attach(v8::Local<v8::Value> pValue);
  void Detach();

  static Type GetValueType(v8::Local<v8::Value> value);
  Type GetType() const { return GetValueType(m_pValue); }

  int ToInt(CJS_Runtime* pRuntime) const;
  bool ToBool(CJS_Runtime* pRuntime) const;
  double ToDouble(CJS_Runtime* pRuntime) const;
  float ToFloat(CJS_Runtime* pRuntime) const;
  CJS_Object* ToCJSObject(CJS_Runtime* pRuntime) const;
  CFX_WideString ToCFXWideString(CJS_Runtime* pRuntime) const;
  CFX_ByteString ToCFXByteString(CJS_Runtime* pRuntime) const;
  v8::Local<v8::Object> ToV8Object(CJS_Runtime* pRuntime) const;
  v8::Local<v8::Array> ToV8Array(CJS_Runtime* pRuntime) const;
  v8::Local<v8::Value> ToV8Value(CJS_Runtime* pRuntime) const;

  // Replace the current |m_pValue| with a v8::Number if possible
  // to make one from the current |m_pValue|.
  void MaybeCoerceToNumber(CJS_Runtime* pRuntime);

  bool IsArrayObject() const;
  bool IsDateObject() const;
  bool ConvertToArray(CJS_Runtime* pRuntime, CJS_Array&) const;
  bool ConvertToDate(CJS_Runtime* pRuntime, CJS_Date&) const;

 protected:
  v8::Local<v8::Value> m_pValue;
};

class CJS_PropValue {
 public:
  explicit CJS_PropValue(CJS_Runtime* pRuntime);
  CJS_PropValue(CJS_Runtime* pRuntime, const CJS_Value&);
  ~CJS_PropValue();

  void StartSetting() { m_bIsSetting = true; }
  void StartGetting() { m_bIsSetting = false; }
  bool IsSetting() const { return m_bIsSetting; }
  bool IsGetting() const { return !m_bIsSetting; }
  CJS_Runtime* GetJSRuntime() const { return m_pJSRuntime; }
  CJS_Value* GetJSValue() { return &m_Value; }

  // These calls may re-enter JS (and hence invalidate objects).
  void operator<<(int val);
  void operator>>(int&) const;
  void operator<<(bool val);
  void operator>>(bool&) const;
  void operator<<(double val);
  void operator>>(double&) const;
  void operator<<(CJS_Object* pObj);
  void operator>>(CJS_Object*& ppObj) const;
  void operator<<(CJS_Document* pJsDoc);
  void operator>>(CJS_Document*& ppJsDoc) const;
  void operator<<(CFX_ByteString);
  void operator>>(CFX_ByteString&) const;
  void operator<<(CFX_WideString);
  void operator>>(CFX_WideString&) const;
  void operator<<(const FX_WCHAR* c_string);
  void operator<<(v8::Local<v8::Object>);
  void operator>>(v8::Local<v8::Object>&) const;
  void operator>>(CJS_Array& array) const;
  void operator<<(CJS_Array& array);
  void operator<<(CJS_Date& date);
  void operator>>(CJS_Date& date) const;

 private:
  bool m_bIsSetting;
  CJS_Value m_Value;
  CJS_Runtime* const m_pJSRuntime;
};

class CJS_Array {
 public:
  CJS_Array();
  CJS_Array(const CJS_Array& other);
  virtual ~CJS_Array();

  void Attach(v8::Local<v8::Array> pArray);
  int GetLength(CJS_Runtime* pRuntime) const;

  // These two calls may re-enter JS (and hence invalidate objects).
  void GetElement(CJS_Runtime* pRuntime,
                  unsigned index,
                  CJS_Value& value) const;
  void SetElement(CJS_Runtime* pRuntime,
                  unsigned index,
                  const CJS_Value& value);

  v8::Local<v8::Array> ToV8Array(CJS_Runtime* pRuntime) const;

 private:
  mutable v8::Local<v8::Array> m_pArray;
};

class CJS_Date {
 public:
  CJS_Date();
  CJS_Date(CJS_Runtime* pRuntime, double dMsec_time);
  CJS_Date(CJS_Runtime* pRuntime,
           int year,
           int mon,
           int day,
           int hour,
           int min,
           int sec);
  virtual ~CJS_Date();

  void Attach(v8::Local<v8::Date> pDate);
  bool IsValidDate(CJS_Runtime* pRuntime) const;

  int GetYear(CJS_Runtime* pRuntime) const;
  void SetYear(CJS_Runtime* pRuntime, int iYear);

  int GetMonth(CJS_Runtime* pRuntime) const;
  void SetMonth(CJS_Runtime* pRuntime, int iMonth);

  int GetDay(CJS_Runtime* pRuntime) const;
  void SetDay(CJS_Runtime* pRuntime, int iDay);

  int GetHours(CJS_Runtime* pRuntime) const;
  void SetHours(CJS_Runtime* pRuntime, int iHours);

  int GetMinutes(CJS_Runtime* pRuntime) const;
  void SetMinutes(CJS_Runtime* pRuntime, int minutes);

  int GetSeconds(CJS_Runtime* pRuntime) const;
  void SetSeconds(CJS_Runtime* pRuntime, int seconds);

  v8::Local<v8::Date> ToV8Date(CJS_Runtime* pRuntime) const;
  double ToDouble(CJS_Runtime* pRuntime) const;
  CFX_WideString ToString(CJS_Runtime* pRuntime) const;

 protected:
  v8::Local<v8::Date> m_pDate;
};

double JS_GetDateTime();
int JS_GetYearFromTime(double dt);
int JS_GetMonthFromTime(double dt);
int JS_GetDayFromTime(double dt);
int JS_GetHourFromTime(double dt);
int JS_GetMinFromTime(double dt);
int JS_GetSecFromTime(double dt);
double JS_DateParse(const CFX_WideString& str);
double JS_MakeDay(int nYear, int nMonth, int nDay);
double JS_MakeTime(int nHour, int nMin, int nSec, int nMs);
double JS_MakeDate(double day, double time);
bool JS_PortIsNan(double d);
double JS_LocalTime(double d);

// Some JS methods have the bizarre convention that they may also be called
// with a single argument which is an object containing the actual arguments
// as its properties. The varying arguments to this method are the property
// names as wchar_t string literals corresponding to each positional argument.
// The result will always contain |nKeywords| value, with unspecified ones
// being set to type VT_unknown.
std::vector<CJS_Value> JS_ExpandKeywordParams(
    CJS_Runtime* pRuntime,
    const std::vector<CJS_Value>& originals,
    size_t nKeywords,
    ...);

#endif  // FPDFSDK_JAVASCRIPT_JS_VALUE_H_
