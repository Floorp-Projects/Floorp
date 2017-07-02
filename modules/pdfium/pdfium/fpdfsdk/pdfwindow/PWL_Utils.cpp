// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/pdfwindow/PWL_Utils.h"

#include <algorithm>
#include <memory>

#include "core/fpdfdoc/cpvt_word.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"
#include "fpdfsdk/fxedit/fxet_edit.h"
#include "fpdfsdk/pdfwindow/PWL_Icon.h"
#include "fpdfsdk/pdfwindow/PWL_Wnd.h"

CFX_FloatRect CPWL_Utils::OffsetRect(const CFX_FloatRect& rect,
                                     FX_FLOAT x,
                                     FX_FLOAT y) {
  return CFX_FloatRect(rect.left + x, rect.bottom + y, rect.right + x,
                       rect.top + y);
}

CPVT_WordRange CPWL_Utils::OverlapWordRange(const CPVT_WordRange& wr1,
                                            const CPVT_WordRange& wr2) {
  CPVT_WordRange wrRet;

  if (wr2.EndPos.WordCmp(wr1.BeginPos) < 0 ||
      wr2.BeginPos.WordCmp(wr1.EndPos) > 0)
    return wrRet;
  if (wr1.EndPos.WordCmp(wr2.BeginPos) < 0 ||
      wr1.BeginPos.WordCmp(wr2.EndPos) > 0)
    return wrRet;

  if (wr1.BeginPos.WordCmp(wr2.BeginPos) < 0) {
    wrRet.BeginPos = wr2.BeginPos;
  } else {
    wrRet.BeginPos = wr1.BeginPos;
  }

  if (wr1.EndPos.WordCmp(wr2.EndPos) < 0) {
    wrRet.EndPos = wr1.EndPos;
  } else {
    wrRet.EndPos = wr2.EndPos;
  }

  return wrRet;
}

CFX_ByteString CPWL_Utils::GetAP_Check(const CFX_FloatRect& crBBox) {
  const FX_FLOAT fWidth = crBBox.right - crBBox.left;
  const FX_FLOAT fHeight = crBBox.top - crBBox.bottom;

  CFX_PointF pts[8][3] = {{CFX_PointF(0.28f, 0.52f), CFX_PointF(0.27f, 0.48f),
                           CFX_PointF(0.29f, 0.40f)},
                          {CFX_PointF(0.30f, 0.33f), CFX_PointF(0.31f, 0.29f),
                           CFX_PointF(0.31f, 0.28f)},
                          {CFX_PointF(0.39f, 0.28f), CFX_PointF(0.49f, 0.29f),
                           CFX_PointF(0.77f, 0.67f)},
                          {CFX_PointF(0.76f, 0.68f), CFX_PointF(0.78f, 0.69f),
                           CFX_PointF(0.76f, 0.75f)},
                          {CFX_PointF(0.76f, 0.75f), CFX_PointF(0.73f, 0.80f),
                           CFX_PointF(0.68f, 0.75f)},
                          {CFX_PointF(0.68f, 0.74f), CFX_PointF(0.68f, 0.74f),
                           CFX_PointF(0.44f, 0.47f)},
                          {CFX_PointF(0.43f, 0.47f), CFX_PointF(0.40f, 0.47f),
                           CFX_PointF(0.41f, 0.58f)},
                          {CFX_PointF(0.40f, 0.60f), CFX_PointF(0.28f, 0.66f),
                           CFX_PointF(0.30f, 0.56f)}};

  for (size_t i = 0; i < FX_ArraySize(pts); ++i) {
    for (size_t j = 0; j < FX_ArraySize(pts[0]); ++j) {
      pts[i][j].x = pts[i][j].x * fWidth + crBBox.left;
      pts[i][j].y *= pts[i][j].y * fHeight + crBBox.bottom;
    }
  }

  CFX_ByteTextBuf csAP;
  csAP << pts[0][0].x << " " << pts[0][0].y << " m\n";

  for (size_t i = 0; i < FX_ArraySize(pts); ++i) {
    size_t nNext = i < FX_ArraySize(pts) - 1 ? i + 1 : 0;

    FX_FLOAT px1 = pts[i][1].x - pts[i][0].x;
    FX_FLOAT py1 = pts[i][1].y - pts[i][0].y;
    FX_FLOAT px2 = pts[i][2].x - pts[nNext][0].x;
    FX_FLOAT py2 = pts[i][2].y - pts[nNext][0].y;

    csAP << pts[i][0].x + px1 * FX_BEZIER << " "
         << pts[i][0].y + py1 * FX_BEZIER << " "
         << pts[nNext][0].x + px2 * FX_BEZIER << " "
         << pts[nNext][0].y + py2 * FX_BEZIER << " " << pts[nNext][0].x << " "
         << pts[nNext][0].y << " c\n";
  }

  return csAP.MakeString();
}

CFX_ByteString CPWL_Utils::GetAP_Circle(const CFX_FloatRect& crBBox) {
  CFX_ByteTextBuf csAP;

  FX_FLOAT fWidth = crBBox.right - crBBox.left;
  FX_FLOAT fHeight = crBBox.top - crBBox.bottom;

  CFX_PointF pt1(crBBox.left, crBBox.bottom + fHeight / 2);
  CFX_PointF pt2(crBBox.left + fWidth / 2, crBBox.top);
  CFX_PointF pt3(crBBox.right, crBBox.bottom + fHeight / 2);
  CFX_PointF pt4(crBBox.left + fWidth / 2, crBBox.bottom);

  csAP << pt1.x << " " << pt1.y << " m\n";

  FX_FLOAT px = pt2.x - pt1.x;
  FX_FLOAT py = pt2.y - pt1.y;

  csAP << pt1.x << " " << pt1.y + py * FX_BEZIER << " "
       << pt2.x - px * FX_BEZIER << " " << pt2.y << " " << pt2.x << " " << pt2.y
       << " c\n";

  px = pt3.x - pt2.x;
  py = pt2.y - pt3.y;

  csAP << pt2.x + px * FX_BEZIER << " " << pt2.y << " " << pt3.x << " "
       << pt3.y + py * FX_BEZIER << " " << pt3.x << " " << pt3.y << " c\n";

  px = pt3.x - pt4.x;
  py = pt3.y - pt4.y;

  csAP << pt3.x << " " << pt3.y - py * FX_BEZIER << " "
       << pt4.x + px * FX_BEZIER << " " << pt4.y << " " << pt4.x << " " << pt4.y
       << " c\n";

  px = pt4.x - pt1.x;
  py = pt1.y - pt4.y;

  csAP << pt4.x - px * FX_BEZIER << " " << pt4.y << " " << pt1.x << " "
       << pt1.y - py * FX_BEZIER << " " << pt1.x << " " << pt1.y << " c\n";

  return csAP.MakeString();
}

