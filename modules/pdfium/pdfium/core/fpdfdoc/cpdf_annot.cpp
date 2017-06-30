// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_annot.h"

#include <utility>

#include "core/fpdfapi/page/cpdf_form.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_boolean.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/render/cpdf_rendercontext.h"
#include "core/fpdfapi/render/cpdf_renderoptions.h"
#include "core/fpdfdoc/cpvt_generateap.h"
#include "core/fxcrt/fx_memory.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"
#include "third_party/base/ptr_util.h"

namespace {

char kPDFiumKey_HasGeneratedAP[] = "PDFIUM_HasGeneratedAP";

bool IsTextMarkupAnnotation(CPDF_Annot::Subtype type) {
  return type == CPDF_Annot::Subtype::HIGHLIGHT ||
         type == CPDF_Annot::Subtype::SQUIGGLY ||
         type == CPDF_Annot::Subtype::STRIKEOUT ||
         type == CPDF_Annot::Subtype::UNDERLINE;
}

bool ShouldGenerateAPForAnnotation(CPDF_Dictionary* pAnnotDict) {
  // If AP dictionary exists, we use the appearance defined in the
  // existing AP dictionary.
  if (pAnnotDict->KeyExist("AP"))
    return false;

  return !CPDF_Annot::IsAnnotationHidden(pAnnotDict);
}

CPDF_Form* AnnotGetMatrix(const CPDF_Page* pPage,
                          CPDF_Annot* pAnnot,
                          CPDF_Annot::AppearanceMode mode,
                          const CFX_Matrix* pUser2Device,
                          CFX_Matrix* matrix) {
  CPDF_Form* pForm = pAnnot->GetAPForm(pPage, mode);
  if (!pForm)
    return nullptr;

  CFX_FloatRect form_bbox = pForm->m_pFormDict->GetRectFor("BBox");
  CFX_Matrix form_matrix = pForm->m_pFormDict->GetMatrixFor("Matrix");
  form_matrix.TransformRect(form_bbox);
  matrix->MatchRect(pAnnot->GetRect(), form_bbox);
  matrix->Concat(*pUser2Device);
  return pForm;
}

}  // namespace

CPDF_Annot::CPDF_Annot(std::unique_ptr<CPDF_Dictionary> pDict,
                       CPDF_Document* pDocument)
    : m_pAnnotDict(std::move(pDict)), m_pDocument(pDocument) {
  Init();
}

CPDF_Annot::CPDF_Annot(CPDF_Dictionary* pDict, CPDF_Document* pDocument)
    : m_pAnnotDict(pDict), m_pDocument(pDocument) {
  Init();
}

CPDF_Annot::~CPDF_Annot() {
  ClearCachedAP();
}

void CPDF_Annot::Init() {
  m_nSubtype = StringToAnnotSubtype(m_pAnnotDict->GetStringFor("Subtype"));
  m_bIsTextMarkupAnnotation = IsTextMarkupAnnotation(m_nSubtype);
  m_bHasGeneratedAP = m_pAnnotDict->GetBooleanFor(kPDFiumKey_HasGeneratedAP);
  GenerateAPIfNeeded();
}

void CPDF_Annot::GenerateAPIfNeeded() {
  if (!ShouldGenerateAPForAnnotation(m_pAnnotDict.Get()))
    return;

  CPDF_Dictionary* pDict = m_pAnnotDict.Get();
  bool result = false;
  if (m_nSubtype == CPDF_Annot::Subtype::CIRCLE)
    result = CPVT_GenerateAP::GenerateCircleAP(m_pDocument, pDict);
  else if (m_nSubtype == CPDF_Annot::Subtype::HIGHLIGHT)
    result = CPVT_GenerateAP::GenerateHighlightAP(m_pDocument, pDict);
  else if (m_nSubtype == CPDF_Annot::Subtype::INK)
    result = CPVT_GenerateAP::GenerateInkAP(m_pDocument, pDict);
  else if (m_nSubtype == CPDF_Annot::Subtype::POPUP)
    result = CPVT_GenerateAP::GeneratePopupAP(m_pDocument, pDict);
  else if (m_nSubtype == CPDF_Annot::Subtype::SQUARE)
    result = CPVT_GenerateAP::GenerateSquareAP(m_pDocument, pDict);
  else if (m_nSubtype == CPDF_Annot::Subtype::SQUIGGLY)
    result = CPVT_GenerateAP::GenerateSquigglyAP(m_pDocument, pDict);
  else if (m_nSubtype == CPDF_Annot::Subtype::STRIKEOUT)
    result = CPVT_GenerateAP::GenerateStrikeOutAP(m_pDocument, pDict);
  else if (m_nSubtype == CPDF_Annot::Subtype::TEXT)
    result = CPVT_GenerateAP::GenerateTextAP(m_pDocument, pDict);
  else if (m_nSubtype == CPDF_Annot::Subtype::UNDERLINE)
    result = CPVT_GenerateAP::GenerateUnderlineAP(m_pDocument, pDict);

  if (result) {
    m_pAnnotDict->SetNewFor<CPDF_Boolean>(kPDFiumKey_HasGeneratedAP, result);
    m_bHasGeneratedAP = result;
  }
}

