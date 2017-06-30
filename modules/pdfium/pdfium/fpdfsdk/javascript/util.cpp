// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/util.h"

#include <time.h>

#include <algorithm>
#include <string>
#include <vector>

#include "core/fxcrt/fx_ext.h"
#include "fpdfsdk/javascript/JS_Define.h"
#include "fpdfsdk/javascript/JS_EventHandler.h"
#include "fpdfsdk/javascript/JS_Object.h"
#include "fpdfsdk/javascript/JS_Value.h"
#include "fpdfsdk/javascript/PublicMethods.h"
#include "fpdfsdk/javascript/cjs_event_context.h"
#include "fpdfsdk/javascript/cjs_runtime.h"
#include "fpdfsdk/javascript/resource.h"

#if _FX_OS_ == _FX_ANDROID_
#include <ctype.h>
#endif

JSConstSpec CJS_Util::ConstSpecs[] = {{0, JSConstSpec::Number, 0, 0}};

JSPropertySpec CJS_Util::PropertySpecs[] = {{0, 0, 0}};

JSMethodSpec CJS_Util::MethodSpecs[] = {
    {"printd", printd_static},         {"printf", printf_static},
    {"printx", printx_static},         {"scand", scand_static},
    {"byteToChar", byteToChar_static}, {0, 0}};

IMPLEMENT_JS_CLASS(CJS_Util, util)

#define UTIL_INT 0
#define UTIL_DOUBLE 1
#define UTIL_STRING 2

namespace {

// Map PDF-style directives to equivalent wcsftime directives. Not
// all have direct equivalents, though.
struct TbConvert {
  const FX_WCHAR* lpszJSMark;
  const FX_WCHAR* lpszCppMark;
};

// Map PDF-style directives lacking direct wcsftime directives to
// the value with which they will be replaced.
struct TbConvertAdditional {
  const FX_WCHAR* lpszJSMark;
  int iValue;
};

const TbConvert TbConvertTable[] = {
    {L"mmmm", L"%B"}, {L"mmm", L"%b"}, {L"mm", L"%m"},   {L"dddd", L"%A"},
    {L"ddd", L"%a"},  {L"dd", L"%d"},  {L"yyyy", L"%Y"}, {L"yy", L"%y"},
    {L"HH", L"%H"},   {L"hh", L"%I"},  {L"MM", L"%M"},   {L"ss", L"%S"},
    {L"TT", L"%p"},
#if defined(_WIN32)
    {L"tt", L"%p"},   {L"h", L"%#I"},
#else
    {L"tt", L"%P"},   {L"h", L"%l"},
#endif
};

int ParseDataType(std::wstring* sFormat) {
  bool bPercent = false;
  for (size_t i = 0; i < sFormat->length(); ++i) {
    wchar_t c = (*sFormat)[i];
    if (c == L'%') {
      bPercent = true;
      continue;
    }

    if (bPercent) {
      if (c == L'c' || c == L'C' || c == L'd' || c == L'i' || c == L'o' ||
          c == L'u' || c == L'x' || c == L'X') {
        return UTIL_INT;
      }
      if (c == L'e' || c == L'E' || c == L'f' || c == L'g' || c == L'G') {
        return UTIL_DOUBLE;
      }
      if (c == L's' || c == L'S') {
        // Map s to S since we always deal internally
        // with wchar_t strings.
        (*sFormat)[i] = L'S';
        return UTIL_STRING;
      }
      if (c == L'.' || c == L'+' || c == L'-' || c == L'#' || c == L' ' ||
          FXSYS_iswdigit(c)) {
        continue;
      }
      break;
    }
  }

  return -1;
}

}  // namespace

util::util(CJS_Object* pJSObject) : CJS_EmbedObj(pJSObject) {}

util::~util() {}