CFX_ByteString CPWL_Utils::GetAP_Cross(const CFX_FloatRect& crBBox) {
  CFX_ByteTextBuf csAP;

  csAP << crBBox.left << " " << crBBox.top << " m\n";
  csAP << crBBox.right << " " << crBBox.bottom << " l\n";
  csAP << crBBox.left << " " << crBBox.bottom << " m\n";
  csAP << crBBox.right << " " << crBBox.top << " l\n";

  return csAP.MakeString();
}

CFX_ByteString CPWL_Utils::GetAP_Diamond(const CFX_FloatRect& crBBox) {
  CFX_ByteTextBuf csAP;

  FX_FLOAT fWidth = crBBox.right - crBBox.left;
  FX_FLOAT fHeight = crBBox.top - crBBox.bottom;

  CFX_PointF pt1(crBBox.left, crBBox.bottom + fHeight / 2);
  CFX_PointF pt2(crBBox.left + fWidth / 2, crBBox.top);
  CFX_PointF pt3(crBBox.right, crBBox.bottom + fHeight / 2);
  CFX_PointF pt4(crBBox.left + fWidth / 2, crBBox.bottom);

  csAP << pt1.x << " " << pt1.y << " m\n";
  csAP << pt2.x << " " << pt2.y << " l\n";
  csAP << pt3.x << " " << pt3.y << " l\n";
  csAP << pt4.x << " " << pt4.y << " l\n";
  csAP << pt1.x << " " << pt1.y << " l\n";

  return csAP.MakeString();
}

CFX_ByteString CPWL_Utils::GetAP_Square(const CFX_FloatRect& crBBox) {
  CFX_ByteTextBuf csAP;

  csAP << crBBox.left << " " << crBBox.top << " m\n";
  csAP << crBBox.right << " " << crBBox.top << " l\n";
  csAP << crBBox.right << " " << crBBox.bottom << " l\n";
  csAP << crBBox.left << " " << crBBox.bottom << " l\n";
  csAP << crBBox.left << " " << crBBox.top << " l\n";

  return csAP.MakeString();
}

CFX_ByteString CPWL_Utils::GetAP_Star(const CFX_FloatRect& crBBox) {
  CFX_ByteTextBuf csAP;

  FX_FLOAT fRadius =
      (crBBox.top - crBBox.bottom) / (1 + (FX_FLOAT)cos(FX_PI / 5.0f));
  CFX_PointF ptCenter = CFX_PointF((crBBox.left + crBBox.right) / 2.0f,
                                   (crBBox.top + crBBox.bottom) / 2.0f);

  FX_FLOAT px[5], py[5];

  FX_FLOAT fAngel = FX_PI / 10.0f;

  for (int32_t i = 0; i < 5; i++) {
    px[i] = ptCenter.x + fRadius * (FX_FLOAT)cos(fAngel);
    py[i] = ptCenter.y + fRadius * (FX_FLOAT)sin(fAngel);

    fAngel += FX_PI * 2 / 5.0f;
  }

  csAP << px[0] << " " << py[0] << " m\n";

  int32_t nNext = 0;
  for (int32_t j = 0; j < 5; j++) {
    nNext += 2;
    if (nNext >= 5)
      nNext -= 5;
    csAP << px[nNext] << " " << py[nNext] << " l\n";
  }

  return csAP.MakeString();
}

CFX_ByteString CPWL_Utils::GetAP_HalfCircle(const CFX_FloatRect& crBBox,
                                            FX_FLOAT fRotate) {
  CFX_ByteTextBuf csAP;

  FX_FLOAT fWidth = crBBox.right - crBBox.left;
  FX_FLOAT fHeight = crBBox.top - crBBox.bottom;

  CFX_PointF pt1(-fWidth / 2, 0);
  CFX_PointF pt2(0, fHeight / 2);
  CFX_PointF pt3(fWidth / 2, 0);

  FX_FLOAT px, py;

  csAP << cos(fRotate) << " " << sin(fRotate) << " " << -sin(fRotate) << " "
       << cos(fRotate) << " " << crBBox.left + fWidth / 2 << " "
       << crBBox.bottom + fHeight / 2 << " cm\n";

  csAP << pt1.x << " " << pt1.y << " m\n";

  px = pt2.x - pt1.x;
  py = pt2.y - pt1.y;

  csAP << pt1.x << " " << pt1.y + py * FX_BEZIER << " "
       << pt2.x - px * FX_BEZIER << " " << pt2.y << " " << pt2.x << " " << pt2.y
       << " c\n";

  px = pt3.x - pt2.x;
  py = pt2.y - pt3.y;

  csAP << pt2.x + px * FX_BEZIER << " " << pt2.y << " " << pt3.x << " "
       << pt3.y + py * FX_BEZIER << " " << pt3.x << " " << pt3.y << " c\n";

  return csAP.MakeString();
}

CFX_FloatRect CPWL_Utils::InflateRect(const CFX_FloatRect& rcRect,
                                      FX_FLOAT fSize) {
  if (rcRect.IsEmpty())
    return rcRect;

  CFX_FloatRect rcNew(rcRect.left - fSize, rcRect.bottom - fSize,
                      rcRect.right + fSize, rcRect.top + fSize);
  rcNew.Normalize();
  return rcNew;
}

CFX_FloatRect CPWL_Utils::DeflateRect(const CFX_FloatRect& rcRect,
                                      FX_FLOAT fSize) {
  if (rcRect.IsEmpty())
    return rcRect;

  CFX_FloatRect rcNew(rcRect.left + fSize, rcRect.bottom + fSize,
                      rcRect.right - fSize, rcRect.top - fSize);
  rcNew.Normalize();
  return rcNew;
}

CFX_FloatRect CPWL_Utils::ScaleRect(const CFX_FloatRect& rcRect,
                                    FX_FLOAT fScale) {
  FX_FLOAT fHalfWidth = (rcRect.right - rcRect.left) / 2.0f;
  FX_FLOAT fHalfHeight = (rcRect.top - rcRect.bottom) / 2.0f;

  CFX_PointF ptCenter = CFX_PointF((rcRect.left + rcRect.right) / 2,
                                   (rcRect.top + rcRect.bottom) / 2);

  return CFX_FloatRect(
      ptCenter.x - fHalfWidth * fScale, ptCenter.y - fHalfHeight * fScale,
      ptCenter.x + fHalfWidth * fScale, ptCenter.y + fHalfHeight * fScale);
}

