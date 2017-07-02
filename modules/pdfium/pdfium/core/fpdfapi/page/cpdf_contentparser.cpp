// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_contentparser.h"

#include "core/fpdfapi/font/cpdf_type3char.h"
#include "core/fpdfapi/page/cpdf_allstates.h"
#include "core/fpdfapi/page/cpdf_form.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fpdfapi/page/cpdf_path.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fxcrt/fx_safe_types.h"
#include "third_party/base/ptr_util.h"

#define PARSE_STEP_LIMIT 100

CPDF_ContentParser::CPDF_ContentParser()
    : m_Status(Ready),
      m_InternalStage(STAGE_GETCONTENT),
      m_pObjectHolder(nullptr),
      m_bForm(false),
      m_pType3Char(nullptr),
      m_pData(nullptr),
      m_Size(0),
      m_CurrentOffset(0) {}

CPDF_ContentParser::~CPDF_ContentParser() {
  if (!m_pSingleStream)
    FX_Free(m_pData);
}

void CPDF_ContentParser::Start(CPDF_Page* pPage) {
  if (m_Status != Ready || !pPage || !pPage->m_pDocument ||
      !pPage->m_pFormDict) {
    m_Status = Done;
    return;
  }
  m_pObjectHolder = pPage;
  m_bForm = false;
  m_Status = ToBeContinued;
  m_InternalStage = STAGE_GETCONTENT;
  m_CurrentOffset = 0;

  CPDF_Object* pContent = pPage->m_pFormDict->GetDirectObjectFor("Contents");
  if (!pContent) {
    m_Status = Done;
    return;
  }
  if (CPDF_Stream* pStream = pContent->AsStream()) {
    m_nStreams = 0;
    m_pSingleStream = pdfium::MakeUnique<CPDF_StreamAcc>();
    m_pSingleStream->LoadAllData(pStream, false);
  } else if (CPDF_Array* pArray = pContent->AsArray()) {
    m_nStreams = pArray->GetCount();
    if (m_nStreams)
      m_StreamArray.resize(m_nStreams);
    else
      m_Status = Done;
  } else {
    m_Status = Done;
  }
}

void CPDF_ContentParser::Start(CPDF_Form* pForm,
                               CPDF_AllStates* pGraphicStates,
                               const CFX_Matrix* pParentMatrix,
                               CPDF_Type3Char* pType3Char,
                               int level) {
  m_pType3Char = pType3Char;
  m_pObjectHolder = pForm;
  m_bForm = true;
  CFX_Matrix form_matrix = pForm->m_pFormDict->GetMatrixFor("Matrix");
  if (pGraphicStates)
    form_matrix.Concat(pGraphicStates->m_CTM);
  CPDF_Array* pBBox = pForm->m_pFormDict->GetArrayFor("BBox");
  CFX_FloatRect form_bbox;
  CPDF_Path ClipPath;
  if (pBBox) {
    form_bbox = pBBox->GetRect();
    ClipPath.Emplace();
    ClipPath.AppendRect(form_bbox.left, form_bbox.bottom, form_bbox.right,
                        form_bbox.top);
    ClipPath.Transform(&form_matrix);
    if (pParentMatrix)
      ClipPath.Transform(pParentMatrix);

    form_matrix.TransformRect(form_bbox);
    if (pParentMatrix)
      pParentMatrix->TransformRect(form_bbox);
  }

  CPDF_Dictionary* pResources = pForm->m_pFormDict->GetDictFor("Resources");
  m_pParser = pdfium::MakeUnique<CPDF_StreamContentParser>(
      pForm->m_pDocument, pForm->m_pPageResources, pForm->m_pResources,
      pParentMatrix, pForm, pResources, &form_bbox, pGraphicStates, level);
  m_pParser->GetCurStates()->m_CTM = form_matrix;
  m_pParser->GetCurStates()->m_ParentMatrix = form_matrix;
  if (ClipPath) {
    m_pParser->GetCurStates()->m_ClipPath.AppendPath(ClipPath, FXFILL_WINDING,
                                                     true);
  }
  if (pForm->m_Transparency & PDFTRANS_GROUP) {
    CPDF_GeneralState* pState = &m_pParser->GetCurStates()->m_GeneralState;
    pState->SetBlendType(FXDIB_BLEND_NORMAL);
    pState->SetStrokeAlpha(1.0f);
    pState->SetFillAlpha(1.0f);
    pState->SetSoftMask(nullptr);
  }
  m_nStreams = 0;
  m_pSingleStream = pdfium::MakeUnique<CPDF_StreamAcc>();
  m_pSingleStream->LoadAllData(pForm->m_pFormStream, false);
  m_pData = (uint8_t*)m_pSingleStream->GetData();
  m_Size = m_pSingleStream->GetSize();
  m_Status = ToBeContinued;
  m_InternalStage = STAGE_PARSE;
  m_CurrentOffset = 0;
}

