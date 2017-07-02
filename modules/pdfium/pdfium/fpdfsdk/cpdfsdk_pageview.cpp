// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/cpdfsdk_pageview.h"

#include <memory>
#include <vector>

#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/render/cpdf_renderoptions.h"
#include "core/fpdfdoc/cpdf_annotlist.h"
#include "core/fpdfdoc/cpdf_interform.h"
#include "fpdfsdk/cpdfsdk_annot.h"
#include "fpdfsdk/cpdfsdk_annothandlermgr.h"
#include "fpdfsdk/cpdfsdk_annotiteration.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_interform.h"
#include "third_party/base/ptr_util.h"

#ifdef PDF_ENABLE_XFA
#include "fpdfsdk/fpdfxfa/cpdfxfa_page.h"
#include "xfa/fxfa/xfa_ffdocview.h"
#include "xfa/fxfa/xfa_ffpageview.h"
#include "xfa/fxfa/xfa_ffwidgethandler.h"
#include "xfa/fxfa/xfa_rendercontext.h"
#include "xfa/fxgraphics/cfx_graphics.h"
#endif  // PDF_ENABLE_XFA

CPDFSDK_PageView::CPDFSDK_PageView(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                   UnderlyingPageType* page)
    : m_page(page),
      m_pFormFillEnv(pFormFillEnv),
#ifndef PDF_ENABLE_XFA
      m_bOwnsPage(false),
#endif  // PDF_ENABLE_XFA
      m_bEnterWidget(false),
      m_bExitWidget(false),
      m_bOnWidget(false),
      m_bValid(false),
      m_bLocked(false),
      m_bBeingDestroyed(false) {
  CPDFSDK_InterForm* pInterForm = pFormFillEnv->GetInterForm();
  if (pInterForm) {
    CPDF_InterForm* pPDFInterForm = pInterForm->GetInterForm();
#ifdef PDF_ENABLE_XFA
    if (page->GetPDFPage())
      pPDFInterForm->FixPageFields(page->GetPDFPage());
#else   // PDF_ENABLE_XFA
    pPDFInterForm->FixPageFields(page);
#endif  // PDF_ENABLE_XFA
  }
#ifndef PDF_ENABLE_XFA
  m_page->SetView(this);
#endif  // PDF_ENABLE_XFA
}

CPDFSDK_PageView::~CPDFSDK_PageView() {
#ifndef PDF_ENABLE_XFA
  // The call to |ReleaseAnnot| can cause the page pointed to by |m_page| to
  // be freed, which will cause issues if we try to cleanup the pageview pointer
  // in |m_page|. So, reset the pageview pointer before doing anything else.
  m_page->SetView(nullptr);
#endif  // PDF_ENABLE_XFA

  CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
      m_pFormFillEnv->GetAnnotHandlerMgr();
  for (CPDFSDK_Annot* pAnnot : m_SDKAnnotArray)
    pAnnotHandlerMgr->ReleaseAnnot(pAnnot);

  m_SDKAnnotArray.clear();
  m_pAnnotList.reset();

#ifndef PDF_ENABLE_XFA
  if (m_bOwnsPage)
    delete m_page;
#endif  // PDF_ENABLE_XFA
}

