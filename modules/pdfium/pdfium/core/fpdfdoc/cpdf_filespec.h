// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_FILESPEC_H_
#define CORE_FPDFDOC_CPDF_FILESPEC_H_

#include "core/fxcrt/cfx_string_pool_template.h"
#include "core/fxcrt/cfx_weak_ptr.h"
#include "core/fxcrt/fx_string.h"

class CPDF_Object;

class CPDF_FileSpec {
 public:
  explicit CPDF_FileSpec(const CFX_WeakPtr<CFX_ByteStringPool>& pPool);
  explicit CPDF_FileSpec(CPDF_Object* pObj) : m_pObj(pObj) {}

  // Convert a platform dependent file name into pdf format.
  static CFX_WideString EncodeFileName(const CFX_WideStringC& filepath);

  // Convert a pdf file name into platform dependent format.
  static CFX_WideString DecodeFileName(const CFX_WideStringC& filepath);

  CPDF_Object* GetObj() const { return m_pObj; }
  bool GetFileName(CFX_WideString* wsFileName) const;

  // Set this file spec to refer to a file name (not a url).
  void SetFileName(const CFX_WideStringC& wsFileName);

 private:
  CPDF_Object* m_pObj;
};

#endif  // CORE_FPDFDOC_CPDF_FILESPEC_H_
