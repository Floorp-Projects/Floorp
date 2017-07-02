// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpvt_generateap.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/parser/cpdf_boolean.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_simple_parser.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fpdfdoc/cpdf_annot.h"
#include "core/fpdfdoc/cpdf_formfield.h"
#include "core/fpdfdoc/cpvt_color.h"
#include "core/fpdfdoc/cpvt_fontmap.h"
#include "core/fpdfdoc/cpvt_word.h"
#include "third_party/base/ptr_util.h"

namespace {

bool GenerateWidgetAP(CPDF_Document* pDoc,
                      CPDF_Dictionary* pAnnotDict,
                      const int32_t& nWidgetType) {
  CPDF_Dictionary* pFormDict = nullptr;
  if (CPDF_Dictionary* pRootDict = pDoc->GetRoot())
    pFormDict = pRootDict->GetDictFor("AcroForm");
  if (!pFormDict)
    return false;

  CFX_ByteString DA;
  if (CPDF_Object* pDAObj = FPDF_GetFieldAttr(pAnnotDict, "DA"))
    DA = pDAObj->GetString();
  if (DA.IsEmpty())
    DA = pFormDict->GetStringFor("DA");
  if (DA.IsEmpty())
    return false;

  CPDF_SimpleParser syntax(DA.AsStringC());
  syntax.FindTagParamFromStart("Tf", 2);
  CFX_ByteString sFontName(syntax.GetWord());
  sFontName = PDF_NameDecode(sFontName);
  if (sFontName.IsEmpty())
    return false;

  FX_FLOAT fFontSize = FX_atof(syntax.GetWord());
  CPVT_Color crText = CPVT_Color::ParseColor(DA);
  CPDF_Dictionary* pDRDict = pFormDict->GetDictFor("DR");
  if (!pDRDict)
    return false;

  CPDF_Dictionary* pDRFontDict = pDRDict->GetDictFor("Font");
  if (!pDRFontDict)
    return false;

  CPDF_Dictionary* pFontDict = pDRFontDict->GetDictFor(sFontName.Mid(1));
  if (!pFontDict) {
    pFontDict = pDoc->NewIndirect<CPDF_Dictionary>();
    pFontDict->SetNewFor<CPDF_Name>("Type", "Font");
    pFontDict->SetNewFor<CPDF_Name>("Subtype", "Type1");
    pFontDict->SetNewFor<CPDF_Name>("BaseFont", "Helvetica");
    pFontDict->SetNewFor<CPDF_Name>("Encoding", "WinAnsiEncoding");
    pDRFontDict->SetNewFor<CPDF_Reference>(sFontName.Mid(1), pDoc,
                                           pFontDict->GetObjNum());
  }
  CPDF_Font* pDefFont = pDoc->LoadFont(pFontDict);
  if (!pDefFont)
    return false;

  CFX_FloatRect rcAnnot = pAnnotDict->GetRectFor("Rect");
  int32_t nRotate = 0;
  if (CPDF_Dictionary* pMKDict = pAnnotDict->GetDictFor("MK"))
    nRotate = pMKDict->GetIntegerFor("R");

  CFX_FloatRect rcBBox;
  CFX_Matrix matrix;
  switch (nRotate % 360) {
    case 0:
      rcBBox = CFX_FloatRect(0, 0, rcAnnot.right - rcAnnot.left,
                             rcAnnot.top - rcAnnot.bottom);
      break;
    case 90:
      matrix = CFX_Matrix(0, 1, -1, 0, rcAnnot.right - rcAnnot.left, 0);
      rcBBox = CFX_FloatRect(0, 0, rcAnnot.top - rcAnnot.bottom,
                             rcAnnot.right - rcAnnot.left);
      break;
    case 180:
      matrix = CFX_Matrix(-1, 0, 0, -1, rcAnnot.right - rcAnnot.left,
                          rcAnnot.top - rcAnnot.bottom);
      rcBBox = CFX_FloatRect(0, 0, rcAnnot.right - rcAnnot.left,
                             rcAnnot.top - rcAnnot.bottom);
      break;
    case 270:
      matrix = CFX_Matrix(0, -1, 1, 0, 0, rcAnnot.top - rcAnnot.bottom);
      rcBBox = CFX_FloatRect(0, 0, rcAnnot.top - rcAnnot.bottom,
                             rcAnnot.right - rcAnnot.left);
      break;
  }

  BorderStyle nBorderStyle = BorderStyle::SOLID;
  FX_FLOAT fBorderWidth = 1;
  CPVT_Dash dsBorder(3, 0, 0);
  CPVT_Color crLeftTop, crRightBottom;
  if (CPDF_Dictionary* pBSDict = pAnnotDict->GetDictFor("BS")) {
    if (pBSDict->KeyExist("W"))
      fBorderWidth = pBSDict->GetNumberFor("W");

    if (CPDF_Array* pArray = pBSDict->GetArrayFor("D")) {
      dsBorder = CPVT_Dash(pArray->GetIntegerAt(0), pArray->GetIntegerAt(1),
                           pArray->GetIntegerAt(2));
    }
    switch (pBSDict->GetStringFor("S").GetAt(0)) {
      case 'S':
        nBorderStyle = BorderStyle::SOLID;
        break;
      case 'D':
        nBorderStyle = BorderStyle::DASH;
        break;
      case 'B':
        nBorderStyle = BorderStyle::BEVELED;
        fBorderWidth *= 2;
        crLeftTop = CPVT_Color(CPVT_Color::kGray, 1);
        crRightBottom = CPVT_Color(CPVT_Color::kGray, 0.5);
        break;
      case 'I':
        nBorderStyle = BorderStyle::INSET;
        fBorderWidth *= 2;
        crLeftTop = CPVT_Color(CPVT_Color::kGray, 0.5);
        crRightBottom = CPVT_Color(CPVT_Color::kGray, 0.75);
        break;
      case 'U':
        nBorderStyle = BorderStyle::UNDERLINE;
        break;
    }
  }
  CPVT_Color crBorder, crBG;
  if (CPDF_Dictionary* pMKDict = pAnnotDict->GetDictFor("MK")) {
    if (CPDF_Array* pArray = pMKDict->GetArrayFor("BC"))
      crBorder = CPVT_Color::ParseColor(*pArray);
    if (CPDF_Array* pArray = pMKDict->GetArrayFor("BG"))
      crBG = CPVT_Color::ParseColor(*pArray);
  }
  CFX_ByteTextBuf sAppStream;
  CFX_ByteString sBG =
      CPVT_GenerateAP::GenerateColorAP(crBG, PaintOperation::FILL);
  if (sBG.GetLength() > 0) {
    sAppStream << "q\n" << sBG << rcBBox.left << " " << rcBBox.bottom << " "
               << rcBBox.Width() << " " << rcBBox.Height() << " re f\n"
               << "Q\n";
  }
  CFX_ByteString sBorderStream = CPVT_GenerateAP::GenerateBorderAP(
      rcBBox, fBorderWidth, crBorder, crLeftTop, crRightBottom, nBorderStyle,
      dsBorder);
  if (sBorderStream.GetLength() > 0)
    sAppStream << "q\n" << sBorderStream << "Q\n";

  CFX_FloatRect rcBody =
      CFX_FloatRect(rcBBox.left + fBorderWidth, rcBBox.bottom + fBorderWidth,
                    rcBBox.right - fBorderWidth, rcBBox.top - fBorderWidth);
  rcBody.Normalize();

  CPDF_Dictionary* pAPDict = pAnnotDict->GetDictFor("AP");
  if (!pAPDict)
    pAPDict = pAnnotDict->SetNewFor<CPDF_Dictionary>("AP");

  CPDF_Stream* pNormalStream = pAPDict->GetStreamFor("N");
  if (!pNormalStream) {
    pNormalStream = pDoc->NewIndirect<CPDF_Stream>();
    pAPDict->SetNewFor<CPDF_Reference>("N", pDoc, pNormalStream->GetObjNum());
  }
  CPDF_Dictionary* pStreamDict = pNormalStream->GetDict();
  if (pStreamDict) {
    pStreamDict->SetMatrixFor("Matrix", matrix);
    pStreamDict->SetRectFor("BBox", rcBBox);
    CPDF_Dictionary* pStreamResList = pStreamDict->GetDictFor("Resources");
    if (pStreamResList) {
      CPDF_Dictionary* pStreamResFontList = pStreamResList->GetDictFor("Font");
      if (!pStreamResFontList)
        pStreamResFontList = pStreamResList->SetNewFor<CPDF_Dictionary>("Font");
      if (!pStreamResFontList->KeyExist(sFontName)) {
        pStreamResFontList->SetNewFor<CPDF_Reference>(sFontName, pDoc,
                                                      pFontDict->GetObjNum());
      }
    } else {
      pStreamDict->SetFor("Resources", pFormDict->GetDictFor("DR")->Clone());
      pStreamResList = pStreamDict->GetDictFor("Resources");
    }
  }
  switch (nWidgetType) {
    case 0: {
      CFX_WideString swValue =
          FPDF_GetFieldAttr(pAnnotDict, "V")
              ? FPDF_GetFieldAttr(pAnnotDict, "V")->GetUnicodeText()
              : CFX_WideString();
      int32_t nAlign = FPDF_GetFieldAttr(pAnnotDict, "Q")
                           ? FPDF_GetFieldAttr(pAnnotDict, "Q")->GetInteger()
                           : 0;
      uint32_t dwFlags = FPDF_GetFieldAttr(pAnnotDict, "Ff")
                             ? FPDF_GetFieldAttr(pAnnotDict, "Ff")->GetInteger()
                             : 0;
      uint32_t dwMaxLen =
          FPDF_GetFieldAttr(pAnnotDict, "MaxLen")
              ? FPDF_GetFieldAttr(pAnnotDict, "MaxLen")->GetInteger()
              : 0;
      CPVT_FontMap map(
          pDoc, pStreamDict ? pStreamDict->GetDictFor("Resources") : nullptr,
          pDefFont, sFontName.Right(sFontName.GetLength() - 1));
      CPDF_VariableText::Provider prd(&map);
      CPDF_VariableText vt;
      vt.SetProvider(&prd);
      vt.SetPlateRect(rcBody);
      vt.SetAlignment(nAlign);
      if (IsFloatZero(fFontSize))
        vt.SetAutoFontSize(true);
      else
        vt.SetFontSize(fFontSize);

      bool bMultiLine = (dwFlags >> 12) & 1;
      if (bMultiLine) {
        vt.SetMultiLine(true);
        vt.SetAutoReturn(true);
      }
      uint16_t subWord = 0;
      if ((dwFlags >> 13) & 1) {
        subWord = '*';
        vt.SetPasswordChar(subWord);
      }
      bool bCharArray = (dwFlags >> 24) & 1;
      if (bCharArray)
        vt.SetCharArray(dwMaxLen);
      else
        vt.SetLimitChar(dwMaxLen);

      vt.Initialize();
      vt.SetText(swValue);
      vt.RearrangeAll();
      CFX_FloatRect rcContent = vt.GetContentRect();
      CFX_PointF ptOffset;
      if (!bMultiLine) {
        ptOffset =
            CFX_PointF(0.0f, (rcContent.Height() - rcBody.Height()) / 2.0f);
      }
      CFX_ByteString sBody = CPVT_GenerateAP::GenerateEditAP(
          &map, vt.GetIterator(), ptOffset, !bCharArray, subWord);
      if (sBody.GetLength() > 0) {
        sAppStream << "/Tx BMC\n"
                   << "q\n";
        if (rcContent.Width() > rcBody.Width() ||
            rcContent.Height() > rcBody.Height()) {
          sAppStream << rcBody.left << " " << rcBody.bottom << " "
                     << rcBody.Width() << " " << rcBody.Height()
                     << " re\nW\nn\n";
        }
        sAppStream << "BT\n"
                   << CPVT_GenerateAP::GenerateColorAP(crText,
                                                       PaintOperation::FILL)
                   << sBody << "ET\n"
                   << "Q\nEMC\n";
      }
    } break;
    case 1: {
      CFX_WideString swValue =
          FPDF_GetFieldAttr(pAnnotDict, "V")
              ? FPDF_GetFieldAttr(pAnnotDict, "V")->GetUnicodeText()
              : CFX_WideString();
      CPVT_FontMap map(
          pDoc, pStreamDict ? pStreamDict->GetDictFor("Resources") : nullptr,
          pDefFont, sFontName.Right(sFontName.GetLength() - 1));
      CPDF_VariableText::Provider prd(&map);
      CPDF_VariableText vt;
      vt.SetProvider(&prd);
      CFX_FloatRect rcButton = rcBody;
      rcButton.left = rcButton.right - 13;
      rcButton.Normalize();
      CFX_FloatRect rcEdit = rcBody;
      rcEdit.right = rcButton.left;
      rcEdit.Normalize();
      vt.SetPlateRect(rcEdit);
      if (IsFloatZero(fFontSize))
        vt.SetAutoFontSize(true);
      else
        vt.SetFontSize(fFontSize);

      vt.Initialize();
      vt.SetText(swValue);
      vt.RearrangeAll();
      CFX_FloatRect rcContent = vt.GetContentRect();
      CFX_PointF ptOffset =
          CFX_PointF(0.0f, (rcContent.Height() - rcEdit.Height()) / 2.0f);
      CFX_ByteString sEdit = CPVT_GenerateAP::GenerateEditAP(
          &map, vt.GetIterator(), ptOffset, true, 0);
      if (sEdit.GetLength() > 0) {
        sAppStream << "/Tx BMC\n"
                   << "q\n";
        sAppStream << rcEdit.left << " " << rcEdit.bottom << " "
                   << rcEdit.Width() << " " << rcEdit.Height() << " re\nW\nn\n";
        sAppStream << "BT\n"
                   << CPVT_GenerateAP::GenerateColorAP(crText,
                                                       PaintOperation::FILL)
                   << sEdit << "ET\n"
                   << "Q\nEMC\n";
      }
      CFX_ByteString sButton = CPVT_GenerateAP::GenerateColorAP(
          CPVT_Color(CPVT_Color::kRGB, 220.0f / 255.0f, 220.0f / 255.0f,
                     220.0f / 255.0f),
          PaintOperation::FILL);
      if (sButton.GetLength() > 0 && !rcButton.IsEmpty()) {
        sAppStream << "q\n" << sButton;
        sAppStream << rcButton.left << " " << rcButton.bottom << " "
                   << rcButton.Width() << " " << rcButton.Height() << " re f\n";
        sAppStream << "Q\n";
        CFX_ByteString sButtonBorder = CPVT_GenerateAP::GenerateBorderAP(
            rcButton, 2, CPVT_Color(CPVT_Color::kGray, 0),
            CPVT_Color(CPVT_Color::kGray, 1),
            CPVT_Color(CPVT_Color::kGray, 0.5), BorderStyle::BEVELED,
            CPVT_Dash(3, 0, 0));
        if (sButtonBorder.GetLength() > 0)
          sAppStream << "q\n" << sButtonBorder << "Q\n";

        CFX_PointF ptCenter = CFX_PointF((rcButton.left + rcButton.right) / 2,
                                         (rcButton.top + rcButton.bottom) / 2);
        if (IsFloatBigger(rcButton.Width(), 6) &&
            IsFloatBigger(rcButton.Height(), 6)) {
          sAppStream << "q\n"
                     << " 0 g\n";
          sAppStream << ptCenter.x - 3 << " " << ptCenter.y + 1.5f << " m\n";
          sAppStream << ptCenter.x + 3 << " " << ptCenter.y + 1.5f << " l\n";
          sAppStream << ptCenter.x << " " << ptCenter.y - 1.5f << " l\n";
          sAppStream << ptCenter.x - 3 << " " << ptCenter.y + 1.5f << " l f\n";
          sAppStream << sButton << "Q\n";
        }
      }
    } break;
    case 2: {
      CPVT_FontMap map(
          pDoc, pStreamDict ? pStreamDict->GetDictFor("Resources") : nullptr,
          pDefFont, sFontName.Right(sFontName.GetLength() - 1));
      CPDF_VariableText::Provider prd(&map);
      CPDF_Array* pOpts = ToArray(FPDF_GetFieldAttr(pAnnotDict, "Opt"));
      CPDF_Array* pSels = ToArray(FPDF_GetFieldAttr(pAnnotDict, "I"));
      CPDF_Object* pTi = FPDF_GetFieldAttr(pAnnotDict, "TI");
      int32_t nTop = pTi ? pTi->GetInteger() : 0;
      CFX_ByteTextBuf sBody;
      if (pOpts) {
        FX_FLOAT fy = rcBody.top;
        for (size_t i = nTop, sz = pOpts->GetCount(); i < sz; i++) {
          if (IsFloatSmaller(fy, rcBody.bottom))
            break;

          if (CPDF_Object* pOpt = pOpts->GetDirectObjectAt(i)) {
            CFX_WideString swItem;
            if (pOpt->IsString())
              swItem = pOpt->GetUnicodeText();
            else if (CPDF_Array* pArray = pOpt->AsArray())
              swItem = pArray->GetDirectObjectAt(1)->GetUnicodeText();

            bool bSelected = false;
            if (pSels) {
              for (size_t s = 0, ssz = pSels->GetCount(); s < ssz; s++) {
                int value = pSels->GetIntegerAt(s);
                if (value >= 0 && i == static_cast<size_t>(value)) {
                  bSelected = true;
                  break;
                }
              }
            }
            CPDF_VariableText vt;
            vt.SetProvider(&prd);
            vt.SetPlateRect(
                CFX_FloatRect(rcBody.left, 0.0f, rcBody.right, 0.0f));
            vt.SetFontSize(IsFloatZero(fFontSize) ? 12.0f : fFontSize);

            vt.Initialize();
            vt.SetText(swItem);
            vt.RearrangeAll();
            FX_FLOAT fItemHeight = vt.GetContentRect().Height();
            if (bSelected) {
              CFX_FloatRect rcItem = CFX_FloatRect(
                  rcBody.left, fy - fItemHeight, rcBody.right, fy);
              sBody << "q\n"
                    << CPVT_GenerateAP::GenerateColorAP(
                           CPVT_Color(CPVT_Color::kRGB, 0, 51.0f / 255.0f,
                                      113.0f / 255.0f),
                           PaintOperation::FILL)
                    << rcItem.left << " " << rcItem.bottom << " "
                    << rcItem.Width() << " " << rcItem.Height() << " re f\n"
                    << "Q\n";
              sBody << "BT\n"
                    << CPVT_GenerateAP::GenerateColorAP(
                           CPVT_Color(CPVT_Color::kGray, 1),
                           PaintOperation::FILL)
                    << CPVT_GenerateAP::GenerateEditAP(&map, vt.GetIterator(),
                                                       CFX_PointF(0.0f, fy),
                                                       true, 0)
                    << "ET\n";
            } else {
              sBody << "BT\n"
                    << CPVT_GenerateAP::GenerateColorAP(crText,
                                                        PaintOperation::FILL)
                    << CPVT_GenerateAP::GenerateEditAP(&map, vt.GetIterator(),
                                                       CFX_PointF(0.0f, fy),
                                                       true, 0)
                    << "ET\n";
            }
            fy -= fItemHeight;
          }
        }
      }
      if (sBody.GetSize() > 0) {
        sAppStream << "/Tx BMC\nq\n"
                   << rcBody.left << " " << rcBody.bottom << " "
                   << rcBody.Width() << " " << rcBody.Height() << " re\nW\nn\n"
                   << sBody.AsStringC() << "Q\nEMC\n";
      }
    } break;
  }
  if (pNormalStream) {
    pNormalStream->SetData(sAppStream.GetBuffer(), sAppStream.GetSize());
    pStreamDict = pNormalStream->GetDict();
    if (pStreamDict) {
      pStreamDict->SetMatrixFor("Matrix", matrix);
      pStreamDict->SetRectFor("BBox", rcBBox);
      CPDF_Dictionary* pStreamResList = pStreamDict->GetDictFor("Resources");
      if (pStreamResList) {
        CPDF_Dictionary* pStreamResFontList =
            pStreamResList->GetDictFor("Font");
        if (!pStreamResFontList) {
          pStreamResFontList =
              pStreamResList->SetNewFor<CPDF_Dictionary>("Font");
        }
        if (!pStreamResFontList->KeyExist(sFontName)) {
          pStreamResFontList->SetNewFor<CPDF_Reference>(sFontName, pDoc,
                                                        pFontDict->GetObjNum());
        }
      } else {
        pStreamDict->SetFor("Resources", pFormDict->GetDictFor("DR")->Clone());
        pStreamResList = pStreamDict->GetDictFor("Resources");
      }
    }
  }
  return true;
}

CFX_ByteString GetColorStringWithDefault(CPDF_Array* pColor,
                                         const CPVT_Color& crDefaultColor,
                                         PaintOperation nOperation) {
  if (pColor) {
    CPVT_Color color = CPVT_Color::ParseColor(*pColor);
    return CPVT_GenerateAP::GenerateColorAP(color, nOperation);
  }

  return CPVT_GenerateAP::GenerateColorAP(crDefaultColor, nOperation);
}

FX_FLOAT GetBorderWidth(const CPDF_Dictionary& pAnnotDict) {
  if (CPDF_Dictionary* pBorderStyleDict = pAnnotDict.GetDictFor("BS")) {
    if (pBorderStyleDict->KeyExist("W"))
      return pBorderStyleDict->GetNumberFor("W");
  }

  if (CPDF_Array* pBorderArray = pAnnotDict.GetArrayFor("Border")) {
    if (pBorderArray->GetCount() > 2)
      return pBorderArray->GetNumberAt(2);
  }

  return 1;
}

CPDF_Array* GetDashArray(const CPDF_Dictionary& pAnnotDict) {
  if (CPDF_Dictionary* pBorderStyleDict = pAnnotDict.GetDictFor("BS")) {
    if (pBorderStyleDict->GetStringFor("S") == "D")
      return pBorderStyleDict->GetArrayFor("D");
  }

  if (CPDF_Array* pBorderArray = pAnnotDict.GetArrayFor("Border")) {
    if (pBorderArray->GetCount() == 4)
      return pBorderArray->GetArrayAt(3);
  }

  return nullptr;
}

CFX_ByteString GetDashPatternString(const CPDF_Dictionary& pAnnotDict) {
  CPDF_Array* pDashArray = GetDashArray(pAnnotDict);
  if (!pDashArray || pDashArray->IsEmpty())
    return CFX_ByteString();

  // Support maximum of ten elements in the dash array.
  size_t pDashArrayCount = std::min<size_t>(pDashArray->GetCount(), 10);
  CFX_ByteTextBuf sDashStream;

  sDashStream << "[";
  for (size_t i = 0; i < pDashArrayCount; ++i)
    sDashStream << pDashArray->GetNumberAt(i) << " ";
  sDashStream << "] 0 d\n";

  return sDashStream.MakeString();
}

CFX_ByteString GetPopupContentsString(CPDF_Document* pDoc,
                                      const CPDF_Dictionary& pAnnotDict,
                                      CPDF_Font* pDefFont,
                                      const CFX_ByteString& sFontName) {
  CFX_WideString swValue(pAnnotDict.GetUnicodeTextFor("T"));
  swValue += L'\n';
  swValue += pAnnotDict.GetUnicodeTextFor("Contents");
  CPVT_FontMap map(pDoc, nullptr, pDefFont, sFontName);

  CPDF_VariableText::Provider prd(&map);
  CPDF_VariableText vt;
  vt.SetProvider(&prd);
  vt.SetPlateRect(pAnnotDict.GetRectFor("Rect"));
  vt.SetFontSize(12);
  vt.SetAutoReturn(true);
  vt.SetMultiLine(true);

  vt.Initialize();
  vt.SetText(swValue);
  vt.RearrangeAll();
  CFX_PointF ptOffset(3.0f, -3.0f);
  CFX_ByteString sContent = CPVT_GenerateAP::GenerateEditAP(
      &map, vt.GetIterator(), ptOffset, false, 0);

  if (sContent.IsEmpty())
    return CFX_ByteString();

  CFX_ByteTextBuf sAppStream;
  sAppStream << "BT\n"
             << CPVT_GenerateAP::GenerateColorAP(
                    CPVT_Color(CPVT_Color::kRGB, 0, 0, 0), PaintOperation::FILL)
             << sContent << "ET\n"
             << "Q\n";
  return sAppStream.MakeString();
}

std::unique_ptr<CPDF_Dictionary> GenerateExtGStateDict(
    const CPDF_Dictionary& pAnnotDict,
    const CFX_ByteString& sExtGSDictName,
    const CFX_ByteString& sBlendMode) {
  auto pGSDict =
      pdfium::MakeUnique<CPDF_Dictionary>(pAnnotDict.GetByteStringPool());
  pGSDict->SetNewFor<CPDF_String>("Type", "ExtGState", false);

  FX_FLOAT fOpacity =
      pAnnotDict.KeyExist("CA") ? pAnnotDict.GetNumberFor("CA") : 1;
  pGSDict->SetNewFor<CPDF_Number>("CA", fOpacity);
  pGSDict->SetNewFor<CPDF_Number>("ca", fOpacity);
  pGSDict->SetNewFor<CPDF_Boolean>("AIS", false);
  pGSDict->SetNewFor<CPDF_String>("BM", sBlendMode, false);

  auto pExtGStateDict =
      pdfium::MakeUnique<CPDF_Dictionary>(pAnnotDict.GetByteStringPool());
  pExtGStateDict->SetFor(sExtGSDictName, std::move(pGSDict));
  return pExtGStateDict;
}

std::unique_ptr<CPDF_Dictionary> GenerateResourceFontDict(
    CPDF_Document* pDoc,
    const CFX_ByteString& sFontDictName) {
  CPDF_Dictionary* pFontDict = pDoc->NewIndirect<CPDF_Dictionary>();
  pFontDict->SetNewFor<CPDF_Name>("Type", "Font");
  pFontDict->SetNewFor<CPDF_Name>("Subtype", "Type1");
  pFontDict->SetNewFor<CPDF_Name>("BaseFont", "Helvetica");
  pFontDict->SetNewFor<CPDF_Name>("Encoding", "WinAnsiEncoding");

  auto pResourceFontDict =
      pdfium::MakeUnique<CPDF_Dictionary>(pDoc->GetByteStringPool());
  pResourceFontDict->SetNewFor<CPDF_Reference>(sFontDictName, pDoc,
                                               pFontDict->GetObjNum());
  return pResourceFontDict;
}

std::unique_ptr<CPDF_Dictionary> GenerateResourceDict(
    CPDF_Document* pDoc,
    std::unique_ptr<CPDF_Dictionary> pExtGStateDict,
    std::unique_ptr<CPDF_Dictionary> pResourceFontDict) {
  auto pResourceDict =
      pdfium::MakeUnique<CPDF_Dictionary>(pDoc->GetByteStringPool());
  if (pExtGStateDict)
    pResourceDict->SetFor("ExtGState", std::move(pExtGStateDict));
  if (pResourceFontDict)
    pResourceDict->SetFor("Font", std::move(pResourceFontDict));
  return pResourceDict;
}

void GenerateAndSetAPDict(CPDF_Document* pDoc,
                          CPDF_Dictionary* pAnnotDict,
                          const CFX_ByteTextBuf& sAppStream,
                          std::unique_ptr<CPDF_Dictionary> pResourceDict,
                          bool bIsTextMarkupAnnotation) {
  CPDF_Stream* pNormalStream = pDoc->NewIndirect<CPDF_Stream>();
  pNormalStream->SetData(sAppStream.GetBuffer(), sAppStream.GetSize());

  CPDF_Dictionary* pAPDict = pAnnotDict->SetNewFor<CPDF_Dictionary>("AP");
  pAPDict->SetNewFor<CPDF_Reference>("N", pDoc, pNormalStream->GetObjNum());

  CPDF_Dictionary* pStreamDict = pNormalStream->GetDict();
  pStreamDict->SetNewFor<CPDF_Number>("FormType", 1);
  pStreamDict->SetNewFor<CPDF_String>("Subtype", "Form", false);
  pStreamDict->SetMatrixFor("Matrix", CFX_Matrix());

  CFX_FloatRect rect = bIsTextMarkupAnnotation
                           ? CPDF_Annot::RectFromQuadPoints(pAnnotDict)
                           : pAnnotDict->GetRectFor("Rect");
  pStreamDict->SetRectFor("BBox", rect);
  pStreamDict->SetFor("Resources", std::move(pResourceDict));
}

CFX_ByteString GetPaintOperatorString(bool bIsStrokeRect, bool bIsFillRect) {
  if (bIsStrokeRect)
    return bIsFillRect ? "b" : "s";
  return bIsFillRect ? "f" : "n";
}

CFX_ByteString GenerateTextSymbolAP(const CFX_FloatRect& rect) {
  CFX_ByteTextBuf sAppStream;
  sAppStream << CPVT_GenerateAP::GenerateColorAP(
      CPVT_Color(CPVT_Color::kRGB, 1, 1, 0), PaintOperation::FILL);
  sAppStream << CPVT_GenerateAP::GenerateColorAP(
      CPVT_Color(CPVT_Color::kRGB, 0, 0, 0), PaintOperation::STROKE);

  const FX_FLOAT fBorderWidth = 1;
  sAppStream << fBorderWidth << " w\n";

  const FX_FLOAT fHalfWidth = fBorderWidth / 2;
  const FX_FLOAT fTipDelta = 4;

  CFX_FloatRect outerRect1 = rect;
  outerRect1.Deflate(fHalfWidth, fHalfWidth);
  outerRect1.bottom += fTipDelta;

  CFX_FloatRect outerRect2 = outerRect1;
  outerRect2.left += fTipDelta;
  outerRect2.right = outerRect2.left + fTipDelta;
  outerRect2.top = outerRect2.bottom - fTipDelta;
  FX_FLOAT outerRect2Middle = (outerRect2.left + outerRect2.right) / 2;

  // Draw outer boxes.
  sAppStream << outerRect1.left << " " << outerRect1.bottom << " m\n"
             << outerRect1.left << " " << outerRect1.top << " l\n"
             << outerRect1.right << " " << outerRect1.top << " l\n"
             << outerRect1.right << " " << outerRect1.bottom << " l\n"
             << outerRect2.right << " " << outerRect2.bottom << " l\n"
             << outerRect2Middle << " " << outerRect2.top << " l\n"
             << outerRect2.left << " " << outerRect2.bottom << " l\n"
             << outerRect1.left << " " << outerRect1.bottom << " l\n";

  // Draw inner lines.
  CFX_FloatRect lineRect = outerRect1;
  const FX_FLOAT fXDelta = 2;
  const FX_FLOAT fYDelta = (lineRect.top - lineRect.bottom) / 4;

  lineRect.left += fXDelta;
  lineRect.right -= fXDelta;
  for (int i = 0; i < 3; ++i) {
    lineRect.top -= fYDelta;
    sAppStream << lineRect.left << " " << lineRect.top << " m\n"
               << lineRect.right << " " << lineRect.top << " l\n";
  }
  sAppStream << "B*\n";

  return sAppStream.MakeString();
}

}  // namespace

