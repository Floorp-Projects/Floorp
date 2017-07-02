// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/formfiller/cffl_formfiller.h"

#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fxge/cfx_renderdevice.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_pageview.h"
#include "fpdfsdk/cpdfsdk_widget.h"
#include "fpdfsdk/formfiller/cba_fontmap.h"
#include "fpdfsdk/fsdk_common.h"
#include "fpdfsdk/pdfwindow/PWL_Utils.h"

#define GetRed(rgb) ((uint8_t)(rgb))
#define GetGreen(rgb) ((uint8_t)(((uint16_t)(rgb)) >> 8))
#define GetBlue(rgb) ((uint8_t)((rgb) >> 16))

#define FFL_HINT_ELAPSE 800

CFFL_FormFiller::CFFL_FormFiller(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                 CPDFSDK_Annot* pAnnot)
    : m_pFormFillEnv(pFormFillEnv), m_pAnnot(pAnnot), m_bValid(false) {
  m_pWidget = static_cast<CPDFSDK_Widget*>(pAnnot);
}

CFFL_FormFiller::~CFFL_FormFiller() {
  DestroyWindows();
}

void CFFL_FormFiller::DestroyWindows() {
  for (const auto& it : m_Maps) {
    CPWL_Wnd* pWnd = it.second;
    CFFL_PrivateData* pData = (CFFL_PrivateData*)pWnd->GetAttachedData();
    pWnd->InvalidateProvider(this);
    pWnd->Destroy();
    delete pWnd;
    delete pData;
  }
  m_Maps.clear();
}

void CFFL_FormFiller::SetWindowRect(CPDFSDK_PageView* pPageView,
                                    const CFX_FloatRect& rcWindow) {
  if (CPWL_Wnd* pWnd = GetPDFWindow(pPageView, false)) {
    pWnd->Move(CFX_FloatRect(rcWindow), true, false);
  }
}

CFX_FloatRect CFFL_FormFiller::GetWindowRect(CPDFSDK_PageView* pPageView) {
  if (CPWL_Wnd* pWnd = GetPDFWindow(pPageView, false)) {
    return pWnd->GetWindowRect();
  }

  return CFX_FloatRect(0, 0, 0, 0);
}

FX_RECT CFFL_FormFiller::GetViewBBox(CPDFSDK_PageView* pPageView,
                                     CPDFSDK_Annot* pAnnot) {
  ASSERT(pPageView);
  ASSERT(pAnnot);

  CFX_FloatRect rcAnnot = m_pWidget->GetRect();

  if (CPWL_Wnd* pWnd = GetPDFWindow(pPageView, false)) {
    CFX_FloatRect rcWindow = pWnd->GetWindowRect();
    rcAnnot = PWLtoFFL(rcWindow);
  }

  CFX_FloatRect rcWin = rcAnnot;

  CFX_FloatRect rcFocus = GetFocusBox(pPageView);
  if (!rcFocus.IsEmpty())
    rcWin.Union(rcFocus);

  CFX_FloatRect rect = CPWL_Utils::InflateRect(rcWin, 1);

  return rect.GetOuterRect();
}

void CFFL_FormFiller::OnDraw(CPDFSDK_PageView* pPageView,
                             CPDFSDK_Annot* pAnnot,
                             CFX_RenderDevice* pDevice,
                             CFX_Matrix* pUser2Device) {
  ASSERT(pAnnot->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);

  if (CPWL_Wnd* pWnd = GetPDFWindow(pPageView, false)) {
    CFX_Matrix mt = GetCurMatrix();
    mt.Concat(*pUser2Device);
    pWnd->DrawAppearance(pDevice, &mt);
  } else {
    CPDFSDK_Widget* pWidget = (CPDFSDK_Widget*)pAnnot;
    if (CFFL_InteractiveFormFiller::IsVisible(pWidget))
      pWidget->DrawAppearance(pDevice, pUser2Device, CPDF_Annot::Normal,
                              nullptr);
  }
}

void CFFL_FormFiller::OnDrawDeactive(CPDFSDK_PageView* pPageView,
                                     CPDFSDK_Annot* pAnnot,
                                     CFX_RenderDevice* pDevice,
                                     CFX_Matrix* pUser2Device) {
  CPDFSDK_Widget* pWidget = (CPDFSDK_Widget*)pAnnot;
  pWidget->DrawAppearance(pDevice, pUser2Device, CPDF_Annot::Normal, nullptr);
}

