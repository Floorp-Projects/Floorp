// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/render/cpdf_docrenderdata.h"

#include <memory>

#include "core/fpdfapi/font/cpdf_type3font.h"
#include "core/fpdfapi/page/pageint.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/render/cpdf_dibsource.h"
#include "core/fpdfapi/render/cpdf_transferfunc.h"
#include "core/fpdfapi/render/cpdf_type3cache.h"

namespace {

const int kMaxOutputs = 16;

}  // namespace

CPDF_DocRenderData::CPDF_DocRenderData(CPDF_Document* pPDFDoc)
    : m_pPDFDoc(pPDFDoc) {}

CPDF_DocRenderData::~CPDF_DocRenderData() {
  Clear(true);
}

void CPDF_DocRenderData::Clear(bool bRelease) {
  for (auto it = m_Type3FaceMap.begin(); it != m_Type3FaceMap.end();) {
    auto curr_it = it++;
    CPDF_CountedObject<CPDF_Type3Cache>* cache = curr_it->second;
    if (bRelease || cache->use_count() < 2) {
      delete cache->get();
      delete cache;
      m_Type3FaceMap.erase(curr_it);
    }
  }

  for (auto it = m_TransferFuncMap.begin(); it != m_TransferFuncMap.end();) {
    auto curr_it = it++;
    CPDF_CountedObject<CPDF_TransferFunc>* value = curr_it->second;
    if (bRelease || value->use_count() < 2) {
      delete value->get();
      delete value;
      m_TransferFuncMap.erase(curr_it);
    }
  }
}

CPDF_Type3Cache* CPDF_DocRenderData::GetCachedType3(CPDF_Type3Font* pFont) {
  CPDF_CountedObject<CPDF_Type3Cache>* pCache;
  auto it = m_Type3FaceMap.find(pFont);
  if (it == m_Type3FaceMap.end()) {
    pCache = new CPDF_CountedObject<CPDF_Type3Cache>(
        pdfium::MakeUnique<CPDF_Type3Cache>(pFont));
    m_Type3FaceMap[pFont] = pCache;
  } else {
    pCache = it->second;
  }
  return pCache->AddRef();
}

void CPDF_DocRenderData::ReleaseCachedType3(CPDF_Type3Font* pFont) {
  auto it = m_Type3FaceMap.find(pFont);
  if (it != m_Type3FaceMap.end()) {
    it->second->RemoveRef();
    if (it->second->use_count() < 2) {
      delete it->second->get();
      delete it->second;
      m_Type3FaceMap.erase(it);
    }
  }
}

CPDF_TransferFunc* CPDF_DocRenderData::GetTransferFunc(CPDF_Object* pObj) {
  if (!pObj)
    return nullptr;

  auto it = m_TransferFuncMap.find(pObj);
  if (it != m_TransferFuncMap.end()) {
    CPDF_CountedObject<CPDF_TransferFunc>* pTransferCounter = it->second;
    return pTransferCounter->AddRef();
  }

  std::unique_ptr<CPDF_Function> pFuncs[3];
  bool bUniTransfer = true;
  bool bIdentity = true;
  if (CPDF_Array* pArray = pObj->AsArray()) {
    bUniTransfer = false;
    if (pArray->GetCount() < 3)
      return nullptr;

    for (uint32_t i = 0; i < 3; ++i) {
      pFuncs[2 - i] = CPDF_Function::Load(pArray->GetDirectObjectAt(i));
      if (!pFuncs[2 - i])
        return nullptr;
    }
  } else {
    pFuncs[0] = CPDF_Function::Load(pObj);
    if (!pFuncs[0])
      return nullptr;
  }
  CPDF_CountedObject<CPDF_TransferFunc>* pTransferCounter =
      new CPDF_CountedObject<CPDF_TransferFunc>(
          pdfium::MakeUnique<CPDF_TransferFunc>(m_pPDFDoc));
  CPDF_TransferFunc* pTransfer = pTransferCounter->get();
  m_TransferFuncMap[pObj] = pTransferCounter;
  FX_FLOAT output[kMaxOutputs];
  FXSYS_memset(output, 0, sizeof(output));
  FX_FLOAT input;
  int noutput;
  for (int v = 0; v < 256; ++v) {
    input = (FX_FLOAT)v / 255.0f;
    if (bUniTransfer) {
      if (pFuncs[0] && pFuncs[0]->CountOutputs() <= kMaxOutputs)
        pFuncs[0]->Call(&input, 1, output, noutput);
      int o = FXSYS_round(output[0] * 255);
      if (o != v)
        bIdentity = false;
      for (int i = 0; i < 3; ++i)
        pTransfer->m_Samples[i * 256 + v] = o;
      continue;
    }
    for (int i = 0; i < 3; ++i) {
      if (!pFuncs[i] || pFuncs[i]->CountOutputs() > kMaxOutputs) {
        pTransfer->m_Samples[i * 256 + v] = v;
        continue;
      }
      pFuncs[i]->Call(&input, 1, output, noutput);
      int o = FXSYS_round(output[0] * 255);
      if (o != v)
        bIdentity = false;
      pTransfer->m_Samples[i * 256 + v] = o;
    }
  }

  pTransfer->m_bIdentity = bIdentity;
  return pTransferCounter->AddRef();
}

void CPDF_DocRenderData::ReleaseTransferFunc(CPDF_Object* pObj) {
  auto it = m_TransferFuncMap.find(pObj);
  if (it != m_TransferFuncMap.end()) {
    it->second->RemoveRef();
    if (it->second->use_count() < 2) {
      delete it->second->get();
      delete it->second;
      m_TransferFuncMap.erase(it);
    }
  }
}