void CPDFSDK_PageView::PageView_OnDraw(CFX_RenderDevice* pDevice,
                                       CFX_Matrix* pUser2Device,
#ifdef PDF_ENABLE_XFA
                                       CPDF_RenderOptions* pOptions,
                                       const FX_RECT& pClip) {
#else
                                       CPDF_RenderOptions* pOptions) {
#endif  // PDF_ENABLE_XFA
  m_curMatrix = *pUser2Device;

#ifdef PDF_ENABLE_XFA
  CPDFXFA_Page* pPage = GetPDFXFAPage();
  if (!pPage)
    return;

  if (pPage->GetContext()->GetDocType() == DOCTYPE_DYNAMIC_XFA) {
    CFX_Graphics gs(pDevice);
    CFX_RectF rectClip(static_cast<FX_FLOAT>(pClip.left),
                       static_cast<FX_FLOAT>(pClip.top),
                       static_cast<FX_FLOAT>(pClip.Width()),
                       static_cast<FX_FLOAT>(pClip.Height()));
    gs.SetClipRect(rectClip);
    std::unique_ptr<CXFA_RenderContext> pRenderContext(new CXFA_RenderContext);
    CXFA_RenderOptions renderOptions;
    renderOptions.m_bHighlight = true;
    CXFA_FFPageView* xfaView = pPage->GetXFAPageView();
    pRenderContext->StartRender(xfaView, &gs, *pUser2Device, renderOptions);
    pRenderContext->DoRender();
    pRenderContext->StopRender();
    CXFA_FFDocView* docView = xfaView->GetDocView();
    if (!docView)
      return;
    CPDFSDK_Annot* annot = GetFocusAnnot();
    if (!annot)
      return;
    // Render the focus widget
    docView->GetWidgetHandler()->RenderWidget(annot->GetXFAWidget(), &gs,
                                              pUser2Device, false);
    return;
  }
#endif  // PDF_ENABLE_XFA

  // for pdf/static xfa.
  CPDFSDK_AnnotIteration annotIteration(this, true);
  for (const auto& pSDKAnnot : annotIteration) {
    m_pFormFillEnv->GetAnnotHandlerMgr()->Annot_OnDraw(
        this, pSDKAnnot.Get(), pDevice, pUser2Device, pOptions->m_bDrawAnnots);
  }
}

CPDFSDK_Annot* CPDFSDK_PageView::GetFXAnnotAtPoint(const CFX_PointF& point) {
  CPDFSDK_AnnotHandlerMgr* pAnnotMgr = m_pFormFillEnv->GetAnnotHandlerMgr();
  CPDFSDK_AnnotIteration annotIteration(this, false);
  for (const auto& pSDKAnnot : annotIteration) {
    CFX_FloatRect rc = pAnnotMgr->Annot_OnGetViewBBox(this, pSDKAnnot.Get());
    if (pSDKAnnot->GetAnnotSubtype() == CPDF_Annot::Subtype::POPUP)
      continue;
    if (rc.Contains(point))
      return pSDKAnnot.Get();
  }
  return nullptr;
}

CPDFSDK_Annot* CPDFSDK_PageView::GetFXWidgetAtPoint(const CFX_PointF& point) {
  CPDFSDK_AnnotHandlerMgr* pAnnotMgr = m_pFormFillEnv->GetAnnotHandlerMgr();
  CPDFSDK_AnnotIteration annotIteration(this, false);
  for (const auto& pSDKAnnot : annotIteration) {
    bool bHitTest = pSDKAnnot->GetAnnotSubtype() == CPDF_Annot::Subtype::WIDGET;
#ifdef PDF_ENABLE_XFA
    bHitTest = bHitTest ||
               pSDKAnnot->GetAnnotSubtype() == CPDF_Annot::Subtype::XFAWIDGET;
#endif  // PDF_ENABLE_XFA
    if (bHitTest) {
      pAnnotMgr->Annot_OnGetViewBBox(this, pSDKAnnot.Get());
      if (pAnnotMgr->Annot_OnHitTest(this, pSDKAnnot.Get(), point))
        return pSDKAnnot.Get();
    }
  }
  return nullptr;
}

#ifdef PDF_ENABLE_XFA
CPDFSDK_Annot* CPDFSDK_PageView::AddAnnot(CXFA_FFWidget* pPDFAnnot) {
  if (!pPDFAnnot)
    return nullptr;

  CPDFSDK_Annot* pSDKAnnot = GetAnnotByXFAWidget(pPDFAnnot);
  if (pSDKAnnot)
    return pSDKAnnot;

  CPDFSDK_AnnotHandlerMgr* pAnnotHandler = m_pFormFillEnv->GetAnnotHandlerMgr();
  pSDKAnnot = pAnnotHandler->NewAnnot(pPDFAnnot, this);
  if (!pSDKAnnot)
    return nullptr;

  m_SDKAnnotArray.push_back(pSDKAnnot);
  return pSDKAnnot;
}

