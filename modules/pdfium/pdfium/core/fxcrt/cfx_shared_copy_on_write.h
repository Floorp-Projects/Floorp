// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCRT_CFX_SHARED_COPY_ON_WRITE_H_
#define CORE_FXCRT_CFX_SHARED_COPY_ON_WRITE_H_

#include "core/fxcrt/cfx_retain_ptr.h"
#include "core/fxcrt/fx_system.h"

// A shared object with Copy on Write semantics that makes it appear as
// if each one were independent.
template <class ObjClass>
class CFX_SharedCopyOnWrite {
 public:
  CFX_SharedCopyOnWrite() {}
  CFX_SharedCopyOnWrite(const CFX_SharedCopyOnWrite& other)
      : m_pObject(other.m_pObject) {}
  ~CFX_SharedCopyOnWrite() {}

  template <typename... Args>
  ObjClass* Emplace(Args... params) {
    m_pObject.Reset(new CountedObj(params...));
    return m_pObject.Get();
  }

  CFX_SharedCopyOnWrite& operator=(const CFX_SharedCopyOnWrite& that) {
    if (*this != that)
      m_pObject = that.m_pObject;
    return *this;
  }

  void SetNull() { m_pObject.Reset(); }
  const ObjClass* GetObject() const { return m_pObject.Get(); }

  template <typename... Args>
  ObjClass* GetPrivateCopy(Args... params) {
    if (!m_pObject)
      return Emplace(params...);
    if (!m_pObject->HasOneRef())
      m_pObject.Reset(new CountedObj(*m_pObject));
    return m_pObject.Get();
  }

  bool operator==(const CFX_SharedCopyOnWrite& that) const {
    return m_pObject == that.m_pObject;
  }
  bool operator!=(const CFX_SharedCopyOnWrite& that) const {
    return !(*this == that);
  }
  explicit operator bool() const { return !!m_pObject; }

 private:
  class CountedObj : public ObjClass {
   public:
    template <typename... Args>
    // NOLINTNEXTLINE(runtime/explicit)
    CountedObj(Args... params) : ObjClass(params...), m_RefCount(0) {}

    CountedObj(const CountedObj& src) : ObjClass(src), m_RefCount(0) {}
    ~CountedObj() { m_RefCount = 0; }

    bool HasOneRef() const { return m_RefCount == 1; }
    void Retain() { m_RefCount++; }
    void Release() {
      ASSERT(m_RefCount);
      if (--m_RefCount == 0)
        delete this;
    }

   private:
    // To ensure ref counts do not overflow, consider the worst possible case:
    // the entire address space contains nothing but pointers to this object.
    // Since the count increments with each new pointer, the largest value is
    // the number of pointers that can fit into the address space. The size of
    // the address space itself is a good upper bound on it.
    intptr_t m_RefCount;
  };

  CFX_RetainPtr<CountedObj> m_pObject;
};

#endif  // CORE_FXCRT_CFX_SHARED_COPY_ON_WRITE_H_
