// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/pdfwindow/PWL_ScrollBar.h"

#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"
#include "fpdfsdk/pdfwindow/PWL_Utils.h"
#include "fpdfsdk/pdfwindow/PWL_Wnd.h"

PWL_FLOATRANGE::PWL_FLOATRANGE() {
  Default();
}

PWL_FLOATRANGE::PWL_FLOATRANGE(FX_FLOAT min, FX_FLOAT max) {
  Set(min, max);
}

void PWL_FLOATRANGE::Default() {
  fMin = 0;
  fMax = 0;
}

void PWL_FLOATRANGE::Set(FX_FLOAT min, FX_FLOAT max) {
  if (min > max) {
    fMin = max;
    fMax = min;
  } else {
    fMin = min;
    fMax = max;
  }
}

bool PWL_FLOATRANGE::In(FX_FLOAT x) const {
  return (IsFloatBigger(x, fMin) || IsFloatEqual(x, fMin)) &&
         (IsFloatSmaller(x, fMax) || IsFloatEqual(x, fMax));
}

FX_FLOAT PWL_FLOATRANGE::GetWidth() const {
  return fMax - fMin;
}

PWL_SCROLL_PRIVATEDATA::PWL_SCROLL_PRIVATEDATA() {
  Default();
}

void PWL_SCROLL_PRIVATEDATA::Default() {
  ScrollRange.Default();
  fScrollPos = ScrollRange.fMin;
  fClientWidth = 0;
  fBigStep = 10;
  fSmallStep = 1;
}

void PWL_SCROLL_PRIVATEDATA::SetScrollRange(FX_FLOAT min, FX_FLOAT max) {
  ScrollRange.Set(min, max);

  if (IsFloatSmaller(fScrollPos, ScrollRange.fMin))
    fScrollPos = ScrollRange.fMin;
  if (IsFloatBigger(fScrollPos, ScrollRange.fMax))
    fScrollPos = ScrollRange.fMax;
}

void PWL_SCROLL_PRIVATEDATA::SetClientWidth(FX_FLOAT width) {
  fClientWidth = width;
}

void PWL_SCROLL_PRIVATEDATA::SetSmallStep(FX_FLOAT step) {
  fSmallStep = step;
}

void PWL_SCROLL_PRIVATEDATA::SetBigStep(FX_FLOAT step) {
  fBigStep = step;
}

bool PWL_SCROLL_PRIVATEDATA::SetPos(FX_FLOAT pos) {
  if (ScrollRange.In(pos)) {
    fScrollPos = pos;
    return true;
  }
  return false;
}

void PWL_SCROLL_PRIVATEDATA::AddSmall() {
  if (!SetPos(fScrollPos + fSmallStep))
    SetPos(ScrollRange.fMax);
}

void PWL_SCROLL_PRIVATEDATA::SubSmall() {
  if (!SetPos(fScrollPos - fSmallStep))
    SetPos(ScrollRange.fMin);
}

void PWL_SCROLL_PRIVATEDATA::AddBig() {
  if (!SetPos(fScrollPos + fBigStep))
    SetPos(ScrollRange.fMax);
}

void PWL_SCROLL_PRIVATEDATA::SubBig() {
  if (!SetPos(fScrollPos - fBigStep))
    SetPos(ScrollRange.fMin);
}

CPWL_SBButton::CPWL_SBButton(PWL_SCROLLBAR_TYPE eScrollBarType,
                             PWL_SBBUTTON_TYPE eButtonType) {
  m_eScrollBarType = eScrollBarType;
  m_eSBButtonType = eButtonType;

  m_bMouseDown = false;
}

CPWL_SBButton::~CPWL_SBButton() {}

CFX_ByteString CPWL_SBButton::GetClassName() const {
  return "CPWL_SBButton";
}

void CPWL_SBButton::OnCreate(PWL_CREATEPARAM& cp) {
  cp.eCursorType = FXCT_ARROW;
}

