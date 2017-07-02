// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/color.h"

#include <vector>

#include "fpdfsdk/javascript/JS_Define.h"
#include "fpdfsdk/javascript/JS_EventHandler.h"
#include "fpdfsdk/javascript/JS_Object.h"
#include "fpdfsdk/javascript/JS_Value.h"
#include "fpdfsdk/javascript/cjs_event_context.h"
#include "fpdfsdk/javascript/cjs_runtime.h"

JSConstSpec CJS_Color::ConstSpecs[] = {{0, JSConstSpec::Number, 0, 0}};

JSPropertySpec CJS_Color::PropertySpecs[] = {
    {"black", get_black_static, set_black_static},
    {"blue", get_blue_static, set_blue_static},
    {"cyan", get_cyan_static, set_cyan_static},
    {"dkGray", get_dkGray_static, set_dkGray_static},
    {"gray", get_gray_static, set_gray_static},
    {"green", get_green_static, set_green_static},
    {"ltGray", get_ltGray_static, set_ltGray_static},
    {"magenta", get_magenta_static, set_magenta_static},
    {"red", get_red_static, set_red_static},
    {"transparent", get_transparent_static, set_transparent_static},
    {"white", get_white_static, set_white_static},
    {"yellow", get_yellow_static, set_yellow_static},
    {0, 0, 0}};

JSMethodSpec CJS_Color::MethodSpecs[] = {{"convert", convert_static},
                                         {"equal", equal_static},
                                         {0, 0}};

IMPLEMENT_JS_CLASS(CJS_Color, color)

color::color(CJS_Object* pJSObject) : CJS_EmbedObj(pJSObject) {
  m_crTransparent = CPWL_Color(COLORTYPE_TRANSPARENT);
  m_crBlack = CPWL_Color(COLORTYPE_GRAY, 0);
  m_crWhite = CPWL_Color(COLORTYPE_GRAY, 1);
  m_crRed = CPWL_Color(COLORTYPE_RGB, 1, 0, 0);
  m_crGreen = CPWL_Color(COLORTYPE_RGB, 0, 1, 0);
  m_crBlue = CPWL_Color(COLORTYPE_RGB, 0, 0, 1);
  m_crCyan = CPWL_Color(COLORTYPE_CMYK, 1, 0, 0, 0);
  m_crMagenta = CPWL_Color(COLORTYPE_CMYK, 0, 1, 0, 0);
  m_crYellow = CPWL_Color(COLORTYPE_CMYK, 0, 0, 1, 0);
  m_crDKGray = CPWL_Color(COLORTYPE_GRAY, 0.25);
  m_crGray = CPWL_Color(COLORTYPE_GRAY, 0.5);
  m_crLTGray = CPWL_Color(COLORTYPE_GRAY, 0.75);
}

color::~color() {}

void color::ConvertPWLColorToArray(CJS_Runtime* pRuntime,
                                   const CPWL_Color& color,
                                   CJS_Array* array) {
  switch (color.nColorType) {
    case COLORTYPE_TRANSPARENT:
      array->SetElement(pRuntime, 0, CJS_Value(pRuntime, "T"));
      break;
    case COLORTYPE_GRAY:
      array->SetElement(pRuntime, 0, CJS_Value(pRuntime, "G"));
      array->SetElement(pRuntime, 1, CJS_Value(pRuntime, color.fColor1));
      break;
    case COLORTYPE_RGB:
      array->SetElement(pRuntime, 0, CJS_Value(pRuntime, "RGB"));
      array->SetElement(pRuntime, 1, CJS_Value(pRuntime, color.fColor1));
      array->SetElement(pRuntime, 2, CJS_Value(pRuntime, color.fColor2));
      array->SetElement(pRuntime, 3, CJS_Value(pRuntime, color.fColor3));
      break;
    case COLORTYPE_CMYK:
      array->SetElement(pRuntime, 0, CJS_Value(pRuntime, "CMYK"));
      array->SetElement(pRuntime, 1, CJS_Value(pRuntime, color.fColor1));
      array->SetElement(pRuntime, 2, CJS_Value(pRuntime, color.fColor2));
      array->SetElement(pRuntime, 3, CJS_Value(pRuntime, color.fColor3));
      array->SetElement(pRuntime, 4, CJS_Value(pRuntime, color.fColor4));
      break;
  }
}

void color::ConvertArrayToPWLColor(CJS_Runtime* pRuntime,
                                   const CJS_Array& array,
                                   CPWL_Color* color) {
  int nArrayLen = array.GetLength(pRuntime);
  if (nArrayLen < 1)
    return;

  CJS_Value value(pRuntime);
  array.GetElement(pRuntime, 0, value);
  CFX_ByteString sSpace = value.ToCFXByteString(pRuntime);

  double d1 = 0;
  double d2 = 0;
  double d3 = 0;
  double d4 = 0;

  if (nArrayLen > 1) {
    array.GetElement(pRuntime, 1, value);
    d1 = value.ToDouble(pRuntime);
  }

  if (nArrayLen > 2) {
    array.GetElement(pRuntime, 2, value);
    d2 = value.ToDouble(pRuntime);
  }

  if (nArrayLen > 3) {
    array.GetElement(pRuntime, 3, value);
    d3 = value.ToDouble(pRuntime);
  }

  if (nArrayLen > 4) {
    array.GetElement(pRuntime, 4, value);
    d4 = value.ToDouble(pRuntime);
  }

  if (sSpace == "T") {
    *color = CPWL_Color(COLORTYPE_TRANSPARENT);
  } else if (sSpace == "G") {
    *color = CPWL_Color(COLORTYPE_GRAY, (FX_FLOAT)d1);
  } else if (sSpace == "RGB") {
    *color =
        CPWL_Color(COLORTYPE_RGB, (FX_FLOAT)d1, (FX_FLOAT)d2, (FX_FLOAT)d3);
  } else if (sSpace == "CMYK") {
    *color = CPWL_Color(COLORTYPE_CMYK, (FX_FLOAT)d1, (FX_FLOAT)d2,
                        (FX_FLOAT)d3, (FX_FLOAT)d4);
  }
}

