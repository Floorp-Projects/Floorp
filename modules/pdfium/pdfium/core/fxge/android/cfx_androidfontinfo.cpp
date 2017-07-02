// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/android/cfx_androidfontinfo.h"

#include "core/fxcrt/fx_system.h"
#include "core/fxge/android/cfpf_skiafont.h"
#include "core/fxge/android/cfpf_skiafontmgr.h"
#include "core/fxge/cfx_fontmapper.h"

CFX_AndroidFontInfo::CFX_AndroidFontInfo() : m_pFontMgr(nullptr) {}
CFX_AndroidFontInfo::~CFX_AndroidFontInfo() {}
bool CFX_AndroidFontInfo::Init(CFPF_SkiaFontMgr* pFontMgr) {
  if (!pFontMgr)
    return false;

  pFontMgr->LoadSystemFonts();
  m_pFontMgr = pFontMgr;
  return true;
}

bool CFX_AndroidFontInfo::EnumFontList(CFX_FontMapper* pMapper) {
  return false;
}

void* CFX_AndroidFontInfo::MapFont(int weight,
                                   bool bItalic,
                                   int charset,
                                   int pitch_family,
                                   const FX_CHAR* face,
                                   int& iExact) {
  if (!m_pFontMgr)
    return nullptr;

  uint32_t dwStyle = 0;
  if (weight >= 700)
    dwStyle |= FXFONT_BOLD;
  if (bItalic)
    dwStyle |= FXFONT_ITALIC;
  if (pitch_family & FXFONT_FF_FIXEDPITCH)
    dwStyle |= FXFONT_FIXED_PITCH;
  if (pitch_family & FXFONT_FF_SCRIPT)
    dwStyle |= FXFONT_SCRIPT;
  if (pitch_family & FXFONT_FF_ROMAN)
    dwStyle |= FXFONT_SERIF;
  return m_pFontMgr->CreateFont(face, charset, dwStyle,
                                FPF_MATCHFONT_REPLACEANSI);
}

void* CFX_AndroidFontInfo::GetFont(const FX_CHAR* face) {
  return nullptr;
}

uint32_t CFX_AndroidFontInfo::GetFontData(void* hFont,
                                          uint32_t table,
                                          uint8_t* buffer,
                                          uint32_t size) {
  if (!hFont)
    return 0;
  return static_cast<CFPF_SkiaFont*>(hFont)->GetFontData(table, buffer, size);
}

bool CFX_AndroidFontInfo::GetFaceName(void* hFont, CFX_ByteString& name) {
  if (!hFont)
    return false;

  name = static_cast<CFPF_SkiaFont*>(hFont)->GetFamilyName();
  return true;
}

bool CFX_AndroidFontInfo::GetFontCharset(void* hFont, int& charset) {
  if (!hFont)
    return false;

  charset = static_cast<CFPF_SkiaFont*>(hFont)->GetCharset();
  return false;
}

void CFX_AndroidFontInfo::DeleteFont(void* hFont) {
  if (!hFont)
    return;

  static_cast<CFPF_SkiaFont*>(hFont)->Release();
}
