// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PARSER_CPDF_REFERENCE_H_
#define CORE_FPDFAPI_PARSER_CPDF_REFERENCE_H_

#include <memory>
#include <set>

#include "core/fpdfapi/parser/cpdf_object.h"

class CPDF_IndirectObjectHolder;

class CPDF_Reference : public CPDF_Object {
 public:
  CPDF_Reference(CPDF_IndirectObjectHolder* pDoc, uint32_t objnum);
  ~CPDF_Reference() override;

  // CPDF_Object:
  Type GetType() const override;
  std::unique_ptr<CPDF_Object> Clone() const override;
  CPDF_Object* GetDirect() const override;
  CFX_ByteString GetString() const override;
  FX_FLOAT GetNumber() const override;
  int GetInteger() const override;
  CPDF_Dictionary* GetDict() const override;
  bool IsReference() const override;
  CPDF_Reference* AsReference() override;
  const CPDF_Reference* AsReference() const override;

  CPDF_IndirectObjectHolder* GetObjList() const { return m_pObjList; }
  uint32_t GetRefObjNum() const { return m_RefObjNum; }

  void SetRef(CPDF_IndirectObjectHolder* pDoc, uint32_t objnum);

 protected:
  std::unique_ptr<CPDF_Object> CloneNonCyclic(
      bool bDirect,
      std::set<const CPDF_Object*>* pVisited) const override;
  CPDF_Object* SafeGetDirect() const;

  CPDF_IndirectObjectHolder* m_pObjList;
  uint32_t m_RefObjNum;
};

inline CPDF_Reference* ToReference(CPDF_Object* obj) {
  return obj ? obj->AsReference() : nullptr;
}

inline const CPDF_Reference* ToReference(const CPDF_Object* obj) {
  return obj ? obj->AsReference() : nullptr;
}

#endif  // CORE_FPDFAPI_PARSER_CPDF_REFERENCE_H_
