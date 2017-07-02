// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/JS_GlobalData.h"

#include <utility>

#include "core/fdrm/crypto/fx_crypt.h"
#include "third_party/base/stl_util.h"

#define JS_MAXGLOBALDATA (1024 * 4 - 8)

#define READER_JS_GLOBALDATA_FILENAME L"Reader_JsGlobal.Data"
#define PHANTOM_JS_GLOBALDATA_FILENAME L"Phantom_JsGlobal.Data"
#define SDK_JS_GLOBALDATA_FILENAME L"SDK_JsGlobal.Data"

namespace {

const uint8_t JS_RC4KEY[] = {
    0x19, 0xa8, 0xe8, 0x01, 0xf6, 0xa8, 0xb6, 0x4d, 0x82, 0x04, 0x45, 0x6d,
    0xb4, 0xcf, 0xd7, 0x77, 0x67, 0xf9, 0x75, 0x9f, 0xf0, 0xe0, 0x1e, 0x51,
    0xee, 0x46, 0xfd, 0x0b, 0xc9, 0x93, 0x25, 0x55, 0x4a, 0xee, 0xe0, 0x16,
    0xd0, 0xdf, 0x8c, 0xfa, 0x2a, 0xa9, 0x49, 0xfd, 0x97, 0x1c, 0x0e, 0x22,
    0x13, 0x28, 0x7c, 0xaf, 0xc4, 0xfc, 0x9c, 0x12, 0x65, 0x8c, 0x4e, 0x5b,
    0x04, 0x75, 0x89, 0xc9, 0xb1, 0xed, 0x50, 0xca, 0x96, 0x6f, 0x1a, 0x7a,
    0xfe, 0x58, 0x5d, 0xec, 0x19, 0x4a, 0xf6, 0x35, 0x6a, 0x97, 0x14, 0x00,
    0x0e, 0xd0, 0x6b, 0xbb, 0xd5, 0x75, 0x55, 0x8b, 0x6e, 0x6b, 0x19, 0xa0,
    0xf8, 0x77, 0xd5, 0xa3};

// Returns true if non-empty, setting sPropName
bool TrimPropName(CFX_ByteString* sPropName) {
  sPropName->TrimLeft();
  sPropName->TrimRight();
  return sPropName->GetLength() != 0;
}

CJS_GlobalData* g_pInstance = nullptr;

}  // namespace

// static
CJS_GlobalData* CJS_GlobalData::GetRetainedInstance(
    CPDFSDK_FormFillEnvironment* pApp) {
  if (!g_pInstance) {
    g_pInstance = new CJS_GlobalData();
  }
  ++g_pInstance->m_RefCount;
  return g_pInstance;
}

void CJS_GlobalData::Release() {
  if (!--m_RefCount) {
    delete g_pInstance;
    g_pInstance = nullptr;
  }
}

CJS_GlobalData::CJS_GlobalData()
    : m_RefCount(0), m_sFilePath(SDK_JS_GLOBALDATA_FILENAME) {
  LoadGlobalPersistentVariables();
}

CJS_GlobalData::~CJS_GlobalData() {
  SaveGlobalPersisitentVariables();
}

CJS_GlobalData::iterator CJS_GlobalData::FindGlobalVariable(
    const CFX_ByteString& propname) {
  for (auto it = m_arrayGlobalData.begin(); it != m_arrayGlobalData.end();
       ++it) {
    if ((*it)->data.sKey == propname)
      return it;
  }
  return m_arrayGlobalData.end();
}

CJS_GlobalData::const_iterator CJS_GlobalData::FindGlobalVariable(
    const CFX_ByteString& propname) const {
  for (auto it = m_arrayGlobalData.begin(); it != m_arrayGlobalData.end();
       ++it) {
    if ((*it)->data.sKey == propname)
      return it;
  }
  return m_arrayGlobalData.end();
}

CJS_GlobalData_Element* CJS_GlobalData::GetGlobalVariable(
    const CFX_ByteString& propname) {
  auto iter = FindGlobalVariable(propname);
  return iter != m_arrayGlobalData.end() ? iter->get() : nullptr;
}

