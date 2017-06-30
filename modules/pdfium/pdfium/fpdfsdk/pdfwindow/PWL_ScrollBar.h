// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_PDFWINDOW_PWL_SCROLLBAR_H_
#define FPDFSDK_PDFWINDOW_PWL_SCROLLBAR_H_

#include "fpdfsdk/pdfwindow/PWL_Wnd.h"

class CPWL_SBButton;
class CPWL_ScrollBar;

struct PWL_SCROLL_INFO {
 public:
  PWL_SCROLL_INFO()
      : fContentMin(0.0f),
        fContentMax(0.0f),
        fPlateWidth(0.0f),
        fBigStep(0.0f),
        fSmallStep(0.0f) {}

  bool operator==(const PWL_SCROLL_INFO& that) const {
    return fContentMin == that.fContentMin && fContentMax == that.fContentMax &&
           fPlateWidth == that.fPlateWidth && fBigStep == that.fBigStep &&
           fSmallStep == that.fSmallStep;
  }
  bool operator!=(const PWL_SCROLL_INFO& that) const {
    return !(*this == that);
  }

  FX_FLOAT fContentMin;
  FX_FLOAT fContentMax;
  FX_FLOAT fPlateWidth;
  FX_FLOAT fBigStep;
  FX_FLOAT fSmallStep;
};

enum PWL_SCROLLBAR_TYPE { SBT_HSCROLL, SBT_VSCROLL };

enum PWL_SBBUTTON_TYPE { PSBT_MIN, PSBT_MAX, PSBT_POS };

class CPWL_SBButton : public CPWL_Wnd {
 public:
  CPWL_SBButton(PWL_SCROLLBAR_TYPE eScrollBarType,
                PWL_SBBUTTON_TYPE eButtonType);
  ~CPWL_SBButton() override;

  // CPWL_Wnd
  CFX_ByteString GetClassName() const override;
  void OnCreate(PWL_CREATEPARAM& cp) override;
  void GetThisAppearanceStream(CFX_ByteTextBuf& sAppStream) override;
  void DrawThisAppearance(CFX_RenderDevice* pDevice,
                          CFX_Matrix* pUser2Device) override;
  bool OnLButtonDown(const CFX_PointF& point, uint32_t nFlag) override;
  bool OnLButtonUp(const CFX_PointF& point, uint32_t nFlag) override;
  bool OnMouseMove(const CFX_PointF& point, uint32_t nFlag) override;

 protected:
  PWL_SCROLLBAR_TYPE m_eScrollBarType;
  PWL_SBBUTTON_TYPE m_eSBButtonType;

  bool m_bMouseDown;
};

struct PWL_FLOATRANGE {
 public:
  PWL_FLOATRANGE();
  PWL_FLOATRANGE(FX_FLOAT min, FX_FLOAT max);

  bool operator==(const PWL_FLOATRANGE& that) const {
    return fMin == that.fMin && fMax == that.fMax;
  }
  bool operator!=(const PWL_FLOATRANGE& that) const { return !(*this == that); }

  void Default();
  void Set(FX_FLOAT min, FX_FLOAT max);
  bool In(FX_FLOAT x) const;
  FX_FLOAT GetWidth() const;

  FX_FLOAT fMin;
  FX_FLOAT fMax;
};

struct PWL_SCROLL_PRIVATEDATA {
 public:
  PWL_SCROLL_PRIVATEDATA();

  bool operator==(const PWL_SCROLL_PRIVATEDATA& that) const {
    return ScrollRange == that.ScrollRange &&
           fClientWidth == that.fClientWidth && fScrollPos == that.fScrollPos &&
           fBigStep == that.fBigStep && fSmallStep == that.fSmallStep;
  }
  bool operator!=(const PWL_SCROLL_PRIVATEDATA& that) const {
    return !(*this == that);
  }

  void Default();
  void SetScrollRange(FX_FLOAT min, FX_FLOAT max);
  void SetClientWidth(FX_FLOAT width);
  void SetSmallStep(FX_FLOAT step);
  void SetBigStep(FX_FLOAT step);
  bool SetPos(FX_FLOAT pos);

  void AddSmall();
  void SubSmall();
  void AddBig();
  void SubBig();

  PWL_FLOATRANGE ScrollRange;
  FX_FLOAT fClientWidth;
  FX_FLOAT fScrollPos;
  FX_FLOAT fBigStep;
  FX_FLOAT fSmallStep;
};

class CPWL_ScrollBar : public CPWL_Wnd {
 public:
  explicit CPWL_ScrollBar(PWL_SCROLLBAR_TYPE sbType = SBT_HSCROLL);
  ~CPWL_ScrollBar() override;

  // CPWL_Wnd
  CFX_ByteString GetClassName() const override;
  void OnCreate(PWL_CREATEPARAM& cp) override;
  void RePosChildWnd() override;
  void GetThisAppearanceStream(CFX_ByteTextBuf& sAppStream) override;
  void DrawThisAppearance(CFX_RenderDevice* pDevice,
                          CFX_Matrix* pUser2Device) override;
  bool OnLButtonDown(const CFX_PointF& point, uint32_t nFlag) override;
  bool OnLButtonUp(const CFX_PointF& point, uint32_t nFlag) override;
  void OnNotify(CPWL_Wnd* pWnd,
                uint32_t msg,
                intptr_t wParam = 0,
                intptr_t lParam = 0) override;
  void CreateChildWnd(const PWL_CREATEPARAM& cp) override;
  void TimerProc() override;

  FX_FLOAT GetScrollBarWidth() const;
  PWL_SCROLLBAR_TYPE GetScrollBarType() const { return m_sbType; }

  void SetNotifyForever(bool bForever) { m_bNotifyForever = bForever; }

 protected:
  void SetScrollRange(FX_FLOAT fMin, FX_FLOAT fMax, FX_FLOAT fClientWidth);
  void SetScrollPos(FX_FLOAT fPos);
  void MovePosButton(bool bRefresh);
  void SetScrollStep(FX_FLOAT fBigStep, FX_FLOAT fSmallStep);
  void NotifyScrollWindow();
  CFX_FloatRect GetScrollArea() const;

 private:
  void CreateButtons(const PWL_CREATEPARAM& cp);

  void OnMinButtonLBDown(const CFX_PointF& point);
  void OnMinButtonLBUp(const CFX_PointF& point);
  void OnMinButtonMouseMove(const CFX_PointF& point);

  void OnMaxButtonLBDown(const CFX_PointF& point);
  void OnMaxButtonLBUp(const CFX_PointF& point);
  void OnMaxButtonMouseMove(const CFX_PointF& point);

  void OnPosButtonLBDown(const CFX_PointF& point);
  void OnPosButtonLBUp(const CFX_PointF& point);
  void OnPosButtonMouseMove(const CFX_PointF& point);

  FX_FLOAT TrueToFace(FX_FLOAT);
  FX_FLOAT FaceToTrue(FX_FLOAT);

  PWL_SCROLLBAR_TYPE m_sbType;
  PWL_SCROLL_INFO m_OriginInfo;
  CPWL_SBButton* m_pMinButton;
  CPWL_SBButton* m_pMaxButton;
  CPWL_SBButton* m_pPosButton;
  PWL_SCROLL_PRIVATEDATA m_sData;
  bool m_bMouseDown;
  bool m_bMinOrMax;
  bool m_bNotifyForever;
  FX_FLOAT m_nOldPos;
  FX_FLOAT m_fOldPosButton;
};

#endif  // FPDFSDK_PDFWINDOW_PWL_SCROLLBAR_H_
