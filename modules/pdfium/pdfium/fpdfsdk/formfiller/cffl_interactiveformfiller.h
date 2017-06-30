// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_FORMFILLER_CFFL_INTERACTIVEFORMFILLER_H_
#define FPDFSDK_FORMFILLER_CFFL_INTERACTIVEFORMFILLER_H_

#include <map>
#include <memory>

#include "fpdfsdk/cpdfsdk_annot.h"
#include "fpdfsdk/fsdk_define.h"
#include "fpdfsdk/pdfwindow/PWL_Edit.h"

class CFFL_FormFiller;
class CPDFSDK_FormFillEnvironment;
class CPDFSDK_PageView;
class CPDFSDK_Widget;

class CFFL_InteractiveFormFiller : public IPWL_Filler_Notify {
 public:
  explicit CFFL_InteractiveFormFiller(
      CPDFSDK_FormFillEnvironment* pFormFillEnv);
  ~CFFL_InteractiveFormFiller() override;

  bool Annot_HitTest(CPDFSDK_PageView* pPageView,
                     CPDFSDK_Annot* pAnnot,
                     const CFX_PointF& point);
  FX_RECT GetViewBBox(CPDFSDK_PageView* pPageView, CPDFSDK_Annot* pAnnot);
  void OnDraw(CPDFSDK_PageView* pPageView,
              CPDFSDK_Annot* pAnnot,
              CFX_RenderDevice* pDevice,
              CFX_Matrix* pUser2Device);

  void OnDelete(CPDFSDK_Annot* pAnnot);

  void OnMouseEnter(CPDFSDK_PageView* pPageView,
                    CPDFSDK_Annot::ObservedPtr* pAnnot,
                    uint32_t nFlag);
  void OnMouseExit(CPDFSDK_PageView* pPageView,
                   CPDFSDK_Annot::ObservedPtr* pAnnot,
                   uint32_t nFlag);
  bool OnLButtonDown(CPDFSDK_PageView* pPageView,
                     CPDFSDK_Annot::ObservedPtr* pAnnot,
                     uint32_t nFlags,
                     const CFX_PointF& point);
  bool OnLButtonUp(CPDFSDK_PageView* pPageView,
                   CPDFSDK_Annot::ObservedPtr* pAnnot,
                   uint32_t nFlags,
                   const CFX_PointF& point);
  bool OnLButtonDblClk(CPDFSDK_PageView* pPageView,
                       CPDFSDK_Annot::ObservedPtr* pAnnot,
                       uint32_t nFlags,
                       const CFX_PointF& point);
  bool OnMouseMove(CPDFSDK_PageView* pPageView,
                   CPDFSDK_Annot::ObservedPtr* pAnnot,
                   uint32_t nFlags,
                   const CFX_PointF& point);
  bool OnMouseWheel(CPDFSDK_PageView* pPageView,
                    CPDFSDK_Annot::ObservedPtr* pAnnot,
                    uint32_t nFlags,
                    short zDelta,
                    const CFX_PointF& point);
  bool OnRButtonDown(CPDFSDK_PageView* pPageView,
                     CPDFSDK_Annot::ObservedPtr* pAnnot,
                     uint32_t nFlags,
                     const CFX_PointF& point);
  bool OnRButtonUp(CPDFSDK_PageView* pPageView,
                   CPDFSDK_Annot::ObservedPtr* pAnnot,
                   uint32_t nFlags,
                   const CFX_PointF& point);

  bool OnKeyDown(CPDFSDK_Annot* pAnnot, uint32_t nKeyCode, uint32_t nFlags);
  bool OnChar(CPDFSDK_Annot* pAnnot, uint32_t nChar, uint32_t nFlags);

  bool OnSetFocus(CPDFSDK_Annot::ObservedPtr* pAnnot, uint32_t nFlag);
  bool OnKillFocus(CPDFSDK_Annot::ObservedPtr* pAnnot, uint32_t nFlag);

  CFFL_FormFiller* GetFormFiller(CPDFSDK_Annot* pAnnot, bool bRegister);
  void RemoveFormFiller(CPDFSDK_Annot* pAnnot);

