// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_ANDROID_CFPF_SKIADEVICEMODULE_H_
#define CORE_FXGE_ANDROID_CFPF_SKIADEVICEMODULE_H_

class CFPF_SkiaFontMgr;

class CFPF_SkiaDeviceModule {
 public:
  CFPF_SkiaDeviceModule() : m_pFontMgr(nullptr) {}
  ~CFPF_SkiaDeviceModule();

  void Destroy();
  CFPF_SkiaFontMgr* GetFontMgr();

 protected:
  CFPF_SkiaFontMgr* m_pFontMgr;
};

CFPF_SkiaDeviceModule* CFPF_GetSkiaDeviceModule();

#endif  // CORE_FXGE_ANDROID_CFPF_SKIADEVICEMODULE_H_
