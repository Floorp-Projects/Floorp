// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/pdfwindow/PWL_ListBox.h"

#include "fpdfsdk/fxedit/fxet_edit.h"
#include "fpdfsdk/fxedit/fxet_list.h"
#include "fpdfsdk/pdfwindow/PWL_Edit.h"
#include "fpdfsdk/pdfwindow/PWL_EditCtrl.h"
#include "fpdfsdk/pdfwindow/PWL_ScrollBar.h"
#include "fpdfsdk/pdfwindow/PWL_Utils.h"
#include "fpdfsdk/pdfwindow/PWL_Wnd.h"
#include "public/fpdf_fwlevent.h"
#include "third_party/base/ptr_util.h"

CPWL_List_Notify::CPWL_List_Notify(CPWL_ListBox* pList) : m_pList(pList) {
  ASSERT(m_pList);
}

CPWL_List_Notify::~CPWL_List_Notify() {}

void CPWL_List_Notify::IOnSetScrollInfoY(FX_FLOAT fPlateMin,
                                         FX_FLOAT fPlateMax,
                                         FX_FLOAT fContentMin,
                                         FX_FLOAT fContentMax,
                                         FX_FLOAT fSmallStep,
                                         FX_FLOAT fBigStep) {
  PWL_SCROLL_INFO Info;

  Info.fPlateWidth = fPlateMax - fPlateMin;
  Info.fContentMin = fContentMin;
  Info.fContentMax = fContentMax;
  Info.fSmallStep = fSmallStep;
  Info.fBigStep = fBigStep;

  m_pList->OnNotify(m_pList, PNM_SETSCROLLINFO, SBT_VSCROLL, (intptr_t)&Info);

  if (CPWL_ScrollBar* pScroll = m_pList->GetVScrollBar()) {
    if (IsFloatBigger(Info.fPlateWidth, Info.fContentMax - Info.fContentMin) ||
        IsFloatEqual(Info.fPlateWidth, Info.fContentMax - Info.fContentMin)) {
      if (pScroll->IsVisible()) {
        pScroll->SetVisible(false);
        m_pList->RePosChildWnd();
      }
    } else {
      if (!pScroll->IsVisible()) {
        pScroll->SetVisible(true);
        m_pList->RePosChildWnd();
      }
    }
  }
}

void CPWL_List_Notify::IOnSetScrollPosY(FX_FLOAT fy) {
  m_pList->OnNotify(m_pList, PNM_SETSCROLLPOS, SBT_VSCROLL, (intptr_t)&fy);
}

void CPWL_List_Notify::IOnInvalidateRect(CFX_FloatRect* pRect) {
  m_pList->InvalidateRect(pRect);
}

CPWL_ListBox::CPWL_ListBox()
    : m_pList(new CFX_ListCtrl),
      m_bMouseDown(false),
      m_bHoverSel(false),
      m_pFillerNotify(nullptr) {}

CPWL_ListBox::~CPWL_ListBox() {
}

CFX_ByteString CPWL_ListBox::GetClassName() const {
  return "CPWL_ListBox";
}

void CPWL_ListBox::OnCreated() {
  m_pList->SetFontMap(GetFontMap());
  m_pListNotify = pdfium::MakeUnique<CPWL_List_Notify>(this);
  m_pList->SetNotify(m_pListNotify.get());

  SetHoverSel(HasFlag(PLBS_HOVERSEL));
  m_pList->SetMultipleSel(HasFlag(PLBS_MULTIPLESEL));
  m_pList->SetFontSize(GetCreationParam().fFontSize);

  m_bHoverSel = HasFlag(PLBS_HOVERSEL);
}

void CPWL_ListBox::OnDestroy() {
  // Make sure the notifier is removed from the list as we are about to
  // destroy the notifier and don't want to leave a dangling pointer.
  m_pList->SetNotify(nullptr);
  m_pListNotify.reset();
}