bool CPDFSDK_PageView::DeleteAnnot(CPDFSDK_Annot* pAnnot) {
  if (!pAnnot)
    return false;
  CPDFXFA_Page* pPage = pAnnot->GetPDFXFAPage();
  if (!pPage || (pPage->GetContext()->GetDocType() != DOCTYPE_STATIC_XFA &&
                 pPage->GetContext()->GetDocType() != DOCTYPE_DYNAMIC_XFA))
    return false;

  if (GetFocusAnnot() == pAnnot)
    m_pFormFillEnv->KillFocusAnnot(0);
  CPDFSDK_AnnotHandlerMgr* pAnnotHandler = m_pFormFillEnv->GetAnnotHandlerMgr();
  if (pAnnotHandler)
    pAnnotHandler->ReleaseAnnot(pAnnot);

  auto it = std::find(m_SDKAnnotArray.begin(), m_SDKAnnotArray.end(), pAnnot);
  if (it != m_SDKAnnotArray.end())
    m_SDKAnnotArray.erase(it);
  if (m_pCaptureWidget.Get() == pAnnot)
    m_pCaptureWidget.Reset();

  return true;
}
#endif  // PDF_ENABLE_XFA

CPDF_Document* CPDFSDK_PageView::GetPDFDocument() {
  if (m_page) {
#ifdef PDF_ENABLE_XFA
    return m_page->GetContext()->GetPDFDoc();
#else   // PDF_ENABLE_XFA
    return m_page->m_pDocument;
#endif  // PDF_ENABLE_XFA
  }
  return nullptr;
}

CPDF_Page* CPDFSDK_PageView::GetPDFPage() const {
#ifdef PDF_ENABLE_XFA
  return m_page ? m_page->GetPDFPage() : nullptr;
#else   // PDF_ENABLE_XFA
  return m_page;
#endif  // PDF_ENABLE_XFA
}

CPDFSDK_Annot* CPDFSDK_PageView::GetAnnotByDict(CPDF_Dictionary* pDict) {
  for (CPDFSDK_Annot* pAnnot : m_SDKAnnotArray) {
    if (pAnnot->GetPDFAnnot()->GetAnnotDict() == pDict)
      return pAnnot;
  }
  return nullptr;
}

#ifdef PDF_ENABLE_XFA
CPDFSDK_Annot* CPDFSDK_PageView::GetAnnotByXFAWidget(CXFA_FFWidget* hWidget) {
  if (!hWidget)
    return nullptr;

  for (CPDFSDK_Annot* pAnnot : m_SDKAnnotArray) {
    if (pAnnot->GetXFAWidget() == hWidget)
      return pAnnot;
  }
  return nullptr;
}
#endif  // PDF_ENABLE_XFA

bool CPDFSDK_PageView::OnLButtonDown(const CFX_PointF& point, uint32_t nFlag) {
  CPDFSDK_Annot::ObservedPtr pAnnot(GetFXWidgetAtPoint(point));
  if (!pAnnot) {
    m_pFormFillEnv->KillFocusAnnot(nFlag);
    return false;
  }

  CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
      m_pFormFillEnv->GetAnnotHandlerMgr();
  if (!pAnnotHandlerMgr->Annot_OnLButtonDown(this, &pAnnot, nFlag, point))
    return false;

  if (!pAnnot)
    return false;

  m_pFormFillEnv->SetFocusAnnot(&pAnnot);
  return true;
}

#ifdef PDF_ENABLE_XFA
bool CPDFSDK_PageView::OnRButtonDown(const CFX_PointF& point, uint32_t nFlag) {
  CPDFSDK_Annot::ObservedPtr pAnnot(GetFXWidgetAtPoint(point));
  if (!pAnnot)
    return false;

  CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
      m_pFormFillEnv->GetAnnotHandlerMgr();
  bool ok = pAnnotHandlerMgr->Annot_OnRButtonDown(this, &pAnnot, nFlag, point);
  if (!pAnnot)
    return false;

  if (ok)
    m_pFormFillEnv->SetFocusAnnot(&pAnnot);

  return true;
}

bool CPDFSDK_PageView::OnRButtonUp(const CFX_PointF& point, uint32_t nFlag) {
  CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
      m_pFormFillEnv->GetAnnotHandlerMgr();
  CPDFSDK_Annot::ObservedPtr pFXAnnot(GetFXWidgetAtPoint(point));
  if (!pFXAnnot)
    return false;

  if (pAnnotHandlerMgr->Annot_OnRButtonUp(this, &pFXAnnot, nFlag, point))
    m_pFormFillEnv->SetFocusAnnot(&pFXAnnot);

  return true;
}
#endif  // PDF_ENABLE_XFA

