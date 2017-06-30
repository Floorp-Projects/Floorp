// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_CPDFSDK_XFAWIDGETHANDLER_H_
#define FPDFSDK_CPDFSDK_XFAWIDGETHANDLER_H_

#include "core/fxcrt/fx_basic.h"
#include "core/fxcrt/fx_coordinates.h"
#include "fpdfsdk/ipdfsdk_annothandler.h"

class CFX_Matrix;
class CFX_RenderDevice;
class CPDF_Annot;
class CPDFSDK_FormFillEnvironment;
class CPDFSDK_Annot;
class CPDFSDK_PageView;
class CXFA_FFWidget;
class CXFA_FFWidgetHandler;

class CPDFSDK_XFAWidgetHandler : public IPDFSDK_AnnotHandler {
 public:
  explicit CPDFSDK_XFAWidgetHandler(CPDFSDK_FormFillEnvironment* pApp);
  ~CPDFSDK_XFAWidgetHandler() override;

  bool CanAnswer(CPDFSDK_Annot* pAnnot) override;
  CPDFSDK_Annot* NewAnnot(CPDF_Annot* pAnnot, CPDFSDK_PageView* pPage) override;
  CPDFSDK_Annot* NewAnnot(CXFA_FFWidget* pAnnot,
                          CPDFSDK_PageView* pPage) override;
  void ReleaseAnnot(CPDFSDK_Annot* pAnnot) override;
  CFX_FloatRect GetViewBBox(CPDFSDK_PageView* pPageView,
                            CPDFSDK_Annot* pAnnot) override;
  bool HitTest(CPDFSDK_PageView* pPageView,
               CPDFSDK_Annot* pAnnot,
               const CFX_PointF& point) override;
  void OnDraw(CPDFSDK_PageView* pPageView,
              CPDFSDK_Annot* pAnnot,
              CFX_RenderDevice* pDevice,
              CFX_Matrix* pUser2Device,
              bool bDrawAnnots) override;
  void OnLoad(CPDFSDK_Annot* pAnnot) override;
  void OnMouseEnter(CPDFSDK_PageView* pPageView,
                    CPDFSDK_Annot::ObservedPtr* pAnnot,
                    uint32_t nFlag) override;
  void OnMouseExit(CPDFSDK_PageView* pPageView,
                   CPDFSDK_Annot::ObservedPtr* pAnnot,
                   uint32_t nFlag) override;
  bool OnLButtonDown(CPDFSDK_PageView* pPageView,
                     CPDFSDK_Annot::ObservedPtr* pAnnot,
                     uint32_t nFlags,
                     const CFX_PointF& point) override;
  bool OnLButtonUp(CPDFSDK_PageView* pPageView,
                   CPDFSDK_Annot::ObservedPtr* pAnnot,
                   uint32_t nFlags,
                   const CFX_PointF& point) override;
  bool OnLButtonDblClk(CPDFSDK_PageView* pPageView,
                       CPDFSDK_Annot::ObservedPtr* pAnnot,
                       uint32_t nFlags,
                       const CFX_PointF& point) override;
  bool OnMouseMove(CPDFSDK_PageView* pPageView,
                   CPDFSDK_Annot::ObservedPtr* pAnnot,
                   uint32_t nFlags,
                   const CFX_PointF& point) override;
  bool OnMouseWheel(CPDFSDK_PageView* pPageView,
                    CPDFSDK_Annot::ObservedPtr* pAnnot,
                    uint32_t nFlags,
                    short zDelta,
                    const CFX_PointF& point) override;
  bool OnRButtonDown(CPDFSDK_PageView* pPageView,
                     CPDFSDK_Annot::ObservedPtr* pAnnot,
                     uint32_t nFlags,
                     const CFX_PointF& point) override;
  bool OnRButtonUp(CPDFSDK_PageView* pPageView,
                   CPDFSDK_Annot::ObservedPtr* pAnnot,
                   uint32_t nFlags,
                   const CFX_PointF& point) override;
  bool OnRButtonDblClk(CPDFSDK_PageView* pPageView,
                       CPDFSDK_Annot::ObservedPtr* pAnnot,
                       uint32_t nFlags,
                       const CFX_PointF& point) override;
  bool OnChar(CPDFSDK_Annot* pAnnot, uint32_t nChar, uint32_t nFlags) override;
  bool OnKeyDown(CPDFSDK_Annot* pAnnot, int nKeyCode, int nFlag) override;
  bool OnKeyUp(CPDFSDK_Annot* pAnnot, int nKeyCode, int nFlag) override;
  bool OnSetFocus(CPDFSDK_Annot::ObservedPtr* pAnnot, uint32_t nFlag) override;
  bool OnKillFocus(CPDFSDK_Annot::ObservedPtr* pAnnot, uint32_t nFlag) override;
  bool OnXFAChangedFocus(CPDFSDK_Annot::ObservedPtr* pOldAnnot,
                         CPDFSDK_Annot::ObservedPtr* pNewAnnot) override;

 private:
  CXFA_FFWidgetHandler* GetXFAWidgetHandler(CPDFSDK_Annot* pAnnot);
  uint32_t GetFWLFlags(uint32_t dwFlag);

  CPDFSDK_FormFillEnvironment* m_pFormFillEnv;
};

#endif  // FPDFSDK_CPDFSDK_XFAWIDGETHANDLER_H_