void CPWL_ListBox::GetThisAppearanceStream(CFX_ByteTextBuf& sAppStream) {
  CPWL_Wnd::GetThisAppearanceStream(sAppStream);

  CFX_ByteTextBuf sListItems;

  CFX_FloatRect rcPlate = m_pList->GetPlateRect();
  for (int32_t i = 0, sz = m_pList->GetCount(); i < sz; i++) {
    CFX_FloatRect rcItem = m_pList->GetItemRect(i);

    if (rcItem.bottom > rcPlate.top || rcItem.top < rcPlate.bottom)
      continue;

    CFX_PointF ptOffset(rcItem.left, (rcItem.top + rcItem.bottom) * 0.5f);
    if (m_pList->IsItemSelected(i)) {
      sListItems << CPWL_Utils::GetRectFillAppStream(rcItem,
                                                     PWL_DEFAULT_SELBACKCOLOR)
                        .AsStringC();
      CFX_ByteString sItem =
          CPWL_Utils::GetEditAppStream(m_pList->GetItemEdit(i), ptOffset);
      if (sItem.GetLength() > 0) {
        sListItems << "BT\n"
                   << CPWL_Utils::GetColorAppStream(PWL_DEFAULT_SELTEXTCOLOR)
                          .AsStringC()
                   << sItem.AsStringC() << "ET\n";
      }
    } else {
      CFX_ByteString sItem =
          CPWL_Utils::GetEditAppStream(m_pList->GetItemEdit(i), ptOffset);
      if (sItem.GetLength() > 0) {
        sListItems << "BT\n"
                   << CPWL_Utils::GetColorAppStream(GetTextColor()).AsStringC()
                   << sItem.AsStringC() << "ET\n";
      }
    }
  }

  if (sListItems.GetLength() > 0) {
    CFX_ByteTextBuf sClip;
    CFX_FloatRect rcClient = GetClientRect();

    sClip << "q\n";
    sClip << rcClient.left << " " << rcClient.bottom << " "
          << rcClient.right - rcClient.left << " "
          << rcClient.top - rcClient.bottom << " re W n\n";

    sClip << sListItems << "Q\n";

    sAppStream << "/Tx BMC\n" << sClip << "EMC\n";
  }
}

void CPWL_ListBox::DrawThisAppearance(CFX_RenderDevice* pDevice,
                                      CFX_Matrix* pUser2Device) {
  CPWL_Wnd::DrawThisAppearance(pDevice, pUser2Device);

  CFX_FloatRect rcPlate = m_pList->GetPlateRect();
  CFX_FloatRect rcList = GetListRect();
  CFX_FloatRect rcClient = GetClientRect();

  for (int32_t i = 0, sz = m_pList->GetCount(); i < sz; i++) {
    CFX_FloatRect rcItem = m_pList->GetItemRect(i);
    if (rcItem.bottom > rcPlate.top || rcItem.top < rcPlate.bottom)
      continue;

    CFX_PointF ptOffset(rcItem.left, (rcItem.top + rcItem.bottom) * 0.5f);
    if (CFX_Edit* pEdit = m_pList->GetItemEdit(i)) {
      CFX_FloatRect rcContent = pEdit->GetContentRect();
      if (rcContent.Width() > rcClient.Width())
        rcItem.Intersect(rcList);
      else
        rcItem.Intersect(rcClient);
    }

    if (m_pList->IsItemSelected(i)) {
      CFX_SystemHandler* pSysHandler = GetSystemHandler();
      if (pSysHandler && pSysHandler->IsSelectionImplemented()) {
        CFX_Edit::DrawEdit(pDevice, pUser2Device, m_pList->GetItemEdit(i),
                           GetTextColor().ToFXColor(255), rcList, ptOffset,
                           nullptr, pSysHandler, m_pFormFiller);
        pSysHandler->OutputSelectedRect(m_pFormFiller, rcItem);
      } else {
        CPWL_Utils::DrawFillRect(pDevice, pUser2Device, rcItem,
                                 ArgbEncode(255, 0, 51, 113));
        CFX_Edit::DrawEdit(pDevice, pUser2Device, m_pList->GetItemEdit(i),
                           ArgbEncode(255, 255, 255, 255), rcList, ptOffset,
                           nullptr, pSysHandler, m_pFormFiller);
      }
    } else {
      CFX_SystemHandler* pSysHandler = GetSystemHandler();
      CFX_Edit::DrawEdit(pDevice, pUser2Device, m_pList->GetItemEdit(i),
                         GetTextColor().ToFXColor(255), rcList, ptOffset,
                         nullptr, pSysHandler, nullptr);
    }
  }
}