bool FPDF_GenerateAP(CPDF_Document* pDoc, CPDF_Dictionary* pAnnotDict) {
  if (!pAnnotDict || pAnnotDict->GetStringFor("Subtype") != "Widget")
    return false;

  CPDF_Object* pFieldTypeObj = FPDF_GetFieldAttr(pAnnotDict, "FT");
  if (!pFieldTypeObj)
    return false;

  CFX_ByteString field_type = pFieldTypeObj->GetString();
  if (field_type == "Tx")
    return CPVT_GenerateAP::GenerateTextFieldAP(pDoc, pAnnotDict);

  CPDF_Object* pFieldFlagsObj = FPDF_GetFieldAttr(pAnnotDict, "Ff");
  uint32_t flags = pFieldFlagsObj ? pFieldFlagsObj->GetInteger() : 0;
  if (field_type == "Ch") {
    return (flags & (1 << 17))
               ? CPVT_GenerateAP::GenerateComboBoxAP(pDoc, pAnnotDict)
               : CPVT_GenerateAP::GenerateListBoxAP(pDoc, pAnnotDict);
  }

  if (field_type == "Btn") {
    if (!(flags & (1 << 16))) {
      if (!pAnnotDict->KeyExist("AS")) {
        if (CPDF_Dictionary* pParentDict = pAnnotDict->GetDictFor("Parent")) {
          if (pParentDict->KeyExist("AS")) {
            pAnnotDict->SetNewFor<CPDF_String>(
                "AS", pParentDict->GetStringFor("AS"), false);
          }
        }
      }
    }
  }

  return false;
}

