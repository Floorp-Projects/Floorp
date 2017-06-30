// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_dest.h"

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"

namespace {

const FX_CHAR* const g_sZoomModes[] = {"XYZ",  "Fit",   "FitH",  "FitV", "FitR",
                                       "FitB", "FitBH", "FitBV", nullptr};

}  // namespace

int CPDF_Dest::GetPageIndex(CPDF_Document* pDoc) {
  CPDF_Array* pArray = ToArray(m_pObj);
  if (!pArray)
    return 0;

  CPDF_Object* pPage = pArray->GetDirectObjectAt(0);
  if (!pPage)
    return 0;
  if (pPage->IsNumber())
    return pPage->GetInteger();
  if (!pPage->IsDictionary())
    return 0;
  return pDoc->GetPageIndex(pPage->GetObjNum());
}

uint32_t CPDF_Dest::GetPageObjNum() {
  CPDF_Array* pArray = ToArray(m_pObj);
  if (!pArray)
    return 0;

  CPDF_Object* pPage = pArray->GetDirectObjectAt(0);
  if (!pPage)
    return 0;
  if (pPage->IsNumber())
    return pPage->GetInteger();
  if (pPage->IsDictionary())
    return pPage->GetObjNum();
  return 0;
}

int CPDF_Dest::GetZoomMode() {
  CPDF_Array* pArray = ToArray(m_pObj);
  if (!pArray)
    return 0;

  CPDF_Object* pObj = pArray->GetDirectObjectAt(1);
  if (!pObj)
    return 0;

  CFX_ByteString mode = pObj->GetString();
  for (int i = 0; g_sZoomModes[i]; ++i) {
    if (mode == g_sZoomModes[i])
      return i + 1;
  }

  return 0;
}

bool CPDF_Dest::GetXYZ(bool* pHasX,
                       bool* pHasY,
                       bool* pHasZoom,
                       float* pX,
                       float* pY,
                       float* pZoom) const {
  *pHasX = false;
  *pHasY = false;
  *pHasZoom = false;

  CPDF_Array* pArray = ToArray(m_pObj);
  if (!pArray)
    return false;

  if (pArray->GetCount() < 5)
    return false;

  const CPDF_Name* xyz = ToName(pArray->GetDirectObjectAt(1));
  if (!xyz || xyz->GetString() != "XYZ")
    return false;

  const CPDF_Number* numX = ToNumber(pArray->GetDirectObjectAt(2));
  const CPDF_Number* numY = ToNumber(pArray->GetDirectObjectAt(3));
  const CPDF_Number* numZoom = ToNumber(pArray->GetDirectObjectAt(4));

  // If the value is a CPDF_Null then ToNumber will return nullptr.
  *pHasX = !!numX;
  *pHasY = !!numY;
  *pHasZoom = !!numZoom;

  if (numX)
    *pX = numX->GetNumber();
  if (numY)
    *pY = numY->GetNumber();

  // A zoom value of 0 is equivalent to a null value, so treat it as a null.
  if (numZoom) {
    float num = numZoom->GetNumber();
    if (num == 0.0)
      *pHasZoom = false;
    else
      *pZoom = num;
  }

  return true;
}

FX_FLOAT CPDF_Dest::GetParam(int index) {
  CPDF_Array* pArray = ToArray(m_pObj);
  return pArray ? pArray->GetNumberAt(2 + index) : 0;
}

CFX_ByteString CPDF_Dest::GetRemoteName() {
  return m_pObj ? m_pObj->GetString() : CFX_ByteString();
}