bool CPDFSDK_PageView::OnLButtonUp(const CFX_PointF& point, uint32_t nFlag) {
  CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
      m_pFormFillEnv->GetAnnotHandlerMgr();
  CPDFSDK_Annot::ObservedPtr pFXAnnot(GetFXWidgetAtPoint(point));
  CPDFSDK_Annot::ObservedPtr pFocusAnnot(GetFocusAnnot());
  if (pFocusAnnot && pFocusAnnot != pFXAnnot) {
    // Last focus Annot gets a chance to handle the event.
    if (pAnnotHandlerMgr->Annot_OnLButtonUp(this, &pFocusAnnot, nFlag, point))
      return true;
  }
  return pFXAnnot &&
         pAnnotHandlerMgr->Annot_OnLButtonUp(this, &pFXAnnot, nFlag, point);
}

bool CPDFSDK_PageView::OnMouseMove(const CFX_PointF& point, int nFlag) {
  CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
      m_pFormFillEnv->GetAnnotHandlerMgr();
  CPDFSDK_Annot::ObservedPtr pFXAnnot(GetFXAnnotAtPoint(point));
  if (pFXAnnot) {
    if (m_pCaptureWidget && m_pCaptureWidget != pFXAnnot) {
      m_bExitWidget = true;
      m_bEnterWidget = false;
      pAnnotHandlerMgr->Annot_OnMouseExit(this, &m_pCaptureWidget, nFlag);
    }
    m_pCaptureWidget.Reset(pFXAnnot.Get());
    m_bOnWidget = true;
    if (!m_bEnterWidget) {
      m_bEnterWidget = true;
      m_bExitWidget = false;
      pAnnotHandlerMgr->Annot_OnMouseEnter(this, &pFXAnnot, nFlag);
    }
    pAnnotHandlerMgr->Annot_OnMouseMove(this, &pFXAnnot, nFlag, point);
    return true;
  }
  if (m_bOnWidget) {
    m_bOnWidget = false;
    m_bExitWidget = true;
    m_bEnterWidget = false;
    if (m_pCaptureWidget) {
      pAnnotHandlerMgr->Annot_OnMouseExit(this, &m_pCaptureWidget, nFlag);
      m_pCaptureWidget.Reset();
    }
  }
  return false;
}

bool CPDFSDK_PageView::OnMouseWheel(double deltaX,
                                    double deltaY,
                                    const CFX_PointF& point,
                                    int nFlag) {
  CPDFSDK_Annot::ObservedPtr pAnnot(GetFXWidgetAtPoint(point));
  if (!pAnnot)
    return false;

  CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
      m_pFormFillEnv->GetAnnotHandlerMgr();
  return pAnnotHandlerMgr->Annot_OnMouseWheel(this, &pAnnot, nFlag, (int)deltaY,
                                              point);
}

bool CPDFSDK_PageView::OnChar(int nChar, uint32_t nFlag) {
  if (CPDFSDK_Annot* pAnnot = GetFocusAnnot()) {
    CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
        m_pFormFillEnv->GetAnnotHandlerMgr();
    return pAnnotHandlerMgr->Annot_OnChar(pAnnot, nChar, nFlag);
  }

  return false;
}

bool CPDFSDK_PageView::OnKeyDown(int nKeyCode, int nFlag) {
  if (CPDFSDK_Annot* pAnnot = GetFocusAnnot()) {
    CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
        m_pFormFillEnv->GetAnnotHandlerMgr();
    return pAnnotHandlerMgr->Annot_OnKeyDown(pAnnot, nKeyCode, nFlag);
  }
  return false;
}

bool CPDFSDK_PageView::OnKeyUp(int nKeyCode, int nFlag) {
  return false;
}

