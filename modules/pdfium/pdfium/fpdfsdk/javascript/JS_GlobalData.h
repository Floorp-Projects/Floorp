// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_JAVASCRIPT_JS_GLOBALDATA_H_
#define FPDFSDK_JAVASCRIPT_JS_GLOBALDATA_H_

#include <memory>
#include <vector>

#include "core/fxcrt/fx_basic.h"
#include "fpdfsdk/javascript/JS_KeyValue.h"

class CPDFSDK_FormFillEnvironment;

class CJS_GlobalData_Element {
 public:
  CJS_GlobalData_Element() {}
  ~CJS_GlobalData_Element() {}

  CJS_KeyValue data;
  bool bPersistent;
};

class CJS_GlobalData {
 public:
  static CJS_GlobalData* GetRetainedInstance(CPDFSDK_FormFillEnvironment* pApp);
  void Release();

  void SetGlobalVariableNumber(const CFX_ByteString& propname, double dData);
  void SetGlobalVariableBoolean(const CFX_ByteString& propname, bool bData);
  void SetGlobalVariableString(const CFX_ByteString& propname,
                               const CFX_ByteString& sData);
  void SetGlobalVariableObject(const CFX_ByteString& propname,
                               const CJS_GlobalVariableArray& array);
  void SetGlobalVariableNull(const CFX_ByteString& propname);
  bool SetGlobalVariablePersistent(const CFX_ByteString& propname,
                                   bool bPersistent);
  bool DeleteGlobalVariable(const CFX_ByteString& propname);

  int32_t GetSize() const;
  CJS_GlobalData_Element* GetAt(int index) const;

 private:
  using iterator =
      std::vector<std::unique_ptr<CJS_GlobalData_Element>>::iterator;
  using const_iterator =
      std::vector<std::unique_ptr<CJS_GlobalData_Element>>::const_iterator;

  CJS_GlobalData();
  ~CJS_GlobalData();

  void LoadGlobalPersistentVariables();
  void SaveGlobalPersisitentVariables();

  CJS_GlobalData_Element* GetGlobalVariable(const CFX_ByteString& sPropname);
  iterator FindGlobalVariable(const CFX_ByteString& sPropname);
  const_iterator FindGlobalVariable(const CFX_ByteString& sPropname) const;

  void LoadFileBuffer(const FX_WCHAR* sFilePath,
                      uint8_t*& pBuffer,
                      int32_t& nLength);
  void WriteFileBuffer(const FX_WCHAR* sFilePath,
                       const FX_CHAR* pBuffer,
                       int32_t nLength);
  void MakeByteString(const CFX_ByteString& name,
                      CJS_KeyValue* pData,
                      CFX_BinaryBuf& sData);

  size_t m_RefCount;
  std::vector<std::unique_ptr<CJS_GlobalData_Element>> m_arrayGlobalData;
  CFX_WideString m_sFilePath;
};

#endif  // FPDFSDK_JAVASCRIPT_JS_GLOBALDATA_H_