void CFFL_FormFiller::OnMouseEnter(CPDFSDK_PageView* pPageView,
                                   CPDFSDK_Annot* pAnnot) {}

void CFFL_FormFiller::OnMouseExit(CPDFSDK_PageView* pPageView,
                                  CPDFSDK_Annot* pAnnot) {
  EndTimer();
  ASSERT(m_pWidget);
}

bool CFFL_FormFiller::OnLButtonDown(CPDFSDK_PageView* pPageView,
                                    CPDFSDK_Annot* pAnnot,
                                    uint32_t nFlags,
                                    const CFX_PointF& point) {
  if (CPWL_Wnd* pWnd = GetPDFWindow(pPageView, true)) {
    m_bValid = true;
    FX_RECT rect = GetViewBBox(pPageView, pAnnot);
    InvalidateRect(rect);
    if (!rect.Contains(static_cast<int>(point.x), static_cast<int>(point.y)))
      return false;

    return pWnd->OnLButtonDown(WndtoPWL(pPageView, point), nFlags);
  }

  return false;
}

bool CFFL_FormFiller::OnLButtonUp(CPDFSDK_PageView* pPageView,
                                  CPDFSDK_Annot* pAnnot,
                                  uint32_t nFlags,
                                  const CFX_PointF& point) {
  CPWL_Wnd* pWnd = GetPDFWindow(pPageView, false);
  if (!pWnd)
    return false;

  InvalidateRect(GetViewBBox(pPageView, pAnnot));
  pWnd->OnLButtonUp(WndtoPWL(pPageView, point), nFlags);
  return true;
}

bool CFFL_FormFiller::OnLButtonDblClk(CPDFSDK_PageView* pPageView,
                                      CPDFSDK_Annot* pAnnot,
                                      uint32_t nFlags,
                                      const CFX_PointF& point) {
  CPWL_Wnd* pWnd = GetPDFWindow(pPageView, false);
  if (!pWnd)
    return false;

  pWnd->OnLButtonDblClk(WndtoPWL(pPageView, point), nFlags);
  return true;
}

bool CFFL_FormFiller::OnMouseMove(CPDFSDK_PageView* pPageView,
                                  CPDFSDK_Annot* pAnnot,
                                  uint32_t nFlags,
                                  const CFX_PointF& point) {
  if (m_ptOldPos != point)
    m_ptOldPos = point;

  CPWL_Wnd* pWnd = GetPDFWindow(pPageView, false);
  if (!pWnd)
    return false;

  pWnd->OnMouseMove(WndtoPWL(pPageView, point), nFlags);
  return true;
}

bool CFFL_FormFiller::OnMouseWheel(CPDFSDK_PageView* pPageView,
                                   CPDFSDK_Annot* pAnnot,
                                   uint32_t nFlags,
                                   short zDelta,
                                   const CFX_PointF& point) {
  if (!IsValid())
    return false;

  CPWL_Wnd* pWnd = GetPDFWindow(pPageView, true);
  return pWnd && pWnd->OnMouseWheel(zDelta, WndtoPWL(pPageView, point), nFlags);
}

bool CFFL_FormFiller::OnRButtonDown(CPDFSDK_PageView* pPageView,
                                    CPDFSDK_Annot* pAnnot,
                                    uint32_t nFlags,
                                    const CFX_PointF& point) {
  CPWL_Wnd* pWnd = GetPDFWindow(pPageView, true);
  if (!pWnd)
    return false;

  pWnd->OnRButtonDown(WndtoPWL(pPageView, point), nFlags);
  return true;
}

bool CFFL_FormFiller::OnRButtonUp(CPDFSDK_PageView* pPageView,
                                  CPDFSDK_Annot* pAnnot,
                                  uint32_t nFlags,
                                  const CFX_PointF& point) {
  CPWL_Wnd* pWnd = GetPDFWindow(pPageView, false);
  if (!pWnd)
    return false;

  pWnd->OnRButtonUp(WndtoPWL(pPageView, point), nFlags);
  return true;
}

bool CFFL_FormFiller::OnKeyDown(CPDFSDK_Annot* pAnnot,
                                uint32_t nKeyCode,
                                uint32_t nFlags) {
  if (IsValid()) {
    CPDFSDK_PageView* pPageView = GetCurPageView(true);
    ASSERT(pPageView);

    if (CPWL_Wnd* pWnd = GetPDFWindow(pPageView, false)) {
      return pWnd->OnKeyDown(nKeyCode, nFlags);
    }
  }

  return false;
}

