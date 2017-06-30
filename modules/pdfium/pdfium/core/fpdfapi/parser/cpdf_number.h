// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PARSER_CPDF_NUMBER_H_
#define CORE_FPDFAPI_PARSER_CPDF_NUMBER_H_

#include <memory>

#include "core/fpdfapi/parser/cpdf_object.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"

class CPDF_Number : public CPDF_Object {
 public:
  CPDF_Number();
  explicit CPDF_Number(int value);
  explicit CPDF_Number(FX_FLOAT value);
  explicit CPDF_Number(const CFX_ByteStringC& str);
  ~CPDF_Number() override;

  // CPDF_Object:
  Type GetType() const override;
  std::unique_ptr<CPDF_Object> Clone() const override;
  CFX_ByteString GetString() const override;
  FX_FLOAT GetNumber() const override;
  int GetInteger() const override;
  void SetString(const CFX_ByteString& str) override;
  bool IsNumber() const override;
  CPDF_Number* AsNumber() override;
  const CPDF_Number* AsNumber() const override;

  bool IsInteger() const { return m_bInteger; }

 protected:
  bool m_bInteger;
  union {
    int m_Integer;
    FX_FLOAT m_Float;
  };
};

inline CPDF_Number* ToNumber(CPDF_Object* obj) {
  return obj ? obj->AsNumber() : nullptr;
}

inline const CPDF_Number* ToNumber(const CPDF_Object* obj) {
  return obj ? obj->AsNumber() : nullptr;
}

#endif  // CORE_FPDFAPI_PARSER_CPDF_NUMBER_H_
