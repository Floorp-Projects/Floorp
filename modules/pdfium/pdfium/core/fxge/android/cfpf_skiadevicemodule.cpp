// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/android/cfpf_skiadevicemodule.h"

#include "core/fxge/android/cfpf_skiafontmgr.h"

namespace {

CFPF_SkiaDeviceModule* gs_pPFModule = nullptr;

}  // namespace

CFPF_SkiaDeviceModule* CFPF_GetSkiaDeviceModule() {
  if (!gs_pPFModule)
    gs_pPFModule = new CFPF_SkiaDeviceModule;
  return gs_pPFModule;
}

CFPF_SkiaDeviceModule::~CFPF_SkiaDeviceModule() {
  delete m_pFontMgr;
}

void CFPF_SkiaDeviceModule::Destroy() {
  delete gs_pPFModule;
  gs_pPFModule = nullptr;
}

CFPF_SkiaFontMgr* CFPF_SkiaDeviceModule::GetFontMgr() {
  if (!m_pFontMgr) {
    m_pFontMgr = new CFPF_SkiaFontMgr;
    if (!m_pFontMgr->InitFTLibrary()) {
      delete m_pFontMgr;
      return nullptr;
    }
  }
  return m_pFontMgr;
}