bool CPWL_ListBox::OnKeyDown(uint16_t nChar, uint32_t nFlag) {
  CPWL_Wnd::OnKeyDown(nChar, nFlag);

  switch (nChar) {
    default:
      return false;
    case FWL_VKEY_Up:
    case FWL_VKEY_Down:
    case FWL_VKEY_Home:
    case FWL_VKEY_Left:
    case FWL_VKEY_End:
    case FWL_VKEY_Right:
      break;
  }

  switch (nChar) {
    case FWL_VKEY_Up:
      m_pList->OnVK_UP(IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));
      break;
    case FWL_VKEY_Down:
      m_pList->OnVK_DOWN(IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));
      break;
    case FWL_VKEY_Home:
      m_pList->OnVK_HOME(IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));
      break;
    case FWL_VKEY_Left:
      m_pList->OnVK_LEFT(IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));
      break;
    case FWL_VKEY_End:
      m_pList->OnVK_END(IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));
      break;
    case FWL_VKEY_Right:
      m_pList->OnVK_RIGHT(IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));
      break;
    case FWL_VKEY_Delete:
      break;
  }

  bool bExit = false;
  OnNotifySelChanged(true, bExit, nFlag);

  return true;
}

bool CPWL_ListBox::OnChar(uint16_t nChar, uint32_t nFlag) {
  CPWL_Wnd::OnChar(nChar, nFlag);

  if (!m_pList->OnChar(nChar, IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag)))
    return false;

  bool bExit = false;
  OnNotifySelChanged(true, bExit, nFlag);

  return true;
}

bool CPWL_ListBox::OnLButtonDown(const CFX_PointF& point, uint32_t nFlag) {
  CPWL_Wnd::OnLButtonDown(point, nFlag);

  if (ClientHitTest(point)) {
    m_bMouseDown = true;
    SetFocus();
    SetCapture();

    m_pList->OnMouseDown(point, IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));
  }

  return true;
}

bool CPWL_ListBox::OnLButtonUp(const CFX_PointF& point, uint32_t nFlag) {
  CPWL_Wnd::OnLButtonUp(point, nFlag);

  if (m_bMouseDown) {
    ReleaseCapture();
    m_bMouseDown = false;
  }

  bool bExit = false;
  OnNotifySelChanged(false, bExit, nFlag);

  return true;
}

void CPWL_ListBox::SetHoverSel(bool bHoverSel) {
  m_bHoverSel = bHoverSel;
}

bool CPWL_ListBox::OnMouseMove(const CFX_PointF& point, uint32_t nFlag) {
  CPWL_Wnd::OnMouseMove(point, nFlag);

  if (m_bHoverSel && !IsCaptureMouse() && ClientHitTest(point))
    m_pList->Select(m_pList->GetItemIndex(point));
  if (m_bMouseDown)
    m_pList->OnMouseMove(point, IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));

  return true;
}

void CPWL_ListBox::OnNotify(CPWL_Wnd* pWnd,
                            uint32_t msg,
                            intptr_t wParam,
                            intptr_t lParam) {
  CPWL_Wnd::OnNotify(pWnd, msg, wParam, lParam);

  FX_FLOAT fPos;

  switch (msg) {
    case PNM_SETSCROLLINFO:
      switch (wParam) {
        case SBT_VSCROLL:
          if (CPWL_Wnd* pChild = GetVScrollBar()) {
            pChild->OnNotify(pWnd, PNM_SETSCROLLINFO, wParam, lParam);
          }
          break;
      }
      break;
    case PNM_SETSCROLLPOS:
      switch (wParam) {
        case SBT_VSCROLL:
          if (CPWL_Wnd* pChild = GetVScrollBar()) {
            pChild->OnNotify(pWnd, PNM_SETSCROLLPOS, wParam, lParam);
          }
          break;
      }
      break;
    case PNM_SCROLLWINDOW:
      fPos = *(FX_FLOAT*)lParam;
      switch (wParam) {
        case SBT_VSCROLL:
          m_pList->SetScrollPos(CFX_PointF(0, fPos));
          break;
      }
      break;
  }
}

