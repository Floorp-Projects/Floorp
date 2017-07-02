// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_annotlist.h"

#include <memory>
#include <utility>

#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fpdfapi/render/cpdf_renderoptions.h"
#include "core/fpdfdoc/cpdf_annot.h"
#include "core/fpdfdoc/cpdf_interform.h"
#include "core/fpdfdoc/cpdf_occontext.h"
#include "core/fpdfdoc/cpvt_generateap.h"
#include "core/fxge/cfx_renderdevice.h"
#include "third_party/base/ptr_util.h"

namespace {

std::unique_ptr<CPDF_Annot> CreatePopupAnnot(CPDF_Annot* pAnnot,
                                             CPDF_Document* pDocument) {
  CPDF_Dictionary* pParentDict = pAnnot->GetAnnotDict();
  if (!pParentDict)
    return nullptr;

  // TODO(jaepark): We shouldn't strip BOM for some strings and not for others.
  // See pdfium:593.
  CFX_WideString sContents = pParentDict->GetUnicodeTextFor("Contents");
  if (sContents.IsEmpty())
    return nullptr;

  auto pAnnotDict =
      pdfium::MakeUnique<CPDF_Dictionary>(pDocument->GetByteStringPool());
  pAnnotDict->SetNewFor<CPDF_Name>("Type", "Annot");
  pAnnotDict->SetNewFor<CPDF_Name>("Subtype", "Popup");
  pAnnotDict->SetNewFor<CPDF_String>("T", pParentDict->GetStringFor("T"),
                                     false);
  pAnnotDict->SetNewFor<CPDF_String>("Contents", sContents.UTF8Encode(), false);

  CFX_FloatRect rect = pParentDict->GetRectFor("Rect");
  rect.Normalize();
  CFX_FloatRect popupRect(0, 0, 200, 200);
  popupRect.Translate(rect.left, rect.bottom - popupRect.Height());

  pAnnotDict->SetRectFor("Rect", popupRect);
  pAnnotDict->SetNewFor<CPDF_Number>("F", 0);

  auto pPopupAnnot =
      pdfium::MakeUnique<CPDF_Annot>(std::move(pAnnotDict), pDocument);
  pAnnot->SetPopupAnnot(pPopupAnnot.get());
  return pPopupAnnot;
}

}  // namespace

CPDF_AnnotList::CPDF_AnnotList(CPDF_Page* pPage)
    : m_pDocument(pPage->m_pDocument) {
  if (!pPage->m_pFormDict)
    return;

  CPDF_Array* pAnnots = pPage->m_pFormDict->GetArrayFor("Annots");
  if (!pAnnots)
    return;

  CPDF_Dictionary* pRoot = m_pDocument->GetRoot();
  CPDF_Dictionary* pAcroForm = pRoot->GetDictFor("AcroForm");
  bool bRegenerateAP = pAcroForm && pAcroForm->GetBooleanFor("NeedAppearances");
  for (size_t i = 0; i < pAnnots->GetCount(); ++i) {
    CPDF_Dictionary* pDict = ToDictionary(pAnnots->GetDirectObjectAt(i));
    if (!pDict)
      continue;
    const CFX_ByteString subtype = pDict->GetStringFor("Subtype");
    if (subtype == "Popup") {
      // Skip creating Popup annotations in the PDF document since PDFium
      // provides its own Popup annotations.
      continue;
    }
    pAnnots->ConvertToIndirectObjectAt(i, m_pDocument);
    m_AnnotList.push_back(pdfium::MakeUnique<CPDF_Annot>(pDict, m_pDocument));
    if (bRegenerateAP && subtype == "Widget" &&
        CPDF_InterForm::IsUpdateAPEnabled()) {
      FPDF_GenerateAP(m_pDocument, pDict);
    }
  }

  size_t nAnnotListSize = m_AnnotList.size();
  for (size_t i = 0; i < nAnnotListSize; ++i) {
    std::unique_ptr<CPDF_Annot> pPopupAnnot(
        CreatePopupAnnot(m_AnnotList[i].get(), m_pDocument));
    if (pPopupAnnot)
      m_AnnotList.push_back(std::move(pPopupAnnot));
  }
}

