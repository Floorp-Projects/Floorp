// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CORE_FXCRT_CFX_OBSERVABLE_H_
#define CORE_FXCRT_CFX_OBSERVABLE_H_

#include <set>

#include "core/fxcrt/fx_system.h"
#include "third_party/base/stl_util.h"

template <class T>
class CFX_Observable {
 public:
  class ObservedPtr {
   public:
    ObservedPtr() : m_pObservable(nullptr) {}
    explicit ObservedPtr(T* pObservable) : m_pObservable(pObservable) {
      if (m_pObservable)
        m_pObservable->AddObservedPtr(this);
    }
    ObservedPtr(const ObservedPtr& that) : ObservedPtr(that.Get()) {}
    ~ObservedPtr() {
      if (m_pObservable)
        m_pObservable->RemoveObservedPtr(this);
    }
    void Reset(T* pObservable = nullptr) {
      if (m_pObservable)
        m_pObservable->RemoveObservedPtr(this);
      m_pObservable = pObservable;
      if (m_pObservable)
        m_pObservable->AddObservedPtr(this);
    }
    void OnDestroy() {
      ASSERT(m_pObservable);
      m_pObservable = nullptr;
    }
    ObservedPtr& operator=(const ObservedPtr& that) {
      Reset(that.Get());
      return *this;
    }
    bool operator==(const ObservedPtr& that) const {
      return m_pObservable == that.m_pObservable;
    }
    bool operator!=(const ObservedPtr& that) const { return !(*this == that); }
    explicit operator bool() const { return !!m_pObservable; }
    T* Get() const { return m_pObservable; }
    T& operator*() const { return *m_pObservable; }
    T* operator->() const { return m_pObservable; }

   private:
    T* m_pObservable;
  };

  CFX_Observable() {}
  CFX_Observable(const CFX_Observable& that) = delete;
  ~CFX_Observable() { NotifyObservedPtrs(); }
  void AddObservedPtr(ObservedPtr* pObservedPtr) {
    ASSERT(!pdfium::ContainsKey(m_ObservedPtrs, pObservedPtr));
    m_ObservedPtrs.insert(pObservedPtr);
  }
  void RemoveObservedPtr(ObservedPtr* pObservedPtr) {
    ASSERT(pdfium::ContainsKey(m_ObservedPtrs, pObservedPtr));
    m_ObservedPtrs.erase(pObservedPtr);
  }
  void NotifyObservedPtrs() {
    for (auto* pObservedPtr : m_ObservedPtrs)
      pObservedPtr->OnDestroy();
    m_ObservedPtrs.clear();
  }
  CFX_Observable& operator=(const CFX_Observable& that) = delete;

 protected:
  size_t ActiveObservedPtrsForTesting() const { return m_ObservedPtrs.size(); }

 private:
  std::set<ObservedPtr*> m_ObservedPtrs;
};

#endif  // CORE_FXCRT_CFX_OBSERVABLE_H_
