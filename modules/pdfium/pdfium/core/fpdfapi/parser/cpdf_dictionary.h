// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PARSER_CPDF_DICTIONARY_H_
#define CORE_FPDFAPI_PARSER_CPDF_DICTIONARY_H_

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "core/fpdfapi/parser/cpdf_object.h"
#include "core/fxcrt/cfx_string_pool_template.h"
#include "core/fxcrt/cfx_weak_ptr.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_string.h"
#include "third_party/base/ptr_util.h"

class CPDF_IndirectObjectHolder;

class CPDF_Dictionary : public CPDF_Object {
 public:
  using const_iterator =
      std::map<CFX_ByteString, std::unique_ptr<CPDF_Object>>::const_iterator;

  CPDF_Dictionary();
  explicit CPDF_Dictionary(const CFX_WeakPtr<CFX_ByteStringPool>& pPool);
  ~CPDF_Dictionary() override;

  // CPDF_Object:
  Type GetType() const override;
  std::unique_ptr<CPDF_Object> Clone() const override;
  CPDF_Dictionary* GetDict() const override;
  bool IsDictionary() const override;
  CPDF_Dictionary* AsDictionary() override;
  const CPDF_Dictionary* AsDictionary() const override;

  size_t GetCount() const { return m_Map.size(); }
  CPDF_Object* GetObjectFor(const CFX_ByteString& key) const;
  CPDF_Object* GetDirectObjectFor(const CFX_ByteString& key) const;
  CFX_ByteString GetStringFor(const CFX_ByteString& key) const;
  CFX_ByteString GetStringFor(const CFX_ByteString& key,
                              const CFX_ByteString& default_str) const;
  CFX_WideString GetUnicodeTextFor(const CFX_ByteString& key) const;
  int GetIntegerFor(const CFX_ByteString& key) const;
  int GetIntegerFor(const CFX_ByteString& key, int default_int) const;
  bool GetBooleanFor(const CFX_ByteString& key, bool bDefault = false) const;
  FX_FLOAT GetNumberFor(const CFX_ByteString& key) const;
  CPDF_Dictionary* GetDictFor(const CFX_ByteString& key) const;
  CPDF_Stream* GetStreamFor(const CFX_ByteString& key) const;
  CPDF_Array* GetArrayFor(const CFX_ByteString& key) const;
  CFX_FloatRect GetRectFor(const CFX_ByteString& key) const;
  CFX_Matrix GetMatrixFor(const CFX_ByteString& key) const;
  FX_FLOAT GetFloatFor(const CFX_ByteString& key) const {
    return GetNumberFor(key);
  }

  bool KeyExist(const CFX_ByteString& key) const;
  bool IsSignatureDict() const;

  // Set* functions invalidate iterators for the element with the key |key|.
  // Takes ownership of |pObj|, returns an unowned pointer to it.
  CPDF_Object* SetFor(const CFX_ByteString& key,
                      std::unique_ptr<CPDF_Object> pObj);

  // Creates a new object owned by the dictionary and returns an unowned
  // pointer to it.
  template <typename T, typename... Args>
  typename std::enable_if<!CanInternStrings<T>::value, T*>::type SetNewFor(
      const CFX_ByteString& key,
      Args&&... args) {
    return static_cast<T*>(
        SetFor(key, pdfium::MakeUnique<T>(std::forward<Args>(args)...)));
  }
  template <typename T, typename... Args>
  typename std::enable_if<CanInternStrings<T>::value, T*>::type SetNewFor(
      const CFX_ByteString& key,
      Args&&... args) {
    return static_cast<T*>(SetFor(
        key, pdfium::MakeUnique<T>(m_pPool, std::forward<Args>(args)...)));
  }

  // Convenience functions to convert native objects to array form.
  void SetRectFor(const CFX_ByteString& key, const CFX_FloatRect& rect);
  void SetMatrixFor(const CFX_ByteString& key, const CFX_Matrix& matrix);

  void ConvertToIndirectObjectFor(const CFX_ByteString& key,
                                  CPDF_IndirectObjectHolder* pHolder);

  // Invalidates iterators for the element with the key |key|.
  void RemoveFor(const CFX_ByteString& key);

  // Invalidates iterators for the element with the key |oldkey|.
  void ReplaceKey(const CFX_ByteString& oldkey, const CFX_ByteString& newkey);

  const_iterator begin() const { return m_Map.begin(); }
  const_iterator end() const { return m_Map.end(); }

  CFX_WeakPtr<CFX_ByteStringPool> GetByteStringPool() const { return m_pPool; }

 protected:
  CFX_ByteString MaybeIntern(const CFX_ByteString& str);
  std::unique_ptr<CPDF_Object> CloneNonCyclic(
      bool bDirect,
      std::set<const CPDF_Object*>* visited) const override;

  CFX_WeakPtr<CFX_ByteStringPool> m_pPool;
  std::map<CFX_ByteString, std::unique_ptr<CPDF_Object>> m_Map;
};

inline CPDF_Dictionary* ToDictionary(CPDF_Object* obj) {
  return obj ? obj->AsDictionary() : nullptr;
}

inline const CPDF_Dictionary* ToDictionary(const CPDF_Object* obj) {
  return obj ? obj->AsDictionary() : nullptr;
}

inline std::unique_ptr<CPDF_Dictionary> ToDictionary(
    std::unique_ptr<CPDF_Object> obj) {
  CPDF_Dictionary* pDict = ToDictionary(obj.get());
  if (!pDict)
    return nullptr;
  obj.release();
  return std::unique_ptr<CPDF_Dictionary>(pDict);
}

#endif  // CORE_FPDFAPI_PARSER_CPDF_DICTIONARY_H_
