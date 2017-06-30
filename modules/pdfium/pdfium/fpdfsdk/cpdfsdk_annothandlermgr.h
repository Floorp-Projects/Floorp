// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_CPDFSDK_ANNOTHANDLERMGR_H_
#define FPDFSDK_CPDFSDK_ANNOTHANDLERMGR_H_

#include <map>
#include <memory>

#include "core/fpdfdoc/cpdf_annot.h"
#include "core/fxcrt/fx_basic.h"
#include "core/fxcrt/fx_coordinates.h"
#include "fpdfsdk/cpdfsdk_annot.h"

class CFX_Matrix;
class CFX_RenderDevice;
class CPDFSDK_FormFillEnvironment;
class CPDFSDK_BAAnnotHandler;
class CPDFSDK_WidgetHandler;
class CPDFSDK_PageView;
class IPDFSDK_AnnotHandler;

#ifdef PDF_ENABLE_XFA
class CPDFSDK_XFAWidgetHandler;
class CXFA_FFWidget;
#endif  // PDF_ENABLE_XFA

class CPDFSDK_AnnotHandlerMgr {
 public:
  explicit CPDFSDK_AnnotHandlerMgr(CPDFSDK_FormFillEnvironment* pApp);
  ~CPDFSDK_AnnotHandlerMgr();

  CPDFSDK_Annot* NewAnnot(CPDF_Annot* pAnnot, CPDFSDK_PageView* pPageView);
#ifdef PDF_ENABLE_XFA
  CPDFSDK_Annot* NewAnnot(CXFA_FFWidget* pAnnot, CPDFSDK_PageView* pPageView);
#endif  // PDF_ENABLE_XFA
  void ReleaseAnnot(CPDFSDK_Annot* pAnnot);

  void Annot_OnCreate(CPDFSDK_Annot* pAnnot);
  void Annot_OnLoad(CPDFSDK_Annot* pAnnot);

  IPDFSDK_AnnotHandler* GetAnnotHandler(CPDFSDK_Annot* pAnnot) const;
  void Annot_OnDraw(CPDFSDK_PageView* pPageView,
                    CPDFSDK_Annot* pAnnot,
                    CFX_RenderDevice* pDevice,
                    CFX_Matrix* pUser2Device,
                    bool bDrawAnnots);

  void Annot_OnMouseEnter(CPDFSDK_PageView* pPageView,
                          CPDFSDK_Annot::ObservedPtr* pAnnot,
                          uint32_t nFlags);
  void Annot_OnMouseExit(CPDFSDK_PageView* pPageView,
                         CPDFSDK_Annot::ObservedPtr* pAnnot,
                         uint32_t nFlags);
  bool Annot_OnLButtonDown(CPDFSDK_PageView* pPageView,
                           CPDFSDK_Annot::ObservedPtr* pAnnot,
                           uint32_t nFlags,
                           const CFX_PointF& point);
  bool Annot_OnLButtonUp(CPDFSDK_PageView* pPageView,
                         CPDFSDK_Annot::ObservedPtr* pAnnot,
                         uint32_t nFlags,
                         const CFX_PointF& point);
  bool Annot_OnLButtonDblClk(CPDFSDK_PageView* pPageView,
                             CPDFSDK_Annot::ObservedPtr* pAnnot,
                             uint32_t nFlags,
                             const CFX_PointF& point);
  bool Annot_OnMouseMove(CPDFSDK_PageView* pPageView,
                         CPDFSDK_Annot::ObservedPtr* pAnnot,
                         uint32_t nFlags,
                         const CFX_PointF& point);
  bool Annot_OnMouseWheel(CPDFSDK_PageView* pPageView,
                          CPDFSDK_Annot::ObservedPtr* pAnnot,
                          uint32_t nFlags,
                          short zDelta,
                          const CFX_PointF& point);
  bool Annot_OnRButtonDown(CPDFSDK_PageView* pPageView,
                           CPDFSDK_Annot::ObservedPtr* pAnnot,
                           uint32_t nFlags,
                           const CFX_PointF& point);
  bool Annot_OnRButtonUp(CPDFSDK_PageView* pPageView,
                         CPDFSDK_Annot::ObservedPtr* pAnnot,
                         uint32_t nFlags,
                         const CFX_PointF& point);
  bool Annot_OnChar(CPDFSDK_Annot* pAnnot, uint32_t nChar, uint32_t nFlags);
  bool Annot_OnKeyDown(CPDFSDK_Annot* pAnnot, int nKeyCode, int nFlag);
  bool Annot_OnKeyUp(CPDFSDK_Annot* pAnnot, int nKeyCode, int nFlag);

  bool Annot_OnSetFocus(CPDFSDK_Annot::ObservedPtr* pAnnot, uint32_t nFlag);
  bool Annot_OnKillFocus(CPDFSDK_Annot::ObservedPtr* pAnnot, uint32_t nFlag);

#ifdef PDF_ENABLE_XFA
  bool Annot_OnChangeFocus(CPDFSDK_Annot::ObservedPtr* pSetAnnot,
                           CPDFSDK_Annot::ObservedPtr* pKillAnnot);
#endif  // PDF_ENABLE_XFA

  CFX_FloatRect Annot_OnGetViewBBox(CPDFSDK_PageView* pPageView,
                                    CPDFSDK_Annot* pAnnot);
  bool Annot_OnHitTest(CPDFSDK_PageView* pPageView,
                       CPDFSDK_Annot* pAnnot,
                       const CFX_PointF& point);

 private:
  IPDFSDK_AnnotHandler* GetAnnotHandler(CPDF_Annot::Subtype nSubtype) const;
  CPDFSDK_Annot* GetNextAnnot(CPDFSDK_Annot* pSDKAnnot, bool bNext);

  std::unique_ptr<CPDFSDK_BAAnnotHandler> m_pBAAnnotHandler;
  std::unique_ptr<CPDFSDK_WidgetHandler> m_pWidgetHandler;
#ifdef PDF_ENABLE_XFA
  std::unique_ptr<CPDFSDK_XFAWidgetHandler> m_pXFAWidgetHandler;
#endif  // PDF_ENABLE_XFA

  CPDFSDK_FormFillEnvironment* m_pFormFillEnv;
};

#endif  // FPDFSDK_CPDFSDK_ANNOTHANDLERMGR_H_
