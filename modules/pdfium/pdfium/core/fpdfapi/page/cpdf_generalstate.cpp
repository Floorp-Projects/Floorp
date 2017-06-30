// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_generalstate.h"

#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/render/cpdf_dibsource.h"
#include "core/fpdfapi/render/cpdf_docrenderdata.h"
#include "core/fpdfapi/render/cpdf_transferfunc.h"

namespace {

int RI_StringToId(const CFX_ByteString& ri) {
  uint32_t id = ri.GetID();
  if (id == FXBSTR_ID('A', 'b', 's', 'o'))
    return 1;

  if (id == FXBSTR_ID('S', 'a', 't', 'u'))
    return 2;

  if (id == FXBSTR_ID('P', 'e', 'r', 'c'))
    return 3;

  return 0;
}

int GetBlendTypeInternal(const CFX_ByteString& mode) {
  switch (mode.GetID()) {
    case FXBSTR_ID('N', 'o', 'r', 'm'):
    case FXBSTR_ID('C', 'o', 'm', 'p'):
      return FXDIB_BLEND_NORMAL;
    case FXBSTR_ID('M', 'u', 'l', 't'):
      return FXDIB_BLEND_MULTIPLY;
    case FXBSTR_ID('S', 'c', 'r', 'e'):
      return FXDIB_BLEND_SCREEN;
    case FXBSTR_ID('O', 'v', 'e', 'r'):
      return FXDIB_BLEND_OVERLAY;
    case FXBSTR_ID('D', 'a', 'r', 'k'):
      return FXDIB_BLEND_DARKEN;
    case FXBSTR_ID('L', 'i', 'g', 'h'):
      return FXDIB_BLEND_LIGHTEN;
    case FXBSTR_ID('C', 'o', 'l', 'o'):
      if (mode.GetLength() == 10)
        return FXDIB_BLEND_COLORDODGE;
      if (mode.GetLength() == 9)
        return FXDIB_BLEND_COLORBURN;
      return FXDIB_BLEND_COLOR;
    case FXBSTR_ID('H', 'a', 'r', 'd'):
      return FXDIB_BLEND_HARDLIGHT;
    case FXBSTR_ID('S', 'o', 'f', 't'):
      return FXDIB_BLEND_SOFTLIGHT;
    case FXBSTR_ID('D', 'i', 'f', 'f'):
      return FXDIB_BLEND_DIFFERENCE;
    case FXBSTR_ID('E', 'x', 'c', 'l'):
      return FXDIB_BLEND_EXCLUSION;
    case FXBSTR_ID('H', 'u', 'e', 0):
      return FXDIB_BLEND_HUE;
    case FXBSTR_ID('S', 'a', 't', 'u'):
      return FXDIB_BLEND_SATURATION;
    case FXBSTR_ID('L', 'u', 'm', 'i'):
      return FXDIB_BLEND_LUMINOSITY;
  }
  return FXDIB_BLEND_NORMAL;
}

}  // namespace

CPDF_GeneralState::CPDF_GeneralState() {}

CPDF_GeneralState::CPDF_GeneralState(const CPDF_GeneralState& that)
    : m_Ref(that.m_Ref) {}

CPDF_GeneralState::~CPDF_GeneralState() {}

void CPDF_GeneralState::SetRenderIntent(const CFX_ByteString& ri) {
  m_Ref.GetPrivateCopy()->m_RenderIntent = RI_StringToId(ri);
}

int CPDF_GeneralState::GetBlendType() const {
  const StateData* pData = m_Ref.GetObject();
  return pData ? pData->m_BlendType : FXDIB_BLEND_NORMAL;
}

void CPDF_GeneralState::SetBlendType(int type) {
  m_Ref.GetPrivateCopy()->m_BlendType = type;
}

FX_FLOAT CPDF_GeneralState::GetFillAlpha() const {
  const StateData* pData = m_Ref.GetObject();
  return pData ? pData->m_FillAlpha : 1.0f;
}

