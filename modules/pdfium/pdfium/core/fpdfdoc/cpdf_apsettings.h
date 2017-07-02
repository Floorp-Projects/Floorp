// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_APSETTINGS_H_
#define CORE_FPDFDOC_CPDF_APSETTINGS_H_

#include "core/fpdfdoc/cpdf_iconfit.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxge/fx_dib.h"

class CPDF_Dictionary;
class CPDF_FormControl;
class CPDF_Stream;

class CPDF_ApSettings {
 public:
  explicit CPDF_ApSettings(CPDF_Dictionary* pDict);

  bool HasMKEntry(const CFX_ByteString& csEntry) const;
  int GetRotation() const;

  FX_ARGB GetBorderColor(int& iColorType) const {
    return GetColor(iColorType, "BC");
  }

  FX_FLOAT GetOriginalBorderColor(int index) const {
    return GetOriginalColor(index, "BC");
  }

  void GetOriginalBorderColor(int& iColorType, FX_FLOAT fc[4]) const {
    GetOriginalColor(iColorType, fc, "BC");
  }

  FX_ARGB GetBackgroundColor(int& iColorType) const {
    return GetColor(iColorType, "BG");
  }

  FX_FLOAT GetOriginalBackgroundColor(int index) const {
    return GetOriginalColor(index, "BG");
  }

  void GetOriginalBackgroundColor(int& iColorType, FX_FLOAT fc[4]) const {
    GetOriginalColor(iColorType, fc, "BG");
  }

  CFX_WideString GetNormalCaption() const { return GetCaption("CA"); }
  CFX_WideString GetRolloverCaption() const { return GetCaption("RC"); }
  CFX_WideString GetDownCaption() const { return GetCaption("AC"); }
  CPDF_Stream* GetNormalIcon() const { return GetIcon("I"); }
  CPDF_Stream* GetRolloverIcon() const { return GetIcon("RI"); }
  CPDF_Stream* GetDownIcon() const { return GetIcon("IX"); }
  CPDF_IconFit GetIconFit() const;
  int GetTextPosition() const;

 private:
  friend class CPDF_FormControl;

  FX_ARGB GetColor(int& iColorType, const CFX_ByteString& csEntry) const;
  FX_FLOAT GetOriginalColor(int index, const CFX_ByteString& csEntry) const;
  void GetOriginalColor(int& iColorType,
                        FX_FLOAT fc[4],
                        const CFX_ByteString& csEntry) const;

  CFX_WideString GetCaption(const CFX_ByteString& csEntry) const;
  CPDF_Stream* GetIcon(const CFX_ByteString& csEntry) const;

  CPDF_Dictionary* const m_pDict;
};

#endif  // CORE_FPDFDOC_CPDF_APSETTINGS_H_
