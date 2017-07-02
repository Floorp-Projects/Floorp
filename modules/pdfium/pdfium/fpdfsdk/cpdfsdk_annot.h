// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_CPDFSDK_ANNOT_H_
#define FPDFSDK_CPDFSDK_ANNOT_H_

#include "core/fpdfdoc/cpdf_aaction.h"
#include "core/fpdfdoc/cpdf_annot.h"
#include "core/fpdfdoc/cpdf_defaultappearance.h"
#include "core/fxcrt/cfx_observable.h"
#include "core/fxcrt/fx_basic.h"
#include "fpdfsdk/cfx_systemhandler.h"
#include "fpdfsdk/fsdk_common.h"
#include "fpdfsdk/fsdk_define.h"

class CFX_Matrix;
class CFX_RenderDevice;
class CPDF_Page;
class CPDF_RenderOptions;
class CPDFSDK_PageView;

class CPDFSDK_Annot : public CFX_Observable<CPDFSDK_Annot> {
 public:
  explicit CPDFSDK_Annot(CPDFSDK_PageView* pPageView);
  virtual ~CPDFSDK_Annot();

#ifdef PDF_ENABLE_XFA
  virtual bool IsXFAField();
  virtual CXFA_FFWidget* GetXFAWidget() const;
#endif  // PDF_ENABLE_XFA

  virtual FX_FLOAT GetMinWidth() const;
  virtual FX_FLOAT GetMinHeight() const;
  virtual int GetLayoutOrder() const;
  virtual CPDF_Annot* GetPDFAnnot() const;
  virtual CPDF_Annot::Subtype GetAnnotSubtype() const;
  virtual bool IsSignatureWidget() const;
  virtual CFX_FloatRect GetRect() const;

  virtual void SetRect(const CFX_FloatRect& rect);
  virtual void Annot_OnDraw(CFX_RenderDevice* pDevice,
                            CFX_Matrix* pUser2Device,
                            CPDF_RenderOptions* pOptions);

  UnderlyingPageType* GetUnderlyingPage();
  CPDF_Page* GetPDFPage();
#ifdef PDF_ENABLE_XFA
  CPDFXFA_Page* GetPDFXFAPage();
#endif  // PDF_ENABLE_XFA

  void SetPage(CPDFSDK_PageView* pPageView);
  CPDFSDK_PageView* GetPageView() const { return m_pPageView; }

  bool IsSelected();
  void SetSelected(bool bSelected);

 protected:
  CPDFSDK_PageView* m_pPageView;
  bool m_bSelected;
};

#endif  // FPDFSDK_CPDFSDK_ANNOT_H_