bool util::printf(CJS_Runtime* pRuntime,
                  const std::vector<CJS_Value>& params,
                  CJS_Value& vRet,
                  CFX_WideString& sError) {
  int iSize = params.size();
  if (iSize < 1)
    return false;
  std::wstring c_ConvChar(params[0].ToCFXWideString(pRuntime).c_str());
  std::vector<std::wstring> c_strConvers;
  int iOffset = 0;
  int iOffend = 0;
  c_ConvChar.insert(c_ConvChar.begin(), L'S');
  while (iOffset != -1) {
    iOffend = c_ConvChar.find(L"%", iOffset + 1);
    std::wstring strSub;
    if (iOffend == -1)
      strSub = c_ConvChar.substr(iOffset);
    else
      strSub = c_ConvChar.substr(iOffset, iOffend - iOffset);
    c_strConvers.push_back(strSub);
    iOffset = iOffend;
  }

  std::wstring c_strResult;
  std::wstring c_strFormat;
  for (int iIndex = 0; iIndex < (int)c_strConvers.size(); iIndex++) {
    c_strFormat = c_strConvers[iIndex];
    if (iIndex == 0) {
      c_strResult = c_strFormat;
      continue;
    }

    CFX_WideString strSegment;
    if (iIndex >= iSize) {
      c_strResult += c_strFormat;
      continue;
    }

    switch (ParseDataType(&c_strFormat)) {
      case UTIL_INT:
        strSegment.Format(c_strFormat.c_str(), params[iIndex].ToInt(pRuntime));
        break;
      case UTIL_DOUBLE:
        strSegment.Format(c_strFormat.c_str(),
                          params[iIndex].ToDouble(pRuntime));
        break;
      case UTIL_STRING:
        strSegment.Format(c_strFormat.c_str(),
                          params[iIndex].ToCFXWideString(pRuntime).c_str());
        break;
      default:
        strSegment.Format(L"%S", c_strFormat.c_str());
        break;
    }
    c_strResult += strSegment.GetBuffer(strSegment.GetLength() + 1);
  }

  c_strResult.erase(c_strResult.begin());
  vRet = CJS_Value(pRuntime, c_strResult.c_str());
  return true;
}