  static bool IsVisible(CPDFSDK_Widget* pWidget);
  static bool IsReadOnly(CPDFSDK_Widget* pWidget);
  static bool IsFillingAllowed(CPDFSDK_Widget* pWidget);
  static bool IsValidAnnot(CPDFSDK_PageView* pPageView, CPDFSDK_Annot* pAnnot);

  void OnKeyStrokeCommit(CPDFSDK_Annot::ObservedPtr* pWidget,
                         CPDFSDK_PageView* pPageView,
                         bool& bRC,
                         bool& bExit,
                         uint32_t nFlag);
  void OnValidate(CPDFSDK_Annot::ObservedPtr* pWidget,
                  CPDFSDK_PageView* pPageView,
                  bool& bRC,
                  bool& bExit,
                  uint32_t nFlag);

  void OnCalculate(CPDFSDK_Widget* pWidget,
                   CPDFSDK_PageView* pPageView,
                   bool& bExit,
                   uint32_t nFlag);
  void OnFormat(CPDFSDK_Widget* pWidget,
                CPDFSDK_PageView* pPageView,
                bool& bExit,
                uint32_t nFlag);
  void OnButtonUp(CPDFSDK_Annot::ObservedPtr* pWidget,
                  CPDFSDK_PageView* pPageView,
                  bool& bReset,
                  bool& bExit,
                  uint32_t nFlag);
#ifdef PDF_ENABLE_XFA
  void OnClick(CPDFSDK_Widget* pWidget,
               CPDFSDK_PageView* pPageView,
               bool& bReset,
               bool& bExit,
               uint32_t nFlag);
  void OnFull(CPDFSDK_Widget* pWidget,
              CPDFSDK_PageView* pPageView,
              bool& bReset,
              bool& bExit,
              uint32_t nFlag);
  void OnPreOpen(CPDFSDK_Widget* pWidget,
                 CPDFSDK_PageView* pPageView,
                 bool& bReset,
                 bool& bExit,
                 uint32_t nFlag);
  void OnPostOpen(CPDFSDK_Widget* pWidget,
                  CPDFSDK_PageView* pPageView,
                  bool& bReset,
                  bool& bExit,
                  uint32_t nFlag);
#endif  // PDF_ENABLE_XFA

 private:
  using CFFL_Widget2Filler =
      std::map<CPDFSDK_Annot*, std::unique_ptr<CFFL_FormFiller>>;

  // IPWL_Filler_Notify:
  void QueryWherePopup(void* pPrivateData,
                       FX_FLOAT fPopupMin,
                       FX_FLOAT fPopupMax,
                       int32_t& nRet,
                       FX_FLOAT& fPopupRet) override;
  void OnBeforeKeyStroke(void* pPrivateData,
                         CFX_WideString& strChange,
                         const CFX_WideString& strChangeEx,
                         int nSelStart,
                         int nSelEnd,
                         bool bKeyDown,
                         bool& bRC,
                         bool& bExit,
                         uint32_t nFlag) override;
#ifdef PDF_ENABLE_XFA
  void OnPopupPreOpen(void* pPrivateData, bool& bExit, uint32_t nFlag) override;
  void OnPopupPostOpen(void* pPrivateData,
                       bool& bExit,
                       uint32_t nFlag) override;
  void SetFocusAnnotTab(CPDFSDK_Annot* pWidget, bool bSameField, bool bNext);
#endif  // PDF_ENABLE_XFA
  void UnRegisterFormFiller(CPDFSDK_Annot* pAnnot);

  CPDFSDK_FormFillEnvironment* const m_pFormFillEnv;
  CFFL_Widget2Filler m_Maps;
  bool m_bNotifying;
};

class CFFL_PrivateData {
 public:
  CPDFSDK_Widget* pWidget;
  CPDFSDK_PageView* pPageView;
  int nWidgetAge;
  int nValueAge;
};

#endif  // FPDFSDK_FORMFILLER_CFFL_INTERACTIVEFORMFILLER_H_
