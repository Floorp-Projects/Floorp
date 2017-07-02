// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCRT_CFX_WEAK_PTR_H_
#define CORE_FXCRT_CFX_WEAK_PTR_H_

#include <cstddef>
#include <memory>
#include <utility>

#include "core/fxcrt/cfx_retain_ptr.h"
#include "core/fxcrt/fx_system.h"

template <class T, class D = std::default_delete<T>>
class CFX_WeakPtr {
 public:
  CFX_WeakPtr() {}
  CFX_WeakPtr(const CFX_WeakPtr& that) : m_pHandle(that.m_pHandle) {}
  CFX_WeakPtr(CFX_WeakPtr&& that) { Swap(that); }
  explicit CFX_WeakPtr(std::unique_ptr<T, D> pObj)
      : m_pHandle(new Handle(std::move(pObj))) {}

  // Deliberately implicit to allow passing nullptr.
  // NOLINTNEXTLINE(runtime/explicit)
  CFX_WeakPtr(std::nullptr_t arg) {}

  explicit operator bool() const { return m_pHandle && !!m_pHandle->Get(); }
  bool HasOneRef() const { return m_pHandle && m_pHandle->HasOneRef(); }
  T* operator->() { return m_pHandle->Get(); }
  const T* operator->() const { return m_pHandle->Get(); }
  CFX_WeakPtr& operator=(const CFX_WeakPtr& that) {
    m_pHandle = that.m_pHandle;
    return *this;
  }
  bool operator==(const CFX_WeakPtr& that) const {
    return m_pHandle == that.m_pHandle;
  }
  bool operator!=(const CFX_WeakPtr& that) const { return !(*this == that); }

  T* Get() const { return m_pHandle ? m_pHandle->Get() : nullptr; }
  void DeleteObject() {
    if (m_pHandle) {
      m_pHandle->Clear();
      m_pHandle.Reset();
    }
  }
  void Reset() { m_pHandle.Reset(); }
  void Reset(std::unique_ptr<T, D> pObj) {
    m_pHandle.Reset(new Handle(std::move(pObj)));
  }
  void Swap(CFX_WeakPtr& that) { m_pHandle.Swap(that.m_pHandle); }

 private:
  class Handle {
   public:
    explicit Handle(std::unique_ptr<T, D> ptr)
        : m_nCount(0), m_pObj(std::move(ptr)) {}
    void Reset(std::unique_ptr<T, D> ptr) { m_pObj = std::move(ptr); }
    void Clear() {     // Now you're all weak ptrs ...
      m_pObj.reset();  // unique_ptr nulls first before invoking delete.
    }
    T* Get() const { return m_pObj.get(); }
    T* Retain() {
      ++m_nCount;
      return m_pObj.get();
    }
    void Release() {
      if (--m_nCount == 0)
        delete this;
    }
    bool HasOneRef() const { return m_nCount == 1; }

   private:
    ~Handle() {}

    intptr_t m_nCount;
    std::unique_ptr<T, D> m_pObj;
  };

  CFX_RetainPtr<Handle> m_pHandle;
};

#endif  // CORE_FXCRT_CFX_WEAK_PTR_H_