bool CPDF_Annot::ShouldDrawAnnotation() {
  if (IsAnnotationHidden(m_pAnnotDict.Get()))
    return false;

  if (m_nSubtype == CPDF_Annot::Subtype::POPUP && !m_bOpenState)
    return false;

  return true;
}

void CPDF_Annot::ClearCachedAP() {
  m_APMap.clear();
}

CPDF_Annot::Subtype CPDF_Annot::GetSubtype() const {
  return m_nSubtype;
}

CFX_FloatRect CPDF_Annot::RectForDrawing() const {
  if (!m_pAnnotDict)
    return CFX_FloatRect();

  bool bShouldUseQuadPointsCoords =
      m_bIsTextMarkupAnnotation && m_bHasGeneratedAP;
  if (bShouldUseQuadPointsCoords)
    return RectFromQuadPoints(m_pAnnotDict.Get());

  return m_pAnnotDict->GetRectFor("Rect");
}

CFX_FloatRect CPDF_Annot::GetRect() const {
  if (!m_pAnnotDict)
    return CFX_FloatRect();

  CFX_FloatRect rect = RectForDrawing();
  rect.Normalize();
  return rect;
}

uint32_t CPDF_Annot::GetFlags() const {
  return m_pAnnotDict->GetIntegerFor("F");
}

CPDF_Stream* FPDFDOC_GetAnnotAP(CPDF_Dictionary* pAnnotDict,
                                CPDF_Annot::AppearanceMode mode) {
  CPDF_Dictionary* pAP = pAnnotDict->GetDictFor("AP");
  if (!pAP) {
    return nullptr;
  }
  const FX_CHAR* ap_entry = "N";
  if (mode == CPDF_Annot::Down)
    ap_entry = "D";
  else if (mode == CPDF_Annot::Rollover)
    ap_entry = "R";
  if (!pAP->KeyExist(ap_entry))
    ap_entry = "N";

  CPDF_Object* psub = pAP->GetDirectObjectFor(ap_entry);
  if (!psub)
    return nullptr;
  if (CPDF_Stream* pStream = psub->AsStream())
    return pStream;

  if (CPDF_Dictionary* pDict = psub->AsDictionary()) {
    CFX_ByteString as = pAnnotDict->GetStringFor("AS");
    if (as.IsEmpty()) {
      CFX_ByteString value = pAnnotDict->GetStringFor("V");
      if (value.IsEmpty()) {
        CPDF_Dictionary* pParentDict = pAnnotDict->GetDictFor("Parent");
        value = pParentDict ? pParentDict->GetStringFor("V") : CFX_ByteString();
      }
      if (value.IsEmpty() || !pDict->KeyExist(value))
        as = "Off";
      else
        as = value;
    }
    return pDict->GetStreamFor(as);
  }
  return nullptr;
}

CPDF_Form* CPDF_Annot::GetAPForm(const CPDF_Page* pPage, AppearanceMode mode) {
  CPDF_Stream* pStream = FPDFDOC_GetAnnotAP(m_pAnnotDict.Get(), mode);
  if (!pStream)
    return nullptr;

  auto it = m_APMap.find(pStream);
  if (it != m_APMap.end())
    return it->second.get();

  auto pNewForm =
      pdfium::MakeUnique<CPDF_Form>(m_pDocument, pPage->m_pResources, pStream);
  pNewForm->ParseContent(nullptr, nullptr, nullptr);

  CPDF_Form* pResult = pNewForm.get();
  m_APMap[pStream] = std::move(pNewForm);
  return pResult;
}