void CPWL_ListBox::KillFocus() {
  CPWL_Wnd::KillFocus();
}

void CPWL_ListBox::RePosChildWnd() {
  CPWL_Wnd::RePosChildWnd();

  m_pList->SetPlateRect(GetListRect());
}

void CPWL_ListBox::OnNotifySelChanged(bool bKeyDown,
                                      bool& bExit,
                                      uint32_t nFlag) {
  if (!m_pFillerNotify)
    return;

  bool bRC = true;
  CFX_WideString swChange = GetText();
  CFX_WideString strChangeEx;
  int nSelStart = 0;
  int nSelEnd = swChange.GetLength();
  m_pFillerNotify->OnBeforeKeyStroke(GetAttachedData(), swChange, strChangeEx,
                                     nSelStart, nSelEnd, bKeyDown, bRC, bExit,
                                     nFlag);
}

CFX_FloatRect CPWL_ListBox::GetFocusRect() const {
  if (m_pList->IsMultipleSel()) {
    CFX_FloatRect rcCaret = m_pList->GetItemRect(m_pList->GetCaret());
    rcCaret.Intersect(GetClientRect());
    return rcCaret;
  }

  return CPWL_Wnd::GetFocusRect();
}

void CPWL_ListBox::AddString(const CFX_WideString& str) {
  m_pList->AddString(str);
}

CFX_WideString CPWL_ListBox::GetText() const {
  return m_pList->GetText();
}

void CPWL_ListBox::SetFontSize(FX_FLOAT fFontSize) {
  m_pList->SetFontSize(fFontSize);
}

FX_FLOAT CPWL_ListBox::GetFontSize() const {
  return m_pList->GetFontSize();
}

void CPWL_ListBox::Select(int32_t nItemIndex) {
  m_pList->Select(nItemIndex);
}

void CPWL_ListBox::SetCaret(int32_t nItemIndex) {
  m_pList->SetCaret(nItemIndex);
}

void CPWL_ListBox::SetTopVisibleIndex(int32_t nItemIndex) {
  m_pList->SetTopItem(nItemIndex);
}

void CPWL_ListBox::ScrollToListItem(int32_t nItemIndex) {
  m_pList->ScrollToListItem(nItemIndex);
}

void CPWL_ListBox::ResetContent() {
  m_pList->Empty();
}

void CPWL_ListBox::Reset() {
  m_pList->Cancel();
}

bool CPWL_ListBox::IsMultipleSel() const {
  return m_pList->IsMultipleSel();
}

int32_t CPWL_ListBox::GetCaretIndex() const {
  return m_pList->GetCaret();
}

int32_t CPWL_ListBox::GetCurSel() const {
  return m_pList->GetSelect();
}

bool CPWL_ListBox::IsItemSelected(int32_t nItemIndex) const {
  return m_pList->IsItemSelected(nItemIndex);
}

int32_t CPWL_ListBox::GetTopVisibleIndex() const {
  m_pList->ScrollToListItem(m_pList->GetFirstSelected());
  return m_pList->GetTopItem();
}

int32_t CPWL_ListBox::GetCount() const {
  return m_pList->GetCount();
}

int32_t CPWL_ListBox::FindNext(int32_t nIndex, FX_WCHAR nChar) const {
  return m_pList->FindNext(nIndex, nChar);
}

CFX_FloatRect CPWL_ListBox::GetContentRect() const {
  return m_pList->GetContentRect();
}

FX_FLOAT CPWL_ListBox::GetFirstHeight() const {
  return m_pList->GetFirstHeight();
}

CFX_FloatRect CPWL_ListBox::GetListRect() const {
  return CPWL_Utils::DeflateRect(
      GetWindowRect(), (FX_FLOAT)(GetBorderWidth() + GetInnerBorderWidth()));
}

bool CPWL_ListBox::OnMouseWheel(short zDelta,
                                const CFX_PointF& point,
                                uint32_t nFlag) {
  if (zDelta < 0)
    m_pList->OnVK_DOWN(IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));
  else
    m_pList->OnVK_UP(IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));

  bool bExit = false;
  OnNotifySelChanged(false, bExit, nFlag);
  return true;
}