CFX_ByteString CPWL_Utils::GetRectFillAppStream(const CFX_FloatRect& rect,
                                                const CPWL_Color& color) {
  CFX_ByteTextBuf sAppStream;
  CFX_ByteString sColor = GetColorAppStream(color, true);
  if (sColor.GetLength() > 0) {
    sAppStream << "q\n" << sColor;
    sAppStream << rect.left << " " << rect.bottom << " "
               << rect.right - rect.left << " " << rect.top - rect.bottom
               << " re f\nQ\n";
  }

  return sAppStream.MakeString();
}

CFX_ByteString CPWL_Utils::GetCircleFillAppStream(const CFX_FloatRect& rect,
                                                  const CPWL_Color& color) {
  CFX_ByteTextBuf sAppStream;
  CFX_ByteString sColor = GetColorAppStream(color, true);
  if (sColor.GetLength() > 0) {
    sAppStream << "q\n" << sColor << CPWL_Utils::GetAP_Circle(rect) << "f\nQ\n";
  }
  return sAppStream.MakeString();
}

CFX_FloatRect CPWL_Utils::GetCenterSquare(const CFX_FloatRect& rect) {
  FX_FLOAT fWidth = rect.right - rect.left;
  FX_FLOAT fHeight = rect.top - rect.bottom;

  FX_FLOAT fCenterX = (rect.left + rect.right) / 2.0f;
  FX_FLOAT fCenterY = (rect.top + rect.bottom) / 2.0f;

  FX_FLOAT fRadius = (fWidth > fHeight) ? fHeight / 2 : fWidth / 2;

  return CFX_FloatRect(fCenterX - fRadius, fCenterY - fRadius,
                       fCenterX + fRadius, fCenterY + fRadius);
}

CFX_ByteString CPWL_Utils::GetEditAppStream(CFX_Edit* pEdit,
                                            const CFX_PointF& ptOffset,
                                            const CPVT_WordRange* pRange,
                                            bool bContinuous,
                                            uint16_t SubWord) {
  return CFX_Edit::GetEditAppearanceStream(pEdit, ptOffset, pRange, bContinuous,
                                           SubWord);
}

CFX_ByteString CPWL_Utils::GetEditSelAppStream(CFX_Edit* pEdit,
                                               const CFX_PointF& ptOffset,
                                               const CPVT_WordRange* pRange) {
  return CFX_Edit::GetSelectAppearanceStream(pEdit, ptOffset, pRange);
}

