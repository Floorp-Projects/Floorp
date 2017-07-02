// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_allstates.h"

#include <algorithm>

#include "core/fpdfapi/page/cpdf_pageobjectholder.h"
#include "core/fpdfapi/page/cpdf_streamcontentparser.h"
#include "core/fpdfapi/page/pageint.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fxge/cfx_graphstatedata.h"

namespace {

FX_FLOAT ClipFloat(FX_FLOAT f) {
  return std::max(0.0f, std::min(1.0f, f));
}

}  // namespace

CPDF_AllStates::CPDF_AllStates()
    : m_TextLeading(0), m_TextRise(0), m_TextHorzScale(1.0f) {}

CPDF_AllStates::~CPDF_AllStates() {}

void CPDF_AllStates::Copy(const CPDF_AllStates& src) {
  CopyStates(src);
  m_TextMatrix = src.m_TextMatrix;
  m_ParentMatrix = src.m_ParentMatrix;
  m_CTM = src.m_CTM;
  m_TextPos = src.m_TextPos;
  m_TextLinePos = src.m_TextLinePos;
  m_TextLeading = src.m_TextLeading;
  m_TextRise = src.m_TextRise;
  m_TextHorzScale = src.m_TextHorzScale;
}

void CPDF_AllStates::SetLineDash(CPDF_Array* pArray,
                                 FX_FLOAT phase,
                                 FX_FLOAT scale) {
  m_GraphState.SetLineDash(pArray, phase, scale);
}

void CPDF_AllStates::ProcessExtGS(CPDF_Dictionary* pGS,
                                  CPDF_StreamContentParser* pParser) {
  for (const auto& it : *pGS) {
    const CFX_ByteString& key_str = it.first;
    CPDF_Object* pElement = it.second.get();
    CPDF_Object* pObject = pElement ? pElement->GetDirect() : nullptr;
    if (!pObject)
      continue;

    uint32_t key = key_str.GetID();
    switch (key) {
      case FXBSTR_ID('L', 'W', 0, 0):
        m_GraphState.SetLineWidth(pObject->GetNumber());
        break;
      case FXBSTR_ID('L', 'C', 0, 0):
        m_GraphState.SetLineCap(
            static_cast<CFX_GraphStateData::LineCap>(pObject->GetInteger()));
        break;
      case FXBSTR_ID('L', 'J', 0, 0):
        m_GraphState.SetLineJoin(
            static_cast<CFX_GraphStateData::LineJoin>(pObject->GetInteger()));
        break;
      case FXBSTR_ID('M', 'L', 0, 0):
        m_GraphState.SetMiterLimit(pObject->GetNumber());
        break;
      case FXBSTR_ID('D', 0, 0, 0): {
        CPDF_Array* pDash = pObject->AsArray();
        if (!pDash)
          break;

        CPDF_Array* pArray = pDash->GetArrayAt(0);
        if (!pArray)
          break;

        SetLineDash(pArray, pDash->GetNumberAt(1), 1.0f);
        break;
      }
      case FXBSTR_ID('R', 'I', 0, 0):
        m_GeneralState.SetRenderIntent(pObject->GetString());
        break;
      case FXBSTR_ID('F', 'o', 'n', 't'): {
        CPDF_Array* pFont = pObject->AsArray();
        if (!pFont)
          break;

        m_TextState.SetFontSize(pFont->GetNumberAt(1));
        m_TextState.SetFont(pParser->FindFont(pFont->GetStringAt(0)));
        break;
      }
      case FXBSTR_ID('T', 'R', 0, 0):
        if (pGS->KeyExist("TR2")) {
          continue;
        }
      case FXBSTR_ID('T', 'R', '2', 0):
        m_GeneralState.SetTR(pObject && !pObject->IsName() ? pObject : nullptr);
        break;
      case FXBSTR_ID('B', 'M', 0, 0): {
        CPDF_Array* pArray = pObject->AsArray();
        m_GeneralState.SetBlendMode(pArray ? pArray->GetStringAt(0)
                                           : pObject->GetString());
        if (m_GeneralState.GetBlendType() > FXDIB_BLEND_MULTIPLY)
          pParser->GetPageObjectHolder()->SetBackgroundAlphaNeeded(true);
        break;
      }
      case FXBSTR_ID('S', 'M', 'a', 's'):
        if (ToDictionary(pObject)) {
          m_GeneralState.SetSoftMask(pObject);
          m_GeneralState.SetSMaskMatrix(pParser->GetCurStates()->m_CTM);
        } else {
          m_GeneralState.SetSoftMask(nullptr);
        }
        break;
      case FXBSTR_ID('C', 'A', 0, 0):
        m_GeneralState.SetStrokeAlpha(ClipFloat(pObject->GetNumber()));
        break;
      case FXBSTR_ID('c', 'a', 0, 0):
        m_GeneralState.SetFillAlpha(ClipFloat(pObject->GetNumber()));
        break;
      case FXBSTR_ID('O', 'P', 0, 0):
        m_GeneralState.SetStrokeOP(!!pObject->GetInteger());
        if (!pGS->KeyExist("op"))
          m_GeneralState.SetFillOP(!!pObject->GetInteger());
        break;
      case FXBSTR_ID('o', 'p', 0, 0):
        m_GeneralState.SetFillOP(!!pObject->GetInteger());
        break;
      case FXBSTR_ID('O', 'P', 'M', 0):
        m_GeneralState.SetOPMode(pObject->GetInteger());
        break;
      case FXBSTR_ID('B', 'G', 0, 0):
        if (pGS->KeyExist("BG2")) {
          continue;
        }
      case FXBSTR_ID('B', 'G', '2', 0):
        m_GeneralState.SetBG(pObject);
        break;
      case FXBSTR_ID('U', 'C', 'R', 0):
        if (pGS->KeyExist("UCR2")) {
          continue;
        }
      case FXBSTR_ID('U', 'C', 'R', '2'):
        m_GeneralState.SetUCR(pObject);
        break;
      case FXBSTR_ID('H', 'T', 0, 0):
        m_GeneralState.SetHT(pObject);
        break;
      case FXBSTR_ID('F', 'L', 0, 0):
        m_GeneralState.SetFlatness(pObject->GetNumber());
        break;
      case FXBSTR_ID('S', 'M', 0, 0):
        m_GeneralState.SetSmoothness(pObject->GetNumber());
        break;
      case FXBSTR_ID('S', 'A', 0, 0):
        m_GeneralState.SetStrokeAdjust(!!pObject->GetInteger());
        break;
      case FXBSTR_ID('A', 'I', 'S', 0):
        m_GeneralState.SetAlphaSource(!!pObject->GetInteger());
        break;
      case FXBSTR_ID('T', 'K', 0, 0):
        m_GeneralState.SetTextKnockout(!!pObject->GetInteger());
        break;
    }
  }
  m_GeneralState.SetMatrix(m_CTM);
}