bool CFFL_FormFiller::OnChar(CPDFSDK_Annot* pAnnot,
                             uint32_t nChar,
                             uint32_t nFlags) {
  if (IsValid()) {
    CPDFSDK_PageView* pPageView = GetCurPageView(true);
    ASSERT(pPageView);

    if (CPWL_Wnd* pWnd = GetPDFWindow(pPageView, false)) {
      return pWnd->OnChar(nChar, nFlags);
    }
  }

  return false;
}

void CFFL_FormFiller::SetFocusForAnnot(CPDFSDK_Annot* pAnnot, uint32_t nFlag) {
  CPDFSDK_Widget* pWidget = (CPDFSDK_Widget*)pAnnot;
  UnderlyingPageType* pPage = pWidget->GetUnderlyingPage();
  CPDFSDK_PageView* pPageView = m_pFormFillEnv->GetPageView(pPage, true);
  if (CPWL_Wnd* pWnd = GetPDFWindow(pPageView, true))
    pWnd->SetFocus();

  m_bValid = true;
  InvalidateRect(GetViewBBox(pPageView, pAnnot));
}

void CFFL_FormFiller::KillFocusForAnnot(CPDFSDK_Annot* pAnnot, uint32_t nFlag) {
  if (!IsValid())
    return;

  CPDFSDK_PageView* pPageView = GetCurPageView(false);
  if (!pPageView)
    return;

  CommitData(pPageView, nFlag);

  if (CPWL_Wnd* pWnd = GetPDFWindow(pPageView, false))
    pWnd->KillFocus();

  bool bDestroyPDFWindow;
  switch (m_pWidget->GetFieldType()) {
    case FIELDTYPE_PUSHBUTTON:
    case FIELDTYPE_CHECKBOX:
    case FIELDTYPE_RADIOBUTTON:
      bDestroyPDFWindow = true;
      break;
    default:
      bDestroyPDFWindow = false;
      break;
  }
  EscapeFiller(pPageView, bDestroyPDFWindow);
}

bool CFFL_FormFiller::IsValid() const {
  return m_bValid;
}

PWL_CREATEPARAM CFFL_FormFiller::GetCreateParam() {
  ASSERT(m_pFormFillEnv);

  PWL_CREATEPARAM cp;
  cp.pParentWnd = nullptr;
  cp.pProvider.Reset(this);
  cp.rcRectWnd = GetPDFWindowRect();

  uint32_t dwCreateFlags = PWS_BORDER | PWS_BACKGROUND | PWS_VISIBLE;
  uint32_t dwFieldFlag = m_pWidget->GetFieldFlags();
  if (dwFieldFlag & FIELDFLAG_READONLY) {
    dwCreateFlags |= PWS_READONLY;
  }

  FX_COLORREF color;
  if (m_pWidget->GetFillColor(color)) {
    cp.sBackgroundColor =
        CPWL_Color(GetRed(color), GetGreen(color), GetBlue(color));
  }

  if (m_pWidget->GetBorderColor(color)) {
    cp.sBorderColor =
        CPWL_Color(GetRed(color), GetGreen(color), GetBlue(color));
  }

  cp.sTextColor = CPWL_Color(COLORTYPE_GRAY, 0);

  if (m_pWidget->GetTextColor(color)) {
    cp.sTextColor = CPWL_Color(GetRed(color), GetGreen(color), GetBlue(color));
  }

  cp.fFontSize = m_pWidget->GetFontSize();
  cp.dwBorderWidth = m_pWidget->GetBorderWidth();

  cp.nBorderStyle = m_pWidget->GetBorderStyle();
  switch (cp.nBorderStyle) {
    case BorderStyle::DASH:
      cp.sDash = CPWL_Dash(3, 3, 0);
      break;
    case BorderStyle::BEVELED:
      cp.dwBorderWidth *= 2;
      break;
    case BorderStyle::INSET:
      cp.dwBorderWidth *= 2;
      break;
    default:
      break;
  }

  if (cp.fFontSize <= 0)
    dwCreateFlags |= PWS_AUTOFONTSIZE;

  cp.dwFlags = dwCreateFlags;
  cp.pSystemHandler = m_pFormFillEnv->GetSysHandler();
  return cp;
}