bool color::transparent(CJS_Runtime* pRuntime,
                        CJS_PropValue& vp,
                        CFX_WideString& sError) {
  return PropertyHelper(pRuntime, vp, &m_crTransparent);
}

bool color::black(CJS_Runtime* pRuntime,
                  CJS_PropValue& vp,
                  CFX_WideString& sError) {
  return PropertyHelper(pRuntime, vp, &m_crBlack);
}

bool color::white(CJS_Runtime* pRuntime,
                  CJS_PropValue& vp,
                  CFX_WideString& sError) {
  return PropertyHelper(pRuntime, vp, &m_crWhite);
}

bool color::red(CJS_Runtime* pRuntime,
                CJS_PropValue& vp,
                CFX_WideString& sError) {
  return PropertyHelper(pRuntime, vp, &m_crRed);
}

bool color::green(CJS_Runtime* pRuntime,
                  CJS_PropValue& vp,
                  CFX_WideString& sError) {
  return PropertyHelper(pRuntime, vp, &m_crGreen);
}

bool color::blue(CJS_Runtime* pRuntime,
                 CJS_PropValue& vp,
                 CFX_WideString& sError) {
  return PropertyHelper(pRuntime, vp, &m_crBlue);
}

bool color::cyan(CJS_Runtime* pRuntime,
                 CJS_PropValue& vp,
                 CFX_WideString& sError) {
  return PropertyHelper(pRuntime, vp, &m_crCyan);
}

bool color::magenta(CJS_Runtime* pRuntime,
                    CJS_PropValue& vp,
                    CFX_WideString& sError) {
  return PropertyHelper(pRuntime, vp, &m_crMagenta);
}

bool color::yellow(CJS_Runtime* pRuntime,
                   CJS_PropValue& vp,
                   CFX_WideString& sError) {
  return PropertyHelper(pRuntime, vp, &m_crYellow);
}

bool color::dkGray(CJS_Runtime* pRuntime,
                   CJS_PropValue& vp,
                   CFX_WideString& sError) {
  return PropertyHelper(pRuntime, vp, &m_crDKGray);
}

bool color::gray(CJS_Runtime* pRuntime,
                 CJS_PropValue& vp,
                 CFX_WideString& sError) {
  return PropertyHelper(pRuntime, vp, &m_crGray);
}

bool color::ltGray(CJS_Runtime* pRuntime,
                   CJS_PropValue& vp,
                   CFX_WideString& sError) {
  return PropertyHelper(pRuntime, vp, &m_crLTGray);
}

bool color::PropertyHelper(CJS_Runtime* pRuntime,
                           CJS_PropValue& vp,
                           CPWL_Color* var) {
  CJS_Array array;
  if (vp.IsGetting()) {
    ConvertPWLColorToArray(pRuntime, *var, &array);
    vp << array;
    return true;
  }
  if (!vp.GetJSValue()->ConvertToArray(pRuntime, array))
    return false;

  ConvertArrayToPWLColor(pRuntime, array, var);
  return true;
}

bool color::convert(CJS_Runtime* pRuntime,
                    const std::vector<CJS_Value>& params,
                    CJS_Value& vRet,
                    CFX_WideString& sError) {
  int iSize = params.size();
  if (iSize < 2)
    return false;

  CJS_Array aSource;
  if (!params[0].ConvertToArray(pRuntime, aSource))
    return false;

  CPWL_Color crSource;
  ConvertArrayToPWLColor(pRuntime, aSource, &crSource);

  CFX_ByteString sDestSpace = params[1].ToCFXByteString(pRuntime);
  int nColorType = COLORTYPE_TRANSPARENT;

  if (sDestSpace == "T") {
    nColorType = COLORTYPE_TRANSPARENT;
  } else if (sDestSpace == "G") {
    nColorType = COLORTYPE_GRAY;
  } else if (sDestSpace == "RGB") {
    nColorType = COLORTYPE_RGB;
  } else if (sDestSpace == "CMYK") {
    nColorType = COLORTYPE_CMYK;
  }

  CJS_Array aDest;
  CPWL_Color crDest = crSource.ConvertColorType(nColorType);
  ConvertPWLColorToArray(pRuntime, crDest, &aDest);
  vRet = CJS_Value(pRuntime, aDest);

  return true;
}

bool color::equal(CJS_Runtime* pRuntime,
                  const std::vector<CJS_Value>& params,
                  CJS_Value& vRet,
                  CFX_WideString& sError) {
  if (params.size() < 2)
    return false;

  CJS_Array array1;
  CJS_Array array2;
  if (!params[0].ConvertToArray(pRuntime, array1))
    return false;
  if (!params[1].ConvertToArray(pRuntime, array2))
    return false;

  CPWL_Color color1;
  CPWL_Color color2;
  ConvertArrayToPWLColor(pRuntime, array1, &color1);
  ConvertArrayToPWLColor(pRuntime, array2, &color2);
  color1 = color1.ConvertColorType(color2.nColorType);
  vRet = CJS_Value(pRuntime, color1 == color2);
  return true;
}
