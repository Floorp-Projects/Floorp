// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PARSER_CPDF_BOOLEAN_H_
#define CORE_FPDFAPI_PARSER_CPDF_BOOLEAN_H_

#include <memory>

#include "core/fpdfapi/parser/cpdf_object.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"

class CPDF_Boolean : public CPDF_Object {
 public:
  CPDF_Boolean();
  explicit CPDF_Boolean(bool value);
  ~CPDF_Boolean() override;

  // CPDF_Object:
  Type GetType() const override;
  std::unique_ptr<CPDF_Object> Clone() const override;
  CFX_ByteString GetString() const override;
  int GetInteger() const override;
  void SetString(const CFX_ByteString& str) override;
  bool IsBoolean() const override;
  CPDF_Boolean* AsBoolean() override;
  const CPDF_Boolean* AsBoolean() const override;

 protected:
  bool m_bValue;
};

inline CPDF_Boolean* ToBoolean(CPDF_Object* obj) {
  return obj ? obj->AsBoolean() : nullptr;
}

inline const CPDF_Boolean* ToBoolean(const CPDF_Object* obj) {
  return obj ? obj->AsBoolean() : nullptr;
}

#endif  // CORE_FPDFAPI_PARSER_CPDF_BOOLEAN_H_