CPWL_Wnd* CFFL_FormFiller::GetPDFWindow(CPDFSDK_PageView* pPageView,
                                        bool bNew) {
  ASSERT(pPageView);

  auto it = m_Maps.find(pPageView);
  const bool found = it != m_Maps.end();
  CPWL_Wnd* pWnd = found ? it->second : nullptr;
  if (!bNew)
    return pWnd;

  if (found) {
    CFFL_PrivateData* pPrivateData = (CFFL_PrivateData*)pWnd->GetAttachedData();
    if (pPrivateData->nWidgetAge != m_pWidget->GetAppearanceAge()) {
      return ResetPDFWindow(
          pPageView, m_pWidget->GetValueAge() == pPrivateData->nValueAge);
    }
  } else {
    PWL_CREATEPARAM cp = GetCreateParam();
    cp.pAttachedWidget.Reset(m_pWidget);

    CFFL_PrivateData* pPrivateData = new CFFL_PrivateData;
    pPrivateData->pWidget = m_pWidget;
    pPrivateData->pPageView = pPageView;
    pPrivateData->nWidgetAge = m_pWidget->GetAppearanceAge();
    pPrivateData->nValueAge = 0;

    cp.pAttachedData = pPrivateData;

    pWnd = NewPDFWindow(cp, pPageView);
    m_Maps[pPageView] = pWnd;
  }

  return pWnd;
}

void CFFL_FormFiller::DestroyPDFWindow(CPDFSDK_PageView* pPageView) {
  auto it = m_Maps.find(pPageView);
  if (it == m_Maps.end())
    return;

  CPWL_Wnd* pWnd = it->second;
  CFFL_PrivateData* pData = (CFFL_PrivateData*)pWnd->GetAttachedData();
  pWnd->Destroy();
  delete pWnd;
  delete pData;

  m_Maps.erase(it);
}

CFX_Matrix CFFL_FormFiller::GetWindowMatrix(void* pAttachedData) {
  if (CFFL_PrivateData* pPrivateData = (CFFL_PrivateData*)pAttachedData) {
    if (pPrivateData->pPageView) {
      CFX_Matrix mtPageView;
      pPrivateData->pPageView->GetCurrentMatrix(mtPageView);

      CFX_Matrix mt = GetCurMatrix();
      mt.Concat(mtPageView);

      return mt;
    }
  }
  return CFX_Matrix(1, 0, 0, 1, 0, 0);
}

CFX_Matrix CFFL_FormFiller::GetCurMatrix() {
  CFX_Matrix mt;

  CFX_FloatRect rcDA = m_pWidget->GetPDFAnnot()->GetRect();

  switch (m_pWidget->GetRotate()) {
    default:
    case 0:
      mt = CFX_Matrix(1, 0, 0, 1, 0, 0);
      break;
    case 90:
      mt = CFX_Matrix(0, 1, -1, 0, rcDA.right - rcDA.left, 0);
      break;
    case 180:
      mt = CFX_Matrix(-1, 0, 0, -1, rcDA.right - rcDA.left,
                      rcDA.top - rcDA.bottom);
      break;
    case 270:
      mt = CFX_Matrix(0, -1, 1, 0, 0, rcDA.top - rcDA.bottom);
      break;
  }
  mt.e += rcDA.left;
  mt.f += rcDA.bottom;

  return mt;
}

CFX_WideString CFFL_FormFiller::LoadPopupMenuString(int nIndex) {
  ASSERT(m_pFormFillEnv);

  return L"";
}

CFX_FloatRect CFFL_FormFiller::GetPDFWindowRect() const {
  CFX_FloatRect rectAnnot = m_pWidget->GetPDFAnnot()->GetRect();

  FX_FLOAT fWidth = rectAnnot.right - rectAnnot.left;
  FX_FLOAT fHeight = rectAnnot.top - rectAnnot.bottom;
  if ((m_pWidget->GetRotate() / 90) & 0x01)
    return CFX_FloatRect(0, 0, fHeight, fWidth);

  return CFX_FloatRect(0, 0, fWidth, fHeight);
}

CPDFSDK_PageView* CFFL_FormFiller::GetCurPageView(bool renew) {
  UnderlyingPageType* pPage = m_pAnnot->GetUnderlyingPage();
  return m_pFormFillEnv ? m_pFormFillEnv->GetPageView(pPage, renew) : nullptr;
}

CFX_FloatRect CFFL_FormFiller::GetFocusBox(CPDFSDK_PageView* pPageView) {
  if (CPWL_Wnd* pWnd = GetPDFWindow(pPageView, false)) {
    CFX_FloatRect rcFocus = FFLtoWnd(pPageView, PWLtoFFL(pWnd->GetFocusRect()));
    CFX_FloatRect rcPage = pPageView->GetPDFPage()->GetPageBBox();
    if (rcPage.Contains(rcFocus))
      return rcFocus;
  }
  return CFX_FloatRect(0, 0, 0, 0);
}

