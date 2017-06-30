// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_PDFWINDOW_PWL_LISTBOX_H_
#define FPDFSDK_PDFWINDOW_PWL_LISTBOX_H_

#include <memory>

#include "fpdfsdk/fxedit/fx_edit.h"
#include "fpdfsdk/pdfwindow/PWL_Wnd.h"

class CFX_ListCtrl;
class CPWL_List_Notify;
class CPWL_ListBox;
class IPWL_Filler_Notify;
struct CPVT_SecProps;
struct CPVT_WordPlace;
struct CPVT_WordProps;

class CPWL_List_Notify {
 public:
  explicit CPWL_List_Notify(CPWL_ListBox* pList);
  ~CPWL_List_Notify();

  void IOnSetScrollInfoY(FX_FLOAT fPlateMin,
                         FX_FLOAT fPlateMax,
                         FX_FLOAT fContentMin,
                         FX_FLOAT fContentMax,
                         FX_FLOAT fSmallStep,
                         FX_FLOAT fBigStep);
  void IOnSetScrollPosY(FX_FLOAT fy);
  void IOnInvalidateRect(CFX_FloatRect* pRect);

  void IOnSetCaret(bool bVisible,
                   const CFX_PointF& ptHead,
                   const CFX_PointF& ptFoot,
                   const CPVT_WordPlace& place);

 private:
  CPWL_ListBox* m_pList;
};

class CPWL_ListBox : public CPWL_Wnd {
 public:
  CPWL_ListBox();
  ~CPWL_ListBox() override;

  // CPWL_Wnd
  CFX_ByteString GetClassName() const override;
  void OnCreated() override;
  void OnDestroy() override;
  void GetThisAppearanceStream(CFX_ByteTextBuf& sAppStream) override;
  void DrawThisAppearance(CFX_RenderDevice* pDevice,
                          CFX_Matrix* pUser2Device) override;
  bool OnKeyDown(uint16_t nChar, uint32_t nFlag) override;
  bool OnChar(uint16_t nChar, uint32_t nFlag) override;
  bool OnLButtonDown(const CFX_PointF& point, uint32_t nFlag) override;
  bool OnLButtonUp(const CFX_PointF& point, uint32_t nFlag) override;
  bool OnMouseMove(const CFX_PointF& point, uint32_t nFlag) override;
  bool OnMouseWheel(short zDelta,
                    const CFX_PointF& point,
                    uint32_t nFlag) override;
  void KillFocus() override;
  void OnNotify(CPWL_Wnd* pWnd,
                uint32_t msg,
                intptr_t wParam = 0,
                intptr_t lParam = 0) override;
  void RePosChildWnd() override;
  CFX_FloatRect GetFocusRect() const override;
  void SetFontSize(FX_FLOAT fFontSize) override;
  FX_FLOAT GetFontSize() const override;

  virtual CFX_WideString GetText() const;

  void OnNotifySelChanged(bool bKeyDown, bool& bExit, uint32_t nFlag);

  void AddString(const CFX_WideString& str);
  void SetTopVisibleIndex(int32_t nItemIndex);
  void ScrollToListItem(int32_t nItemIndex);
  void ResetContent();
  void Reset();
  void Select(int32_t nItemIndex);
  void SetCaret(int32_t nItemIndex);
  void SetHoverSel(bool bHoverSel);

  int32_t GetCount() const;
  bool IsMultipleSel() const;
  int32_t GetCaretIndex() const;
  int32_t GetCurSel() const;
  bool IsItemSelected(int32_t nItemIndex) const;
  int32_t GetTopVisibleIndex() const;
  int32_t FindNext(int32_t nIndex, FX_WCHAR nChar) const;
  CFX_FloatRect GetContentRect() const;
  FX_FLOAT GetFirstHeight() const;
  CFX_FloatRect GetListRect() const;

  void SetFillerNotify(IPWL_Filler_Notify* pNotify) {
    m_pFillerNotify = pNotify;
  }

  void AttachFFLData(CFFL_FormFiller* pData) { m_pFormFiller = pData; }

 protected:
  std::unique_ptr<CFX_ListCtrl> m_pList;
  std::unique_ptr<CPWL_List_Notify> m_pListNotify;
  bool m_bMouseDown;
  bool m_bHoverSel;
  IPWL_Filler_Notify* m_pFillerNotify;

 private:
  CFFL_FormFiller* m_pFormFiller;  // Not owned.
};

#endif  // FPDFSDK_PDFWINDOW_PWL_LISTBOX_H_