// Static.
bool CPVT_GenerateAP::GenerateComboBoxAP(CPDF_Document* pDoc,
                                         CPDF_Dictionary* pAnnotDict) {
  return GenerateWidgetAP(pDoc, pAnnotDict, 1);
}

// Static.
bool CPVT_GenerateAP::GenerateListBoxAP(CPDF_Document* pDoc,
                                        CPDF_Dictionary* pAnnotDict) {
  return GenerateWidgetAP(pDoc, pAnnotDict, 2);
}

// Static.
bool CPVT_GenerateAP::GenerateTextFieldAP(CPDF_Document* pDoc,
                                          CPDF_Dictionary* pAnnotDict) {
  return GenerateWidgetAP(pDoc, pAnnotDict, 0);
}

bool CPVT_GenerateAP::GenerateCircleAP(CPDF_Document* pDoc,
                                       CPDF_Dictionary* pAnnotDict) {
  CFX_ByteTextBuf sAppStream;
  CFX_ByteString sExtGSDictName = "GS";
  sAppStream << "/" << sExtGSDictName << " gs ";

  CPDF_Array* pInteriorColor = pAnnotDict->GetArrayFor("IC");
  sAppStream << GetColorStringWithDefault(pInteriorColor,
                                          CPVT_Color(CPVT_Color::kTransparent),
                                          PaintOperation::FILL);

  sAppStream << GetColorStringWithDefault(pAnnotDict->GetArrayFor("C"),
                                          CPVT_Color(CPVT_Color::kRGB, 0, 0, 0),
                                          PaintOperation::STROKE);

  FX_FLOAT fBorderWidth = GetBorderWidth(*pAnnotDict);
  bool bIsStrokeRect = fBorderWidth > 0;

  if (bIsStrokeRect) {
    sAppStream << fBorderWidth << " w ";
    sAppStream << GetDashPatternString(*pAnnotDict);
  }

  CFX_FloatRect rect = pAnnotDict->GetRectFor("Rect");
  rect.Normalize();

  if (bIsStrokeRect) {
    // Deflating rect because stroking a path entails painting all points whose
    // perpendicular distance from the path in user space is less than or equal
    // to half the line width.
    rect.Deflate(fBorderWidth / 2, fBorderWidth / 2);
  }

  const FX_FLOAT fMiddleX = (rect.left + rect.right) / 2;
  const FX_FLOAT fMiddleY = (rect.top + rect.bottom) / 2;

  // |fL| is precalculated approximate value of 4 * tan((3.14 / 2) / 4) / 3,
  // where |fL| * radius is a good approximation of control points for
  // arc with 90 degrees.
  const FX_FLOAT fL = 0.5523f;
  const FX_FLOAT fDeltaX = fL * rect.Width() / 2.0;
  const FX_FLOAT fDeltaY = fL * rect.Height() / 2.0;

  // Starting point
  sAppStream << fMiddleX << " " << rect.top << " m\n";
  // First Bezier Curve
  sAppStream << fMiddleX + fDeltaX << " " << rect.top << " " << rect.right
             << " " << fMiddleY + fDeltaY << " " << rect.right << " "
             << fMiddleY << " c\n";
  // Second Bezier Curve
  sAppStream << rect.right << " " << fMiddleY - fDeltaY << " "
             << fMiddleX + fDeltaX << " " << rect.bottom << " " << fMiddleX
             << " " << rect.bottom << " c\n";
  // Third Bezier Curve
  sAppStream << fMiddleX - fDeltaX << " " << rect.bottom << " " << rect.left
             << " " << fMiddleY - fDeltaY << " " << rect.left << " " << fMiddleY
             << " c\n";
  // Fourth Bezier Curve
  sAppStream << rect.left << " " << fMiddleY + fDeltaY << " "
             << fMiddleX - fDeltaX << " " << rect.top << " " << fMiddleX << " "
             << rect.top << " c\n";

  bool bIsFillRect = pInteriorColor && !pInteriorColor->IsEmpty();
  sAppStream << GetPaintOperatorString(bIsStrokeRect, bIsFillRect) << "\n";

  auto pExtGStateDict =
      GenerateExtGStateDict(*pAnnotDict, sExtGSDictName, "Normal");
  auto pResourceDict =
      GenerateResourceDict(pDoc, std::move(pExtGStateDict), nullptr);
  GenerateAndSetAPDict(pDoc, pAnnotDict, sAppStream, std::move(pResourceDict),
                       false /*IsTextMarkupAnnotation*/);
  return true;
}