void CPDF_GeneralState::SetFillAlpha(FX_FLOAT alpha) {
  m_Ref.GetPrivateCopy()->m_FillAlpha = alpha;
}

FX_FLOAT CPDF_GeneralState::GetStrokeAlpha() const {
  const StateData* pData = m_Ref.GetObject();
  return pData ? pData->m_StrokeAlpha : 1.0f;
}

void CPDF_GeneralState::SetStrokeAlpha(FX_FLOAT alpha) {
  m_Ref.GetPrivateCopy()->m_StrokeAlpha = alpha;
}

CPDF_Object* CPDF_GeneralState::GetSoftMask() const {
  const StateData* pData = m_Ref.GetObject();
  return pData ? pData->m_pSoftMask : nullptr;
}

void CPDF_GeneralState::SetSoftMask(CPDF_Object* pObject) {
  m_Ref.GetPrivateCopy()->m_pSoftMask = pObject;
}

CPDF_Object* CPDF_GeneralState::GetTR() const {
  const StateData* pData = m_Ref.GetObject();
  return pData ? pData->m_pTR : nullptr;
}

void CPDF_GeneralState::SetTR(CPDF_Object* pObject) {
  m_Ref.GetPrivateCopy()->m_pTR = pObject;
}

CPDF_TransferFunc* CPDF_GeneralState::GetTransferFunc() const {
  const StateData* pData = m_Ref.GetObject();
  return pData ? pData->m_pTransferFunc : nullptr;
}

void CPDF_GeneralState::SetTransferFunc(CPDF_TransferFunc* pFunc) {
  m_Ref.GetPrivateCopy()->m_pTransferFunc = pFunc;
}

void CPDF_GeneralState::SetBlendMode(const CFX_ByteString& mode) {
  StateData* pData = m_Ref.GetPrivateCopy();
  pData->m_BlendMode = mode;
  pData->m_BlendType = GetBlendTypeInternal(mode);
}

const CFX_Matrix* CPDF_GeneralState::GetSMaskMatrix() const {
  const StateData* pData = m_Ref.GetObject();
  return pData ? &pData->m_SMaskMatrix : nullptr;
}

void CPDF_GeneralState::SetSMaskMatrix(const CFX_Matrix& matrix) {
  m_Ref.GetPrivateCopy()->m_SMaskMatrix = matrix;
}

bool CPDF_GeneralState::GetFillOP() const {
  const StateData* pData = m_Ref.GetObject();
  return pData && pData->m_FillOP;
}

void CPDF_GeneralState::SetFillOP(bool op) {
  m_Ref.GetPrivateCopy()->m_FillOP = op;
}

void CPDF_GeneralState::SetStrokeOP(bool op) {
  m_Ref.GetPrivateCopy()->m_StrokeOP = op;
}

bool CPDF_GeneralState::GetStrokeOP() const {
  const StateData* pData = m_Ref.GetObject();
  return pData && pData->m_StrokeOP;
}

int CPDF_GeneralState::GetOPMode() const {
  return m_Ref.GetObject()->m_OPMode;
}

void CPDF_GeneralState::SetOPMode(int mode) {
  m_Ref.GetPrivateCopy()->m_OPMode = mode;
}

void CPDF_GeneralState::SetBG(CPDF_Object* pObject) {
  m_Ref.GetPrivateCopy()->m_pBG = pObject;
}

void CPDF_GeneralState::SetUCR(CPDF_Object* pObject) {
  m_Ref.GetPrivateCopy()->m_pUCR = pObject;
}

void CPDF_GeneralState::SetHT(CPDF_Object* pObject) {
  m_Ref.GetPrivateCopy()->m_pHT = pObject;
}

void CPDF_GeneralState::SetFlatness(FX_FLOAT flatness) {
  m_Ref.GetPrivateCopy()->m_Flatness = flatness;
}