CFX_ByteString CPWL_Utils::GetPushButtonAppStream(const CFX_FloatRect& rcBBox,
                                                  IPVT_FontMap* pFontMap,
                                                  CPDF_Stream* pIconStream,
                                                  CPDF_IconFit& IconFit,
                                                  const CFX_WideString& sLabel,
                                                  const CPWL_Color& crText,
                                                  FX_FLOAT fFontSize,
                                                  int32_t nLayOut) {
  const FX_FLOAT fAutoFontScale = 1.0f / 3.0f;

  std::unique_ptr<CFX_Edit> pEdit(new CFX_Edit);
  pEdit->SetFontMap(pFontMap);
  pEdit->SetAlignmentH(1, true);
  pEdit->SetAlignmentV(1, true);
  pEdit->SetMultiLine(false, true);
  pEdit->SetAutoReturn(false, true);
  if (IsFloatZero(fFontSize))
    pEdit->SetAutoFontSize(true, true);
  else
    pEdit->SetFontSize(fFontSize);

  pEdit->Initialize();
  pEdit->SetText(sLabel);

  CFX_FloatRect rcLabelContent = pEdit->GetContentRect();
  CPWL_Icon Icon;
  PWL_CREATEPARAM cp;
  cp.dwFlags = PWS_VISIBLE;
  Icon.Create(cp);
  Icon.SetIconFit(&IconFit);
  Icon.SetPDFStream(pIconStream);

  CFX_FloatRect rcLabel = CFX_FloatRect(0, 0, 0, 0);
  CFX_FloatRect rcIcon = CFX_FloatRect(0, 0, 0, 0);
  FX_FLOAT fWidth = 0.0f;
  FX_FLOAT fHeight = 0.0f;

  switch (nLayOut) {
    case PPBL_LABEL:
      rcLabel = rcBBox;
      rcIcon = CFX_FloatRect(0, 0, 0, 0);
      break;
    case PPBL_ICON:
      rcIcon = rcBBox;
      rcLabel = CFX_FloatRect(0, 0, 0, 0);
      break;
    case PPBL_ICONTOPLABELBOTTOM:

      if (pIconStream) {
        if (IsFloatZero(fFontSize)) {
          fHeight = rcBBox.top - rcBBox.bottom;
          rcLabel = CFX_FloatRect(rcBBox.left, rcBBox.bottom, rcBBox.right,
                                  rcBBox.bottom + fHeight * fAutoFontScale);
          rcIcon =
              CFX_FloatRect(rcBBox.left, rcLabel.top, rcBBox.right, rcBBox.top);
        } else {
          fHeight = rcLabelContent.Height();

          if (rcBBox.bottom + fHeight > rcBBox.top) {
            rcIcon = CFX_FloatRect(0, 0, 0, 0);
            rcLabel = rcBBox;
          } else {
            rcLabel = CFX_FloatRect(rcBBox.left, rcBBox.bottom, rcBBox.right,
                                    rcBBox.bottom + fHeight);
            rcIcon = CFX_FloatRect(rcBBox.left, rcLabel.top, rcBBox.right,
                                   rcBBox.top);
          }
        }
      } else {
        rcLabel = rcBBox;
        rcIcon = CFX_FloatRect(0, 0, 0, 0);
      }

      break;
    case PPBL_LABELTOPICONBOTTOM:

      if (pIconStream) {
        if (IsFloatZero(fFontSize)) {
          fHeight = rcBBox.top - rcBBox.bottom;
          rcLabel =
              CFX_FloatRect(rcBBox.left, rcBBox.top - fHeight * fAutoFontScale,
                            rcBBox.right, rcBBox.top);
          rcIcon = CFX_FloatRect(rcBBox.left, rcBBox.bottom, rcBBox.right,
                                 rcLabel.bottom);
        } else {
          fHeight = rcLabelContent.Height();

          if (rcBBox.bottom + fHeight > rcBBox.top) {
            rcIcon = CFX_FloatRect(0, 0, 0, 0);
            rcLabel = rcBBox;
          } else {
            rcLabel = CFX_FloatRect(rcBBox.left, rcBBox.top - fHeight,
                                    rcBBox.right, rcBBox.top);
            rcIcon = CFX_FloatRect(rcBBox.left, rcBBox.bottom, rcBBox.right,
                                   rcLabel.bottom);
          }
        }
      } else {
        rcLabel = rcBBox;
        rcIcon = CFX_FloatRect(0, 0, 0, 0);
      }

      break;
    case PPBL_ICONLEFTLABELRIGHT:

      if (pIconStream) {
        if (IsFloatZero(fFontSize)) {
          fWidth = rcBBox.right - rcBBox.left;
          rcLabel = CFX_FloatRect(rcBBox.right - fWidth * fAutoFontScale,
                                  rcBBox.bottom, rcBBox.right, rcBBox.top);
          rcIcon = CFX_FloatRect(rcBBox.left, rcBBox.bottom, rcLabel.left,
                                 rcBBox.top);

          if (rcLabelContent.Width() < fWidth * fAutoFontScale) {
          } else {
            if (rcLabelContent.Width() < fWidth) {
              rcLabel = CFX_FloatRect(rcBBox.right - rcLabelContent.Width(),
                                      rcBBox.bottom, rcBBox.right, rcBBox.top);
              rcIcon = CFX_FloatRect(rcBBox.left, rcBBox.bottom, rcLabel.left,
                                     rcBBox.top);
            } else {
              rcLabel = rcBBox;
              rcIcon = CFX_FloatRect(0, 0, 0, 0);
            }
          }
        } else {
          fWidth = rcLabelContent.Width();

          if (rcBBox.left + fWidth > rcBBox.right) {
            rcLabel = rcBBox;
            rcIcon = CFX_FloatRect(0, 0, 0, 0);
          } else {
            rcLabel = CFX_FloatRect(rcBBox.right - fWidth, rcBBox.bottom,
                                    rcBBox.right, rcBBox.top);
            rcIcon = CFX_FloatRect(rcBBox.left, rcBBox.bottom, rcLabel.left,
                                   rcBBox.top);
          }
        }
      } else {
        rcLabel = rcBBox;
        rcIcon = CFX_FloatRect(0, 0, 0, 0);
      }

      break;
    case PPBL_LABELLEFTICONRIGHT:

      if (pIconStream) {
        if (IsFloatZero(fFontSize)) {
          fWidth = rcBBox.right - rcBBox.left;
          rcLabel =
              CFX_FloatRect(rcBBox.left, rcBBox.bottom,
                            rcBBox.left + fWidth * fAutoFontScale, rcBBox.top);
          rcIcon = CFX_FloatRect(rcLabel.right, rcBBox.bottom, rcBBox.right,
                                 rcBBox.top);

          if (rcLabelContent.Width() < fWidth * fAutoFontScale) {
          } else {
            if (rcLabelContent.Width() < fWidth) {
              rcLabel = CFX_FloatRect(rcBBox.left, rcBBox.bottom,
                                      rcBBox.left + rcLabelContent.Width(),
                                      rcBBox.top);
              rcIcon = CFX_FloatRect(rcLabel.right, rcBBox.bottom, rcBBox.right,
                                     rcBBox.top);
            } else {
              rcLabel = rcBBox;
              rcIcon = CFX_FloatRect(0, 0, 0, 0);
            }
          }
        } else {
          fWidth = rcLabelContent.Width();

          if (rcBBox.left + fWidth > rcBBox.right) {
            rcLabel = rcBBox;
            rcIcon = CFX_FloatRect(0, 0, 0, 0);
          } else {
            rcLabel = CFX_FloatRect(rcBBox.left, rcBBox.bottom,
                                    rcBBox.left + fWidth, rcBBox.top);
            rcIcon = CFX_FloatRect(rcLabel.right, rcBBox.bottom, rcBBox.right,
                                   rcBBox.top);
          }
        }
      } else {
        rcLabel = rcBBox;
        rcIcon = CFX_FloatRect(0, 0, 0, 0);
      }

      break;
    case PPBL_LABELOVERICON:
      rcLabel = rcBBox;
      rcIcon = rcBBox;
      break;
  }

  CFX_ByteTextBuf sAppStream, sTemp;

  if (!rcIcon.IsEmpty()) {
    Icon.Move(rcIcon, false, false);
    sTemp << Icon.GetImageAppStream();
  }

  Icon.Destroy();

  if (!rcLabel.IsEmpty()) {
    pEdit->SetPlateRect(rcLabel);
    CFX_ByteString sEdit =
        CPWL_Utils::GetEditAppStream(pEdit.get(), CFX_PointF(0.0f, 0.0f));
    if (sEdit.GetLength() > 0) {
      sTemp << "BT\n"
            << CPWL_Utils::GetColorAppStream(crText) << sEdit << "ET\n";
    }
  }

  if (sTemp.GetSize() > 0) {
    sAppStream << "q\n"
               << rcBBox.left << " " << rcBBox.bottom << " "
               << rcBBox.right - rcBBox.left << " "
               << rcBBox.top - rcBBox.bottom << " re W n\n";
    sAppStream << sTemp << "Q\n";
  }
  return sAppStream.MakeString();
}

CFX_ByteString CPWL_Utils::GetColorAppStream(const CPWL_Color& color,
                                             const bool& bFillOrStroke) {
  CFX_ByteTextBuf sColorStream;

  switch (color.nColorType) {
    case COLORTYPE_RGB:
      sColorStream << color.fColor1 << " " << color.fColor2 << " "
                   << color.fColor3 << " " << (bFillOrStroke ? "rg" : "RG")
                   << "\n";
      break;
    case COLORTYPE_GRAY:
      sColorStream << color.fColor1 << " " << (bFillOrStroke ? "g" : "G")
                   << "\n";
      break;
    case COLORTYPE_CMYK:
      sColorStream << color.fColor1 << " " << color.fColor2 << " "
                   << color.fColor3 << " " << color.fColor4 << " "
                   << (bFillOrStroke ? "k" : "K") << "\n";
      break;
  }

  return sColorStream.MakeString();
}