void CJS_GlobalData::SetGlobalVariableNumber(const CFX_ByteString& propname,
                                             double dData) {
  CFX_ByteString sPropName(propname);
  if (!TrimPropName(&sPropName))
    return;

  if (CJS_GlobalData_Element* pData = GetGlobalVariable(sPropName)) {
    pData->data.nType = JS_GlobalDataType::NUMBER;
    pData->data.dData = dData;
    return;
  }
  std::unique_ptr<CJS_GlobalData_Element> pNewData(new CJS_GlobalData_Element);
  pNewData->data.sKey = sPropName;
  pNewData->data.nType = JS_GlobalDataType::NUMBER;
  pNewData->data.dData = dData;
  m_arrayGlobalData.push_back(std::move(pNewData));
}

void CJS_GlobalData::SetGlobalVariableBoolean(const CFX_ByteString& propname,
                                              bool bData) {
  CFX_ByteString sPropName(propname);
  if (!TrimPropName(&sPropName))
    return;

  if (CJS_GlobalData_Element* pData = GetGlobalVariable(sPropName)) {
    pData->data.nType = JS_GlobalDataType::BOOLEAN;
    pData->data.bData = bData;
    return;
  }
  std::unique_ptr<CJS_GlobalData_Element> pNewData(new CJS_GlobalData_Element);
  pNewData->data.sKey = sPropName;
  pNewData->data.nType = JS_GlobalDataType::BOOLEAN;
  pNewData->data.bData = bData;
  m_arrayGlobalData.push_back(std::move(pNewData));
}

void CJS_GlobalData::SetGlobalVariableString(const CFX_ByteString& propname,
                                             const CFX_ByteString& sData) {
  CFX_ByteString sPropName(propname);
  if (!TrimPropName(&sPropName))
    return;

  if (CJS_GlobalData_Element* pData = GetGlobalVariable(sPropName)) {
    pData->data.nType = JS_GlobalDataType::STRING;
    pData->data.sData = sData;
    return;
  }
  std::unique_ptr<CJS_GlobalData_Element> pNewData(new CJS_GlobalData_Element);
  pNewData->data.sKey = sPropName;
  pNewData->data.nType = JS_GlobalDataType::STRING;
  pNewData->data.sData = sData;
  m_arrayGlobalData.push_back(std::move(pNewData));
}

void CJS_GlobalData::SetGlobalVariableObject(
    const CFX_ByteString& propname,
    const CJS_GlobalVariableArray& array) {
  CFX_ByteString sPropName(propname);
  if (!TrimPropName(&sPropName))
    return;

  if (CJS_GlobalData_Element* pData = GetGlobalVariable(sPropName)) {
    pData->data.nType = JS_GlobalDataType::OBJECT;
    pData->data.objData.Copy(array);
    return;
  }
  std::unique_ptr<CJS_GlobalData_Element> pNewData(new CJS_GlobalData_Element);
  pNewData->data.sKey = sPropName;
  pNewData->data.nType = JS_GlobalDataType::OBJECT;
  pNewData->data.objData.Copy(array);
  m_arrayGlobalData.push_back(std::move(pNewData));
}

void CJS_GlobalData::SetGlobalVariableNull(const CFX_ByteString& propname) {
  CFX_ByteString sPropName(propname);
  if (!TrimPropName(&sPropName))
    return;

  if (CJS_GlobalData_Element* pData = GetGlobalVariable(sPropName)) {
    pData->data.nType = JS_GlobalDataType::NULLOBJ;
    return;
  }
  std::unique_ptr<CJS_GlobalData_Element> pNewData(new CJS_GlobalData_Element);
  pNewData->data.sKey = sPropName;
  pNewData->data.nType = JS_GlobalDataType::NULLOBJ;
  m_arrayGlobalData.push_back(std::move(pNewData));
}

