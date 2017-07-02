// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_JAVASCRIPT_JS_KEYVALUE_H_
#define FPDFSDK_JAVASCRIPT_JS_KEYVALUE_H_

#include <memory>
#include <vector>

#include "core/fxcrt/fx_basic.h"

enum class JS_GlobalDataType { NUMBER = 0, BOOLEAN, STRING, OBJECT, NULLOBJ };

class CJS_KeyValue;

class CJS_GlobalVariableArray {
 public:
  CJS_GlobalVariableArray();
  ~CJS_GlobalVariableArray();

  void Add(CJS_KeyValue* p);
  int Count() const;
  CJS_KeyValue* GetAt(int index) const;
  void Copy(const CJS_GlobalVariableArray& array);

 private:
  std::vector<std::unique_ptr<CJS_KeyValue>> m_Array;
};

class CJS_KeyValue {
 public:
  CJS_KeyValue();
  ~CJS_KeyValue();

  CFX_ByteString sKey;
  JS_GlobalDataType nType;
  double dData;
  bool bData;
  CFX_ByteString sData;
  CJS_GlobalVariableArray objData;
};

#endif  // FPDFSDK_JAVASCRIPT_JS_KEYVALUE_H_