void CPWL_SBButton::GetThisAppearanceStream(CFX_ByteTextBuf& sAppStream) {
  CPWL_Wnd::GetThisAppearanceStream(sAppStream);

  if (!IsVisible())
    return;

  CFX_ByteTextBuf sButton;

  CFX_FloatRect rectWnd = GetWindowRect();

  if (rectWnd.IsEmpty())
    return;

  sAppStream << "q\n";

  CFX_PointF ptCenter = GetCenterPoint();

  switch (m_eScrollBarType) {
    case SBT_HSCROLL:
      switch (m_eSBButtonType) {
        case PSBT_MIN: {
          CFX_PointF pt1(ptCenter.x - PWL_TRIANGLE_HALFLEN * 0.5f, ptCenter.y);
          CFX_PointF pt2(ptCenter.x + PWL_TRIANGLE_HALFLEN * 0.5f,
                         ptCenter.y + PWL_TRIANGLE_HALFLEN);
          CFX_PointF pt3(ptCenter.x + PWL_TRIANGLE_HALFLEN * 0.5f,
                         ptCenter.y - PWL_TRIANGLE_HALFLEN);

          if (rectWnd.right - rectWnd.left > PWL_TRIANGLE_HALFLEN * 2 &&
              rectWnd.top - rectWnd.bottom > PWL_TRIANGLE_HALFLEN) {
            sButton << "0 g\n";
            sButton << pt1.x << " " << pt1.y << " m\n";
            sButton << pt2.x << " " << pt2.y << " l\n";
            sButton << pt3.x << " " << pt3.y << " l\n";
            sButton << pt1.x << " " << pt1.y << " l f\n";

            sAppStream << sButton;
          }
        } break;
        case PSBT_MAX: {
          CFX_PointF pt1(ptCenter.x + PWL_TRIANGLE_HALFLEN * 0.5f, ptCenter.y);
          CFX_PointF pt2(ptCenter.x - PWL_TRIANGLE_HALFLEN * 0.5f,
                         ptCenter.y + PWL_TRIANGLE_HALFLEN);
          CFX_PointF pt3(ptCenter.x - PWL_TRIANGLE_HALFLEN * 0.5f,
                         ptCenter.y - PWL_TRIANGLE_HALFLEN);

          if (rectWnd.right - rectWnd.left > PWL_TRIANGLE_HALFLEN * 2 &&
              rectWnd.top - rectWnd.bottom > PWL_TRIANGLE_HALFLEN) {
            sButton << "0 g\n";
            sButton << pt1.x << " " << pt1.y << " m\n";
            sButton << pt2.x << " " << pt2.y << " l\n";
            sButton << pt3.x << " " << pt3.y << " l\n";
            sButton << pt1.x << " " << pt1.y << " l f\n";

            sAppStream << sButton;
          }
        } break;
        default:
          break;
      }
      break;
    case SBT_VSCROLL:
      switch (m_eSBButtonType) {
        case PSBT_MIN: {
          CFX_PointF pt1(ptCenter.x - PWL_TRIANGLE_HALFLEN,
                         ptCenter.y - PWL_TRIANGLE_HALFLEN * 0.5f);
          CFX_PointF pt2(ptCenter.x + PWL_TRIANGLE_HALFLEN,
                         ptCenter.y - PWL_TRIANGLE_HALFLEN * 0.5f);
          CFX_PointF pt3(ptCenter.x, ptCenter.y + PWL_TRIANGLE_HALFLEN * 0.5f);

          if (rectWnd.right - rectWnd.left > PWL_TRIANGLE_HALFLEN * 2 &&
              rectWnd.top - rectWnd.bottom > PWL_TRIANGLE_HALFLEN) {
            sButton << "0 g\n";
            sButton << pt1.x << " " << pt1.y << " m\n";
            sButton << pt2.x << " " << pt2.y << " l\n";
            sButton << pt3.x << " " << pt3.y << " l\n";
            sButton << pt1.x << " " << pt1.y << " l f\n";

            sAppStream << sButton;
          }
        } break;
        case PSBT_MAX: {
          CFX_PointF pt1(ptCenter.x - PWL_TRIANGLE_HALFLEN,
                         ptCenter.y + PWL_TRIANGLE_HALFLEN * 0.5f);
          CFX_PointF pt2(ptCenter.x + PWL_TRIANGLE_HALFLEN,
                         ptCenter.y + PWL_TRIANGLE_HALFLEN * 0.5f);
          CFX_PointF pt3(ptCenter.x, ptCenter.y - PWL_TRIANGLE_HALFLEN * 0.5f);

          if (rectWnd.right - rectWnd.left > PWL_TRIANGLE_HALFLEN * 2 &&
              rectWnd.top - rectWnd.bottom > PWL_TRIANGLE_HALFLEN) {
            sButton << "0 g\n";
            sButton << pt1.x << " " << pt1.y << " m\n";
            sButton << pt2.x << " " << pt2.y << " l\n";
            sButton << pt3.x << " " << pt3.y << " l\n";
            sButton << pt1.x << " " << pt1.y << " l f\n";

            sAppStream << sButton;
          }
        } break;
        default:
          break;
      }
      break;
    default:
      break;
  }

  sAppStream << "Q\n";
}

