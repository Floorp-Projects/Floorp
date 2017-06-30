// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/Consts.h"

#include "fpdfsdk/javascript/JS_Define.h"
#include "fpdfsdk/javascript/JS_Object.h"
#include "fpdfsdk/javascript/JS_Value.h"

JSConstSpec CJS_Border::ConstSpecs[] = {
    {"s", JSConstSpec::String, 0, "solid"},
    {"b", JSConstSpec::String, 0, "beveled"},
    {"d", JSConstSpec::String, 0, "dashed"},
    {"i", JSConstSpec::String, 0, "inset"},
    {"u", JSConstSpec::String, 0, "underline"},
    {0, JSConstSpec::Number, 0, 0}};
IMPLEMENT_JS_CLASS_CONST(CJS_Border, border)

JSConstSpec CJS_Display::ConstSpecs[] = {{"visible", JSConstSpec::Number, 0, 0},
                                         {"hidden", JSConstSpec::Number, 1, 0},
                                         {"noPrint", JSConstSpec::Number, 2, 0},
                                         {"noView", JSConstSpec::Number, 3, 0},
                                         {0, JSConstSpec::Number, 0, 0}};
IMPLEMENT_JS_CLASS_CONST(CJS_Display, display)

JSConstSpec CJS_Font::ConstSpecs[] = {
    {"Times", JSConstSpec::String, 0, "Times-Roman"},
    {"TimesB", JSConstSpec::String, 0, "Times-Bold"},
    {"TimesI", JSConstSpec::String, 0, "Times-Italic"},
    {"TimesBI", JSConstSpec::String, 0, "Times-BoldItalic"},
    {"Helv", JSConstSpec::String, 0, "Helvetica"},
    {"HelvB", JSConstSpec::String, 0, "Helvetica-Bold"},
    {"HelvI", JSConstSpec::String, 0, "Helvetica-Oblique"},
    {"HelvBI", JSConstSpec::String, 0, "Helvetica-BoldOblique"},
    {"Cour", JSConstSpec::String, 0, "Courier"},
    {"CourB", JSConstSpec::String, 0, "Courier-Bold"},
    {"CourI", JSConstSpec::String, 0, "Courier-Oblique"},
    {"CourBI", JSConstSpec::String, 0, "Courier-BoldOblique"},
    {"Symbol", JSConstSpec::String, 0, "Symbol"},
    {"ZapfD", JSConstSpec::String, 0, "ZapfDingbats"},
    {0, JSConstSpec::Number, 0, 0}};
IMPLEMENT_JS_CLASS_CONST(CJS_Font, font)

JSConstSpec CJS_Highlight::ConstSpecs[] = {
    {"n", JSConstSpec::String, 0, "none"},
    {"i", JSConstSpec::String, 0, "invert"},
    {"p", JSConstSpec::String, 0, "push"},
    {"o", JSConstSpec::String, 0, "outline"},
    {0, JSConstSpec::Number, 0, 0}};
IMPLEMENT_JS_CLASS_CONST(CJS_Highlight, highlight)

JSConstSpec CJS_Position::ConstSpecs[] = {
    {"textOnly", JSConstSpec::Number, 0, 0},
    {"iconOnly", JSConstSpec::Number, 1, 0},
    {"iconTextV", JSConstSpec::Number, 2, 0},
    {"textIconV", JSConstSpec::Number, 3, 0},
    {"iconTextH", JSConstSpec::Number, 4, 0},
    {"textIconH", JSConstSpec::Number, 5, 0},
    {"overlay", JSConstSpec::Number, 6, 0},
    {0, JSConstSpec::Number, 0, 0}};
IMPLEMENT_JS_CLASS_CONST(CJS_Position, position)

JSConstSpec CJS_ScaleHow::ConstSpecs[] = {
    {"proportional", JSConstSpec::Number, 0, 0},
    {"anamorphic", JSConstSpec::Number, 1, 0},
    {0, JSConstSpec::Number, 0, 0}};
IMPLEMENT_JS_CLASS_CONST(CJS_ScaleHow, scaleHow)