CFX_ByteString CPWL_Utils::GetBorderAppStream(const CFX_FloatRect& rect,
                                              FX_FLOAT fWidth,
                                              const CPWL_Color& color,
                                              const CPWL_Color& crLeftTop,
                                              const CPWL_Color& crRightBottom,
                                              BorderStyle nStyle,
                                              const CPWL_Dash& dash) {
  CFX_ByteTextBuf sAppStream;
  CFX_ByteString sColor;

  FX_FLOAT fLeft = rect.left;
  FX_FLOAT fRight = rect.right;
  FX_FLOAT fTop = rect.top;
  FX_FLOAT fBottom = rect.bottom;

  if (fWidth > 0.0f) {
    FX_FLOAT fHalfWidth = fWidth / 2.0f;

    sAppStream << "q\n";

    switch (nStyle) {
      default:
      case BorderStyle::SOLID:
        sColor = CPWL_Utils::GetColorAppStream(color, true);
        if (sColor.GetLength() > 0) {
          sAppStream << sColor;
          sAppStream << fLeft << " " << fBottom << " " << fRight - fLeft << " "
                     << fTop - fBottom << " re\n";
          sAppStream << fLeft + fWidth << " " << fBottom + fWidth << " "
                     << fRight - fLeft - fWidth * 2 << " "
                     << fTop - fBottom - fWidth * 2 << " re\n";
          sAppStream << "f*\n";
        }
        break;
      case BorderStyle::DASH:
        sColor = CPWL_Utils::GetColorAppStream(color, false);
        if (sColor.GetLength() > 0) {
          sAppStream << sColor;
          sAppStream << fWidth << " w"
                     << " [" << dash.nDash << " " << dash.nGap << "] "
                     << dash.nPhase << " d\n";
          sAppStream << fLeft + fWidth / 2 << " " << fBottom + fWidth / 2
                     << " m\n";
          sAppStream << fLeft + fWidth / 2 << " " << fTop - fWidth / 2
                     << " l\n";
          sAppStream << fRight - fWidth / 2 << " " << fTop - fWidth / 2
                     << " l\n";
          sAppStream << fRight - fWidth / 2 << " " << fBottom + fWidth / 2
                     << " l\n";
          sAppStream << fLeft + fWidth / 2 << " " << fBottom + fWidth / 2
                     << " l S\n";
        }
        break;
      case BorderStyle::BEVELED:
      case BorderStyle::INSET:
        sColor = CPWL_Utils::GetColorAppStream(crLeftTop, true);
        if (sColor.GetLength() > 0) {
          sAppStream << sColor;
          sAppStream << fLeft + fHalfWidth << " " << fBottom + fHalfWidth
                     << " m\n";
          sAppStream << fLeft + fHalfWidth << " " << fTop - fHalfWidth
                     << " l\n";
          sAppStream << fRight - fHalfWidth << " " << fTop - fHalfWidth
                     << " l\n";
          sAppStream << fRight - fHalfWidth * 2 << " " << fTop - fHalfWidth * 2
                     << " l\n";
          sAppStream << fLeft + fHalfWidth * 2 << " " << fTop - fHalfWidth * 2
                     << " l\n";
          sAppStream << fLeft + fHalfWidth * 2 << " "
                     << fBottom + fHalfWidth * 2 << " l f\n";
        }

        sColor = CPWL_Utils::GetColorAppStream(crRightBottom, true);
        if (sColor.GetLength() > 0) {
          sAppStream << sColor;
          sAppStream << fRight - fHalfWidth << " " << fTop - fHalfWidth
                     << " m\n";
          sAppStream << fRight - fHalfWidth << " " << fBottom + fHalfWidth
                     << " l\n";
          sAppStream << fLeft + fHalfWidth << " " << fBottom + fHalfWidth
                     << " l\n";
          sAppStream << fLeft + fHalfWidth * 2 << " "
                     << fBottom + fHalfWidth * 2 << " l\n";
          sAppStream << fRight - fHalfWidth * 2 << " "
                     << fBottom + fHalfWidth * 2 << " l\n";
          sAppStream << fRight - fHalfWidth * 2 << " " << fTop - fHalfWidth * 2
                     << " l f\n";
        }

        sColor = CPWL_Utils::GetColorAppStream(color, true);
        if (sColor.GetLength() > 0) {
          sAppStream << sColor;
          sAppStream << fLeft << " " << fBottom << " " << fRight - fLeft << " "
                     << fTop - fBottom << " re\n";
          sAppStream << fLeft + fHalfWidth << " " << fBottom + fHalfWidth << " "
                     << fRight - fLeft - fHalfWidth * 2 << " "
                     << fTop - fBottom - fHalfWidth * 2 << " re f*\n";
        }
        break;
      case BorderStyle::UNDERLINE:
        sColor = CPWL_Utils::GetColorAppStream(color, false);
        if (sColor.GetLength() > 0) {
          sAppStream << sColor;
          sAppStream << fWidth << " w\n";
          sAppStream << fLeft << " " << fBottom + fWidth / 2 << " m\n";
          sAppStream << fRight << " " << fBottom + fWidth / 2 << " l S\n";
        }
        break;
    }

    sAppStream << "Q\n";
  }

  return sAppStream.MakeString();
}

CFX_ByteString CPWL_Utils::GetCircleBorderAppStream(
    const CFX_FloatRect& rect,
    FX_FLOAT fWidth,
    const CPWL_Color& color,
    const CPWL_Color& crLeftTop,
    const CPWL_Color& crRightBottom,
    BorderStyle nStyle,
    const CPWL_Dash& dash) {
  CFX_ByteTextBuf sAppStream;
  CFX_ByteString sColor;

  if (fWidth > 0.0f) {
    sAppStream << "q\n";

    switch (nStyle) {
      default:
      case BorderStyle::SOLID:
      case BorderStyle::UNDERLINE: {
        sColor = CPWL_Utils::GetColorAppStream(color, false);
        if (sColor.GetLength() > 0) {
          sAppStream << "q\n" << fWidth << " w\n" << sColor
                     << CPWL_Utils::GetAP_Circle(
                            CPWL_Utils::DeflateRect(rect, fWidth / 2.0f))
                     << " S\nQ\n";
        }
      } break;
      case BorderStyle::DASH: {
        sColor = CPWL_Utils::GetColorAppStream(color, false);
        if (sColor.GetLength() > 0) {
          sAppStream << "q\n" << fWidth << " w\n"
                     << "[" << dash.nDash << " " << dash.nGap << "] "
                     << dash.nPhase << " d\n" << sColor
                     << CPWL_Utils::GetAP_Circle(
                            CPWL_Utils::DeflateRect(rect, fWidth / 2.0f))
                     << " S\nQ\n";
        }
      } break;
      case BorderStyle::BEVELED: {
        FX_FLOAT fHalfWidth = fWidth / 2.0f;

        sColor = CPWL_Utils::GetColorAppStream(color, false);
        if (sColor.GetLength() > 0) {
          sAppStream << "q\n" << fHalfWidth << " w\n" << sColor
                     << CPWL_Utils::GetAP_Circle(rect) << " S\nQ\n";
        }

        sColor = CPWL_Utils::GetColorAppStream(crLeftTop, false);
        if (sColor.GetLength() > 0) {
          sAppStream << "q\n" << fHalfWidth << " w\n" << sColor
                     << CPWL_Utils::GetAP_HalfCircle(
                            CPWL_Utils::DeflateRect(rect, fHalfWidth * 0.75f),
                            FX_PI / 4.0f)
                     << " S\nQ\n";
        }

        sColor = CPWL_Utils::GetColorAppStream(crRightBottom, false);
        if (sColor.GetLength() > 0) {
          sAppStream << "q\n" << fHalfWidth << " w\n" << sColor
                     << CPWL_Utils::GetAP_HalfCircle(
                            CPWL_Utils::DeflateRect(rect, fHalfWidth * 0.75f),
                            FX_PI * 5 / 4.0f)
                     << " S\nQ\n";
        }
      } break;
      case BorderStyle::INSET: {
        FX_FLOAT fHalfWidth = fWidth / 2.0f;

        sColor = CPWL_Utils::GetColorAppStream(color, false);
        if (sColor.GetLength() > 0) {
          sAppStream << "q\n" << fHalfWidth << " w\n" << sColor
                     << CPWL_Utils::GetAP_Circle(rect) << " S\nQ\n";
        }

        sColor = CPWL_Utils::GetColorAppStream(crLeftTop, false);
        if (sColor.GetLength() > 0) {
          sAppStream << "q\n" << fHalfWidth << " w\n" << sColor
                     << CPWL_Utils::GetAP_HalfCircle(
                            CPWL_Utils::DeflateRect(rect, fHalfWidth * 0.75f),
                            FX_PI / 4.0f)
                     << " S\nQ\n";
        }

        sColor = CPWL_Utils::GetColorAppStream(crRightBottom, false);
        if (sColor.GetLength() > 0) {
          sAppStream << "q\n" << fHalfWidth << " w\n" << sColor
                     << CPWL_Utils::GetAP_HalfCircle(
                            CPWL_Utils::DeflateRect(rect, fHalfWidth * 0.75f),
                            FX_PI * 5 / 4.0f)
                     << " S\nQ\n";
        }
      } break;
    }

    sAppStream << "Q\n";
  }

  return sAppStream.MakeString();
}

