// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PARSER_CPDF_ARRAY_H_
#define CORE_FPDFAPI_PARSER_CPDF_ARRAY_H_

#include <memory>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/fpdfapi/parser/cpdf_indirect_object_holder.h"
#include "core/fpdfapi/parser/cpdf_object.h"
#include "core/fxcrt/fx_basic.h"
#include "core/fxcrt/fx_coordinates.h"
#include "third_party/base/ptr_util.h"

class CPDF_Array : public CPDF_Object {
 public:
  using const_iterator =
      std::vector<std::unique_ptr<CPDF_Object>>::const_iterator;

  CPDF_Array();
  explicit CPDF_Array(const CFX_WeakPtr<CFX_ByteStringPool>& pPool);
  ~CPDF_Array() override;

  // CPDF_Object:
  Type GetType() const override;
  std::unique_ptr<CPDF_Object> Clone() const override;
  bool IsArray() const override;
  CPDF_Array* AsArray() override;
  const CPDF_Array* AsArray() const override;

  bool IsEmpty() const { return m_Objects.empty(); }
  size_t GetCount() const { return m_Objects.size(); }
  CPDF_Object* GetObjectAt(size_t index) const;
  CPDF_Object* GetDirectObjectAt(size_t index) const;
  CFX_ByteString GetStringAt(size_t index) const;
  int GetIntegerAt(size_t index) const;
  FX_FLOAT GetNumberAt(size_t index) const;
  CPDF_Dictionary* GetDictAt(size_t index) const;
  CPDF_Stream* GetStreamAt(size_t index) const;
  CPDF_Array* GetArrayAt(size_t index) const;
  FX_FLOAT GetFloatAt(size_t index) const { return GetNumberAt(index); }
  CFX_Matrix GetMatrix();
  CFX_FloatRect GetRect();

  // Takes ownership of |pObj|, returns unowned pointer to it.
  CPDF_Object* Add(std::unique_ptr<CPDF_Object> pObj);
  CPDF_Object* SetAt(size_t index, std::unique_ptr<CPDF_Object> pObj);
  CPDF_Object* InsertAt(size_t index, std::unique_ptr<CPDF_Object> pObj);

  // Creates object owned by the array, returns unowned pointer to it.
  // We have special cases for objects that can intern strings from
  // a ByteStringPool.
  template <typename T, typename... Args>
  typename std::enable_if<!CanInternStrings<T>::value, T*>::type AddNew(
      Args&&... args) {
    return static_cast<T*>(
        Add(pdfium::MakeUnique<T>(std::forward<Args>(args)...)));
  }
  template <typename T, typename... Args>
  typename std::enable_if<CanInternStrings<T>::value, T*>::type AddNew(
      Args&&... args) {
    return static_cast<T*>(
        Add(pdfium::MakeUnique<T>(m_pPool, std::forward<Args>(args)...)));
  }
  template <typename T, typename... Args>
  typename std::enable_if<!CanInternStrings<T>::value, T*>::type SetNewAt(
      size_t index,
      Args&&... args) {
    return static_cast<T*>(
        SetAt(index, pdfium::MakeUnique<T>(std::forward<Args>(args)...)));
  }
  template <typename T, typename... Args>
  typename std::enable_if<CanInternStrings<T>::value, T*>::type SetNewAt(
      size_t index,
      Args&&... args) {
    return static_cast<T*>(SetAt(
        index, pdfium::MakeUnique<T>(m_pPool, std::forward<Args>(args)...)));
  }
  template <typename T, typename... Args>
  typename std::enable_if<!CanInternStrings<T>::value, T*>::type InsertNewAt(
      size_t index,
      Args&&... args) {
    return static_cast<T*>(
        InsertAt(index, pdfium::MakeUnique<T>(std::forward<Args>(args)...)));
  }
  template <typename T, typename... Args>
  typename std::enable_if<CanInternStrings<T>::value, T*>::type InsertNewAt(
      size_t index,
      Args&&... args) {
    return static_cast<T*>(InsertAt(
        index, pdfium::MakeUnique<T>(m_pPool, std::forward<Args>(args)...)));
  }

  void RemoveAt(size_t index, size_t nCount = 1);
  void ConvertToIndirectObjectAt(size_t index, CPDF_IndirectObjectHolder* pDoc);

  const_iterator begin() const { return m_Objects.begin(); }
  const_iterator end() const { return m_Objects.end(); }

 protected:
  std::unique_ptr<CPDF_Object> CloneNonCyclic(
      bool bDirect,
      std::set<const CPDF_Object*>* pVisited) const override;

  std::vector<std::unique_ptr<CPDF_Object>> m_Objects;
  CFX_WeakPtr<CFX_ByteStringPool> m_pPool;
};

inline CPDF_Array* ToArray(CPDF_Object* obj) {
  return obj ? obj->AsArray() : nullptr;
}

inline const CPDF_Array* ToArray(const CPDF_Object* obj) {
  return obj ? obj->AsArray() : nullptr;
}

inline std::unique_ptr<CPDF_Array> ToArray(std::unique_ptr<CPDF_Object> obj) {
  CPDF_Array* pArray = ToArray(obj.get());
  if (!pArray)
    return nullptr;
  obj.release();
  return std::unique_ptr<CPDF_Array>(pArray);
}

#endif  // CORE_FPDFAPI_PARSER_CPDF_ARRAY_H_