// Static.
CFX_FloatRect CPDF_Annot::RectFromQuadPoints(CPDF_Dictionary* pAnnotDict) {
  CPDF_Array* pArray = pAnnotDict->GetArrayFor("QuadPoints");
  if (!pArray)
    return CFX_FloatRect();

  // QuadPoints are defined with 4 pairs of numbers
  // ([ pair0, pair1, pair2, pair3 ]), where
  // pair0 = top_left
  // pair1 = top_right
  // pair2 = bottom_left
  // pair3 = bottom_right
  //
  // On the other hand, /Rect is define as 2 pairs [pair0, pair1] where:
  // pair0 = bottom_left
  // pair1 = top_right.
  return CFX_FloatRect(pArray->GetNumberAt(4), pArray->GetNumberAt(5),
                       pArray->GetNumberAt(2), pArray->GetNumberAt(3));
}

// Static.
bool CPDF_Annot::IsAnnotationHidden(CPDF_Dictionary* pAnnotDict) {
  return !!(pAnnotDict->GetIntegerFor("F") & ANNOTFLAG_HIDDEN);
}

// Static.
CPDF_Annot::Subtype CPDF_Annot::StringToAnnotSubtype(
    const CFX_ByteString& sSubtype) {
  if (sSubtype == "Text")
    return CPDF_Annot::Subtype::TEXT;
  if (sSubtype == "Link")
    return CPDF_Annot::Subtype::LINK;
  if (sSubtype == "FreeText")
    return CPDF_Annot::Subtype::FREETEXT;
  if (sSubtype == "Line")
    return CPDF_Annot::Subtype::LINE;
  if (sSubtype == "Square")
    return CPDF_Annot::Subtype::SQUARE;
  if (sSubtype == "Circle")
    return CPDF_Annot::Subtype::CIRCLE;
  if (sSubtype == "Polygon")
    return CPDF_Annot::Subtype::POLYGON;
  if (sSubtype == "PolyLine")
    return CPDF_Annot::Subtype::POLYLINE;
  if (sSubtype == "Highlight")
    return CPDF_Annot::Subtype::HIGHLIGHT;
  if (sSubtype == "Underline")
    return CPDF_Annot::Subtype::UNDERLINE;
  if (sSubtype == "Squiggly")
    return CPDF_Annot::Subtype::SQUIGGLY;
  if (sSubtype == "StrikeOut")
    return CPDF_Annot::Subtype::STRIKEOUT;
  if (sSubtype == "Stamp")
    return CPDF_Annot::Subtype::STAMP;
  if (sSubtype == "Caret")
    return CPDF_Annot::Subtype::CARET;
  if (sSubtype == "Ink")
    return CPDF_Annot::Subtype::INK;
  if (sSubtype == "Popup")
    return CPDF_Annot::Subtype::POPUP;
  if (sSubtype == "FileAttachment")
    return CPDF_Annot::Subtype::FILEATTACHMENT;
  if (sSubtype == "Sound")
    return CPDF_Annot::Subtype::SOUND;
  if (sSubtype == "Movie")
    return CPDF_Annot::Subtype::MOVIE;
  if (sSubtype == "Widget")
    return CPDF_Annot::Subtype::WIDGET;
  if (sSubtype == "Screen")
    return CPDF_Annot::Subtype::SCREEN;
  if (sSubtype == "PrinterMark")
    return CPDF_Annot::Subtype::PRINTERMARK;
  if (sSubtype == "TrapNet")
    return CPDF_Annot::Subtype::TRAPNET;
  if (sSubtype == "Watermark")
    return CPDF_Annot::Subtype::WATERMARK;
  if (sSubtype == "3D")
    return CPDF_Annot::Subtype::THREED;
  if (sSubtype == "RichMedia")
    return CPDF_Annot::Subtype::RICHMEDIA;
  if (sSubtype == "XFAWidget")
    return CPDF_Annot::Subtype::XFAWIDGET;
  return CPDF_Annot::Subtype::UNKNOWN;
}