CFX_ByteString CPWL_Utils::GetAppStream_Check(const CFX_FloatRect& rcBBox,
                                              const CPWL_Color& crText) {
  CFX_ByteTextBuf sAP;
  sAP << "q\n"
      << CPWL_Utils::GetColorAppStream(crText, true)
      << CPWL_Utils::GetAP_Check(rcBBox) << "f\nQ\n";
  return sAP.MakeString();
}

CFX_ByteString CPWL_Utils::GetAppStream_Circle(const CFX_FloatRect& rcBBox,
                                               const CPWL_Color& crText) {
  CFX_ByteTextBuf sAP;
  sAP << "q\n"
      << CPWL_Utils::GetColorAppStream(crText, true)
      << CPWL_Utils::GetAP_Circle(rcBBox) << "f\nQ\n";
  return sAP.MakeString();
}

CFX_ByteString CPWL_Utils::GetAppStream_Cross(const CFX_FloatRect& rcBBox,
                                              const CPWL_Color& crText) {
  CFX_ByteTextBuf sAP;
  sAP << "q\n"
      << CPWL_Utils::GetColorAppStream(crText, false)
      << CPWL_Utils::GetAP_Cross(rcBBox) << "S\nQ\n";
  return sAP.MakeString();
}

CFX_ByteString CPWL_Utils::GetAppStream_Diamond(const CFX_FloatRect& rcBBox,
                                                const CPWL_Color& crText) {
  CFX_ByteTextBuf sAP;
  sAP << "q\n1 w\n"
      << CPWL_Utils::GetColorAppStream(crText, true)
      << CPWL_Utils::GetAP_Diamond(rcBBox) << "f\nQ\n";
  return sAP.MakeString();
}

CFX_ByteString CPWL_Utils::GetAppStream_Square(const CFX_FloatRect& rcBBox,
                                               const CPWL_Color& crText) {
  CFX_ByteTextBuf sAP;
  sAP << "q\n"
      << CPWL_Utils::GetColorAppStream(crText, true)
      << CPWL_Utils::GetAP_Square(rcBBox) << "f\nQ\n";
  return sAP.MakeString();
}

CFX_ByteString CPWL_Utils::GetAppStream_Star(const CFX_FloatRect& rcBBox,
                                             const CPWL_Color& crText) {
  CFX_ByteTextBuf sAP;
  sAP << "q\n"
      << CPWL_Utils::GetColorAppStream(crText, true)
      << CPWL_Utils::GetAP_Star(rcBBox) << "f\nQ\n";
  return sAP.MakeString();
}

CFX_ByteString CPWL_Utils::GetCheckBoxAppStream(const CFX_FloatRect& rcBBox,
                                                int32_t nStyle,
                                                const CPWL_Color& crText) {
  CFX_FloatRect rcCenter = GetCenterSquare(rcBBox);
  switch (nStyle) {
    default:
    case PCS_CHECK:
      return GetAppStream_Check(rcCenter, crText);
    case PCS_CIRCLE:
      return GetAppStream_Circle(ScaleRect(rcCenter, 2.0f / 3.0f), crText);
    case PCS_CROSS:
      return GetAppStream_Cross(rcCenter, crText);
    case PCS_DIAMOND:
      return GetAppStream_Diamond(ScaleRect(rcCenter, 2.0f / 3.0f), crText);
    case PCS_SQUARE:
      return GetAppStream_Square(ScaleRect(rcCenter, 2.0f / 3.0f), crText);
    case PCS_STAR:
      return GetAppStream_Star(ScaleRect(rcCenter, 2.0f / 3.0f), crText);
  }
}

CFX_ByteString CPWL_Utils::GetRadioButtonAppStream(const CFX_FloatRect& rcBBox,
                                                   int32_t nStyle,
                                                   const CPWL_Color& crText) {
  CFX_FloatRect rcCenter = GetCenterSquare(rcBBox);
  switch (nStyle) {
    default:
    case PCS_CHECK:
      return GetAppStream_Check(rcCenter, crText);
    case PCS_CIRCLE:
      return GetAppStream_Circle(ScaleRect(rcCenter, 1.0f / 2.0f), crText);
    case PCS_CROSS:
      return GetAppStream_Cross(rcCenter, crText);
    case PCS_DIAMOND:
      return GetAppStream_Diamond(ScaleRect(rcCenter, 2.0f / 3.0f), crText);
    case PCS_SQUARE:
      return GetAppStream_Square(ScaleRect(rcCenter, 2.0f / 3.0f), crText);
    case PCS_STAR:
      return GetAppStream_Star(ScaleRect(rcCenter, 2.0f / 3.0f), crText);
  }
}