CPDF_AnnotList::~CPDF_AnnotList() {}

void CPDF_AnnotList::DisplayPass(CPDF_Page* pPage,
                                 CFX_RenderDevice* pDevice,
                                 CPDF_RenderContext* pContext,
                                 bool bPrinting,
                                 const CFX_Matrix* pMatrix,
                                 bool bWidgetPass,
                                 CPDF_RenderOptions* pOptions,
                                 FX_RECT* clip_rect) {
  for (const auto& pAnnot : m_AnnotList) {
    bool bWidget = pAnnot->GetSubtype() == CPDF_Annot::Subtype::WIDGET;
    if ((bWidgetPass && !bWidget) || (!bWidgetPass && bWidget))
      continue;

    uint32_t annot_flags = pAnnot->GetFlags();
    if (annot_flags & ANNOTFLAG_HIDDEN)
      continue;

    if (bPrinting && (annot_flags & ANNOTFLAG_PRINT) == 0)
      continue;

    if (!bPrinting && (annot_flags & ANNOTFLAG_NOVIEW))
      continue;

    if (pOptions) {
      CFX_RetainPtr<CPDF_OCContext> pOCContext = pOptions->m_pOCContext;
      CPDF_Dictionary* pAnnotDict = pAnnot->GetAnnotDict();
      if (pOCContext && pAnnotDict &&
          !pOCContext->CheckOCGVisible(pAnnotDict->GetDictFor("OC"))) {
        continue;
      }
    }
    CFX_FloatRect annot_rect_f = pAnnot->GetRect();
    CFX_Matrix matrix = *pMatrix;
    if (clip_rect) {
      matrix.TransformRect(annot_rect_f);

      FX_RECT annot_rect = annot_rect_f.GetOuterRect();
      annot_rect.Intersect(*clip_rect);
      if (annot_rect.IsEmpty())
        continue;
    }
    if (pContext) {
      pAnnot->DrawInContext(pPage, pContext, &matrix, CPDF_Annot::Normal);
    } else if (!pAnnot->DrawAppearance(pPage, pDevice, &matrix,
                                       CPDF_Annot::Normal, pOptions)) {
      pAnnot->DrawBorder(pDevice, &matrix, pOptions);
    }
  }
}

void CPDF_AnnotList::DisplayAnnots(CPDF_Page* pPage,
                                   CFX_RenderDevice* pDevice,
                                   CPDF_RenderContext* pContext,
                                   bool bPrinting,
                                   const CFX_Matrix* pUser2Device,
                                   uint32_t dwAnnotFlags,
                                   CPDF_RenderOptions* pOptions,
                                   FX_RECT* pClipRect) {
  if (dwAnnotFlags & ANNOTFLAG_INVISIBLE) {
    DisplayPass(pPage, pDevice, pContext, bPrinting, pUser2Device, false,
                pOptions, pClipRect);
  }
  if (dwAnnotFlags & ANNOTFLAG_HIDDEN) {
    DisplayPass(pPage, pDevice, pContext, bPrinting, pUser2Device, true,
                pOptions, pClipRect);
  }
}

void CPDF_AnnotList::DisplayAnnots(CPDF_Page* pPage,
                                   CPDF_RenderContext* pContext,
                                   bool bPrinting,
                                   const CFX_Matrix* pMatrix,
                                   bool bShowWidget,
                                   CPDF_RenderOptions* pOptions) {
  uint32_t dwAnnotFlags = bShowWidget ? ANNOTFLAG_INVISIBLE | ANNOTFLAG_HIDDEN
                                      : ANNOTFLAG_INVISIBLE;
  DisplayAnnots(pPage, nullptr, pContext, bPrinting, pMatrix, dwAnnotFlags,
                pOptions, nullptr);
}