bool util::printd(CJS_Runtime* pRuntime,
                  const std::vector<CJS_Value>& params,
                  CJS_Value& vRet,
                  CFX_WideString& sError) {
  int iSize = params.size();
  if (iSize < 2)
    return false;

  CJS_Value p1 = params[0];
  CJS_Value p2 = params[1];
  CJS_Date jsDate;
  if (!p2.ConvertToDate(pRuntime, jsDate)) {
    sError = JSGetStringFromID(IDS_STRING_JSPRINT1);
    return false;
  }

  if (!jsDate.IsValidDate(pRuntime)) {
    sError = JSGetStringFromID(IDS_STRING_JSPRINT2);
    return false;
  }

  if (p1.GetType() == CJS_Value::VT_number) {
    CFX_WideString swResult;
    switch (p1.ToInt(pRuntime)) {
      case 0:
        swResult.Format(L"D:%04d%02d%02d%02d%02d%02d", jsDate.GetYear(pRuntime),
                        jsDate.GetMonth(pRuntime) + 1, jsDate.GetDay(pRuntime),
                        jsDate.GetHours(pRuntime), jsDate.GetMinutes(pRuntime),
                        jsDate.GetSeconds(pRuntime));
        break;
      case 1:
        swResult.Format(L"%04d.%02d.%02d %02d:%02d:%02d",
                        jsDate.GetYear(pRuntime), jsDate.GetMonth(pRuntime) + 1,
                        jsDate.GetDay(pRuntime), jsDate.GetHours(pRuntime),
                        jsDate.GetMinutes(pRuntime),
                        jsDate.GetSeconds(pRuntime));
        break;
      case 2:
        swResult.Format(L"%04d/%02d/%02d %02d:%02d:%02d",
                        jsDate.GetYear(pRuntime), jsDate.GetMonth(pRuntime) + 1,
                        jsDate.GetDay(pRuntime), jsDate.GetHours(pRuntime),
                        jsDate.GetMinutes(pRuntime),
                        jsDate.GetSeconds(pRuntime));
        break;
      default:
        sError = JSGetStringFromID(IDS_STRING_JSVALUEERROR);
        return false;
    }

    vRet = CJS_Value(pRuntime, swResult.c_str());
    return true;
  }

  if (p1.GetType() == CJS_Value::VT_string) {
    if (iSize > 2 && params[2].ToBool(pRuntime)) {
      sError = JSGetStringFromID(IDS_STRING_JSNOTSUPPORT);
      return false;  // currently, it doesn't support XFAPicture.
    }

    // Convert PDF-style format specifiers to wcsftime specifiers. Remove any
    // pre-existing %-directives before inserting our own.
    std::basic_string<wchar_t> cFormat = p1.ToCFXWideString(pRuntime).c_str();
    cFormat.erase(std::remove(cFormat.begin(), cFormat.end(), '%'),
                  cFormat.end());

    for (size_t i = 0; i < FX_ArraySize(TbConvertTable); ++i) {
      int iStart = 0;
      int iEnd;
      while ((iEnd = cFormat.find(TbConvertTable[i].lpszJSMark, iStart)) !=
             -1) {
        cFormat.replace(iEnd, FXSYS_wcslen(TbConvertTable[i].lpszJSMark),
                        TbConvertTable[i].lpszCppMark);
        iStart = iEnd;
      }
    }

    int iYear = jsDate.GetYear(pRuntime);
    int iMonth = jsDate.GetMonth(pRuntime);
    int iDay = jsDate.GetDay(pRuntime);
    int iHour = jsDate.GetHours(pRuntime);
    int iMin = jsDate.GetMinutes(pRuntime);
    int iSec = jsDate.GetSeconds(pRuntime);

    TbConvertAdditional cTableAd[] = {
        {L"m", iMonth + 1}, {L"d", iDay},
        {L"H", iHour},      {L"h", iHour > 12 ? iHour - 12 : iHour},
        {L"M", iMin},       {L"s", iSec},
    };

    for (size_t i = 0; i < FX_ArraySize(cTableAd); ++i) {
      wchar_t tszValue[16];
      CFX_WideString sValue;
      sValue.Format(L"%d", cTableAd[i].iValue);
      memcpy(tszValue, (wchar_t*)sValue.GetBuffer(sValue.GetLength() + 1),
             (sValue.GetLength() + 1) * sizeof(wchar_t));

      int iStart = 0;
      int iEnd;
      while ((iEnd = cFormat.find(cTableAd[i].lpszJSMark, iStart)) != -1) {
        if (iEnd > 0) {
          if (cFormat[iEnd - 1] == L'%') {
            iStart = iEnd + 1;
            continue;
          }
        }
        cFormat.replace(iEnd, FXSYS_wcslen(cTableAd[i].lpszJSMark), tszValue);
        iStart = iEnd;
      }
    }

    struct tm time = {};
    time.tm_year = iYear - 1900;
    time.tm_mon = iMonth;
    time.tm_mday = iDay;
    time.tm_hour = iHour;
    time.tm_min = iMin;
    time.tm_sec = iSec;

    wchar_t buf[64] = {};
    wcsftime(buf, 64, cFormat.c_str(), &time);
    cFormat = buf;
    vRet = CJS_Value(pRuntime, cFormat.c_str());
    return true;
  }

  sError = JSGetStringFromID(IDS_STRING_JSTYPEERROR);
  return false;
}

bool util::printx(CJS_Runtime* pRuntime,
                  const std::vector<CJS_Value>& params,
                  CJS_Value& vRet,
                  CFX_WideString& sError) {
  if (params.size() < 2) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }

  vRet = CJS_Value(pRuntime, printx(params[0].ToCFXWideString(pRuntime),
                                    params[1].ToCFXWideString(pRuntime))
                                 .c_str());

  return true;
}

enum CaseMode { kPreserveCase, kUpperCase, kLowerCase };

static FX_WCHAR TranslateCase(FX_WCHAR input, CaseMode eMode) {
  if (eMode == kLowerCase && input >= 'A' && input <= 'Z')
    return input | 0x20;
  if (eMode == kUpperCase && input >= 'a' && input <= 'z')
    return input & ~0x20;
  return input;
}

