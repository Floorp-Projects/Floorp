// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PARSER_CPDF_NAME_H_
#define CORE_FPDFAPI_PARSER_CPDF_NAME_H_

#include <memory>

#include "core/fpdfapi/parser/cpdf_object.h"
#include "core/fxcrt/cfx_string_pool_template.h"
#include "core/fxcrt/cfx_weak_ptr.h"

class CPDF_Name : public CPDF_Object {
 public:
  CPDF_Name(CFX_WeakPtr<CFX_ByteStringPool> pPool, const CFX_ByteString& str);
  ~CPDF_Name() override;

  // CPDF_Object:
  Type GetType() const override;
  std::unique_ptr<CPDF_Object> Clone() const override;
  CFX_ByteString GetString() const override;
  CFX_WideString GetUnicodeText() const override;
  void SetString(const CFX_ByteString& str) override;
  bool IsName() const override;
  CPDF_Name* AsName() override;
  const CPDF_Name* AsName() const override;

 protected:
  CFX_ByteString m_Name;
};

inline CPDF_Name* ToName(CPDF_Object* obj) {
  return obj ? obj->AsName() : nullptr;
}

inline const CPDF_Name* ToName(const CPDF_Object* obj) {
  return obj ? obj->AsName() : nullptr;
}

#endif  // CORE_FPDFAPI_PARSER_CPDF_NAME_H_