bool CPVT_GenerateAP::GenerateHighlightAP(CPDF_Document* pDoc,
                                          CPDF_Dictionary* pAnnotDict) {
  CFX_ByteTextBuf sAppStream;
  CFX_ByteString sExtGSDictName = "GS";
  sAppStream << "/" << sExtGSDictName << " gs ";

  sAppStream << GetColorStringWithDefault(pAnnotDict->GetArrayFor("C"),
                                          CPVT_Color(CPVT_Color::kRGB, 1, 1, 0),
                                          PaintOperation::FILL);

  CFX_FloatRect rect = CPDF_Annot::RectFromQuadPoints(pAnnotDict);
  rect.Normalize();

  sAppStream << rect.left << " " << rect.top << " m " << rect.right << " "
             << rect.top << " l " << rect.right << " " << rect.bottom << " l "
             << rect.left << " " << rect.bottom << " l "
             << "h f\n";

  auto pExtGStateDict =
      GenerateExtGStateDict(*pAnnotDict, sExtGSDictName, "Multiply");
  auto pResourceDict =
      GenerateResourceDict(pDoc, std::move(pExtGStateDict), nullptr);
  GenerateAndSetAPDict(pDoc, pAnnotDict, sAppStream, std::move(pResourceDict),
                       true /*IsTextMarkupAnnotation*/);

  return true;
}