bool CJS_GlobalData::SetGlobalVariablePersistent(const CFX_ByteString& propname,
                                                 bool bPersistent) {
  CFX_ByteString sPropName(propname);
  if (!TrimPropName(&sPropName))
    return false;

  CJS_GlobalData_Element* pData = GetGlobalVariable(sPropName);
  if (!pData)
    return false;

  pData->bPersistent = bPersistent;
  return true;
}

bool CJS_GlobalData::DeleteGlobalVariable(const CFX_ByteString& propname) {
  CFX_ByteString sPropName(propname);
  if (!TrimPropName(&sPropName))
    return false;

  auto iter = FindGlobalVariable(sPropName);
  if (iter == m_arrayGlobalData.end())
    return false;

  m_arrayGlobalData.erase(iter);
  return true;
}

int32_t CJS_GlobalData::GetSize() const {
  return pdfium::CollectionSize<int32_t>(m_arrayGlobalData);
}

CJS_GlobalData_Element* CJS_GlobalData::GetAt(int index) const {
  if (index < 0 || index >= GetSize())
    return nullptr;
  return m_arrayGlobalData[index].get();
}

void CJS_GlobalData::LoadGlobalPersistentVariables() {
  uint8_t* pBuffer = nullptr;
  int32_t nLength = 0;

  LoadFileBuffer(m_sFilePath.c_str(), pBuffer, nLength);
  CRYPT_ArcFourCryptBlock(pBuffer, nLength, JS_RC4KEY, sizeof(JS_RC4KEY));

  if (pBuffer) {
    uint8_t* p = pBuffer;
    uint16_t wType = *((uint16_t*)p);
    p += sizeof(uint16_t);

    if (wType == (uint16_t)(('X' << 8) | 'F')) {
      uint16_t wVersion = *((uint16_t*)p);
      p += sizeof(uint16_t);

      ASSERT(wVersion <= 2);

      uint32_t dwCount = *((uint32_t*)p);
      p += sizeof(uint32_t);

      uint32_t dwSize = *((uint32_t*)p);
      p += sizeof(uint32_t);

      if (dwSize == nLength - sizeof(uint16_t) * 2 - sizeof(uint32_t) * 2) {
        for (int32_t i = 0, sz = dwCount; i < sz; i++) {
          if (p > pBuffer + nLength)
            break;

          uint32_t dwNameLen = *((uint32_t*)p);
          p += sizeof(uint32_t);

          if (p + dwNameLen > pBuffer + nLength)
            break;

          CFX_ByteString sEntry = CFX_ByteString(p, dwNameLen);
          p += sizeof(char) * dwNameLen;

          JS_GlobalDataType wDataType =
              static_cast<JS_GlobalDataType>(*((uint16_t*)p));
          p += sizeof(uint16_t);

          switch (wDataType) {
            case JS_GlobalDataType::NUMBER: {
              double dData = 0;
              switch (wVersion) {
                case 1: {
                  uint32_t dwData = *((uint32_t*)p);
                  p += sizeof(uint32_t);
                  dData = dwData;
                } break;
                case 2: {
                  dData = *((double*)p);
                  p += sizeof(double);
                } break;
              }
              SetGlobalVariableNumber(sEntry, dData);
              SetGlobalVariablePersistent(sEntry, true);
            } break;
            case JS_GlobalDataType::BOOLEAN: {
              uint16_t wData = *((uint16_t*)p);
              p += sizeof(uint16_t);
              SetGlobalVariableBoolean(sEntry, (bool)(wData == 1));
              SetGlobalVariablePersistent(sEntry, true);
            } break;
            case JS_GlobalDataType::STRING: {
              uint32_t dwLength = *((uint32_t*)p);
              p += sizeof(uint32_t);

              if (p + dwLength > pBuffer + nLength)
                break;

              SetGlobalVariableString(sEntry, CFX_ByteString(p, dwLength));
              SetGlobalVariablePersistent(sEntry, true);
              p += sizeof(char) * dwLength;
            } break;
            case JS_GlobalDataType::NULLOBJ: {
              SetGlobalVariableNull(sEntry);
              SetGlobalVariablePersistent(sEntry, true);
            }
            case JS_GlobalDataType::OBJECT:
              break;
          }
        }
      }
    }
    FX_Free(pBuffer);
  }
}

