// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/cpdf_modulemgr.h"

#include "core/fpdfapi/page/cpdf_pagemodule.h"
#include "core/fxcodec/fx_codec.h"
#include "third_party/base/ptr_util.h"

namespace {

CPDF_ModuleMgr* g_pDefaultMgr = nullptr;

}  // namespace

// static
CPDF_ModuleMgr* CPDF_ModuleMgr::Get() {
  if (!g_pDefaultMgr)
    g_pDefaultMgr = new CPDF_ModuleMgr;
  return g_pDefaultMgr;
}

// static
void CPDF_ModuleMgr::Destroy() {
  delete g_pDefaultMgr;
  g_pDefaultMgr = nullptr;
}

CPDF_ModuleMgr::CPDF_ModuleMgr() : m_pCodecModule(nullptr) {}

CPDF_ModuleMgr::~CPDF_ModuleMgr() {}

void CPDF_ModuleMgr::InitPageModule() {
  m_pPageModule = pdfium::MakeUnique<CPDF_PageModule>();
}

CCodec_FaxModule* CPDF_ModuleMgr::GetFaxModule() {
  return m_pCodecModule ? m_pCodecModule->GetFaxModule() : nullptr;
}

CCodec_JpegModule* CPDF_ModuleMgr::GetJpegModule() {
  return m_pCodecModule ? m_pCodecModule->GetJpegModule() : nullptr;
}

CCodec_JpxModule* CPDF_ModuleMgr::GetJpxModule() {
  return m_pCodecModule ? m_pCodecModule->GetJpxModule() : nullptr;
}

CCodec_Jbig2Module* CPDF_ModuleMgr::GetJbig2Module() {
  return m_pCodecModule ? m_pCodecModule->GetJbig2Module() : nullptr;
}

CCodec_IccModule* CPDF_ModuleMgr::GetIccModule() {
  return m_pCodecModule ? m_pCodecModule->GetIccModule() : nullptr;
}

CCodec_FlateModule* CPDF_ModuleMgr::GetFlateModule() {
  return m_pCodecModule ? m_pCodecModule->GetFlateModule() : nullptr;
}