CFX_ByteString CPWL_Utils::GetDropButtonAppStream(const CFX_FloatRect& rcBBox) {
  CFX_ByteTextBuf sAppStream;

  if (!rcBBox.IsEmpty()) {
    sAppStream << "q\n"
               << CPWL_Utils::GetColorAppStream(
                      CPWL_Color(COLORTYPE_RGB, 220.0f / 255.0f,
                                 220.0f / 255.0f, 220.0f / 255.0f),
                      true);
    sAppStream << rcBBox.left << " " << rcBBox.bottom << " "
               << rcBBox.right - rcBBox.left << " "
               << rcBBox.top - rcBBox.bottom << " re f\n";
    sAppStream << "Q\n";

    sAppStream << "q\n"
               << CPWL_Utils::GetBorderAppStream(
                      rcBBox, 2, CPWL_Color(COLORTYPE_GRAY, 0),
                      CPWL_Color(COLORTYPE_GRAY, 1),
                      CPWL_Color(COLORTYPE_GRAY, 0.5), BorderStyle::BEVELED,
                      CPWL_Dash(3, 0, 0))
               << "Q\n";

    CFX_PointF ptCenter = CFX_PointF((rcBBox.left + rcBBox.right) / 2,
                                     (rcBBox.top + rcBBox.bottom) / 2);
    if (IsFloatBigger(rcBBox.right - rcBBox.left, 6) &&
        IsFloatBigger(rcBBox.top - rcBBox.bottom, 6)) {
      sAppStream << "q\n"
                 << " 0 g\n";
      sAppStream << ptCenter.x - 3 << " " << ptCenter.y + 1.5f << " m\n";
      sAppStream << ptCenter.x + 3 << " " << ptCenter.y + 1.5f << " l\n";
      sAppStream << ptCenter.x << " " << ptCenter.y - 1.5f << " l\n";
      sAppStream << ptCenter.x - 3 << " " << ptCenter.y + 1.5f << " l f\n";
      sAppStream << "Q\n";
    }
  }

  return sAppStream.MakeString();
}

void CPWL_Utils::DrawFillRect(CFX_RenderDevice* pDevice,
                              CFX_Matrix* pUser2Device,
                              const CFX_FloatRect& rect,
                              const FX_COLORREF& color) {
  CFX_PathData path;
  CFX_FloatRect rcTemp(rect);
  path.AppendRect(rcTemp.left, rcTemp.bottom, rcTemp.right, rcTemp.top);
  pDevice->DrawPath(&path, pUser2Device, nullptr, color, 0, FXFILL_WINDING);
}

void CPWL_Utils::DrawFillArea(CFX_RenderDevice* pDevice,
                              CFX_Matrix* pUser2Device,
                              const CFX_PointF* pPts,
                              int32_t nCount,
                              const FX_COLORREF& color) {
  CFX_PathData path;
  path.AppendPoint(pPts[0], FXPT_TYPE::MoveTo, false);
  for (int32_t i = 1; i < nCount; i++)
    path.AppendPoint(pPts[i], FXPT_TYPE::LineTo, false);

  pDevice->DrawPath(&path, pUser2Device, nullptr, color, 0, FXFILL_ALTERNATE);
}

void CPWL_Utils::DrawStrokeRect(CFX_RenderDevice* pDevice,
                                CFX_Matrix* pUser2Device,
                                const CFX_FloatRect& rect,
                                const FX_COLORREF& color,
                                FX_FLOAT fWidth) {
  CFX_PathData path;
  CFX_FloatRect rcTemp(rect);
  path.AppendRect(rcTemp.left, rcTemp.bottom, rcTemp.right, rcTemp.top);

  CFX_GraphStateData gsd;
  gsd.m_LineWidth = fWidth;

  pDevice->DrawPath(&path, pUser2Device, &gsd, 0, color, FXFILL_ALTERNATE);
}

void CPWL_Utils::DrawStrokeLine(CFX_RenderDevice* pDevice,
                                CFX_Matrix* pUser2Device,
                                const CFX_PointF& ptMoveTo,
                                const CFX_PointF& ptLineTo,
                                const FX_COLORREF& color,
                                FX_FLOAT fWidth) {
  CFX_PathData path;
  path.AppendPoint(ptMoveTo, FXPT_TYPE::MoveTo, false);
  path.AppendPoint(ptLineTo, FXPT_TYPE::LineTo, false);

  CFX_GraphStateData gsd;
  gsd.m_LineWidth = fWidth;

  pDevice->DrawPath(&path, pUser2Device, &gsd, 0, color, FXFILL_ALTERNATE);
}

void CPWL_Utils::DrawFillRect(CFX_RenderDevice* pDevice,
                              CFX_Matrix* pUser2Device,
                              const CFX_FloatRect& rect,
                              const CPWL_Color& color,
                              int32_t nTransparency) {
  CPWL_Utils::DrawFillRect(pDevice, pUser2Device, rect,
                           color.ToFXColor(nTransparency));
}

void CPWL_Utils::DrawShadow(CFX_RenderDevice* pDevice,
                            CFX_Matrix* pUser2Device,
                            bool bVertical,
                            bool bHorizontal,
                            CFX_FloatRect rect,
                            int32_t nTransparency,
                            int32_t nStartGray,
                            int32_t nEndGray) {
  FX_FLOAT fStepGray = 1.0f;

  if (bVertical) {
    fStepGray = (nEndGray - nStartGray) / rect.Height();

    for (FX_FLOAT fy = rect.bottom + 0.5f; fy <= rect.top - 0.5f; fy += 1.0f) {
      int32_t nGray = nStartGray + (int32_t)(fStepGray * (fy - rect.bottom));
      CPWL_Utils::DrawStrokeLine(
          pDevice, pUser2Device, CFX_PointF(rect.left, fy),
          CFX_PointF(rect.right, fy),
          ArgbEncode(nTransparency, nGray, nGray, nGray), 1.5f);
    }
  }

  if (bHorizontal) {
    fStepGray = (nEndGray - nStartGray) / rect.Width();

    for (FX_FLOAT fx = rect.left + 0.5f; fx <= rect.right - 0.5f; fx += 1.0f) {
      int32_t nGray = nStartGray + (int32_t)(fStepGray * (fx - rect.left));
      CPWL_Utils::DrawStrokeLine(
          pDevice, pUser2Device, CFX_PointF(fx, rect.bottom),
          CFX_PointF(fx, rect.top),
          ArgbEncode(nTransparency, nGray, nGray, nGray), 1.5f);
    }
  }
}

