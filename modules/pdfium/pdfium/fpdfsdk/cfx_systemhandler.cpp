// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/cfx_systemhandler.h"

#include <memory>

#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fxge/cfx_fontmapper.h"
#include "core/fxge/cfx_fontmgr.h"
#include "core/fxge/cfx_gemodule.h"
#include "fpdfsdk/cpdfsdk_annot.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_pageview.h"
#include "fpdfsdk/cpdfsdk_widget.h"
#include "fpdfsdk/formfiller/cffl_formfiller.h"

namespace {

int CharSet2CP(int charset) {
  if (charset == FXFONT_SHIFTJIS_CHARSET)
    return 932;
  if (charset == FXFONT_GB2312_CHARSET)
    return 936;
  if (charset == FXFONT_HANGUL_CHARSET)
    return 949;
  if (charset == FXFONT_CHINESEBIG5_CHARSET)
    return 950;
  return 0;
}

}  // namespace

void CFX_SystemHandler::InvalidateRect(CPDFSDK_Widget* widget, FX_RECT rect) {
  CPDFSDK_PageView* pPageView = widget->GetPageView();
  UnderlyingPageType* pPage = widget->GetUnderlyingPage();
  if (!pPage || !pPageView)
    return;

  CFX_Matrix page2device;
  pPageView->GetCurrentMatrix(page2device);

  CFX_Matrix device2page;
  device2page.SetReverse(page2device);

  CFX_PointF left_top = device2page.Transform(CFX_PointF(
      static_cast<FX_FLOAT>(rect.left), static_cast<FX_FLOAT>(rect.top)));
  CFX_PointF right_bottom = device2page.Transform(CFX_PointF(
      static_cast<FX_FLOAT>(rect.right), static_cast<FX_FLOAT>(rect.bottom)));

  CFX_FloatRect rcPDF(left_top.x, right_bottom.y, right_bottom.x, left_top.y);
  rcPDF.Normalize();
  m_pFormFillEnv->Invalidate(pPage, rcPDF.ToFxRect());
}

void CFX_SystemHandler::OutputSelectedRect(CFFL_FormFiller* pFormFiller,
                                           CFX_FloatRect& rect) {
  if (!pFormFiller)
    return;

  CFX_PointF ptA = pFormFiller->PWLtoFFL(CFX_PointF(rect.left, rect.bottom));
  CFX_PointF ptB = pFormFiller->PWLtoFFL(CFX_PointF(rect.right, rect.top));

  CPDFSDK_Annot* pAnnot = pFormFiller->GetSDKAnnot();
  UnderlyingPageType* pPage = pAnnot->GetUnderlyingPage();
  ASSERT(pPage);

  m_pFormFillEnv->OutputSelectedRect(pPage,
                                     CFX_FloatRect(ptA.x, ptA.y, ptB.x, ptB.y));
}

bool CFX_SystemHandler::IsSelectionImplemented() const {
  if (!m_pFormFillEnv)
    return false;

  FPDF_FORMFILLINFO* pInfo = m_pFormFillEnv->GetFormFillInfo();
  return pInfo && pInfo->FFI_OutputSelectedRect;
}

void CFX_SystemHandler::SetCursor(int32_t nCursorType) {
  m_pFormFillEnv->SetCursor(nCursorType);
}

bool CFX_SystemHandler::FindNativeTrueTypeFont(CFX_ByteString sFontFaceName) {
  CFX_FontMgr* pFontMgr = CFX_GEModule::Get()->GetFontMgr();
  if (!pFontMgr)
    return false;

  CFX_FontMapper* pFontMapper = pFontMgr->GetBuiltinMapper();
  if (!pFontMapper)
    return false;

  if (pFontMapper->m_InstalledTTFonts.empty())
    pFontMapper->LoadInstalledFonts();

  for (const auto& font : pFontMapper->m_InstalledTTFonts) {
    if (font.Compare(sFontFaceName.AsStringC()))
      return true;
  }
  for (const auto& fontPair : pFontMapper->m_LocalizedTTFonts) {
    if (fontPair.first.Compare(sFontFaceName.AsStringC()))
      return true;
  }
  return false;
}

CPDF_Font* CFX_SystemHandler::AddNativeTrueTypeFontToPDF(
    CPDF_Document* pDoc,
    CFX_ByteString sFontFaceName,
    uint8_t nCharset) {
  if (!pDoc)
    return nullptr;

  std::unique_ptr<CFX_Font> pFXFont(new CFX_Font);
  pFXFont->LoadSubst(sFontFaceName, true, 0, 0, 0, CharSet2CP(nCharset), false);
  return pDoc->AddFont(pFXFont.get(), nCharset, false);
}

int32_t CFX_SystemHandler::SetTimer(int32_t uElapse,
                                    TimerCallback lpTimerFunc) {
  return m_pFormFillEnv->SetTimer(uElapse, lpTimerFunc);
}

void CFX_SystemHandler::KillTimer(int32_t nID) {
  m_pFormFillEnv->KillTimer(nID);
}

bool CFX_SystemHandler::IsSHIFTKeyDown(uint32_t nFlag) const {
  return !!m_pFormFillEnv->IsSHIFTKeyDown(nFlag);
}

bool CFX_SystemHandler::IsCTRLKeyDown(uint32_t nFlag) const {
  return !!m_pFormFillEnv->IsCTRLKeyDown(nFlag);
}

bool CFX_SystemHandler::IsALTKeyDown(uint32_t nFlag) const {
  return !!m_pFormFillEnv->IsALTKeyDown(nFlag);
}