CFX_WideString util::printx(const CFX_WideString& wsFormat,
                            const CFX_WideString& wsSource) {
  CFX_WideString wsResult;
  FX_STRSIZE iSourceIdx = 0;
  FX_STRSIZE iFormatIdx = 0;
  CaseMode eCaseMode = kPreserveCase;
  bool bEscaped = false;
  while (iFormatIdx < wsFormat.GetLength()) {
    if (bEscaped) {
      bEscaped = false;
      wsResult += wsFormat[iFormatIdx];
      ++iFormatIdx;
      continue;
    }
    switch (wsFormat[iFormatIdx]) {
      case '\\': {
        bEscaped = true;
        ++iFormatIdx;
      } break;
      case '<': {
        eCaseMode = kLowerCase;
        ++iFormatIdx;
      } break;
      case '>': {
        eCaseMode = kUpperCase;
        ++iFormatIdx;
      } break;
      case '=': {
        eCaseMode = kPreserveCase;
        ++iFormatIdx;
      } break;
      case '?': {
        if (iSourceIdx < wsSource.GetLength()) {
          wsResult += TranslateCase(wsSource[iSourceIdx], eCaseMode);
          ++iSourceIdx;
        }
        ++iFormatIdx;
      } break;
      case 'X': {
        if (iSourceIdx < wsSource.GetLength()) {
          if ((wsSource[iSourceIdx] >= '0' && wsSource[iSourceIdx] <= '9') ||
              (wsSource[iSourceIdx] >= 'a' && wsSource[iSourceIdx] <= 'z') ||
              (wsSource[iSourceIdx] >= 'A' && wsSource[iSourceIdx] <= 'Z')) {
            wsResult += TranslateCase(wsSource[iSourceIdx], eCaseMode);
            ++iFormatIdx;
          }
          ++iSourceIdx;
        } else {
          ++iFormatIdx;
        }
      } break;
      case 'A': {
        if (iSourceIdx < wsSource.GetLength()) {
          if ((wsSource[iSourceIdx] >= 'a' && wsSource[iSourceIdx] <= 'z') ||
              (wsSource[iSourceIdx] >= 'A' && wsSource[iSourceIdx] <= 'Z')) {
            wsResult += TranslateCase(wsSource[iSourceIdx], eCaseMode);
            ++iFormatIdx;
          }
          ++iSourceIdx;
        } else {
          ++iFormatIdx;
        }
      } break;
      case '9': {
        if (iSourceIdx < wsSource.GetLength()) {
          if (wsSource[iSourceIdx] >= '0' && wsSource[iSourceIdx] <= '9') {
            wsResult += wsSource[iSourceIdx];
            ++iFormatIdx;
          }
          ++iSourceIdx;
        } else {
          ++iFormatIdx;
        }
      } break;
      case '*': {
        if (iSourceIdx < wsSource.GetLength()) {
          wsResult += TranslateCase(wsSource[iSourceIdx], eCaseMode);
          ++iSourceIdx;
        } else {
          ++iFormatIdx;
        }
      } break;
      default: {
        wsResult += wsFormat[iFormatIdx];
        ++iFormatIdx;
      } break;
    }
  }
  return wsResult;
}

bool util::scand(CJS_Runtime* pRuntime,
                 const std::vector<CJS_Value>& params,
                 CJS_Value& vRet,
                 CFX_WideString& sError) {
  int iSize = params.size();
  if (iSize < 2)
    return false;

  CFX_WideString sFormat = params[0].ToCFXWideString(pRuntime);
  CFX_WideString sDate = params[1].ToCFXWideString(pRuntime);
  double dDate = JS_GetDateTime();
  if (sDate.GetLength() > 0) {
    dDate = CJS_PublicMethods::MakeRegularDate(sDate, sFormat, nullptr);
  }

  if (!JS_PortIsNan(dDate)) {
    vRet = CJS_Value(pRuntime, CJS_Date(pRuntime, dDate));
  } else {
    vRet.SetNull(pRuntime);
  }

  return true;
}

bool util::byteToChar(CJS_Runtime* pRuntime,
                      const std::vector<CJS_Value>& params,
                      CJS_Value& vRet,
                      CFX_WideString& sError) {
  if (params.size() < 1) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }

  int arg = params[0].ToInt(pRuntime);
  if (arg < 0 || arg > 255) {
    sError = JSGetStringFromID(IDS_STRING_JSVALUEERROR);
    return false;
  }

  CFX_WideString wStr(static_cast<FX_WCHAR>(arg));
  vRet = CJS_Value(pRuntime, wStr.c_str());
  return true;
}