// Static.
CFX_ByteString CPDF_Annot::AnnotSubtypeToString(CPDF_Annot::Subtype nSubtype) {
  if (nSubtype == CPDF_Annot::Subtype::TEXT)
    return "Text";
  if (nSubtype == CPDF_Annot::Subtype::LINK)
    return "Link";
  if (nSubtype == CPDF_Annot::Subtype::FREETEXT)
    return "FreeText";
  if (nSubtype == CPDF_Annot::Subtype::LINE)
    return "Line";
  if (nSubtype == CPDF_Annot::Subtype::SQUARE)
    return "Square";
  if (nSubtype == CPDF_Annot::Subtype::CIRCLE)
    return "Circle";
  if (nSubtype == CPDF_Annot::Subtype::POLYGON)
    return "Polygon";
  if (nSubtype == CPDF_Annot::Subtype::POLYLINE)
    return "PolyLine";
  if (nSubtype == CPDF_Annot::Subtype::HIGHLIGHT)
    return "Highlight";
  if (nSubtype == CPDF_Annot::Subtype::UNDERLINE)
    return "Underline";
  if (nSubtype == CPDF_Annot::Subtype::SQUIGGLY)
    return "Squiggly";
  if (nSubtype == CPDF_Annot::Subtype::STRIKEOUT)
    return "StrikeOut";
  if (nSubtype == CPDF_Annot::Subtype::STAMP)
    return "Stamp";
  if (nSubtype == CPDF_Annot::Subtype::CARET)
    return "Caret";
  if (nSubtype == CPDF_Annot::Subtype::INK)
    return "Ink";
  if (nSubtype == CPDF_Annot::Subtype::POPUP)
    return "Popup";
  if (nSubtype == CPDF_Annot::Subtype::FILEATTACHMENT)
    return "FileAttachment";
  if (nSubtype == CPDF_Annot::Subtype::SOUND)
    return "Sound";
  if (nSubtype == CPDF_Annot::Subtype::MOVIE)
    return "Movie";
  if (nSubtype == CPDF_Annot::Subtype::WIDGET)
    return "Widget";
  if (nSubtype == CPDF_Annot::Subtype::SCREEN)
    return "Screen";
  if (nSubtype == CPDF_Annot::Subtype::PRINTERMARK)
    return "PrinterMark";
  if (nSubtype == CPDF_Annot::Subtype::TRAPNET)
    return "TrapNet";
  if (nSubtype == CPDF_Annot::Subtype::WATERMARK)
    return "Watermark";
  if (nSubtype == CPDF_Annot::Subtype::THREED)
    return "3D";
  if (nSubtype == CPDF_Annot::Subtype::RICHMEDIA)
    return "RichMedia";
  if (nSubtype == CPDF_Annot::Subtype::XFAWIDGET)
    return "XFAWidget";
  return "";
}

bool CPDF_Annot::DrawAppearance(CPDF_Page* pPage,
                                CFX_RenderDevice* pDevice,
                                const CFX_Matrix* pUser2Device,
                                AppearanceMode mode,
                                const CPDF_RenderOptions* pOptions) {
  if (!ShouldDrawAnnotation())
    return false;

  // It might happen that by the time this annotation instance was created,
  // it was flagged as "hidden" (e.g. /F 2), and hence CPVT_GenerateAP decided
  // to not "generate" its AP.
  // If for a reason the object is no longer hidden, but still does not have
  // its "AP" generated, generate it now.
  GenerateAPIfNeeded();

  CFX_Matrix matrix;
  CPDF_Form* pForm = AnnotGetMatrix(pPage, this, mode, pUser2Device, &matrix);
  if (!pForm)
    return false;

  CPDF_RenderContext context(pPage);
  context.AppendLayer(pForm, &matrix);
  context.Render(pDevice, pOptions, nullptr);
  return true;
}

bool CPDF_Annot::DrawInContext(const CPDF_Page* pPage,
                               CPDF_RenderContext* pContext,
                               const CFX_Matrix* pUser2Device,
                               AppearanceMode mode) {
  if (!ShouldDrawAnnotation())
    return false;

  // It might happen that by the time this annotation instance was created,
  // it was flagged as "hidden" (e.g. /F 2), and hence CPVT_GenerateAP decided
  // to not "generate" its AP.
  // If for a reason the object is no longer hidden, but still does not have
  // its "AP" generated, generate it now.
  GenerateAPIfNeeded();

  CFX_Matrix matrix;
  CPDF_Form* pForm = AnnotGetMatrix(pPage, this, mode, pUser2Device, &matrix);
  if (!pForm)
    return false;

  pContext->AppendLayer(pForm, &matrix);
  return true;
}

