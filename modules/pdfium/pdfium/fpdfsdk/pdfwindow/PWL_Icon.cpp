// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/pdfwindow/PWL_Icon.h"

#include <algorithm>

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "fpdfsdk/pdfwindow/PWL_Utils.h"
#include "fpdfsdk/pdfwindow/PWL_Wnd.h"

CPWL_Image::CPWL_Image() : m_pPDFStream(nullptr) {}

CPWL_Image::~CPWL_Image() {}

CFX_ByteString CPWL_Image::GetImageAppStream() {
  CFX_ByteTextBuf sAppStream;

  CFX_ByteString sAlias = GetImageAlias();
  CFX_FloatRect rcPlate = GetClientRect();
  CFX_Matrix mt;
  mt.SetReverse(GetImageMatrix());

  FX_FLOAT fHScale = 1.0f;
  FX_FLOAT fVScale = 1.0f;
  GetScale(fHScale, fVScale);

  FX_FLOAT fx = 0.0f;
  FX_FLOAT fy = 0.0f;
  GetImageOffset(fx, fy);

  if (m_pPDFStream && sAlias.GetLength() > 0) {
    sAppStream << "q\n";
    sAppStream << rcPlate.left << " " << rcPlate.bottom << " "
               << rcPlate.right - rcPlate.left << " "
               << rcPlate.top - rcPlate.bottom << " re W n\n";

    sAppStream << fHScale << " 0 0 " << fVScale << " " << rcPlate.left + fx
               << " " << rcPlate.bottom + fy << " cm\n";
    sAppStream << mt.a << " " << mt.b << " " << mt.c << " " << mt.d << " "
               << mt.e << " " << mt.f << " cm\n";

    sAppStream << "0 g 0 G 1 w /" << sAlias.AsStringC() << " Do\n"
               << "Q\n";
  }

  return sAppStream.MakeString();
}

void CPWL_Image::SetPDFStream(CPDF_Stream* pStream) {
  m_pPDFStream = pStream;
}

CPDF_Stream* CPWL_Image::GetPDFStream() {
  return m_pPDFStream;
}

void CPWL_Image::GetImageSize(FX_FLOAT& fWidth, FX_FLOAT& fHeight) {
  fWidth = 0.0f;
  fHeight = 0.0f;

  if (m_pPDFStream) {
    if (CPDF_Dictionary* pDict = m_pPDFStream->GetDict()) {
      CFX_FloatRect rect = pDict->GetRectFor("BBox");

      fWidth = rect.right - rect.left;
      fHeight = rect.top - rect.bottom;
    }
  }
}

CFX_Matrix CPWL_Image::GetImageMatrix() {
  if (m_pPDFStream) {
    if (CPDF_Dictionary* pDict = m_pPDFStream->GetDict()) {
      return pDict->GetMatrixFor("Matrix");
    }
  }

  return CFX_Matrix();
}

CFX_ByteString CPWL_Image::GetImageAlias() {
  if (!m_sImageAlias.IsEmpty())
    return m_sImageAlias;

  if (m_pPDFStream) {
    if (CPDF_Dictionary* pDict = m_pPDFStream->GetDict()) {
      return pDict->GetStringFor("Name");
    }
  }

  return CFX_ByteString();
}

void CPWL_Image::SetImageAlias(const FX_CHAR* sImageAlias) {
  m_sImageAlias = sImageAlias;
}

void CPWL_Image::GetScale(FX_FLOAT& fHScale, FX_FLOAT& fVScale) {
  fHScale = 1.0f;
  fVScale = 1.0f;
}

void CPWL_Image::GetImageOffset(FX_FLOAT& x, FX_FLOAT& y) {
  x = 0.0f;
  y = 0.0f;
}

CPWL_Icon::CPWL_Icon() : m_pIconFit(nullptr) {}

CPWL_Icon::~CPWL_Icon() {}

CPDF_IconFit* CPWL_Icon::GetIconFit() {
  return m_pIconFit;
}

int32_t CPWL_Icon::GetScaleMethod() {
  if (m_pIconFit)
    return m_pIconFit->GetScaleMethod();

  return 0;
}

bool CPWL_Icon::IsProportionalScale() {
  if (m_pIconFit)
    return m_pIconFit->IsProportionalScale();

  return false;
}

void CPWL_Icon::GetIconPosition(FX_FLOAT& fLeft, FX_FLOAT& fBottom) {
  if (m_pIconFit) {
    fLeft = 0.0f;
    fBottom = 0.0f;
    CPDF_Array* pA = m_pIconFit->GetDict()
                         ? m_pIconFit->GetDict()->GetArrayFor("A")
                         : nullptr;
    if (pA) {
      size_t dwCount = pA->GetCount();
      if (dwCount > 0)
        fLeft = pA->GetNumberAt(0);
      if (dwCount > 1)
        fBottom = pA->GetNumberAt(1);
    }
  } else {
    fLeft = 0.0f;
    fBottom = 0.0f;
  }
}

void CPWL_Icon::GetScale(FX_FLOAT& fHScale, FX_FLOAT& fVScale) {
  fHScale = 1.0f;
  fVScale = 1.0f;

  if (m_pPDFStream) {
    FX_FLOAT fImageWidth, fImageHeight;
    FX_FLOAT fPlateWidth, fPlateHeight;

    CFX_FloatRect rcPlate = GetClientRect();
    fPlateWidth = rcPlate.right - rcPlate.left;
    fPlateHeight = rcPlate.top - rcPlate.bottom;

    GetImageSize(fImageWidth, fImageHeight);

    int32_t nScaleMethod = GetScaleMethod();

    switch (nScaleMethod) {
      default:
      case 0:
        fHScale = fPlateWidth / std::max(fImageWidth, 1.0f);
        fVScale = fPlateHeight / std::max(fImageHeight, 1.0f);
        break;
      case 1:
        if (fPlateWidth < fImageWidth)
          fHScale = fPlateWidth / std::max(fImageWidth, 1.0f);
        if (fPlateHeight < fImageHeight)
          fVScale = fPlateHeight / std::max(fImageHeight, 1.0f);
        break;
      case 2:
        if (fPlateWidth > fImageWidth)
          fHScale = fPlateWidth / std::max(fImageWidth, 1.0f);
        if (fPlateHeight > fImageHeight)
          fVScale = fPlateHeight / std::max(fImageHeight, 1.0f);
        break;
      case 3:
        break;
    }

    FX_FLOAT fMinScale;
    if (IsProportionalScale()) {
      fMinScale = std::min(fHScale, fVScale);
      fHScale = fMinScale;
      fVScale = fMinScale;
    }
  }
}

void CPWL_Icon::GetImageOffset(FX_FLOAT& x, FX_FLOAT& y) {
  FX_FLOAT fLeft, fBottom;

  GetIconPosition(fLeft, fBottom);
  x = 0.0f;
  y = 0.0f;

  FX_FLOAT fImageWidth, fImageHeight;
  GetImageSize(fImageWidth, fImageHeight);

  FX_FLOAT fHScale, fVScale;
  GetScale(fHScale, fVScale);

  FX_FLOAT fImageFactWidth = fImageWidth * fHScale;
  FX_FLOAT fImageFactHeight = fImageHeight * fVScale;

  FX_FLOAT fPlateWidth, fPlateHeight;
  CFX_FloatRect rcPlate = GetClientRect();
  fPlateWidth = rcPlate.right - rcPlate.left;
  fPlateHeight = rcPlate.top - rcPlate.bottom;

  x = (fPlateWidth - fImageFactWidth) * fLeft;
  y = (fPlateHeight - fImageFactHeight) * fBottom;
}