CFX_FloatRect CFFL_FormFiller::FFLtoPWL(const CFX_FloatRect& rect) {
  CFX_Matrix mt;
  mt.SetReverse(GetCurMatrix());

  CFX_FloatRect temp = rect;
  mt.TransformRect(temp);

  return temp;
}

CFX_FloatRect CFFL_FormFiller::PWLtoFFL(const CFX_FloatRect& rect) {
  CFX_Matrix mt = GetCurMatrix();

  CFX_FloatRect temp = rect;
  mt.TransformRect(temp);

  return temp;
}

CFX_PointF CFFL_FormFiller::FFLtoPWL(const CFX_PointF& point) {
  CFX_Matrix mt;
  mt.SetReverse(GetCurMatrix());
  return mt.Transform(point);
}

CFX_PointF CFFL_FormFiller::PWLtoFFL(const CFX_PointF& point) {
  return GetCurMatrix().Transform(point);
}

CFX_PointF CFFL_FormFiller::WndtoPWL(CPDFSDK_PageView* pPageView,
                                     const CFX_PointF& pt) {
  return FFLtoPWL(pt);
}

CFX_FloatRect CFFL_FormFiller::FFLtoWnd(CPDFSDK_PageView* pPageView,
                                        const CFX_FloatRect& rect) {
  return rect;
}

bool CFFL_FormFiller::CommitData(CPDFSDK_PageView* pPageView, uint32_t nFlag) {
  if (IsDataChanged(pPageView)) {
    bool bRC = true;
    bool bExit = false;
    CFFL_InteractiveFormFiller* pFormFiller =
        m_pFormFillEnv->GetInteractiveFormFiller();
    CPDFSDK_Annot::ObservedPtr pObserved(m_pWidget);
    pFormFiller->OnKeyStrokeCommit(&pObserved, pPageView, bRC, bExit, nFlag);
    if (!pObserved || bExit)
      return true;
    if (!bRC) {
      ResetPDFWindow(pPageView, false);
      return true;
    }
    pFormFiller->OnValidate(&pObserved, pPageView, bRC, bExit, nFlag);
    if (!pObserved || bExit)
      return true;
    if (!bRC) {
      ResetPDFWindow(pPageView, false);
      return true;
    }
    SaveData(pPageView);
    pFormFiller->OnCalculate(m_pWidget, pPageView, bExit, nFlag);
    if (bExit)
      return true;

    pFormFiller->OnFormat(m_pWidget, pPageView, bExit, nFlag);
  }
  return true;
}

bool CFFL_FormFiller::IsDataChanged(CPDFSDK_PageView* pPageView) {
  return false;
}

void CFFL_FormFiller::SaveData(CPDFSDK_PageView* pPageView) {}

#ifdef PDF_ENABLE_XFA
bool CFFL_FormFiller::IsFieldFull(CPDFSDK_PageView* pPageView) {
  return false;
}
#endif  // PDF_ENABLE_XFA

void CFFL_FormFiller::SetChangeMark() {
  m_pFormFillEnv->OnChange();
}

void CFFL_FormFiller::GetActionData(CPDFSDK_PageView* pPageView,
                                    CPDF_AAction::AActionType type,
                                    PDFSDK_FieldAction& fa) {
  fa.sValue = m_pWidget->GetValue();
}

void CFFL_FormFiller::SetActionData(CPDFSDK_PageView* pPageView,
                                    CPDF_AAction::AActionType type,
                                    const PDFSDK_FieldAction& fa) {}

bool CFFL_FormFiller::IsActionDataChanged(CPDF_AAction::AActionType type,
                                          const PDFSDK_FieldAction& faOld,
                                          const PDFSDK_FieldAction& faNew) {
  return false;
}

void CFFL_FormFiller::SaveState(CPDFSDK_PageView* pPageView) {}

void CFFL_FormFiller::RestoreState(CPDFSDK_PageView* pPageView) {}

CPWL_Wnd* CFFL_FormFiller::ResetPDFWindow(CPDFSDK_PageView* pPageView,
                                          bool bRestoreValue) {
  return GetPDFWindow(pPageView, false);
}

void CFFL_FormFiller::TimerProc() {}

CFX_SystemHandler* CFFL_FormFiller::GetSystemHandler() const {
  return m_pFormFillEnv->GetSysHandler();
}