bool CPVT_GenerateAP::GenerateInkAP(CPDF_Document* pDoc,
                                    CPDF_Dictionary* pAnnotDict) {
  FX_FLOAT fBorderWidth = GetBorderWidth(*pAnnotDict);
  bool bIsStroke = fBorderWidth > 0;

  if (!bIsStroke)
    return false;

  CPDF_Array* pInkList = pAnnotDict->GetArrayFor("InkList");
  if (!pInkList || pInkList->IsEmpty())
    return false;

  CFX_ByteTextBuf sAppStream;
  CFX_ByteString sExtGSDictName = "GS";
  sAppStream << "/" << sExtGSDictName << " gs ";

  sAppStream << GetColorStringWithDefault(pAnnotDict->GetArrayFor("C"),
                                          CPVT_Color(CPVT_Color::kRGB, 0, 0, 0),
                                          PaintOperation::STROKE);

  sAppStream << fBorderWidth << " w ";
  sAppStream << GetDashPatternString(*pAnnotDict);

  // Set inflated rect as a new rect because paths near the border with large
  // width should not be clipped to the original rect.
  CFX_FloatRect rect = pAnnotDict->GetRectFor("Rect");
  rect.Inflate(fBorderWidth / 2, fBorderWidth / 2);
  pAnnotDict->SetRectFor("Rect", rect);

  for (size_t i = 0; i < pInkList->GetCount(); i++) {
    CPDF_Array* pInkCoordList = pInkList->GetArrayAt(i);
    if (!pInkCoordList || pInkCoordList->GetCount() < 2)
      continue;

    sAppStream << pInkCoordList->GetNumberAt(0) << " "
               << pInkCoordList->GetNumberAt(1) << " m ";

    for (size_t j = 0; j < pInkCoordList->GetCount() - 1; j += 2) {
      sAppStream << pInkCoordList->GetNumberAt(j) << " "
                 << pInkCoordList->GetNumberAt(j + 1) << " l ";
    }

    sAppStream << "S\n";
  }

  auto pExtGStateDict =
      GenerateExtGStateDict(*pAnnotDict, sExtGSDictName, "Normal");
  auto pResourceDict =
      GenerateResourceDict(pDoc, std::move(pExtGStateDict), nullptr);
  GenerateAndSetAPDict(pDoc, pAnnotDict, sAppStream, std::move(pResourceDict),
                       false /*IsTextMarkupAnnotation*/);
  return true;
}

