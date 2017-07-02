// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/JS_KeyValue.h"

CJS_GlobalVariableArray::CJS_GlobalVariableArray() {}

CJS_GlobalVariableArray::~CJS_GlobalVariableArray() {}

void CJS_GlobalVariableArray::Copy(const CJS_GlobalVariableArray& array) {
  m_Array.clear();
  for (int i = 0, sz = array.Count(); i < sz; i++) {
    CJS_KeyValue* pOldObjData = array.GetAt(i);
    switch (pOldObjData->nType) {
      case JS_GlobalDataType::NUMBER: {
        CJS_KeyValue* pNewObjData = new CJS_KeyValue;
        pNewObjData->sKey = pOldObjData->sKey;
        pNewObjData->nType = pOldObjData->nType;
        pNewObjData->dData = pOldObjData->dData;
        Add(pNewObjData);
      } break;
      case JS_GlobalDataType::BOOLEAN: {
        CJS_KeyValue* pNewObjData = new CJS_KeyValue;
        pNewObjData->sKey = pOldObjData->sKey;
        pNewObjData->nType = pOldObjData->nType;
        pNewObjData->bData = pOldObjData->bData;
        Add(pNewObjData);
      } break;
      case JS_GlobalDataType::STRING: {
        CJS_KeyValue* pNewObjData = new CJS_KeyValue;
        pNewObjData->sKey = pOldObjData->sKey;
        pNewObjData->nType = pOldObjData->nType;
        pNewObjData->sData = pOldObjData->sData;
        Add(pNewObjData);
      } break;
      case JS_GlobalDataType::OBJECT: {
        CJS_KeyValue* pNewObjData = new CJS_KeyValue;
        pNewObjData->sKey = pOldObjData->sKey;
        pNewObjData->nType = pOldObjData->nType;
        pNewObjData->objData.Copy(pOldObjData->objData);
        Add(pNewObjData);
      } break;
      case JS_GlobalDataType::NULLOBJ: {
        CJS_KeyValue* pNewObjData = new CJS_KeyValue;
        pNewObjData->sKey = pOldObjData->sKey;
        pNewObjData->nType = pOldObjData->nType;
        Add(pNewObjData);
      } break;
    }
  }
}

void CJS_GlobalVariableArray::Add(CJS_KeyValue* p) {
  m_Array.push_back(std::unique_ptr<CJS_KeyValue>(p));
}

int CJS_GlobalVariableArray::Count() const {
  return m_Array.size();
}

CJS_KeyValue* CJS_GlobalVariableArray::GetAt(int index) const {
  return m_Array.at(index).get();
}

CJS_KeyValue::CJS_KeyValue() {}

CJS_KeyValue::~CJS_KeyValue() {}