void CFFL_FormFiller::EscapeFiller(CPDFSDK_PageView* pPageView,
                                   bool bDestroyPDFWindow) {
  m_bValid = false;

  InvalidateRect(GetViewBBox(pPageView, m_pWidget));
  if (bDestroyPDFWindow)
    DestroyPDFWindow(pPageView);
}

void CFFL_FormFiller::InvalidateRect(const FX_RECT& rect) {
  m_pFormFillEnv->Invalidate(m_pWidget->GetUnderlyingPage(), rect);
}

CFFL_Button::CFFL_Button(CPDFSDK_FormFillEnvironment* pApp,
                         CPDFSDK_Annot* pWidget)
    : CFFL_FormFiller(pApp, pWidget), m_bMouseIn(false), m_bMouseDown(false) {}

CFFL_Button::~CFFL_Button() {}

void CFFL_Button::OnMouseEnter(CPDFSDK_PageView* pPageView,
                               CPDFSDK_Annot* pAnnot) {
  m_bMouseIn = true;
  InvalidateRect(GetViewBBox(pPageView, pAnnot));
}

void CFFL_Button::OnMouseExit(CPDFSDK_PageView* pPageView,
                              CPDFSDK_Annot* pAnnot) {
  m_bMouseIn = false;

  InvalidateRect(GetViewBBox(pPageView, pAnnot));
  EndTimer();
  ASSERT(m_pWidget);
}

bool CFFL_Button::OnLButtonDown(CPDFSDK_PageView* pPageView,
                                CPDFSDK_Annot* pAnnot,
                                uint32_t nFlags,
                                const CFX_PointF& point) {
  if (!pAnnot->GetRect().Contains(point))
    return false;

  m_bMouseDown = true;
  m_bValid = true;
  InvalidateRect(GetViewBBox(pPageView, pAnnot));
  return true;
}

bool CFFL_Button::OnLButtonUp(CPDFSDK_PageView* pPageView,
                              CPDFSDK_Annot* pAnnot,
                              uint32_t nFlags,
                              const CFX_PointF& point) {
  if (!pAnnot->GetRect().Contains(point))
    return false;

  m_bMouseDown = false;
  m_pWidget->GetPDFPage();

  InvalidateRect(GetViewBBox(pPageView, pAnnot));
  return true;
}

bool CFFL_Button::OnMouseMove(CPDFSDK_PageView* pPageView,
                              CPDFSDK_Annot* pAnnot,
                              uint32_t nFlags,
                              const CFX_PointF& point) {
  ASSERT(m_pFormFillEnv);
  return true;
}

void CFFL_Button::OnDraw(CPDFSDK_PageView* pPageView,
                         CPDFSDK_Annot* pAnnot,
                         CFX_RenderDevice* pDevice,
                         CFX_Matrix* pUser2Device) {
  ASSERT(pPageView);
  CPDFSDK_Widget* pWidget = (CPDFSDK_Widget*)pAnnot;
  CPDF_FormControl* pCtrl = pWidget->GetFormControl();
  CPDF_FormControl::HighlightingMode eHM = pCtrl->GetHighlightingMode();

  if (eHM != CPDF_FormControl::Push) {
    pWidget->DrawAppearance(pDevice, pUser2Device, CPDF_Annot::Normal, nullptr);
    return;
  }

  if (m_bMouseDown) {
    if (pWidget->IsWidgetAppearanceValid(CPDF_Annot::Down))
      pWidget->DrawAppearance(pDevice, pUser2Device, CPDF_Annot::Down, nullptr);
    else
      pWidget->DrawAppearance(pDevice, pUser2Device, CPDF_Annot::Normal,
                              nullptr);
  } else if (m_bMouseIn) {
    if (pWidget->IsWidgetAppearanceValid(CPDF_Annot::Rollover))
      pWidget->DrawAppearance(pDevice, pUser2Device, CPDF_Annot::Rollover,
                              nullptr);
    else
      pWidget->DrawAppearance(pDevice, pUser2Device, CPDF_Annot::Normal,
                              nullptr);
  } else {
    pWidget->DrawAppearance(pDevice, pUser2Device, CPDF_Annot::Normal, nullptr);
  }
}

void CFFL_Button::OnDrawDeactive(CPDFSDK_PageView* pPageView,
                                 CPDFSDK_Annot* pAnnot,
                                 CFX_RenderDevice* pDevice,
                                 CFX_Matrix* pUser2Device) {
  OnDraw(pPageView, pAnnot, pDevice, pUser2Device);
}