bool CPVT_GenerateAP::GenerateTextAP(CPDF_Document* pDoc,
                                     CPDF_Dictionary* pAnnotDict) {
  CFX_ByteTextBuf sAppStream;
  CFX_ByteString sExtGSDictName = "GS";
  sAppStream << "/" << sExtGSDictName << " gs ";

  CFX_FloatRect rect = pAnnotDict->GetRectFor("Rect");
  const FX_FLOAT fNoteLength = 20;
  CFX_FloatRect noteRect(rect.left, rect.bottom, rect.left + fNoteLength,
                         rect.bottom + fNoteLength);
  pAnnotDict->SetRectFor("Rect", noteRect);

  sAppStream << GenerateTextSymbolAP(noteRect);

  auto pExtGStateDict =
      GenerateExtGStateDict(*pAnnotDict, sExtGSDictName, "Normal");
  auto pResourceDict =
      GenerateResourceDict(pDoc, std::move(pExtGStateDict), nullptr);
  GenerateAndSetAPDict(pDoc, pAnnotDict, sAppStream, std::move(pResourceDict),
                       false /*IsTextMarkupAnnotation*/);
  return true;
}

bool CPVT_GenerateAP::GenerateUnderlineAP(CPDF_Document* pDoc,
                                          CPDF_Dictionary* pAnnotDict) {
  CFX_ByteTextBuf sAppStream;
  CFX_ByteString sExtGSDictName = "GS";
  sAppStream << "/" << sExtGSDictName << " gs ";

  sAppStream << GetColorStringWithDefault(pAnnotDict->GetArrayFor("C"),
                                          CPVT_Color(CPVT_Color::kRGB, 0, 0, 0),
                                          PaintOperation::STROKE);

  CFX_FloatRect rect = CPDF_Annot::RectFromQuadPoints(pAnnotDict);
  rect.Normalize();

  FX_FLOAT fLineWidth = 1.0;
  sAppStream << fLineWidth << " w " << rect.left << " "
             << rect.bottom + fLineWidth << " m " << rect.right << " "
             << rect.bottom + fLineWidth << " l S\n";

  auto pExtGStateDict =
      GenerateExtGStateDict(*pAnnotDict, sExtGSDictName, "Normal");
  auto pResourceDict =
      GenerateResourceDict(pDoc, std::move(pExtGStateDict), nullptr);
  GenerateAndSetAPDict(pDoc, pAnnotDict, sAppStream, std::move(pResourceDict),
                       true /*IsTextMarkupAnnotation*/);
  return true;
}

