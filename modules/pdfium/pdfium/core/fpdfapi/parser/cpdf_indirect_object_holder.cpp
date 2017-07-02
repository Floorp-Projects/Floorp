// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/parser/cpdf_indirect_object_holder.h"

#include <algorithm>
#include <utility>

#include "core/fpdfapi/parser/cpdf_object.h"
#include "core/fpdfapi/parser/cpdf_parser.h"
#include "third_party/base/logging.h"

CPDF_IndirectObjectHolder::CPDF_IndirectObjectHolder()
    : m_LastObjNum(0),
      m_pByteStringPool(pdfium::MakeUnique<CFX_ByteStringPool>()) {}

CPDF_IndirectObjectHolder::~CPDF_IndirectObjectHolder() {
  m_pByteStringPool.DeleteObject();  // Make weak.
}

CPDF_Object* CPDF_IndirectObjectHolder::GetIndirectObject(
    uint32_t objnum) const {
  auto it = m_IndirectObjs.find(objnum);
  return it != m_IndirectObjs.end() ? it->second.get() : nullptr;
}

CPDF_Object* CPDF_IndirectObjectHolder::GetOrParseIndirectObject(
    uint32_t objnum) {
  if (objnum == 0)
    return nullptr;

  CPDF_Object* pObj = GetIndirectObject(objnum);
  if (pObj)
    return pObj->GetObjNum() != CPDF_Object::kInvalidObjNum ? pObj : nullptr;

  std::unique_ptr<CPDF_Object> pNewObj = ParseIndirectObject(objnum);
  if (!pNewObj)
    return nullptr;

  pNewObj->m_ObjNum = objnum;
  m_LastObjNum = std::max(m_LastObjNum, objnum);
  m_IndirectObjs[objnum] = std::move(pNewObj);
  return m_IndirectObjs[objnum].get();
}

std::unique_ptr<CPDF_Object> CPDF_IndirectObjectHolder::ParseIndirectObject(
    uint32_t objnum) {
  return nullptr;
}

CPDF_Object* CPDF_IndirectObjectHolder::AddIndirectObject(
    std::unique_ptr<CPDF_Object> pObj) {
  CHECK(!pObj->m_ObjNum);
  CPDF_Object* pUnowned = pObj.get();
  pObj->m_ObjNum = ++m_LastObjNum;
  if (m_IndirectObjs[m_LastObjNum])
    m_OrphanObjs.push_back(std::move(m_IndirectObjs[m_LastObjNum]));

  m_IndirectObjs[m_LastObjNum] = std::move(pObj);
  return pUnowned;
}

bool CPDF_IndirectObjectHolder::ReplaceIndirectObjectIfHigherGeneration(
    uint32_t objnum,
    std::unique_ptr<CPDF_Object> pObj) {
  ASSERT(objnum);
  if (!pObj)
    return false;

  CPDF_Object* pOldObj = GetIndirectObject(objnum);
  if (pOldObj && pObj->GetGenNum() <= pOldObj->GetGenNum())
    return false;

  pObj->m_ObjNum = objnum;
  m_IndirectObjs[objnum] = std::move(pObj);
  m_LastObjNum = std::max(m_LastObjNum, objnum);
  return true;
}

void CPDF_IndirectObjectHolder::DeleteIndirectObject(uint32_t objnum) {
  CPDF_Object* pObj = GetIndirectObject(objnum);
  if (!pObj || pObj->GetObjNum() == CPDF_Object::kInvalidObjNum)
    return;

  m_IndirectObjs.erase(objnum);
}