void CPDF_ContentParser::Continue(IFX_Pause* pPause) {
  int steps = 0;
  while (m_Status == ToBeContinued) {
    if (m_InternalStage == STAGE_GETCONTENT) {
      if (m_CurrentOffset == m_nStreams) {
        if (!m_StreamArray.empty()) {
          FX_SAFE_UINT32 safeSize = 0;
          for (const auto& stream : m_StreamArray) {
            safeSize += stream->GetSize();
            safeSize += 1;
          }
          if (!safeSize.IsValid()) {
            m_Status = Done;
            return;
          }
          m_Size = safeSize.ValueOrDie();
          m_pData = FX_Alloc(uint8_t, m_Size);
          uint32_t pos = 0;
          for (const auto& stream : m_StreamArray) {
            FXSYS_memcpy(m_pData + pos, stream->GetData(), stream->GetSize());
            pos += stream->GetSize();
            m_pData[pos++] = ' ';
          }
          m_StreamArray.clear();
        } else {
          m_pData = (uint8_t*)m_pSingleStream->GetData();
          m_Size = m_pSingleStream->GetSize();
        }
        m_InternalStage = STAGE_PARSE;
        m_CurrentOffset = 0;
      } else {
        CPDF_Array* pContent =
            m_pObjectHolder->m_pFormDict->GetArrayFor("Contents");
        m_StreamArray[m_CurrentOffset] = pdfium::MakeUnique<CPDF_StreamAcc>();
        CPDF_Stream* pStreamObj = ToStream(
            pContent ? pContent->GetDirectObjectAt(m_CurrentOffset) : nullptr);
        m_StreamArray[m_CurrentOffset]->LoadAllData(pStreamObj, false);
        m_CurrentOffset++;
      }
    }
    if (m_InternalStage == STAGE_PARSE) {
      if (!m_pParser) {
        m_pParser = pdfium::MakeUnique<CPDF_StreamContentParser>(
            m_pObjectHolder->m_pDocument, m_pObjectHolder->m_pPageResources,
            nullptr, nullptr, m_pObjectHolder, m_pObjectHolder->m_pResources,
            &m_pObjectHolder->m_BBox, nullptr, 0);
        m_pParser->GetCurStates()->m_ColorState.SetDefault();
      }
      if (m_CurrentOffset >= m_Size) {
        m_InternalStage = STAGE_CHECKCLIP;
      } else {
        m_CurrentOffset +=
            m_pParser->Parse(m_pData + m_CurrentOffset,
                             m_Size - m_CurrentOffset, PARSE_STEP_LIMIT);
      }
    }
    if (m_InternalStage == STAGE_CHECKCLIP) {
      if (m_pType3Char) {
        m_pType3Char->m_bColored = m_pParser->IsColored();
        m_pType3Char->m_Width =
            FXSYS_round(m_pParser->GetType3Data()[0] * 1000);
        m_pType3Char->m_BBox.left =
            FXSYS_round(m_pParser->GetType3Data()[2] * 1000);
        m_pType3Char->m_BBox.bottom =
            FXSYS_round(m_pParser->GetType3Data()[3] * 1000);
        m_pType3Char->m_BBox.right =
            FXSYS_round(m_pParser->GetType3Data()[4] * 1000);
        m_pType3Char->m_BBox.top =
            FXSYS_round(m_pParser->GetType3Data()[5] * 1000);
      }
      for (auto& pObj : *m_pObjectHolder->GetPageObjectList()) {
        if (!pObj->m_ClipPath)
          continue;
        if (pObj->m_ClipPath.GetPathCount() != 1)
          continue;
        if (pObj->m_ClipPath.GetTextCount())
          continue;
        CPDF_Path ClipPath = pObj->m_ClipPath.GetPath(0);
        if (!ClipPath.IsRect() || pObj->IsShading())
          continue;

        CFX_PointF point0 = ClipPath.GetPoint(0);
        CFX_PointF point2 = ClipPath.GetPoint(2);
        CFX_FloatRect old_rect(point0.x, point0.y, point2.x, point2.y);
        CFX_FloatRect obj_rect(pObj->m_Left, pObj->m_Bottom, pObj->m_Right,
                               pObj->m_Top);
        if (old_rect.Contains(obj_rect))
          pObj->m_ClipPath.SetNull();
      }
      m_Status = Done;
      return;
    }
    steps++;
    if (pPause && pPause->NeedToPauseNow())
      break;
  }
}
