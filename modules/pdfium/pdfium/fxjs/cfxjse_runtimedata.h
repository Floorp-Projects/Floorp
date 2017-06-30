// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_CFXJSE_RUNTIMEDATA_H_
#define FXJS_CFXJSE_RUNTIMEDATA_H_

#include <memory>

#include "v8/include/v8.h"

class CFXJSE_RuntimeList;

class CFXJSE_RuntimeData {
 public:
  ~CFXJSE_RuntimeData();

  static CFXJSE_RuntimeData* Get(v8::Isolate* pIsolate);

  v8::Isolate* m_pIsolate;
  v8::Global<v8::FunctionTemplate> m_hRootContextGlobalTemplate;
  v8::Global<v8::Context> m_hRootContext;

 protected:
  explicit CFXJSE_RuntimeData(v8::Isolate* pIsolate);

  static std::unique_ptr<CFXJSE_RuntimeData> Create(v8::Isolate* pIsolate);

 private:
  CFXJSE_RuntimeData(const CFXJSE_RuntimeData&) = delete;
  CFXJSE_RuntimeData& operator=(const CFXJSE_RuntimeData&) = delete;
};

#endif  // FXJS_CFXJSE_RUNTIMEDATA_H_