void CPDF_Annot::DrawBorder(CFX_RenderDevice* pDevice,
                            const CFX_Matrix* pUser2Device,
                            const CPDF_RenderOptions* pOptions) {
  if (GetSubtype() == CPDF_Annot::Subtype::POPUP)
    return;

  uint32_t annot_flags = GetFlags();
  if (annot_flags & ANNOTFLAG_HIDDEN) {
    return;
  }
  bool bPrinting = pDevice->GetDeviceClass() == FXDC_PRINTER ||
                   (pOptions && (pOptions->m_Flags & RENDER_PRINTPREVIEW));
  if (bPrinting && (annot_flags & ANNOTFLAG_PRINT) == 0) {
    return;
  }
  if (!bPrinting && (annot_flags & ANNOTFLAG_NOVIEW)) {
    return;
  }
  CPDF_Dictionary* pBS = m_pAnnotDict->GetDictFor("BS");
  char style_char;
  FX_FLOAT width;
  CPDF_Array* pDashArray = nullptr;
  if (!pBS) {
    CPDF_Array* pBorderArray = m_pAnnotDict->GetArrayFor("Border");
    style_char = 'S';
    if (pBorderArray) {
      width = pBorderArray->GetNumberAt(2);
      if (pBorderArray->GetCount() == 4) {
        pDashArray = pBorderArray->GetArrayAt(3);
        if (!pDashArray) {
          return;
        }
        size_t nLen = pDashArray->GetCount();
        size_t i = 0;
        for (; i < nLen; ++i) {
          CPDF_Object* pObj = pDashArray->GetDirectObjectAt(i);
          if (pObj && pObj->GetInteger()) {
            break;
          }
        }
        if (i == nLen) {
          return;
        }
        style_char = 'D';
      }
    } else {
      width = 1;
    }
  } else {
    CFX_ByteString style = pBS->GetStringFor("S");
    pDashArray = pBS->GetArrayFor("D");
    style_char = style[1];
    width = pBS->GetNumberFor("W");
  }
  if (width <= 0) {
    return;
  }
  CPDF_Array* pColor = m_pAnnotDict->GetArrayFor("C");
  uint32_t argb = 0xff000000;
  if (pColor) {
    int R = (int32_t)(pColor->GetNumberAt(0) * 255);
    int G = (int32_t)(pColor->GetNumberAt(1) * 255);
    int B = (int32_t)(pColor->GetNumberAt(2) * 255);
    argb = ArgbEncode(0xff, R, G, B);
  }
  CFX_GraphStateData graph_state;
  graph_state.m_LineWidth = width;
  if (style_char == 'D') {
    if (pDashArray) {
      size_t dash_count = pDashArray->GetCount();
      if (dash_count % 2) {
        dash_count++;
      }
      graph_state.m_DashArray = FX_Alloc(FX_FLOAT, dash_count);
      graph_state.m_DashCount = dash_count;
      size_t i;
      for (i = 0; i < pDashArray->GetCount(); ++i) {
        graph_state.m_DashArray[i] = pDashArray->GetNumberAt(i);
      }
      if (i < dash_count) {
        graph_state.m_DashArray[i] = graph_state.m_DashArray[i - 1];
      }
    } else {
      graph_state.m_DashArray = FX_Alloc(FX_FLOAT, 2);
      graph_state.m_DashCount = 2;
      graph_state.m_DashArray[0] = graph_state.m_DashArray[1] = 3 * 1.0f;
    }
  }
  CFX_FloatRect rect = GetRect();
  CFX_PathData path;
  width /= 2;
  path.AppendRect(rect.left + width, rect.bottom + width, rect.right - width,
                  rect.top - width);
  int fill_type = 0;
  if (pOptions && (pOptions->m_Flags & RENDER_NOPATHSMOOTH))
    fill_type |= FXFILL_NOPATHSMOOTH;

  pDevice->DrawPath(&path, pUser2Device, &graph_state, argb, argb, fill_type);
}
