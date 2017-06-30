// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/parser/cpdf_array.h"

#include <set>
#include <utility>

#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "third_party/base/logging.h"
#include "third_party/base/stl_util.h"

CPDF_Array::CPDF_Array() {}

CPDF_Array::CPDF_Array(const CFX_WeakPtr<CFX_ByteStringPool>& pPool)
    : m_pPool(pPool) {}

CPDF_Array::~CPDF_Array() {
  // Break cycles for cyclic references.
  m_ObjNum = kInvalidObjNum;
  for (auto& it : m_Objects) {
    if (it && it->GetObjNum() == kInvalidObjNum)
      it.release();
  }
}

CPDF_Object::Type CPDF_Array::GetType() const {
  return ARRAY;
}

bool CPDF_Array::IsArray() const {
  return true;
}

CPDF_Array* CPDF_Array::AsArray() {
  return this;
}

const CPDF_Array* CPDF_Array::AsArray() const {
  return this;
}

std::unique_ptr<CPDF_Object> CPDF_Array::Clone() const {
  return CloneObjectNonCyclic(false);
}

std::unique_ptr<CPDF_Object> CPDF_Array::CloneNonCyclic(
    bool bDirect,
    std::set<const CPDF_Object*>* pVisited) const {
  pVisited->insert(this);
  auto pCopy = pdfium::MakeUnique<CPDF_Array>();
  for (const auto& pValue : m_Objects) {
    if (!pdfium::ContainsKey(*pVisited, pValue.get()))
      pCopy->m_Objects.push_back(pValue->CloneNonCyclic(bDirect, pVisited));
  }
  return std::move(pCopy);
}

CFX_FloatRect CPDF_Array::GetRect() {
  CFX_FloatRect rect;
  if (!IsArray() || m_Objects.size() != 4)
    return rect;

  rect.left = GetNumberAt(0);
  rect.bottom = GetNumberAt(1);
  rect.right = GetNumberAt(2);
  rect.top = GetNumberAt(3);
  return rect;
}

CFX_Matrix CPDF_Array::GetMatrix() {
  CFX_Matrix matrix;
  if (!IsArray() || m_Objects.size() != 6)
    return CFX_Matrix();

  return CFX_Matrix(GetNumberAt(0), GetNumberAt(1), GetNumberAt(2),
                    GetNumberAt(3), GetNumberAt(4), GetNumberAt(5));
}

CPDF_Object* CPDF_Array::GetObjectAt(size_t i) const {
  if (i >= m_Objects.size())
    return nullptr;
  return m_Objects[i].get();
}

CPDF_Object* CPDF_Array::GetDirectObjectAt(size_t i) const {
  if (i >= m_Objects.size())
    return nullptr;
  return m_Objects[i]->GetDirect();
}

CFX_ByteString CPDF_Array::GetStringAt(size_t i) const {
  if (i >= m_Objects.size())
    return CFX_ByteString();
  return m_Objects[i]->GetString();
}

int CPDF_Array::GetIntegerAt(size_t i) const {
  if (i >= m_Objects.size())
    return 0;
  return m_Objects[i]->GetInteger();
}

FX_FLOAT CPDF_Array::GetNumberAt(size_t i) const {
  if (i >= m_Objects.size())
    return 0;
  return m_Objects[i]->GetNumber();
}

CPDF_Dictionary* CPDF_Array::GetDictAt(size_t i) const {
  CPDF_Object* p = GetDirectObjectAt(i);
  if (!p)
    return nullptr;
  if (CPDF_Dictionary* pDict = p->AsDictionary())
    return pDict;
  if (CPDF_Stream* pStream = p->AsStream())
    return pStream->GetDict();
  return nullptr;
}

CPDF_Stream* CPDF_Array::GetStreamAt(size_t i) const {
  return ToStream(GetDirectObjectAt(i));
}

CPDF_Array* CPDF_Array::GetArrayAt(size_t i) const {
  return ToArray(GetDirectObjectAt(i));
}

void CPDF_Array::RemoveAt(size_t i, size_t nCount) {
  if (i >= m_Objects.size())
    return;

  if (nCount <= 0 || nCount > m_Objects.size() - i)
    return;

  m_Objects.erase(m_Objects.begin() + i, m_Objects.begin() + i + nCount);
}

void CPDF_Array::ConvertToIndirectObjectAt(size_t i,
                                           CPDF_IndirectObjectHolder* pHolder) {
  if (i >= m_Objects.size())
    return;

  if (!m_Objects[i] || m_Objects[i]->IsReference())
    return;

  CPDF_Object* pNew = pHolder->AddIndirectObject(std::move(m_Objects[i]));
  m_Objects[i] = pdfium::MakeUnique<CPDF_Reference>(pHolder, pNew->GetObjNum());
}

CPDF_Object* CPDF_Array::SetAt(size_t i, std::unique_ptr<CPDF_Object> pObj) {
  ASSERT(IsArray());
  ASSERT(!pObj || pObj->IsInline());
  if (i >= m_Objects.size()) {
    ASSERT(false);
    return nullptr;
  }
  CPDF_Object* pRet = pObj.get();
  m_Objects[i] = std::move(pObj);
  return pRet;
}

CPDF_Object* CPDF_Array::InsertAt(size_t index,
                                  std::unique_ptr<CPDF_Object> pObj) {
  ASSERT(IsArray());
  CHECK(!pObj || pObj->IsInline());
  CPDF_Object* pRet = pObj.get();
  if (index >= m_Objects.size()) {
    // Allocate space first.
    m_Objects.resize(index + 1);
    m_Objects[index] = std::move(pObj);
  } else {
    // Directly insert.
    m_Objects.insert(m_Objects.begin() + index, std::move(pObj));
  }
  return pRet;
}

CPDF_Object* CPDF_Array::Add(std::unique_ptr<CPDF_Object> pObj) {
  ASSERT(IsArray());
  CHECK(!pObj || pObj->IsInline());
  CPDF_Object* pRet = pObj.get();
  m_Objects.push_back(std::move(pObj));
  return pRet;
}
