// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_CPDFSDK_BAANNOT_H_
#define FPDFSDK_CPDFSDK_BAANNOT_H_

#include "core/fpdfdoc/cpdf_aaction.h"
#include "core/fpdfdoc/cpdf_action.h"
#include "core/fpdfdoc/cpdf_annot.h"
#include "core/fpdfdoc/cpdf_defaultappearance.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_string.h"
#include "fpdfsdk/cfx_systemhandler.h"
#include "fpdfsdk/cpdfsdk_annot.h"

class CFX_Matrix;
class CFX_RenderDevice;
class CPDF_Dictionary;
class CPDF_RenderOptions;
class CPDFSDK_PageView;

class CPDFSDK_BAAnnot : public CPDFSDK_Annot {
 public:
  CPDFSDK_BAAnnot(CPDF_Annot* pAnnot, CPDFSDK_PageView* pPageView);
  ~CPDFSDK_BAAnnot() override;

  // CPDFSDK_Annot
  CPDF_Annot::Subtype GetAnnotSubtype() const override;
  void SetRect(const CFX_FloatRect& rect) override;
  CFX_FloatRect GetRect() const override;
  CPDF_Annot* GetPDFAnnot() const override;
  void Annot_OnDraw(CFX_RenderDevice* pDevice,
                    CFX_Matrix* pUser2Device,
                    CPDF_RenderOptions* pOptions) override;

  CPDF_Dictionary* GetAnnotDict() const;
  CPDF_Annot* GetPDFPopupAnnot() const;

  void SetContents(const CFX_WideString& sContents);
  CFX_WideString GetContents() const;

  void SetAnnotName(const CFX_WideString& sName);
  CFX_WideString GetAnnotName() const;

  void SetModifiedDate(const FX_SYSTEMTIME& st);
  FX_SYSTEMTIME GetModifiedDate() const;

  void SetFlags(uint32_t nFlags);
  uint32_t GetFlags() const;

  void SetAppState(const CFX_ByteString& str);
  CFX_ByteString GetAppState() const;

  void SetStructParent(int key);
  int GetStructParent() const;

  void SetBorderWidth(int nWidth);
  int GetBorderWidth() const;

  void SetBorderStyle(BorderStyle nStyle);
  BorderStyle GetBorderStyle() const;

  void SetColor(FX_COLORREF color);
  void RemoveColor();
  bool GetColor(FX_COLORREF& color) const;

  bool IsVisible() const;

  CPDF_Action GetAction() const;
  void SetAction(const CPDF_Action& a);
  void RemoveAction();

  CPDF_AAction GetAAction() const;
  void SetAAction(const CPDF_AAction& aa);
  void RemoveAAction();

  virtual CPDF_Action GetAAction(CPDF_AAction::AActionType eAAT);
  virtual bool IsAppearanceValid();
  virtual bool IsAppearanceValid(CPDF_Annot::AppearanceMode mode);
  virtual void DrawAppearance(CFX_RenderDevice* pDevice,
                              const CFX_Matrix* pUser2Device,
                              CPDF_Annot::AppearanceMode mode,
                              const CPDF_RenderOptions* pOptions);

  void DrawBorder(CFX_RenderDevice* pDevice,
                  const CFX_Matrix* pUser2Device,
                  const CPDF_RenderOptions* pOptions);

  void ClearCachedAP();

  void WriteAppearance(const CFX_ByteString& sAPType,
                       const CFX_FloatRect& rcBBox,
                       const CFX_Matrix& matrix,
                       const CFX_ByteString& sContents,
                       const CFX_ByteString& sAPState = "");

  void SetOpenState(bool bState);

 protected:
  CPDF_Annot* const m_pAnnot;
};

#endif  // FPDFSDK_CPDFSDK_BAANNOT_H_