JSConstSpec CJS_ScaleWhen::ConstSpecs[] = {
    {"always", JSConstSpec::Number, 0, 0},
    {"never", JSConstSpec::Number, 1, 0},
    {"tooBig", JSConstSpec::Number, 2, 0},
    {"tooSmall", JSConstSpec::Number, 3, 0},
    {0, JSConstSpec::Number, 0, 0}};
IMPLEMENT_JS_CLASS_CONST(CJS_ScaleWhen, scaleWhen)

JSConstSpec CJS_Style::ConstSpecs[] = {
    {"ch", JSConstSpec::String, 0, "check"},
    {"cr", JSConstSpec::String, 0, "cross"},
    {"di", JSConstSpec::String, 0, "diamond"},
    {"ci", JSConstSpec::String, 0, "circle"},
    {"st", JSConstSpec::String, 0, "star"},
    {"sq", JSConstSpec::String, 0, "square"},
    {0, JSConstSpec::Number, 0, 0}};
IMPLEMENT_JS_CLASS_CONST(CJS_Style, style)

JSConstSpec CJS_Zoomtype::ConstSpecs[] = {
    {"none", JSConstSpec::String, 0, "NoVary"},
    {"fitP", JSConstSpec::String, 0, "FitPage"},
    {"fitW", JSConstSpec::String, 0, "FitWidth"},
    {"fitH", JSConstSpec::String, 0, "FitHeight"},
    {"fitV", JSConstSpec::String, 0, "FitVisibleWidth"},
    {"pref", JSConstSpec::String, 0, "Preferred"},
    {"refW", JSConstSpec::String, 0, "ReflowWidth"},
    {0, JSConstSpec::Number, 0, 0}};
IMPLEMENT_JS_CLASS_CONST(CJS_Zoomtype, zoomtype)

#define GLOBAL_STRING(rt, name, value)                                \
  (rt)->DefineGlobalConst(                                            \
      (name), [](const v8::FunctionCallbackInfo<v8::Value>& info) {   \
        info.GetReturnValue().Set(                                    \
            CFXJS_Engine::CurrentEngineFromIsolate(info.GetIsolate()) \
                ->NewString(value));                                  \
      })

void CJS_GlobalConsts::DefineJSObjects(CJS_Runtime* pRuntime) {
  GLOBAL_STRING(pRuntime, L"IDS_GREATER_THAN",
                L"Invalid value: must be greater than or equal to % s.");

  GLOBAL_STRING(pRuntime, L"IDS_GT_AND_LT",
                L"Invalid value: must be greater than or equal to % s "
                L"and less than or equal to % s.");

  GLOBAL_STRING(pRuntime, L"IDS_LESS_THAN",
                L"Invalid value: must be less than or equal to % s.");

  GLOBAL_STRING(pRuntime, L"IDS_INVALID_MONTH", L"**Invalid**");
  GLOBAL_STRING(
      pRuntime, L"IDS_INVALID_DATE",
      L"Invalid date / time: please ensure that the date / time exists.Field");

  GLOBAL_STRING(pRuntime, L"IDS_INVALID_VALUE",
                L"The value entered does not match the format of the field");

  GLOBAL_STRING(pRuntime, L"IDS_AM", L"am");
  GLOBAL_STRING(pRuntime, L"IDS_PM", L"pm");
  GLOBAL_STRING(pRuntime, L"IDS_MONTH_INFO",
                L"January[1] February[2] March[3] April[4] May[5] "
                L"June[6] July[7] August[8] September[9] October[10] "
                L"November[11] December[12] Sept[9] Jan[1] Feb[2] Mar[3] "
                L"Apr[4] Jun[6] Jul[7] Aug[8] Sep[9] Oct[10] Nov[11] "
                L"Dec[12]");

  GLOBAL_STRING(pRuntime, L"IDS_STARTUP_CONSOLE_MSG", L"** ^ _ ^ **");
}