void CPDF_GeneralState::SetSmoothness(FX_FLOAT smoothness) {
  m_Ref.GetPrivateCopy()->m_Smoothness = smoothness;
}

bool CPDF_GeneralState::GetStrokeAdjust() const {
  const StateData* pData = m_Ref.GetObject();
  return pData && pData->m_StrokeAdjust;
}

void CPDF_GeneralState::SetStrokeAdjust(bool adjust) {
  m_Ref.GetPrivateCopy()->m_StrokeAdjust = adjust;
}

void CPDF_GeneralState::SetAlphaSource(bool source) {
  m_Ref.GetPrivateCopy()->m_AlphaSource = source;
}

void CPDF_GeneralState::SetTextKnockout(bool knockout) {
  m_Ref.GetPrivateCopy()->m_TextKnockout = knockout;
}

void CPDF_GeneralState::SetMatrix(const CFX_Matrix& matrix) {
  m_Ref.GetPrivateCopy()->m_Matrix = matrix;
}

CFX_Matrix* CPDF_GeneralState::GetMutableMatrix() {
  return &m_Ref.GetPrivateCopy()->m_Matrix;
}

CPDF_GeneralState::StateData::StateData()
    : m_BlendMode("Normal"),
      m_BlendType(0),
      m_pSoftMask(nullptr),
      m_StrokeAlpha(1.0),
      m_FillAlpha(1.0f),
      m_pTR(nullptr),
      m_pTransferFunc(nullptr),
      m_RenderIntent(0),
      m_StrokeAdjust(false),
      m_AlphaSource(false),
      m_TextKnockout(false),
      m_StrokeOP(false),
      m_FillOP(false),
      m_OPMode(0),
      m_pBG(nullptr),
      m_pUCR(nullptr),
      m_pHT(nullptr),
      m_Flatness(1.0f),
      m_Smoothness(0.0f) {
  m_SMaskMatrix.SetIdentity();
  m_Matrix.SetIdentity();
}

CPDF_GeneralState::StateData::StateData(const StateData& that)
    : m_BlendMode(that.m_BlendMode),
      m_BlendType(that.m_BlendType),
      m_pSoftMask(that.m_pSoftMask),
      m_StrokeAlpha(that.m_StrokeAlpha),
      m_FillAlpha(that.m_FillAlpha),
      m_pTR(that.m_pTR),
      m_pTransferFunc(that.m_pTransferFunc),
      m_RenderIntent(that.m_RenderIntent),
      m_StrokeAdjust(that.m_StrokeAdjust),
      m_AlphaSource(that.m_AlphaSource),
      m_TextKnockout(that.m_TextKnockout),
      m_StrokeOP(that.m_StrokeOP),
      m_FillOP(that.m_FillOP),
      m_OPMode(that.m_OPMode),
      m_pBG(that.m_pBG),
      m_pUCR(that.m_pUCR),
      m_pHT(that.m_pHT),
      m_Flatness(that.m_Flatness),
      m_Smoothness(that.m_Smoothness) {
  m_Matrix = that.m_Matrix;
  m_SMaskMatrix = that.m_SMaskMatrix;

  if (that.m_pTransferFunc && that.m_pTransferFunc->m_pPDFDoc) {
    CPDF_DocRenderData* pDocCache =
        that.m_pTransferFunc->m_pPDFDoc->GetRenderData();
    if (pDocCache)
      m_pTransferFunc = pDocCache->GetTransferFunc(m_pTR);
  }
}

CPDF_GeneralState::StateData::~StateData() {
  if (m_pTransferFunc && m_pTransferFunc->m_pPDFDoc) {
    CPDF_DocRenderData* pDocCache = m_pTransferFunc->m_pPDFDoc->GetRenderData();
    if (pDocCache)
      pDocCache->ReleaseTransferFunc(m_pTR);
  }
}
