// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_CFXJSE_CLASS_H_
#define FXJS_CFXJSE_CLASS_H_

#include "fxjs/cfxjse_arguments.h"
#include "fxjs/fxjse.h"
#include "v8/include/v8.h"

class CFXJSE_Context;
class CFXJSE_Value;

class CFXJSE_Class {
 public:
  static CFXJSE_Class* Create(CFXJSE_Context* pContext,
                              const FXJSE_CLASS_DESCRIPTOR* lpClassDefintion,
                              bool bIsJSGlobal = false);
  static CFXJSE_Class* GetClassFromContext(CFXJSE_Context* pContext,
                                           const CFX_ByteStringC& szName);
  static void SetUpNamedPropHandler(
      v8::Isolate* pIsolate,
      v8::Local<v8::ObjectTemplate>& hObjectTemplate,
      const FXJSE_CLASS_DESCRIPTOR* lpClassDefinition);

  ~CFXJSE_Class();

  CFXJSE_Context* GetContext() { return m_pContext; }
  v8::Global<v8::FunctionTemplate>& GetTemplate() { return m_hTemplate; }

 protected:
  explicit CFXJSE_Class(CFXJSE_Context* lpContext);

  CFX_ByteString m_szClassName;
  const FXJSE_CLASS_DESCRIPTOR* m_lpClassDefinition;
  CFXJSE_Context* m_pContext;
  v8::Global<v8::FunctionTemplate> m_hTemplate;
  friend class CFXJSE_Context;
  friend class CFXJSE_Value;
};

#endif  // FXJS_CFXJSE_CLASS_H_
