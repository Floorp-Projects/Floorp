// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_filespec.h"

#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_object.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fxcrt/fx_system.h"

namespace {

#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_ || \
    _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
CFX_WideString ChangeSlashToPlatform(const FX_WCHAR* str) {
  CFX_WideString result;
  while (*str) {
    if (*str == '/') {
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
      result += ':';
#else
      result += '\\';
#endif
    } else {
      result += *str;
    }
    str++;
  }
  return result;
}

CFX_WideString ChangeSlashToPDF(const FX_WCHAR* str) {
  CFX_WideString result;
  while (*str) {
    if (*str == '\\' || *str == ':')
      result += '/';
    else
      result += *str;

    str++;
  }
  return result;
}
#endif  // _FXM_PLATFORM_APPLE_ || _FXM_PLATFORM_WINDOWS_

}  // namespace

CFX_WideString CPDF_FileSpec::DecodeFileName(const CFX_WideStringC& filepath) {
  if (filepath.GetLength() <= 1)
    return CFX_WideString();

#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  if (filepath.Left(sizeof("/Mac") - 1) == CFX_WideStringC(L"/Mac"))
    return ChangeSlashToPlatform(filepath.c_str() + 1);
  return ChangeSlashToPlatform(filepath.c_str());
#elif _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_

  if (filepath.GetAt(0) != '/')
    return ChangeSlashToPlatform(filepath.c_str());
  if (filepath.GetAt(1) == '/')
    return ChangeSlashToPlatform(filepath.c_str() + 1);
  if (filepath.GetAt(2) == '/') {
    CFX_WideString result;
    result += filepath.GetAt(1);
    result += ':';
    result += ChangeSlashToPlatform(filepath.c_str() + 2);
    return result;
  }
  CFX_WideString result;
  result += '\\';
  result += ChangeSlashToPlatform(filepath.c_str());
  return result;
#else
  return CFX_WideString(filepath);
#endif
}

bool CPDF_FileSpec::GetFileName(CFX_WideString* csFileName) const {
  if (CPDF_Dictionary* pDict = m_pObj->AsDictionary()) {
    *csFileName = pDict->GetUnicodeTextFor("UF");
    if (csFileName->IsEmpty()) {
      *csFileName =
          CFX_WideString::FromLocal(pDict->GetStringFor("F").AsStringC());
    }
    if (pDict->GetStringFor("FS") == "URL")
      return true;
    if (csFileName->IsEmpty()) {
      if (pDict->KeyExist("DOS")) {
        *csFileName =
            CFX_WideString::FromLocal(pDict->GetStringFor("DOS").AsStringC());
      } else if (pDict->KeyExist("Mac")) {
        *csFileName =
            CFX_WideString::FromLocal(pDict->GetStringFor("Mac").AsStringC());
      } else if (pDict->KeyExist("Unix")) {
        *csFileName =
            CFX_WideString::FromLocal(pDict->GetStringFor("Unix").AsStringC());
      } else {
        return false;
      }
    }
  } else if (m_pObj->IsString()) {
    *csFileName = CFX_WideString::FromLocal(m_pObj->GetString().AsStringC());
  } else {
    return false;
  }
  *csFileName = DecodeFileName(csFileName->AsStringC());
  return true;
}

CPDF_FileSpec::CPDF_FileSpec(const CFX_WeakPtr<CFX_ByteStringPool>& pPool) {
  m_pObj = new CPDF_Dictionary(pPool);
  m_pObj->AsDictionary()->SetNewFor<CPDF_Name>("Type", "Filespec");
}

CFX_WideString CPDF_FileSpec::EncodeFileName(const CFX_WideStringC& filepath) {
  if (filepath.GetLength() <= 1)
    return CFX_WideString();

#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
  if (filepath.GetAt(1) == ':') {
    CFX_WideString result;
    result = '/';
    result += filepath.GetAt(0);
    if (filepath.GetAt(2) != '\\')
      result += '/';

    result += ChangeSlashToPDF(filepath.c_str() + 2);
    return result;
  }
  if (filepath.GetAt(0) == '\\' && filepath.GetAt(1) == '\\')
    return ChangeSlashToPDF(filepath.c_str() + 1);

  if (filepath.GetAt(0) == '\\') {
    CFX_WideString result;
    result = '/';
    result += ChangeSlashToPDF(filepath.c_str());
    return result;
  }
  return ChangeSlashToPDF(filepath.c_str());
#elif _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  if (filepath.Left(sizeof("Mac") - 1) == L"Mac") {
    CFX_WideString result;
    result = '/';
    result += ChangeSlashToPDF(filepath.c_str());
    return result;
  }
  return ChangeSlashToPDF(filepath.c_str());
#else
  return CFX_WideString(filepath);
#endif
}

void CPDF_FileSpec::SetFileName(const CFX_WideStringC& wsFileName) {
  if (!m_pObj)
    return;

  CFX_WideString wsStr = EncodeFileName(wsFileName);
  if (m_pObj->IsString()) {
    m_pObj->SetString(CFX_ByteString::FromUnicode(wsStr));
  } else if (CPDF_Dictionary* pDict = m_pObj->AsDictionary()) {
    pDict->SetNewFor<CPDF_String>("F", CFX_ByteString::FromUnicode(wsStr),
                                  false);
    pDict->SetNewFor<CPDF_String>("UF", PDF_EncodeText(wsStr), false);
  }
}