void CPWL_Utils::DrawBorder(CFX_RenderDevice* pDevice,
                            CFX_Matrix* pUser2Device,
                            const CFX_FloatRect& rect,
                            FX_FLOAT fWidth,
                            const CPWL_Color& color,
                            const CPWL_Color& crLeftTop,
                            const CPWL_Color& crRightBottom,
                            BorderStyle nStyle,
                            int32_t nTransparency) {
  FX_FLOAT fLeft = rect.left;
  FX_FLOAT fRight = rect.right;
  FX_FLOAT fTop = rect.top;
  FX_FLOAT fBottom = rect.bottom;

  if (fWidth > 0.0f) {
    FX_FLOAT fHalfWidth = fWidth / 2.0f;

    switch (nStyle) {
      default:
      case BorderStyle::SOLID: {
        CFX_PathData path;
        path.AppendRect(fLeft, fBottom, fRight, fTop);
        path.AppendRect(fLeft + fWidth, fBottom + fWidth, fRight - fWidth,
                        fTop - fWidth);
        pDevice->DrawPath(&path, pUser2Device, nullptr,
                          color.ToFXColor(nTransparency), 0, FXFILL_ALTERNATE);
        break;
      }
      case BorderStyle::DASH: {
        CFX_PathData path;
        path.AppendPoint(
            CFX_PointF(fLeft + fWidth / 2.0f, fBottom + fWidth / 2.0f),
            FXPT_TYPE::MoveTo, false);
        path.AppendPoint(
            CFX_PointF(fLeft + fWidth / 2.0f, fTop - fWidth / 2.0f),
            FXPT_TYPE::LineTo, false);
        path.AppendPoint(
            CFX_PointF(fRight - fWidth / 2.0f, fTop - fWidth / 2.0f),
            FXPT_TYPE::LineTo, false);
        path.AppendPoint(
            CFX_PointF(fRight - fWidth / 2.0f, fBottom + fWidth / 2.0f),
            FXPT_TYPE::LineTo, false);
        path.AppendPoint(
            CFX_PointF(fLeft + fWidth / 2.0f, fBottom + fWidth / 2.0f),
            FXPT_TYPE::LineTo, false);

        CFX_GraphStateData gsd;
        gsd.SetDashCount(2);
        gsd.m_DashArray[0] = 3.0f;
        gsd.m_DashArray[1] = 3.0f;
        gsd.m_DashPhase = 0;

        gsd.m_LineWidth = fWidth;
        pDevice->DrawPath(&path, pUser2Device, &gsd, 0,
                          color.ToFXColor(nTransparency), FXFILL_WINDING);
        break;
      }
      case BorderStyle::BEVELED:
      case BorderStyle::INSET: {
        CFX_GraphStateData gsd;
        gsd.m_LineWidth = fHalfWidth;

        CFX_PathData pathLT;

        pathLT.AppendPoint(CFX_PointF(fLeft + fHalfWidth, fBottom + fHalfWidth),
                           FXPT_TYPE::MoveTo, false);
        pathLT.AppendPoint(CFX_PointF(fLeft + fHalfWidth, fTop - fHalfWidth),
                           FXPT_TYPE::LineTo, false);
        pathLT.AppendPoint(CFX_PointF(fRight - fHalfWidth, fTop - fHalfWidth),
                           FXPT_TYPE::LineTo, false);
        pathLT.AppendPoint(
            CFX_PointF(fRight - fHalfWidth * 2, fTop - fHalfWidth * 2),
            FXPT_TYPE::LineTo, false);
        pathLT.AppendPoint(
            CFX_PointF(fLeft + fHalfWidth * 2, fTop - fHalfWidth * 2),
            FXPT_TYPE::LineTo, false);
        pathLT.AppendPoint(
            CFX_PointF(fLeft + fHalfWidth * 2, fBottom + fHalfWidth * 2),
            FXPT_TYPE::LineTo, false);
        pathLT.AppendPoint(CFX_PointF(fLeft + fHalfWidth, fBottom + fHalfWidth),
                           FXPT_TYPE::LineTo, false);

        pDevice->DrawPath(&pathLT, pUser2Device, &gsd,
                          crLeftTop.ToFXColor(nTransparency), 0,
                          FXFILL_ALTERNATE);

        CFX_PathData pathRB;
        pathRB.AppendPoint(CFX_PointF(fRight - fHalfWidth, fTop - fHalfWidth),
                           FXPT_TYPE::MoveTo, false);
        pathRB.AppendPoint(
            CFX_PointF(fRight - fHalfWidth, fBottom + fHalfWidth),
            FXPT_TYPE::LineTo, false);
        pathRB.AppendPoint(CFX_PointF(fLeft + fHalfWidth, fBottom + fHalfWidth),
                           FXPT_TYPE::LineTo, false);
        pathRB.AppendPoint(
            CFX_PointF(fLeft + fHalfWidth * 2, fBottom + fHalfWidth * 2),
            FXPT_TYPE::LineTo, false);
        pathRB.AppendPoint(
            CFX_PointF(fRight - fHalfWidth * 2, fBottom + fHalfWidth * 2),
            FXPT_TYPE::LineTo, false);
        pathRB.AppendPoint(
            CFX_PointF(fRight - fHalfWidth * 2, fTop - fHalfWidth * 2),
            FXPT_TYPE::LineTo, false);
        pathRB.AppendPoint(CFX_PointF(fRight - fHalfWidth, fTop - fHalfWidth),
                           FXPT_TYPE::LineTo, false);

        pDevice->DrawPath(&pathRB, pUser2Device, &gsd,
                          crRightBottom.ToFXColor(nTransparency), 0,
                          FXFILL_ALTERNATE);

        CFX_PathData path;

        path.AppendRect(fLeft, fBottom, fRight, fTop);
        path.AppendRect(fLeft + fHalfWidth, fBottom + fHalfWidth,
                        fRight - fHalfWidth, fTop - fHalfWidth);

        pDevice->DrawPath(&path, pUser2Device, &gsd,
                          color.ToFXColor(nTransparency), 0, FXFILL_ALTERNATE);
        break;
      }
      case BorderStyle::UNDERLINE: {
        CFX_PathData path;
        path.AppendPoint(CFX_PointF(fLeft, fBottom + fWidth / 2),
                         FXPT_TYPE::MoveTo, false);
        path.AppendPoint(CFX_PointF(fRight, fBottom + fWidth / 2),
                         FXPT_TYPE::LineTo, false);

        CFX_GraphStateData gsd;
        gsd.m_LineWidth = fWidth;

        pDevice->DrawPath(&path, pUser2Device, &gsd, 0,
                          color.ToFXColor(nTransparency), FXFILL_ALTERNATE);
        break;
      }
    }
  }
}