void CPDFSDK_PageView::LoadFXAnnots() {
  CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
      m_pFormFillEnv->GetAnnotHandlerMgr();

  SetLock(true);

#ifdef PDF_ENABLE_XFA
  CFX_RetainPtr<CPDFXFA_Page> protector(m_page);
  if (m_pFormFillEnv->GetXFAContext()->GetDocType() == DOCTYPE_DYNAMIC_XFA) {
    CXFA_FFPageView* pageView = m_page->GetXFAPageView();
    std::unique_ptr<IXFA_WidgetIterator> pWidgetHander(
        pageView->CreateWidgetIterator(
            XFA_TRAVERSEWAY_Form,
            XFA_WidgetStatus_Visible | XFA_WidgetStatus_Viewable));
    if (!pWidgetHander) {
      SetLock(false);
      return;
    }

    while (CXFA_FFWidget* pXFAAnnot = pWidgetHander->MoveToNext()) {
      CPDFSDK_Annot* pAnnot = pAnnotHandlerMgr->NewAnnot(pXFAAnnot, this);
      if (!pAnnot)
        continue;
      m_SDKAnnotArray.push_back(pAnnot);
      pAnnotHandlerMgr->Annot_OnLoad(pAnnot);
    }

    SetLock(false);
    return;
  }
#endif  // PDF_ENABLE_XFA

  CPDF_Page* pPage = GetPDFPage();
  ASSERT(pPage);
  bool bUpdateAP = CPDF_InterForm::IsUpdateAPEnabled();
  // Disable the default AP construction.
  CPDF_InterForm::SetUpdateAP(false);
  m_pAnnotList = pdfium::MakeUnique<CPDF_AnnotList>(pPage);
  CPDF_InterForm::SetUpdateAP(bUpdateAP);

  const size_t nCount = m_pAnnotList->Count();
  for (size_t i = 0; i < nCount; ++i) {
    CPDF_Annot* pPDFAnnot = m_pAnnotList->GetAt(i);
    CheckUnSupportAnnot(GetPDFDocument(), pPDFAnnot);
    CPDFSDK_Annot* pAnnot = pAnnotHandlerMgr->NewAnnot(pPDFAnnot, this);
    if (!pAnnot)
      continue;
    m_SDKAnnotArray.push_back(pAnnot);
    pAnnotHandlerMgr->Annot_OnLoad(pAnnot);
  }

  SetLock(false);
}

void CPDFSDK_PageView::UpdateRects(const std::vector<CFX_FloatRect>& rects) {
  for (const auto& rc : rects)
    m_pFormFillEnv->Invalidate(m_page, rc.ToFxRect());
}

void CPDFSDK_PageView::UpdateView(CPDFSDK_Annot* pAnnot) {
  CFX_FloatRect rcWindow = pAnnot->GetRect();
  m_pFormFillEnv->Invalidate(m_page, rcWindow.ToFxRect());
}

int CPDFSDK_PageView::GetPageIndex() const {
  if (!m_page)
    return -1;

#ifdef PDF_ENABLE_XFA
  int nDocType = m_page->GetContext()->GetDocType();
  switch (nDocType) {
    case DOCTYPE_DYNAMIC_XFA: {
      CXFA_FFPageView* pPageView = m_page->GetXFAPageView();
      return pPageView ? pPageView->GetPageIndex() : -1;
    }
    case DOCTYPE_STATIC_XFA:
    case DOCTYPE_PDF:
      return GetPageIndexForStaticPDF();
    default:
      return -1;
  }
#else   // PDF_ENABLE_XFA
  return GetPageIndexForStaticPDF();
#endif  // PDF_ENABLE_XFA
}

bool CPDFSDK_PageView::IsValidAnnot(const CPDF_Annot* p) const {
  if (!p)
    return false;

  const auto& annots = m_pAnnotList->All();
  auto it = std::find_if(annots.begin(), annots.end(),
                         [p](const std::unique_ptr<CPDF_Annot>& annot) {
                           return annot.get() == p;
                         });
  return it != annots.end();
}

bool CPDFSDK_PageView::IsValidSDKAnnot(const CPDFSDK_Annot* p) const {
  if (!p)
    return false;
  return pdfium::ContainsValue(m_SDKAnnotArray, p);
}

CPDFSDK_Annot* CPDFSDK_PageView::GetFocusAnnot() {
  CPDFSDK_Annot* pFocusAnnot = m_pFormFillEnv->GetFocusAnnot();
  return IsValidSDKAnnot(pFocusAnnot) ? pFocusAnnot : nullptr;
}

int CPDFSDK_PageView::GetPageIndexForStaticPDF() const {
  CPDF_Dictionary* pDict = GetPDFPage()->m_pFormDict;
  CPDF_Document* pDoc = m_pFormFillEnv->GetPDFDocument();
  return (pDoc && pDict) ? pDoc->GetPageIndex(pDict->GetObjNum()) : -1;
}
