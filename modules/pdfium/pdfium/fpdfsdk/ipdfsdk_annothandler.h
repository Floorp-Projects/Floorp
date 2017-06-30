// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_IPDFSDK_ANNOTHANDLER_H_
#define FPDFSDK_IPDFSDK_ANNOTHANDLER_H_

#include "core/fxcrt/fx_basic.h"
#include "core/fxcrt/fx_coordinates.h"
#include "fpdfsdk/cpdfsdk_annot.h"

class CFX_Matrix;
class CFX_RenderDevice;
class CPDF_Annot;
class CPDFSDK_PageView;

#ifdef PDF_ENABLE_XFA
class CXFA_FFWidget;
#endif  // PDF_ENABLE_XFA

class IPDFSDK_AnnotHandler {
 public:
  virtual ~IPDFSDK_AnnotHandler() {}

  virtual bool CanAnswer(CPDFSDK_Annot* pAnnot) = 0;
  virtual CPDFSDK_Annot* NewAnnot(CPDF_Annot* pAnnot,
                                  CPDFSDK_PageView* pPage) = 0;

#ifdef PDF_ENABLE_XFA
  virtual CPDFSDK_Annot* NewAnnot(CXFA_FFWidget* hWidget,
                                  CPDFSDK_PageView* pPage) = 0;
#endif  // PDF_ENABLE_XFA

  virtual void ReleaseAnnot(CPDFSDK_Annot* pAnnot) = 0;
  virtual CFX_FloatRect GetViewBBox(CPDFSDK_PageView* pPageView,
                                    CPDFSDK_Annot* pAnnot) = 0;
  virtual bool HitTest(CPDFSDK_PageView* pPageView,
                       CPDFSDK_Annot* pAnnot,
                       const CFX_PointF& point) = 0;
  virtual void OnDraw(CPDFSDK_PageView* pPageView,
                      CPDFSDK_Annot* pAnnot,
                      CFX_RenderDevice* pDevice,
                      CFX_Matrix* pUser2Device,
                      bool bDrawAnnots) = 0;
  virtual void OnLoad(CPDFSDK_Annot* pAnnot) = 0;

  virtual void OnMouseEnter(CPDFSDK_PageView* pPageView,
                            CPDFSDK_Annot::ObservedPtr* pAnnot,
                            uint32_t nFlag) = 0;
  virtual void OnMouseExit(CPDFSDK_PageView* pPageView,
                           CPDFSDK_Annot::ObservedPtr* pAnnot,
                           uint32_t nFlag) = 0;
  virtual bool OnLButtonDown(CPDFSDK_PageView* pPageView,
                             CPDFSDK_Annot::ObservedPtr* pAnnot,
                             uint32_t nFlags,
                             const CFX_PointF& point) = 0;
  virtual bool OnLButtonUp(CPDFSDK_PageView* pPageView,
                           CPDFSDK_Annot::ObservedPtr* pAnnot,
                           uint32_t nFlags,
                           const CFX_PointF& point) = 0;
  virtual bool OnLButtonDblClk(CPDFSDK_PageView* pPageView,
                               CPDFSDK_Annot::ObservedPtr* pAnnot,
                               uint32_t nFlags,
                               const CFX_PointF& point) = 0;
  virtual bool OnMouseMove(CPDFSDK_PageView* pPageView,
                           CPDFSDK_Annot::ObservedPtr* pAnnot,
                           uint32_t nFlags,
                           const CFX_PointF& point) = 0;
  virtual bool OnMouseWheel(CPDFSDK_PageView* pPageView,
                            CPDFSDK_Annot::ObservedPtr* pAnnot,
                            uint32_t nFlags,
                            short zDelta,
                            const CFX_PointF& point) = 0;
  virtual bool OnRButtonDown(CPDFSDK_PageView* pPageView,
                             CPDFSDK_Annot::ObservedPtr* pAnnot,
                             uint32_t nFlags,
                             const CFX_PointF& point) = 0;
  virtual bool OnRButtonUp(CPDFSDK_PageView* pPageView,
                           CPDFSDK_Annot::ObservedPtr* pAnnot,
                           uint32_t nFlags,
                           const CFX_PointF& point) = 0;
  virtual bool OnRButtonDblClk(CPDFSDK_PageView* pPageView,
                               CPDFSDK_Annot::ObservedPtr* pAnnot,
                               uint32_t nFlags,
                               const CFX_PointF& point) = 0;
  virtual bool OnChar(CPDFSDK_Annot* pAnnot,
                      uint32_t nChar,
                      uint32_t nFlags) = 0;
  virtual bool OnKeyDown(CPDFSDK_Annot* pAnnot, int nKeyCode, int nFlag) = 0;
  virtual bool OnKeyUp(CPDFSDK_Annot* pAnnot, int nKeyCode, int nFlag) = 0;
  virtual bool OnSetFocus(CPDFSDK_Annot::ObservedPtr* pAnnot,
                          uint32_t nFlag) = 0;
  virtual bool OnKillFocus(CPDFSDK_Annot::ObservedPtr* pAnnot,
                           uint32_t nFlag) = 0;
#ifdef PDF_ENABLE_XFA
  virtual bool OnXFAChangedFocus(CPDFSDK_Annot::ObservedPtr* pOldAnnot,
                                 CPDFSDK_Annot::ObservedPtr* pNewAnnot) = 0;
#endif  // PDF_ENABLE_XFA
};

#endif  // FPDFSDK_IPDFSDK_ANNOTHANDLER_H_
