// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CORE_FXCRT_CFX_MAYBE_OWNED_H_
#define CORE_FXCRT_CFX_MAYBE_OWNED_H_

#include <algorithm>
#include <memory>
#include <utility>

#include "core/fxcrt/fx_memory.h"
#include "core/fxcrt/fx_system.h"

// A template that can hold either owned or unowned references, and cleans up
// appropriately.  Possibly the most pernicious anti-pattern imaginable, but
// it crops up throughout the codebase due to a desire to avoid copying-in
// objects or data.
template <typename T, typename D = std::default_delete<T>>
class CFX_MaybeOwned {
 public:
  CFX_MaybeOwned() : m_pObj(nullptr) {}
  explicit CFX_MaybeOwned(T* ptr) : m_pObj(ptr) {}
  explicit CFX_MaybeOwned(std::unique_ptr<T, D> ptr)
      : m_pOwnedObj(std::move(ptr)), m_pObj(m_pOwnedObj.get()) {}

  CFX_MaybeOwned(const CFX_MaybeOwned& that) = delete;
  CFX_MaybeOwned(CFX_MaybeOwned&& that)
      : m_pOwnedObj(that.m_pOwnedObj.release()), m_pObj(that.m_pObj) {
    that.m_pObj = nullptr;
  }

  void Reset(std::unique_ptr<T, D> ptr) {
    m_pOwnedObj = std::move(ptr);
    m_pObj = m_pOwnedObj.get();
  }
  void Reset(T* ptr = nullptr) {
    m_pOwnedObj.reset();
    m_pObj = ptr;
  }

  bool IsOwned() const { return !!m_pOwnedObj; }
  T* Get() const { return m_pObj; }
  std::unique_ptr<T, D> Release() {
    ASSERT(IsOwned());
    return std::move(m_pOwnedObj);
  }

  CFX_MaybeOwned& operator=(const CFX_MaybeOwned& that) = delete;
  CFX_MaybeOwned& operator=(CFX_MaybeOwned&& that) {
    m_pOwnedObj = std::move(that.m_pOwnedObj);
    m_pObj = that.m_pObj;
    that.m_pObj = nullptr;
    return *this;
  }
  CFX_MaybeOwned& operator=(T* ptr) {
    Reset(ptr);
    return *this;
  }
  CFX_MaybeOwned& operator=(std::unique_ptr<T, D> ptr) {
    Reset(std::move(ptr));
    return *this;
  }

  bool operator==(const CFX_MaybeOwned& that) const {
    return Get() == that.Get();
  }
  bool operator==(const std::unique_ptr<T, D>& ptr) const {
    return Get() == ptr.get();
  }
  bool operator==(T* ptr) const { return Get() == ptr; }

  bool operator!=(const CFX_MaybeOwned& that) const { return !(*this == that); }
  bool operator!=(const std::unique_ptr<T, D> ptr) const {
    return !(*this == ptr);
  }
  bool operator!=(T* ptr) const { return !(*this == ptr); }

  explicit operator bool() const { return !!m_pObj; }
  T& operator*() const { return *m_pObj; }
  T* operator->() const { return m_pObj; }

 private:
  std::unique_ptr<T, D> m_pOwnedObj;
  T* m_pObj;
};

#endif  // CORE_FXCRT_CFX_MAYBE_OWNED_H_