void CJS_GlobalData::SaveGlobalPersisitentVariables() {
  uint32_t nCount = 0;
  CFX_BinaryBuf sData;
  for (const auto& pElement : m_arrayGlobalData) {
    if (pElement->bPersistent) {
      CFX_BinaryBuf sElement;
      MakeByteString(pElement->data.sKey, &pElement->data, sElement);
      if (sData.GetSize() + sElement.GetSize() > JS_MAXGLOBALDATA)
        break;

      sData.AppendBlock(sElement.GetBuffer(), sElement.GetSize());
      nCount++;
    }
  }

  CFX_BinaryBuf sFile;
  uint16_t wType = (uint16_t)(('X' << 8) | 'F');
  sFile.AppendBlock(&wType, sizeof(uint16_t));
  uint16_t wVersion = 2;
  sFile.AppendBlock(&wVersion, sizeof(uint16_t));
  sFile.AppendBlock(&nCount, sizeof(uint32_t));
  uint32_t dwSize = sData.GetSize();
  sFile.AppendBlock(&dwSize, sizeof(uint32_t));

  sFile.AppendBlock(sData.GetBuffer(), sData.GetSize());

  CRYPT_ArcFourCryptBlock(sFile.GetBuffer(), sFile.GetSize(), JS_RC4KEY,
                          sizeof(JS_RC4KEY));
  WriteFileBuffer(m_sFilePath.c_str(), (const FX_CHAR*)sFile.GetBuffer(),
                  sFile.GetSize());
}

void CJS_GlobalData::LoadFileBuffer(const FX_WCHAR* sFilePath,
                                    uint8_t*& pBuffer,
                                    int32_t& nLength) {
  // UnSupport.
}

void CJS_GlobalData::WriteFileBuffer(const FX_WCHAR* sFilePath,
                                     const FX_CHAR* pBuffer,
                                     int32_t nLength) {
  // UnSupport.
}

void CJS_GlobalData::MakeByteString(const CFX_ByteString& name,
                                    CJS_KeyValue* pData,
                                    CFX_BinaryBuf& sData) {
  switch (pData->nType) {
    case JS_GlobalDataType::NUMBER: {
      uint32_t dwNameLen = (uint32_t)name.GetLength();
      sData.AppendBlock(&dwNameLen, sizeof(uint32_t));
      sData.AppendString(name);
      sData.AppendBlock(&pData->nType, sizeof(uint16_t));

      double dData = pData->dData;
      sData.AppendBlock(&dData, sizeof(double));
    } break;
    case JS_GlobalDataType::BOOLEAN: {
      uint32_t dwNameLen = (uint32_t)name.GetLength();
      sData.AppendBlock(&dwNameLen, sizeof(uint32_t));
      sData.AppendString(name);
      sData.AppendBlock(&pData->nType, sizeof(uint16_t));

      uint16_t wData = (uint16_t)pData->bData;
      sData.AppendBlock(&wData, sizeof(uint16_t));
    } break;
    case JS_GlobalDataType::STRING: {
      uint32_t dwNameLen = (uint32_t)name.GetLength();
      sData.AppendBlock(&dwNameLen, sizeof(uint32_t));
      sData.AppendString(name);
      sData.AppendBlock(&pData->nType, sizeof(uint16_t));

      uint32_t dwDataLen = (uint32_t)pData->sData.GetLength();
      sData.AppendBlock(&dwDataLen, sizeof(uint32_t));
      sData.AppendString(pData->sData);
    } break;
    case JS_GlobalDataType::NULLOBJ: {
      uint32_t dwNameLen = (uint32_t)name.GetLength();
      sData.AppendBlock(&dwNameLen, sizeof(uint32_t));
      sData.AppendString(name);
      sData.AppendBlock(&pData->nType, sizeof(uint32_t));
    } break;
    default:
      break;
  }
}
