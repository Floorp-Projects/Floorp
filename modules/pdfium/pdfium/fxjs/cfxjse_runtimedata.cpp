// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/cfxjse_runtimedata.h"

#include <utility>

#include "fxjs/cfxjse_isolatetracker.h"
#include "fxjs/fxjs_v8.h"

namespace {

// Duplicates fpdfsdk's cjs_runtime.h, but keeps XFA from depending on it.
// TODO(tsepez): make a single version of this.
class FXJSE_ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
  void* Allocate(size_t length) override { return calloc(1, length); }
  void* AllocateUninitialized(size_t length) override { return malloc(length); }
  void Free(void* data, size_t length) override { free(data); }
};

void Runtime_DisposeCallback(v8::Isolate* pIsolate, bool bOwned) {
  if (FXJS_PerIsolateData* pData = FXJS_PerIsolateData::Get(pIsolate))
    delete pData;
  if (bOwned)
    pIsolate->Dispose();
}

void KillV8() {
  v8::V8::Dispose();
}

}  // namespace

void FXJSE_Initialize() {
  if (!CFXJSE_IsolateTracker::g_pInstance)
    CFXJSE_IsolateTracker::g_pInstance = new CFXJSE_IsolateTracker;

  static bool bV8Initialized = false;
  if (bV8Initialized)
    return;

  bV8Initialized = true;
  atexit(KillV8);
}

void FXJSE_Finalize() {
  if (!CFXJSE_IsolateTracker::g_pInstance)
    return;

  CFXJSE_IsolateTracker::g_pInstance->RemoveAll(Runtime_DisposeCallback);
  delete CFXJSE_IsolateTracker::g_pInstance;
  CFXJSE_IsolateTracker::g_pInstance = nullptr;
}

v8::Isolate* FXJSE_Runtime_Create_Own() {
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      new FXJSE_ArrayBufferAllocator());
  v8::Isolate::CreateParams params;
  params.array_buffer_allocator = allocator.get();
  v8::Isolate* pIsolate = v8::Isolate::New(params);
  ASSERT(pIsolate && CFXJSE_IsolateTracker::g_pInstance);
  CFXJSE_IsolateTracker::g_pInstance->Append(pIsolate, std::move(allocator));
  return pIsolate;
}

void FXJSE_Runtime_Release(v8::Isolate* pIsolate) {
  if (!pIsolate)
    return;
  CFXJSE_IsolateTracker::g_pInstance->Remove(pIsolate, Runtime_DisposeCallback);
}

CFXJSE_RuntimeData::CFXJSE_RuntimeData(v8::Isolate* pIsolate)
    : m_pIsolate(pIsolate) {}

CFXJSE_RuntimeData::~CFXJSE_RuntimeData() {}

std::unique_ptr<CFXJSE_RuntimeData> CFXJSE_RuntimeData::Create(
    v8::Isolate* pIsolate) {
  std::unique_ptr<CFXJSE_RuntimeData> pRuntimeData(
      new CFXJSE_RuntimeData(pIsolate));
  CFXJSE_ScopeUtil_IsolateHandle scope(pIsolate);
  v8::Local<v8::FunctionTemplate> hFuncTemplate =
      v8::FunctionTemplate::New(pIsolate);
  v8::Local<v8::ObjectTemplate> hGlobalTemplate =
      hFuncTemplate->InstanceTemplate();
  hGlobalTemplate->Set(
      v8::Symbol::GetToStringTag(pIsolate),
      v8::String::NewFromUtf8(pIsolate, "global", v8::NewStringType::kNormal)
          .ToLocalChecked());
  v8::Local<v8::Context> hContext =
      v8::Context::New(pIsolate, 0, hGlobalTemplate);
  hContext->SetSecurityToken(v8::External::New(pIsolate, pIsolate));
  pRuntimeData->m_hRootContextGlobalTemplate.Reset(pIsolate, hFuncTemplate);
  pRuntimeData->m_hRootContext.Reset(pIsolate, hContext);
  return pRuntimeData;
}

CFXJSE_RuntimeData* CFXJSE_RuntimeData::Get(v8::Isolate* pIsolate) {
  FXJS_PerIsolateData::SetUp(pIsolate);
  FXJS_PerIsolateData* pData = FXJS_PerIsolateData::Get(pIsolate);
  if (!pData->m_pFXJSERuntimeData)
    pData->m_pFXJSERuntimeData = CFXJSE_RuntimeData::Create(pIsolate);
  return pData->m_pFXJSERuntimeData.get();
}

CFXJSE_IsolateTracker* CFXJSE_IsolateTracker::g_pInstance = nullptr;
