// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_CFXJSE_ISOLATETRACKER_H_
#define FXJS_CFXJSE_ISOLATETRACKER_H_

#include <map>
#include <memory>
#include <vector>

#include "v8/include/v8.h"

#include "fxjs/cfxjse_runtimedata.h"

class CFXJSE_ScopeUtil_IsolateHandle {
 public:
  explicit CFXJSE_ScopeUtil_IsolateHandle(v8::Isolate* pIsolate)
      : m_isolate(pIsolate), m_iscope(pIsolate), m_hscope(pIsolate) {}
  v8::Isolate* GetIsolate() { return m_isolate; }

 private:
  CFXJSE_ScopeUtil_IsolateHandle(const CFXJSE_ScopeUtil_IsolateHandle&) =
      delete;
  void operator=(const CFXJSE_ScopeUtil_IsolateHandle&) = delete;
  void* operator new(size_t size) = delete;
  void operator delete(void*, size_t) = delete;

  v8::Isolate* m_isolate;
  v8::Isolate::Scope m_iscope;
  v8::HandleScope m_hscope;
};

class CFXJSE_ScopeUtil_IsolateHandleRootContext {
 public:
  explicit CFXJSE_ScopeUtil_IsolateHandleRootContext(v8::Isolate* pIsolate)
      : m_parent(pIsolate),
        m_cscope(v8::Local<v8::Context>::New(
            pIsolate,
            CFXJSE_RuntimeData::Get(pIsolate)->m_hRootContext)) {}

 private:
  CFXJSE_ScopeUtil_IsolateHandleRootContext(
      const CFXJSE_ScopeUtil_IsolateHandleRootContext&) = delete;
  void operator=(const CFXJSE_ScopeUtil_IsolateHandleRootContext&) = delete;
  void* operator new(size_t size) = delete;
  void operator delete(void*, size_t) = delete;

  CFXJSE_ScopeUtil_IsolateHandle m_parent;
  v8::Context::Scope m_cscope;
};

class CFXJSE_IsolateTracker {
 public:
  typedef void (*DisposeCallback)(v8::Isolate*, bool bOwnedIsolate);

  CFXJSE_IsolateTracker();
  ~CFXJSE_IsolateTracker();

  void Append(v8::Isolate* pIsolate,
              std::unique_ptr<v8::ArrayBuffer::Allocator> alloc);
  void Remove(v8::Isolate* pIsolate, DisposeCallback lpfnDisposeCallback);
  void RemoveAll(DisposeCallback lpfnDisposeCallback);

  static CFXJSE_IsolateTracker* g_pInstance;

 protected:
  std::vector<v8::Isolate*> m_OwnedIsolates;
  std::map<v8::Isolate*, std::unique_ptr<v8::ArrayBuffer::Allocator>>
      m_AllocatorMap;
};

#endif  // FXJS_CFXJSE_ISOLATETRACKER_H_