bool CPVT_GenerateAP::GeneratePopupAP(CPDF_Document* pDoc,
                                      CPDF_Dictionary* pAnnotDict) {
  CFX_ByteTextBuf sAppStream;
  CFX_ByteString sExtGSDictName = "GS";
  sAppStream << "/" << sExtGSDictName << " gs\n";

  sAppStream << GenerateColorAP(CPVT_Color(CPVT_Color::kRGB, 1, 1, 0),
                                PaintOperation::FILL);
  sAppStream << GenerateColorAP(CPVT_Color(CPVT_Color::kRGB, 0, 0, 0),
                                PaintOperation::STROKE);

  const FX_FLOAT fBorderWidth = 1;
  sAppStream << fBorderWidth << " w\n";

  CFX_FloatRect rect = pAnnotDict->GetRectFor("Rect");
  rect.Normalize();
  rect.Deflate(fBorderWidth / 2, fBorderWidth / 2);

  sAppStream << rect.left << " " << rect.bottom << " " << rect.Width() << " "
             << rect.Height() << " re b\n";

  CFX_ByteString sFontName = "FONT";
  auto pResourceFontDict = GenerateResourceFontDict(pDoc, sFontName);
  CPDF_Font* pDefFont = pDoc->LoadFont(pResourceFontDict.get());
  if (!pDefFont)
    return false;

  auto pExtGStateDict =
      GenerateExtGStateDict(*pAnnotDict, sExtGSDictName, "Normal");
  auto pResourceDict = GenerateResourceDict(pDoc, std::move(pResourceFontDict),
                                            std::move(pExtGStateDict));

  sAppStream << GetPopupContentsString(pDoc, *pAnnotDict, pDefFont, sFontName);
  GenerateAndSetAPDict(pDoc, pAnnotDict, sAppStream, std::move(pResourceDict),
                       false /*IsTextMarkupAnnotation*/);
  return true;
}

bool CPVT_GenerateAP::GenerateSquareAP(CPDF_Document* pDoc,
                                       CPDF_Dictionary* pAnnotDict) {
  CFX_ByteTextBuf sAppStream;
  CFX_ByteString sExtGSDictName = "GS";
  sAppStream << "/" << sExtGSDictName << " gs ";

  CPDF_Array* pInteriorColor = pAnnotDict->GetArrayFor("IC");
  sAppStream << GetColorStringWithDefault(pInteriorColor,
                                          CPVT_Color(CPVT_Color::kTransparent),
                                          PaintOperation::FILL);

  sAppStream << GetColorStringWithDefault(pAnnotDict->GetArrayFor("C"),
                                          CPVT_Color(CPVT_Color::kRGB, 0, 0, 0),
                                          PaintOperation::STROKE);

  FX_FLOAT fBorderWidth = GetBorderWidth(*pAnnotDict);
  bool bIsStrokeRect = fBorderWidth > 0;

  if (bIsStrokeRect) {
    sAppStream << fBorderWidth << " w ";
    sAppStream << GetDashPatternString(*pAnnotDict);
  }

  CFX_FloatRect rect = pAnnotDict->GetRectFor("Rect");
  rect.Normalize();

  if (bIsStrokeRect) {
    // Deflating rect because stroking a path entails painting all points whose
    // perpendicular distance from the path in user space is less than or equal
    // to half the line width.
    rect.Deflate(fBorderWidth / 2, fBorderWidth / 2);
  }

  bool bIsFillRect = pInteriorColor && (pInteriorColor->GetCount() > 0);

  sAppStream << rect.left << " " << rect.bottom << " " << rect.Width() << " "
             << rect.Height() << " re "
             << GetPaintOperatorString(bIsStrokeRect, bIsFillRect) << "\n";

  auto pExtGStateDict =
      GenerateExtGStateDict(*pAnnotDict, sExtGSDictName, "Normal");
  auto pResourceDict =
      GenerateResourceDict(pDoc, std::move(pExtGStateDict), nullptr);
  GenerateAndSetAPDict(pDoc, pAnnotDict, sAppStream, std::move(pResourceDict),
                       false /*IsTextMarkupAnnotation*/);
  return true;
}

bool CPVT_GenerateAP::GenerateSquigglyAP(CPDF_Document* pDoc,
                                         CPDF_Dictionary* pAnnotDict) {
  CFX_ByteTextBuf sAppStream;
  CFX_ByteString sExtGSDictName = "GS";
  sAppStream << "/" << sExtGSDictName << " gs ";

  sAppStream << GetColorStringWithDefault(pAnnotDict->GetArrayFor("C"),
                                          CPVT_Color(CPVT_Color::kRGB, 0, 0, 0),
                                          PaintOperation::STROKE);

  CFX_FloatRect rect = CPDF_Annot::RectFromQuadPoints(pAnnotDict);
  rect.Normalize();

  FX_FLOAT fLineWidth = 1.0;
  sAppStream << fLineWidth << " w ";

  const FX_FLOAT fDelta = 2.0;
  const FX_FLOAT fTop = rect.bottom + fDelta;
  const FX_FLOAT fBottom = rect.bottom;

  sAppStream << rect.left << " " << fTop << " m ";

  FX_FLOAT fX = rect.left + fDelta;
  bool isUpwards = false;

  while (fX < rect.right) {
    sAppStream << fX << " " << (isUpwards ? fTop : fBottom) << " l ";

    fX += fDelta;
    isUpwards = !isUpwards;
  }

  FX_FLOAT fRemainder = rect.right - (fX - fDelta);
  if (isUpwards)
    sAppStream << rect.right << " " << fBottom + fRemainder << " l ";
  else
    sAppStream << rect.right << " " << fTop - fRemainder << " l ";

  sAppStream << "S\n";

  auto pExtGStateDict =
      GenerateExtGStateDict(*pAnnotDict, sExtGSDictName, "Normal");
  auto pResourceDict =
      GenerateResourceDict(pDoc, std::move(pExtGStateDict), nullptr);
  GenerateAndSetAPDict(pDoc, pAnnotDict, sAppStream, std::move(pResourceDict),
                       true /*IsTextMarkupAnnotation*/);
  return true;
}

bool CPVT_GenerateAP::GenerateStrikeOutAP(CPDF_Document* pDoc,
                                          CPDF_Dictionary* pAnnotDict) {
  CFX_ByteTextBuf sAppStream;
  CFX_ByteString sExtGSDictName = "GS";
  sAppStream << "/" << sExtGSDictName << " gs ";

  sAppStream << GetColorStringWithDefault(pAnnotDict->GetArrayFor("C"),
                                          CPVT_Color(CPVT_Color::kRGB, 0, 0, 0),
                                          PaintOperation::STROKE);

  CFX_FloatRect rect = CPDF_Annot::RectFromQuadPoints(pAnnotDict);
  rect.Normalize();

  FX_FLOAT fLineWidth = 1.0;
  FX_FLOAT fY = (rect.top + rect.bottom) / 2;
  sAppStream << fLineWidth << " w " << rect.left << " " << fY << " m "
             << rect.right << " " << fY << " l S\n";

  auto pExtGStateDict =
      GenerateExtGStateDict(*pAnnotDict, sExtGSDictName, "Normal");
  auto pResourceDict =
      GenerateResourceDict(pDoc, std::move(pExtGStateDict), nullptr);
  GenerateAndSetAPDict(pDoc, pAnnotDict, sAppStream, std::move(pResourceDict),
                       true /*IsTextMarkupAnnotation*/);
  return true;
}