void CPWL_SBButton::DrawThisAppearance(CFX_RenderDevice* pDevice,
                                       CFX_Matrix* pUser2Device) {
  if (!IsVisible())
    return;

  CFX_FloatRect rectWnd = GetWindowRect();
  if (rectWnd.IsEmpty())
    return;

  CFX_PointF ptCenter = GetCenterPoint();
  int32_t nTransparency = GetTransparency();

  switch (m_eScrollBarType) {
    case SBT_HSCROLL:
      CPWL_Wnd::DrawThisAppearance(pDevice, pUser2Device);
      switch (m_eSBButtonType) {
        case PSBT_MIN: {
          CFX_PointF pt1(ptCenter.x - PWL_TRIANGLE_HALFLEN * 0.5f, ptCenter.y);
          CFX_PointF pt2(ptCenter.x + PWL_TRIANGLE_HALFLEN * 0.5f,
                         ptCenter.y + PWL_TRIANGLE_HALFLEN);
          CFX_PointF pt3(ptCenter.x + PWL_TRIANGLE_HALFLEN * 0.5f,
                         ptCenter.y - PWL_TRIANGLE_HALFLEN);

          if (rectWnd.right - rectWnd.left > PWL_TRIANGLE_HALFLEN * 2 &&
              rectWnd.top - rectWnd.bottom > PWL_TRIANGLE_HALFLEN) {
            CFX_PathData path;
            path.AppendPoint(pt1, FXPT_TYPE::MoveTo, false);
            path.AppendPoint(pt2, FXPT_TYPE::LineTo, false);
            path.AppendPoint(pt3, FXPT_TYPE::LineTo, false);
            path.AppendPoint(pt1, FXPT_TYPE::LineTo, false);

            pDevice->DrawPath(&path, pUser2Device, nullptr,
                              PWL_DEFAULT_BLACKCOLOR.ToFXColor(nTransparency),
                              0, FXFILL_ALTERNATE);
          }
        } break;
        case PSBT_MAX: {
          CFX_PointF pt1(ptCenter.x + PWL_TRIANGLE_HALFLEN * 0.5f, ptCenter.y);
          CFX_PointF pt2(ptCenter.x - PWL_TRIANGLE_HALFLEN * 0.5f,
                         ptCenter.y + PWL_TRIANGLE_HALFLEN);
          CFX_PointF pt3(ptCenter.x - PWL_TRIANGLE_HALFLEN * 0.5f,
                         ptCenter.y - PWL_TRIANGLE_HALFLEN);

          if (rectWnd.right - rectWnd.left > PWL_TRIANGLE_HALFLEN * 2 &&
              rectWnd.top - rectWnd.bottom > PWL_TRIANGLE_HALFLEN) {
            CFX_PathData path;
            path.AppendPoint(pt1, FXPT_TYPE::MoveTo, false);
            path.AppendPoint(pt2, FXPT_TYPE::LineTo, false);
            path.AppendPoint(pt3, FXPT_TYPE::LineTo, false);
            path.AppendPoint(pt1, FXPT_TYPE::LineTo, false);

            pDevice->DrawPath(&path, pUser2Device, nullptr,
                              PWL_DEFAULT_BLACKCOLOR.ToFXColor(nTransparency),
                              0, FXFILL_ALTERNATE);
          }
        } break;
        default:
          break;
      }
      break;
    case SBT_VSCROLL:
      switch (m_eSBButtonType) {
        case PSBT_MIN: {
          // draw border
          CFX_FloatRect rcDraw = rectWnd;
          CPWL_Utils::DrawStrokeRect(pDevice, pUser2Device, rcDraw,
                                     ArgbEncode(nTransparency, 100, 100, 100),
                                     0.0f);

          // draw inner border
          rcDraw = CPWL_Utils::DeflateRect(rectWnd, 0.5f);
          CPWL_Utils::DrawStrokeRect(pDevice, pUser2Device, rcDraw,
                                     ArgbEncode(nTransparency, 255, 255, 255),
                                     1.0f);

          // draw background

          rcDraw = CPWL_Utils::DeflateRect(rectWnd, 1.0f);

          if (IsEnabled())
            CPWL_Utils::DrawShadow(pDevice, pUser2Device, true, false, rcDraw,
                                   nTransparency, 80, 220);
          else
            CPWL_Utils::DrawFillRect(pDevice, pUser2Device, rcDraw,
                                     ArgbEncode(255, 255, 255, 255));

          // draw arrow

          if (rectWnd.top - rectWnd.bottom > 6.0f) {
            FX_FLOAT fX = rectWnd.left + 1.5f;
            FX_FLOAT fY = rectWnd.bottom;
            CFX_PointF pts[7] = {CFX_PointF(fX + 2.5f, fY + 4.0f),
                                 CFX_PointF(fX + 2.5f, fY + 3.0f),
                                 CFX_PointF(fX + 4.5f, fY + 5.0f),
                                 CFX_PointF(fX + 6.5f, fY + 3.0f),
                                 CFX_PointF(fX + 6.5f, fY + 4.0f),
                                 CFX_PointF(fX + 4.5f, fY + 6.0f),
                                 CFX_PointF(fX + 2.5f, fY + 4.0f)};

            if (IsEnabled())
              CPWL_Utils::DrawFillArea(
                  pDevice, pUser2Device, pts, 7,
                  ArgbEncode(nTransparency, 255, 255, 255));
            else
              CPWL_Utils::DrawFillArea(
                  pDevice, pUser2Device, pts, 7,
                  PWL_DEFAULT_HEAVYGRAYCOLOR.ToFXColor(255));
          }
        } break;
        case PSBT_MAX: {
          // draw border
          CFX_FloatRect rcDraw = rectWnd;
          CPWL_Utils::DrawStrokeRect(pDevice, pUser2Device, rcDraw,
                                     ArgbEncode(nTransparency, 100, 100, 100),
                                     0.0f);

          // draw inner border
          rcDraw = CPWL_Utils::DeflateRect(rectWnd, 0.5f);
          CPWL_Utils::DrawStrokeRect(pDevice, pUser2Device, rcDraw,
                                     ArgbEncode(nTransparency, 255, 255, 255),
                                     1.0f);

          // draw background
          rcDraw = CPWL_Utils::DeflateRect(rectWnd, 1.0f);
          if (IsEnabled())
            CPWL_Utils::DrawShadow(pDevice, pUser2Device, true, false, rcDraw,
                                   nTransparency, 80, 220);
          else
            CPWL_Utils::DrawFillRect(pDevice, pUser2Device, rcDraw,
                                     ArgbEncode(255, 255, 255, 255));

          // draw arrow

          if (rectWnd.top - rectWnd.bottom > 6.0f) {
            FX_FLOAT fX = rectWnd.left + 1.5f;
            FX_FLOAT fY = rectWnd.bottom;

            CFX_PointF pts[7] = {CFX_PointF(fX + 2.5f, fY + 5.0f),
                                 CFX_PointF(fX + 2.5f, fY + 6.0f),
                                 CFX_PointF(fX + 4.5f, fY + 4.0f),
                                 CFX_PointF(fX + 6.5f, fY + 6.0f),
                                 CFX_PointF(fX + 6.5f, fY + 5.0f),
                                 CFX_PointF(fX + 4.5f, fY + 3.0f),
                                 CFX_PointF(fX + 2.5f, fY + 5.0f)};

            if (IsEnabled())
              CPWL_Utils::DrawFillArea(
                  pDevice, pUser2Device, pts, 7,
                  ArgbEncode(nTransparency, 255, 255, 255));
            else
              CPWL_Utils::DrawFillArea(
                  pDevice, pUser2Device, pts, 7,
                  PWL_DEFAULT_HEAVYGRAYCOLOR.ToFXColor(255));
          }
        } break;
        case PSBT_POS: {
          // draw border
          CFX_FloatRect rcDraw = rectWnd;
          CPWL_Utils::DrawStrokeRect(pDevice, pUser2Device, rcDraw,
                                     ArgbEncode(nTransparency, 100, 100, 100),
                                     0.0f);

          // draw inner border
          rcDraw = CPWL_Utils::DeflateRect(rectWnd, 0.5f);
          CPWL_Utils::DrawStrokeRect(pDevice, pUser2Device, rcDraw,
                                     ArgbEncode(nTransparency, 255, 255, 255),
                                     1.0f);

          if (IsEnabled()) {
            // draw shadow effect

            CFX_PointF ptTop = CFX_PointF(rectWnd.left, rectWnd.top - 1.0f);
            CFX_PointF ptBottom =
                CFX_PointF(rectWnd.left, rectWnd.bottom + 1.0f);

            ptTop.x += 1.5f;
            ptBottom.x += 1.5f;

            CPWL_Utils::DrawStrokeLine(pDevice, pUser2Device, ptTop, ptBottom,
                                       ArgbEncode(nTransparency, 210, 210, 210),
                                       1.0f);

            ptTop.x += 1.0f;
            ptBottom.x += 1.0f;

            CPWL_Utils::DrawStrokeLine(pDevice, pUser2Device, ptTop, ptBottom,
                                       ArgbEncode(nTransparency, 220, 220, 220),
                                       1.0f);

            ptTop.x += 1.0f;
            ptBottom.x += 1.0f;

            CPWL_Utils::DrawStrokeLine(pDevice, pUser2Device, ptTop, ptBottom,
                                       ArgbEncode(nTransparency, 240, 240, 240),
                                       1.0f);

            ptTop.x += 1.0f;
            ptBottom.x += 1.0f;

            CPWL_Utils::DrawStrokeLine(pDevice, pUser2Device, ptTop, ptBottom,
                                       ArgbEncode(nTransparency, 240, 240, 240),
                                       1.0f);

            ptTop.x += 1.0f;
            ptBottom.x += 1.0f;

            CPWL_Utils::DrawStrokeLine(pDevice, pUser2Device, ptTop, ptBottom,
                                       ArgbEncode(nTransparency, 210, 210, 210),
                                       1.0f);

            ptTop.x += 1.0f;
            ptBottom.x += 1.0f;

            CPWL_Utils::DrawStrokeLine(pDevice, pUser2Device, ptTop, ptBottom,
                                       ArgbEncode(nTransparency, 180, 180, 180),
                                       1.0f);

            ptTop.x += 1.0f;
            ptBottom.x += 1.0f;

            CPWL_Utils::DrawStrokeLine(pDevice, pUser2Device, ptTop, ptBottom,
                                       ArgbEncode(nTransparency, 150, 150, 150),
                                       1.0f);

            ptTop.x += 1.0f;
            ptBottom.x += 1.0f;

            CPWL_Utils::DrawStrokeLine(pDevice, pUser2Device, ptTop, ptBottom,
                                       ArgbEncode(nTransparency, 150, 150, 150),
                                       1.0f);

            ptTop.x += 1.0f;
            ptBottom.x += 1.0f;

            CPWL_Utils::DrawStrokeLine(pDevice, pUser2Device, ptTop, ptBottom,
                                       ArgbEncode(nTransparency, 180, 180, 180),
                                       1.0f);

            ptTop.x += 1.0f;
            ptBottom.x += 1.0f;

            CPWL_Utils::DrawStrokeLine(pDevice, pUser2Device, ptTop, ptBottom,
                                       ArgbEncode(nTransparency, 210, 210, 210),
                                       1.0f);
          } else {
            CPWL_Utils::DrawFillRect(pDevice, pUser2Device, rcDraw,
                                     ArgbEncode(255, 255, 255, 255));
          }

          // draw friction

          if (rectWnd.Height() > 8.0f) {
            FX_COLORREF crStroke = ArgbEncode(nTransparency, 120, 120, 120);
            if (!IsEnabled())
              crStroke = PWL_DEFAULT_HEAVYGRAYCOLOR.ToFXColor(255);

            FX_FLOAT nFrictionWidth = 5.0f;
            FX_FLOAT nFrictionHeight = 5.5f;

            CFX_PointF ptLeft =
                CFX_PointF(ptCenter.x - nFrictionWidth / 2.0f,
                           ptCenter.y - nFrictionHeight / 2.0f + 0.5f);
            CFX_PointF ptRight =
                CFX_PointF(ptCenter.x + nFrictionWidth / 2.0f,
                           ptCenter.y - nFrictionHeight / 2.0f + 0.5f);

            CPWL_Utils::DrawStrokeLine(pDevice, pUser2Device, ptLeft, ptRight,
                                       crStroke, 1.0f);

            ptLeft.y += 2.0f;
            ptRight.y += 2.0f;

            CPWL_Utils::DrawStrokeLine(pDevice, pUser2Device, ptLeft, ptRight,
                                       crStroke, 1.0f);

            ptLeft.y += 2.0f;
            ptRight.y += 2.0f;

            CPWL_Utils::DrawStrokeLine(pDevice, pUser2Device, ptLeft, ptRight,
                                       crStroke, 1.0f);
          }
        } break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

bool CPWL_SBButton::OnLButtonDown(const CFX_PointF& point, uint32_t nFlag) {
  CPWL_Wnd::OnLButtonDown(point, nFlag);

  if (CPWL_Wnd* pParent = GetParentWindow())
    pParent->OnNotify(this, PNM_LBUTTONDOWN, 0, (intptr_t)&point);

  m_bMouseDown = true;
  SetCapture();

  return true;
}

bool CPWL_SBButton::OnLButtonUp(const CFX_PointF& point, uint32_t nFlag) {
  CPWL_Wnd::OnLButtonUp(point, nFlag);

  if (CPWL_Wnd* pParent = GetParentWindow())
    pParent->OnNotify(this, PNM_LBUTTONUP, 0, (intptr_t)&point);

  m_bMouseDown = false;
  ReleaseCapture();

  return true;
}

bool CPWL_SBButton::OnMouseMove(const CFX_PointF& point, uint32_t nFlag) {
  CPWL_Wnd::OnMouseMove(point, nFlag);

  if (CPWL_Wnd* pParent = GetParentWindow()) {
    pParent->OnNotify(this, PNM_MOUSEMOVE, 0, (intptr_t)&point);
  }

  return true;
}

CPWL_ScrollBar::CPWL_ScrollBar(PWL_SCROLLBAR_TYPE sbType)
    : m_sbType(sbType),
      m_pMinButton(nullptr),
      m_pMaxButton(nullptr),
      m_pPosButton(nullptr),
      m_bMouseDown(false),
      m_bMinOrMax(false),
      m_bNotifyForever(true) {}

CPWL_ScrollBar::~CPWL_ScrollBar() {}

CFX_ByteString CPWL_ScrollBar::GetClassName() const {
  return "CPWL_ScrollBar";
}

void CPWL_ScrollBar::OnCreate(PWL_CREATEPARAM& cp) {
  cp.eCursorType = FXCT_ARROW;
}

void CPWL_ScrollBar::RePosChildWnd() {
  CFX_FloatRect rcClient = GetClientRect();
  CFX_FloatRect rcMinButton, rcMaxButton;
  FX_FLOAT fBWidth = 0;

  switch (m_sbType) {
    case SBT_HSCROLL:
      if (rcClient.right - rcClient.left >
          PWL_SCROLLBAR_BUTTON_WIDTH * 2 + PWL_SCROLLBAR_POSBUTTON_MINWIDTH +
              2) {
        rcMinButton = CFX_FloatRect(rcClient.left, rcClient.bottom,
                                    rcClient.left + PWL_SCROLLBAR_BUTTON_WIDTH,
                                    rcClient.top);
        rcMaxButton =
            CFX_FloatRect(rcClient.right - PWL_SCROLLBAR_BUTTON_WIDTH,
                          rcClient.bottom, rcClient.right, rcClient.top);
      } else {
        fBWidth = (rcClient.right - rcClient.left -
                   PWL_SCROLLBAR_POSBUTTON_MINWIDTH - 2) /
                  2;

        if (fBWidth > 0) {
          rcMinButton = CFX_FloatRect(rcClient.left, rcClient.bottom,
                                      rcClient.left + fBWidth, rcClient.top);
          rcMaxButton = CFX_FloatRect(rcClient.right - fBWidth, rcClient.bottom,
                                      rcClient.right, rcClient.top);
        } else {
          SetVisible(false);
        }
      }
      break;
    case SBT_VSCROLL:
      if (IsFloatBigger(rcClient.top - rcClient.bottom,
                        PWL_SCROLLBAR_BUTTON_WIDTH * 2 +
                            PWL_SCROLLBAR_POSBUTTON_MINWIDTH + 2)) {
        rcMinButton = CFX_FloatRect(rcClient.left,
                                    rcClient.top - PWL_SCROLLBAR_BUTTON_WIDTH,
                                    rcClient.right, rcClient.top);
        rcMaxButton =
            CFX_FloatRect(rcClient.left, rcClient.bottom, rcClient.right,
                          rcClient.bottom + PWL_SCROLLBAR_BUTTON_WIDTH);
      } else {
        fBWidth = (rcClient.top - rcClient.bottom -
                   PWL_SCROLLBAR_POSBUTTON_MINWIDTH - 2) /
                  2;

        if (IsFloatBigger(fBWidth, 0)) {
          rcMinButton = CFX_FloatRect(rcClient.left, rcClient.top - fBWidth,
                                      rcClient.right, rcClient.top);
          rcMaxButton =
              CFX_FloatRect(rcClient.left, rcClient.bottom, rcClient.right,
                            rcClient.bottom + fBWidth);
        } else {
          SetVisible(false);
        }
      }
      break;
  }

  if (m_pMinButton)
    m_pMinButton->Move(rcMinButton, true, false);
  if (m_pMaxButton)
    m_pMaxButton->Move(rcMaxButton, true, false);
  MovePosButton(false);
}

void CPWL_ScrollBar::GetThisAppearanceStream(CFX_ByteTextBuf& sAppStream) {
  CFX_FloatRect rectWnd = GetWindowRect();

  if (IsVisible() && !rectWnd.IsEmpty()) {
    CFX_ByteTextBuf sButton;

    sButton << "q\n";
    sButton << "0 w\n"
            << CPWL_Utils::GetColorAppStream(GetBackgroundColor(), true)
                   .AsStringC();
    sButton << rectWnd.left << " " << rectWnd.bottom << " "
            << rectWnd.right - rectWnd.left << " "
            << rectWnd.top - rectWnd.bottom << " re b Q\n";

    sAppStream << sButton;
  }
}

void CPWL_ScrollBar::DrawThisAppearance(CFX_RenderDevice* pDevice,
                                        CFX_Matrix* pUser2Device) {
  CFX_FloatRect rectWnd = GetWindowRect();

  if (IsVisible() && !rectWnd.IsEmpty()) {
    CPWL_Utils::DrawFillRect(pDevice, pUser2Device, rectWnd,
                             GetBackgroundColor(), GetTransparency());

    CPWL_Utils::DrawStrokeLine(
        pDevice, pUser2Device,
        CFX_PointF(rectWnd.left + 2.0f, rectWnd.top - 2.0f),
        CFX_PointF(rectWnd.left + 2.0f, rectWnd.bottom + 2.0f),
        ArgbEncode(GetTransparency(), 100, 100, 100), 1.0f);

    CPWL_Utils::DrawStrokeLine(
        pDevice, pUser2Device,
        CFX_PointF(rectWnd.right - 2.0f, rectWnd.top - 2.0f),
        CFX_PointF(rectWnd.right - 2.0f, rectWnd.bottom + 2.0f),
        ArgbEncode(GetTransparency(), 100, 100, 100), 1.0f);
  }
}

bool CPWL_ScrollBar::OnLButtonDown(const CFX_PointF& point, uint32_t nFlag) {
  CPWL_Wnd::OnLButtonDown(point, nFlag);

  if (HasFlag(PWS_AUTOTRANSPARENT)) {
    if (GetTransparency() != 255) {
      SetTransparency(255);
      InvalidateRect();
    }
  }

  CFX_FloatRect rcMinArea, rcMaxArea;

  if (m_pPosButton && m_pPosButton->IsVisible()) {
    CFX_FloatRect rcClient = GetClientRect();
    CFX_FloatRect rcPosButton = m_pPosButton->GetWindowRect();

    switch (m_sbType) {
      case SBT_HSCROLL:
        rcMinArea =
            CFX_FloatRect(rcClient.left + PWL_SCROLLBAR_BUTTON_WIDTH,
                          rcClient.bottom, rcPosButton.left, rcClient.top);
        rcMaxArea = CFX_FloatRect(rcPosButton.right, rcClient.bottom,
                                  rcClient.right - PWL_SCROLLBAR_BUTTON_WIDTH,
                                  rcClient.top);

        break;
      case SBT_VSCROLL:
        rcMinArea =
            CFX_FloatRect(rcClient.left, rcPosButton.top, rcClient.right,
                          rcClient.top - PWL_SCROLLBAR_BUTTON_WIDTH);
        rcMaxArea = CFX_FloatRect(rcClient.left,
                                  rcClient.bottom + PWL_SCROLLBAR_BUTTON_WIDTH,
                                  rcClient.right, rcPosButton.bottom);
        break;
    }

    rcMinArea.Normalize();
    rcMaxArea.Normalize();

    if (rcMinArea.Contains(point)) {
      m_sData.SubBig();
      MovePosButton(true);
      NotifyScrollWindow();
    }

    if (rcMaxArea.Contains(point)) {
      m_sData.AddBig();
      MovePosButton(true);
      NotifyScrollWindow();
    }
  }

  return true;
}

bool CPWL_ScrollBar::OnLButtonUp(const CFX_PointF& point, uint32_t nFlag) {
  CPWL_Wnd::OnLButtonUp(point, nFlag);

  if (HasFlag(PWS_AUTOTRANSPARENT)) {
    if (GetTransparency() != PWL_SCROLLBAR_TRANSPARENCY) {
      SetTransparency(PWL_SCROLLBAR_TRANSPARENCY);
      InvalidateRect();
    }
  }

  EndTimer();
  m_bMouseDown = false;

  return true;
}

void CPWL_ScrollBar::OnNotify(CPWL_Wnd* pWnd,
                              uint32_t msg,
                              intptr_t wParam,
                              intptr_t lParam) {
  CPWL_Wnd::OnNotify(pWnd, msg, wParam, lParam);

  switch (msg) {
    case PNM_LBUTTONDOWN:
      if (pWnd == m_pMinButton) {
        OnMinButtonLBDown(*(CFX_PointF*)lParam);
      }

      if (pWnd == m_pMaxButton) {
        OnMaxButtonLBDown(*(CFX_PointF*)lParam);
      }

      if (pWnd == m_pPosButton) {
        OnPosButtonLBDown(*(CFX_PointF*)lParam);
      }
      break;
    case PNM_LBUTTONUP:
      if (pWnd == m_pMinButton) {
        OnMinButtonLBUp(*(CFX_PointF*)lParam);
      }

      if (pWnd == m_pMaxButton) {
        OnMaxButtonLBUp(*(CFX_PointF*)lParam);
      }

      if (pWnd == m_pPosButton) {
        OnPosButtonLBUp(*(CFX_PointF*)lParam);
      }
      break;
    case PNM_MOUSEMOVE:
      if (pWnd == m_pMinButton) {
        OnMinButtonMouseMove(*(CFX_PointF*)lParam);
      }

      if (pWnd == m_pMaxButton) {
        OnMaxButtonMouseMove(*(CFX_PointF*)lParam);
      }

      if (pWnd == m_pPosButton) {
        OnPosButtonMouseMove(*(CFX_PointF*)lParam);
      }
      break;
    case PNM_SETSCROLLINFO: {
      PWL_SCROLL_INFO* pInfo = reinterpret_cast<PWL_SCROLL_INFO*>(lParam);
      if (pInfo && *pInfo != m_OriginInfo) {
        m_OriginInfo = *pInfo;
        FX_FLOAT fMax =
            pInfo->fContentMax - pInfo->fContentMin - pInfo->fPlateWidth;
        fMax = fMax > 0.0f ? fMax : 0.0f;
        SetScrollRange(0, fMax, pInfo->fPlateWidth);
        SetScrollStep(pInfo->fBigStep, pInfo->fSmallStep);
      }
    } break;
    case PNM_SETSCROLLPOS: {
      FX_FLOAT fPos = *(FX_FLOAT*)lParam;
      switch (m_sbType) {
        case SBT_HSCROLL:
          fPos = fPos - m_OriginInfo.fContentMin;
          break;
        case SBT_VSCROLL:
          fPos = m_OriginInfo.fContentMax - fPos;
          break;
      }
      SetScrollPos(fPos);
    } break;
  }
}

void CPWL_ScrollBar::CreateButtons(const PWL_CREATEPARAM& cp) {
  PWL_CREATEPARAM scp = cp;
  scp.pParentWnd = this;
  scp.dwBorderWidth = 2;
  scp.nBorderStyle = BorderStyle::BEVELED;

  scp.dwFlags =
      PWS_VISIBLE | PWS_CHILD | PWS_BORDER | PWS_BACKGROUND | PWS_NOREFRESHCLIP;

  if (!m_pMinButton) {
    m_pMinButton = new CPWL_SBButton(m_sbType, PSBT_MIN);
    m_pMinButton->Create(scp);
  }

  if (!m_pMaxButton) {
    m_pMaxButton = new CPWL_SBButton(m_sbType, PSBT_MAX);
    m_pMaxButton->Create(scp);
  }

  if (!m_pPosButton) {
    m_pPosButton = new CPWL_SBButton(m_sbType, PSBT_POS);
    m_pPosButton->SetVisible(false);
    m_pPosButton->Create(scp);
  }
}

FX_FLOAT CPWL_ScrollBar::GetScrollBarWidth() const {
  if (!IsVisible())
    return 0;

  return PWL_SCROLLBAR_WIDTH;
}

void CPWL_ScrollBar::SetScrollRange(FX_FLOAT fMin,
                                    FX_FLOAT fMax,
                                    FX_FLOAT fClientWidth) {
  if (m_pPosButton) {
    m_sData.SetScrollRange(fMin, fMax);
    m_sData.SetClientWidth(fClientWidth);

    if (IsFloatSmaller(m_sData.ScrollRange.GetWidth(), 0.0f)) {
      m_pPosButton->SetVisible(false);
    } else {
      m_pPosButton->SetVisible(true);
      MovePosButton(true);
    }
  }
}

void CPWL_ScrollBar::SetScrollPos(FX_FLOAT fPos) {
  FX_FLOAT fOldPos = m_sData.fScrollPos;

  m_sData.SetPos(fPos);

  if (!IsFloatEqual(m_sData.fScrollPos, fOldPos))
    MovePosButton(true);
}

void CPWL_ScrollBar::SetScrollStep(FX_FLOAT fBigStep, FX_FLOAT fSmallStep) {
  m_sData.SetBigStep(fBigStep);
  m_sData.SetSmallStep(fSmallStep);
}

void CPWL_ScrollBar::MovePosButton(bool bRefresh) {
  ASSERT(m_pMinButton);
  ASSERT(m_pMaxButton);

  if (m_pPosButton->IsVisible()) {
    CFX_FloatRect rcClient;
    CFX_FloatRect rcPosArea, rcPosButton;

    rcClient = GetClientRect();
    rcPosArea = GetScrollArea();

    FX_FLOAT fLeft, fRight, fTop, fBottom;

    switch (m_sbType) {
      case SBT_HSCROLL:
        fLeft = TrueToFace(m_sData.fScrollPos);
        fRight = TrueToFace(m_sData.fScrollPos + m_sData.fClientWidth);

        if (fRight - fLeft < PWL_SCROLLBAR_POSBUTTON_MINWIDTH)
          fRight = fLeft + PWL_SCROLLBAR_POSBUTTON_MINWIDTH;

        if (fRight > rcPosArea.right) {
          fRight = rcPosArea.right;
          fLeft = fRight - PWL_SCROLLBAR_POSBUTTON_MINWIDTH;
        }

        rcPosButton =
            CFX_FloatRect(fLeft, rcPosArea.bottom, fRight, rcPosArea.top);

        break;
      case SBT_VSCROLL:
        fBottom = TrueToFace(m_sData.fScrollPos + m_sData.fClientWidth);
        fTop = TrueToFace(m_sData.fScrollPos);

        if (IsFloatSmaller(fTop - fBottom, PWL_SCROLLBAR_POSBUTTON_MINWIDTH))
          fBottom = fTop - PWL_SCROLLBAR_POSBUTTON_MINWIDTH;

        if (IsFloatSmaller(fBottom, rcPosArea.bottom)) {
          fBottom = rcPosArea.bottom;
          fTop = fBottom + PWL_SCROLLBAR_POSBUTTON_MINWIDTH;
        }

        rcPosButton =
            CFX_FloatRect(rcPosArea.left, fBottom, rcPosArea.right, fTop);

        break;
    }

    m_pPosButton->Move(rcPosButton, true, bRefresh);
  }
}

void CPWL_ScrollBar::OnMinButtonLBDown(const CFX_PointF& point) {
  m_sData.SubSmall();
  MovePosButton(true);
  NotifyScrollWindow();

  m_bMinOrMax = true;

  EndTimer();
  BeginTimer(100);
}

void CPWL_ScrollBar::OnMinButtonLBUp(const CFX_PointF& point) {}

void CPWL_ScrollBar::OnMinButtonMouseMove(const CFX_PointF& point) {}

void CPWL_ScrollBar::OnMaxButtonLBDown(const CFX_PointF& point) {
  m_sData.AddSmall();
  MovePosButton(true);
  NotifyScrollWindow();

  m_bMinOrMax = false;

  EndTimer();
  BeginTimer(100);
}

void CPWL_ScrollBar::OnMaxButtonLBUp(const CFX_PointF& point) {}

void CPWL_ScrollBar::OnMaxButtonMouseMove(const CFX_PointF& point) {}

void CPWL_ScrollBar::OnPosButtonLBDown(const CFX_PointF& point) {
  m_bMouseDown = true;

  if (m_pPosButton) {
    CFX_FloatRect rcPosButton = m_pPosButton->GetWindowRect();

    switch (m_sbType) {
      case SBT_HSCROLL:
        m_nOldPos = point.x;
        m_fOldPosButton = rcPosButton.left;
        break;
      case SBT_VSCROLL:
        m_nOldPos = point.y;
        m_fOldPosButton = rcPosButton.top;
        break;
    }
  }
}

void CPWL_ScrollBar::OnPosButtonLBUp(const CFX_PointF& point) {
  if (m_bMouseDown) {
    if (!m_bNotifyForever)
      NotifyScrollWindow();
  }
  m_bMouseDown = false;
}

void CPWL_ScrollBar::OnPosButtonMouseMove(const CFX_PointF& point) {
  FX_FLOAT fOldScrollPos = m_sData.fScrollPos;

  FX_FLOAT fNewPos = 0;

  switch (m_sbType) {
    case SBT_HSCROLL:
      if (FXSYS_fabs(point.x - m_nOldPos) < 1)
        return;
      fNewPos = FaceToTrue(m_fOldPosButton + point.x - m_nOldPos);
      break;
    case SBT_VSCROLL:
      if (FXSYS_fabs(point.y - m_nOldPos) < 1)
        return;
      fNewPos = FaceToTrue(m_fOldPosButton + point.y - m_nOldPos);
      break;
  }

  if (m_bMouseDown) {
    switch (m_sbType) {
      case SBT_HSCROLL:

        if (IsFloatSmaller(fNewPos, m_sData.ScrollRange.fMin)) {
          fNewPos = m_sData.ScrollRange.fMin;
        }

        if (IsFloatBigger(fNewPos, m_sData.ScrollRange.fMax)) {
          fNewPos = m_sData.ScrollRange.fMax;
        }

        m_sData.SetPos(fNewPos);

        break;
      case SBT_VSCROLL:

        if (IsFloatSmaller(fNewPos, m_sData.ScrollRange.fMin)) {
          fNewPos = m_sData.ScrollRange.fMin;
        }

        if (IsFloatBigger(fNewPos, m_sData.ScrollRange.fMax)) {
          fNewPos = m_sData.ScrollRange.fMax;
        }

        m_sData.SetPos(fNewPos);

        break;
    }

    if (!IsFloatEqual(fOldScrollPos, m_sData.fScrollPos)) {
      MovePosButton(true);

      if (m_bNotifyForever)
        NotifyScrollWindow();
    }
  }
}

void CPWL_ScrollBar::NotifyScrollWindow() {
  if (CPWL_Wnd* pParent = GetParentWindow()) {
    FX_FLOAT fPos;
    switch (m_sbType) {
      case SBT_HSCROLL:
        fPos = m_OriginInfo.fContentMin + m_sData.fScrollPos;
        break;
      case SBT_VSCROLL:
        fPos = m_OriginInfo.fContentMax - m_sData.fScrollPos;
        break;
    }
    pParent->OnNotify(this, PNM_SCROLLWINDOW, (intptr_t)m_sbType,
                      (intptr_t)&fPos);
  }
}

CFX_FloatRect CPWL_ScrollBar::GetScrollArea() const {
  CFX_FloatRect rcClient = GetClientRect();
  CFX_FloatRect rcArea;

  if (!m_pMinButton || !m_pMaxButton)
    return rcClient;

  CFX_FloatRect rcMin = m_pMinButton->GetWindowRect();
  CFX_FloatRect rcMax = m_pMaxButton->GetWindowRect();

  FX_FLOAT fMinWidth = rcMin.right - rcMin.left;
  FX_FLOAT fMinHeight = rcMin.top - rcMin.bottom;
  FX_FLOAT fMaxWidth = rcMax.right - rcMax.left;
  FX_FLOAT fMaxHeight = rcMax.top - rcMax.bottom;

  switch (m_sbType) {
    case SBT_HSCROLL:
      if (rcClient.right - rcClient.left > fMinWidth + fMaxWidth + 2) {
        rcArea = CFX_FloatRect(rcClient.left + fMinWidth + 1, rcClient.bottom,
                               rcClient.right - fMaxWidth - 1, rcClient.top);
      } else {
        rcArea = CFX_FloatRect(rcClient.left + fMinWidth + 1, rcClient.bottom,
                               rcClient.left + fMinWidth + 1, rcClient.top);
      }
      break;
    case SBT_VSCROLL:
      if (rcClient.top - rcClient.bottom > fMinHeight + fMaxHeight + 2) {
        rcArea = CFX_FloatRect(rcClient.left, rcClient.bottom + fMinHeight + 1,
                               rcClient.right, rcClient.top - fMaxHeight - 1);
      } else {
        rcArea =
            CFX_FloatRect(rcClient.left, rcClient.bottom + fMinHeight + 1,
                          rcClient.right, rcClient.bottom + fMinHeight + 1);
      }
      break;
  }

  rcArea.Normalize();

  return rcArea;
}

FX_FLOAT CPWL_ScrollBar::TrueToFace(FX_FLOAT fTrue) {
  CFX_FloatRect rcPosArea;
  rcPosArea = GetScrollArea();

  FX_FLOAT fFactWidth = m_sData.ScrollRange.GetWidth() + m_sData.fClientWidth;
  fFactWidth = fFactWidth == 0 ? 1 : fFactWidth;

  FX_FLOAT fFace = 0;

  switch (m_sbType) {
    case SBT_HSCROLL:
      fFace = rcPosArea.left +
              fTrue * (rcPosArea.right - rcPosArea.left) / fFactWidth;
      break;
    case SBT_VSCROLL:
      fFace = rcPosArea.top -
              fTrue * (rcPosArea.top - rcPosArea.bottom) / fFactWidth;
      break;
  }

  return fFace;
}

FX_FLOAT CPWL_ScrollBar::FaceToTrue(FX_FLOAT fFace) {
  CFX_FloatRect rcPosArea;
  rcPosArea = GetScrollArea();

  FX_FLOAT fFactWidth = m_sData.ScrollRange.GetWidth() + m_sData.fClientWidth;
  fFactWidth = fFactWidth == 0 ? 1 : fFactWidth;

  FX_FLOAT fTrue = 0;

  switch (m_sbType) {
    case SBT_HSCROLL:
      fTrue = (fFace - rcPosArea.left) * fFactWidth /
              (rcPosArea.right - rcPosArea.left);
      break;
    case SBT_VSCROLL:
      fTrue = (rcPosArea.top - fFace) * fFactWidth /
              (rcPosArea.top - rcPosArea.bottom);
      break;
  }

  return fTrue;
}

void CPWL_ScrollBar::CreateChildWnd(const PWL_CREATEPARAM& cp) {
  CreateButtons(cp);
}

void CPWL_ScrollBar::TimerProc() {
  PWL_SCROLL_PRIVATEDATA sTemp = m_sData;
  if (m_bMinOrMax)
    m_sData.SubSmall();
  else
    m_sData.AddSmall();

  if (sTemp != m_sData) {
    MovePosButton(true);
    NotifyScrollWindow();
  }
}