#define GLOBAL_ARRAY(rt, name, ...)                                          \
  {                                                                          \
    const FX_WCHAR* values[] = {__VA_ARGS__};                                \
    v8::Local<v8::Array> array = (rt)->NewArray();                           \
    for (size_t i = 0; i < FX_ArraySize(values); ++i)                        \
      array->Set(i, (rt)->NewString(values[i]));                             \
    (rt)->SetConstArray((name), array);                                      \
    (rt)->DefineGlobalConst(                                                 \
        (name), [](const v8::FunctionCallbackInfo<v8::Value>& info) {        \
          CJS_Runtime* pCurrentRuntime =                                     \
              CJS_Runtime::CurrentRuntimeFromIsolate(info.GetIsolate());     \
          if (pCurrentRuntime)                                               \
            info.GetReturnValue().Set(pCurrentRuntime->GetConstArray(name)); \
        });                                                                  \
  }

void CJS_GlobalArrays::DefineJSObjects(CJS_Runtime* pRuntime) {
  GLOBAL_ARRAY(pRuntime, L"RE_NUMBER_ENTRY_DOT_SEP", L"[+-]?\\d*\\.?\\d*");
  GLOBAL_ARRAY(pRuntime, L"RE_NUMBER_COMMIT_DOT_SEP",
               L"[+-]?\\d+(\\.\\d+)?",  // -1.0 or -1
               L"[+-]?\\.\\d+",         // -.1
               L"[+-]?\\d+\\.");        // -1.

  GLOBAL_ARRAY(pRuntime, L"RE_NUMBER_ENTRY_COMMA_SEP", L"[+-]?\\d*,?\\d*");
  GLOBAL_ARRAY(pRuntime, L"RE_NUMBER_COMMIT_COMMA_SEP",
               L"[+-]?\\d+([.,]\\d+)?",  // -1,0 or -1
               L"[+-]?[.,]\\d+",         // -,1
               L"[+-]?\\d+[.,]");        // -1,

  GLOBAL_ARRAY(pRuntime, L"RE_ZIP_ENTRY", L"\\d{0,5}");
  GLOBAL_ARRAY(pRuntime, L"RE_ZIP_COMMIT", L"\\d{5}");
  GLOBAL_ARRAY(pRuntime, L"RE_ZIP4_ENTRY", L"\\d{0,5}(\\.|[- ])?\\d{0,4}");
  GLOBAL_ARRAY(pRuntime, L"RE_ZIP4_COMMIT", L"\\d{5}(\\.|[- ])?\\d{4}");
  GLOBAL_ARRAY(pRuntime, L"RE_PHONE_ENTRY",
               // 555-1234 or 408 555-1234
               L"\\d{0,3}(\\.|[- ])?\\d{0,3}(\\.|[- ])?\\d{0,4}",

               // (408
               L"\\(\\d{0,3}",

               // (408) 555-1234
               // (allow the addition of parens as an afterthought)
               L"\\(\\d{0,3}\\)(\\.|[- ])?\\d{0,3}(\\.|[- ])?\\d{0,4}",

               // (408 555-1234
               L"\\(\\d{0,3}(\\.|[- ])?\\d{0,3}(\\.|[- ])?\\d{0,4}",

               // 408) 555-1234
               L"\\d{0,3}\\)(\\.|[- ])?\\d{0,3}(\\.|[- ])?\\d{0,4}",

               // international
               L"011(\\.|[- \\d])*");

  GLOBAL_ARRAY(
      pRuntime, L"RE_PHONE_COMMIT", L"\\d{3}(\\.|[- ])?\\d{4}",  // 555-1234
      L"\\d{3}(\\.|[- ])?\\d{3}(\\.|[- ])?\\d{4}",               // 408 555-1234
      L"\\(\\d{3}\\)(\\.|[- ])?\\d{3}(\\.|[- ])?\\d{4}",  // (408) 555-1234
      L"011(\\.|[- \\d])*");                              // international

  GLOBAL_ARRAY(pRuntime, L"RE_SSN_ENTRY",
               L"\\d{0,3}(\\.|[- ])?\\d{0,2}(\\.|[- ])?\\d{0,4}");

  GLOBAL_ARRAY(pRuntime, L"RE_SSN_COMMIT",
               L"\\d{3}(\\.|[- ])?\\d{2}(\\.|[- ])?\\d{4}");
}