// Static.
CFX_ByteString CPVT_GenerateAP::GenerateEditAP(
    IPVT_FontMap* pFontMap,
    CPDF_VariableText::Iterator* pIterator,
    const CFX_PointF& ptOffset,
    bool bContinuous,
    uint16_t SubWord) {
  CFX_ByteTextBuf sEditStream;
  CFX_ByteTextBuf sLineStream;
  CFX_ByteTextBuf sWords;
  CFX_PointF ptOld;
  CFX_PointF ptNew;
  int32_t nCurFontIndex = -1;
  CPVT_WordPlace oldplace;

  pIterator->SetAt(0);
  while (pIterator->NextWord()) {
    CPVT_WordPlace place = pIterator->GetAt();
    if (bContinuous) {
      if (place.LineCmp(oldplace) != 0) {
        if (sWords.GetSize() > 0) {
          sLineStream << GetWordRenderString(sWords.MakeString());
          sEditStream << sLineStream;
          sLineStream.Clear();
          sWords.Clear();
        }
        CPVT_Word word;
        if (pIterator->GetWord(word)) {
          ptNew = CFX_PointF(word.ptWord.x + ptOffset.x,
                             word.ptWord.y + ptOffset.y);
        } else {
          CPVT_Line line;
          pIterator->GetLine(line);
          ptNew = CFX_PointF(line.ptLine.x + ptOffset.x,
                             line.ptLine.y + ptOffset.y);
        }
        if (ptNew != ptOld) {
          sLineStream << ptNew.x - ptOld.x << " " << ptNew.y - ptOld.y
                      << " Td\n";
          ptOld = ptNew;
        }
      }
      CPVT_Word word;
      if (pIterator->GetWord(word)) {
        if (word.nFontIndex != nCurFontIndex) {
          if (sWords.GetSize() > 0) {
            sLineStream << GetWordRenderString(sWords.MakeString());
            sWords.Clear();
          }
          sLineStream << GetFontSetString(pFontMap, word.nFontIndex,
                                          word.fFontSize);
          nCurFontIndex = word.nFontIndex;
        }
        sWords << GetPDFWordString(pFontMap, nCurFontIndex, word.Word, SubWord);
      }
      oldplace = place;
    } else {
      CPVT_Word word;
      if (pIterator->GetWord(word)) {
        ptNew =
            CFX_PointF(word.ptWord.x + ptOffset.x, word.ptWord.y + ptOffset.y);
        if (ptNew != ptOld) {
          sEditStream << ptNew.x - ptOld.x << " " << ptNew.y - ptOld.y
                      << " Td\n";
          ptOld = ptNew;
        }
        if (word.nFontIndex != nCurFontIndex) {
          sEditStream << GetFontSetString(pFontMap, word.nFontIndex,
                                          word.fFontSize);
          nCurFontIndex = word.nFontIndex;
        }
        sEditStream << GetWordRenderString(
            GetPDFWordString(pFontMap, nCurFontIndex, word.Word, SubWord));
      }
    }
  }
  if (sWords.GetSize() > 0) {
    sLineStream << GetWordRenderString(sWords.MakeString());
    sEditStream << sLineStream;
    sWords.Clear();
  }
  return sEditStream.MakeString();
}

// Static.
CFX_ByteString CPVT_GenerateAP::GenerateBorderAP(
    const CFX_FloatRect& rect,
    FX_FLOAT fWidth,
    const CPVT_Color& color,
    const CPVT_Color& crLeftTop,
    const CPVT_Color& crRightBottom,
    BorderStyle nStyle,
    const CPVT_Dash& dash) {
  CFX_ByteTextBuf sAppStream;
  CFX_ByteString sColor;
  FX_FLOAT fLeft = rect.left;
  FX_FLOAT fRight = rect.right;
  FX_FLOAT fTop = rect.top;
  FX_FLOAT fBottom = rect.bottom;
  if (fWidth > 0.0f) {
    FX_FLOAT fHalfWidth = fWidth / 2.0f;
    switch (nStyle) {
      default:
      case BorderStyle::SOLID:
        sColor = GenerateColorAP(color, PaintOperation::FILL);
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
        sColor = GenerateColorAP(color, PaintOperation::STROKE);
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
        sColor = GenerateColorAP(crLeftTop, PaintOperation::FILL);
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
        sColor = GenerateColorAP(crRightBottom, PaintOperation::FILL);
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
        sColor = GenerateColorAP(color, PaintOperation::FILL);
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
        sColor = GenerateColorAP(color, PaintOperation::STROKE);
        if (sColor.GetLength() > 0) {
          sAppStream << sColor;
          sAppStream << fWidth << " w\n";
          sAppStream << fLeft << " " << fBottom + fWidth / 2 << " m\n";
          sAppStream << fRight << " " << fBottom + fWidth / 2 << " l S\n";
        }
        break;
    }
  }
  return sAppStream.MakeString();
}

// Static.
CFX_ByteString CPVT_GenerateAP::GenerateColorAP(const CPVT_Color& color,
                                                PaintOperation nOperation) {
  CFX_ByteTextBuf sColorStream;
  switch (color.nColorType) {
    case CPVT_Color::kRGB:
      sColorStream << color.fColor1 << " " << color.fColor2 << " "
                   << color.fColor3 << " "
                   << (nOperation == PaintOperation::STROKE ? "RG" : "rg")
                   << "\n";
      break;
    case CPVT_Color::kGray:
      sColorStream << color.fColor1 << " "
                   << (nOperation == PaintOperation::STROKE ? "G" : "g")
                   << "\n";
      break;
    case CPVT_Color::kCMYK:
      sColorStream << color.fColor1 << " " << color.fColor2 << " "
                   << color.fColor3 << " " << color.fColor4 << " "
                   << (nOperation == PaintOperation::STROKE ? "K" : "k")
                   << "\n";
      break;
    case CPVT_Color::kTransparent:
      break;
  }
  return sColorStream.MakeString();
}

// Static.
CFX_ByteString CPVT_GenerateAP::GetPDFWordString(IPVT_FontMap* pFontMap,
                                                 int32_t nFontIndex,
                                                 uint16_t Word,
                                                 uint16_t SubWord) {
  CFX_ByteString sWord;
  if (SubWord > 0) {
    sWord.Format("%c", SubWord);
    return sWord;
  }

  if (!pFontMap)
    return sWord;

  if (CPDF_Font* pPDFFont = pFontMap->GetPDFFont(nFontIndex)) {
    if (pPDFFont->GetBaseFont().Compare("Symbol") == 0 ||
        pPDFFont->GetBaseFont().Compare("ZapfDingbats") == 0) {
      sWord.Format("%c", Word);
    } else {
      uint32_t dwCharCode = pPDFFont->CharCodeFromUnicode(Word);
      if (dwCharCode != CPDF_Font::kInvalidCharCode)
        pPDFFont->AppendChar(sWord, dwCharCode);
    }
  }
  return sWord;
}

// Static.
CFX_ByteString CPVT_GenerateAP::GetWordRenderString(
    const CFX_ByteString& strWords) {
  if (strWords.GetLength() > 0)
    return PDF_EncodeString(strWords) + " Tj\n";
  return "";
}

// Static.
CFX_ByteString CPVT_GenerateAP::GetFontSetString(IPVT_FontMap* pFontMap,
                                                 int32_t nFontIndex,
                                                 FX_FLOAT fFontSize) {
  CFX_ByteTextBuf sRet;
  if (pFontMap) {
    CFX_ByteString sFontAlias = pFontMap->GetPDFFontAlias(nFontIndex);
    if (sFontAlias.GetLength() > 0 && fFontSize > 0)
      sRet << "/" << sFontAlias << " " << fFontSize << " Tf\n";
  }
  return sRet.MakeString();
}
