// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/edit/editint.h"

#include <memory>
#include <vector>

#include "core/fpdfapi/edit/cpdf_creator.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_crypto_handler.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_parser.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_security_handler.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fxcrt/cfx_maybe_owned.h"
#include "core/fxcrt/fx_ext.h"
#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"

#define PDF_OBJECTSTREAM_MAXLENGTH (256 * 1024)
#define PDF_XREFSTREAM_MAXSIZE 10000

#define FX_GETBYTEOFFSET32(a) 0
#define FX_GETBYTEOFFSET40(a) 0
#define FX_GETBYTEOFFSET48(a) 0
#define FX_GETBYTEOFFSET56(a) 0
#define FX_GETBYTEOFFSET24(a) ((uint8_t)(a >> 24))
#define FX_GETBYTEOFFSET16(a) ((uint8_t)(a >> 16))
#define FX_GETBYTEOFFSET8(a) ((uint8_t)(a >> 8))
#define FX_GETBYTEOFFSET0(a) ((uint8_t)(a))

// TODO(ochang): Make helper for appending "objnum 0 R ".

namespace {

int32_t PDF_CreatorAppendObject(const CPDF_Object* pObj,
                                CFX_FileBufferArchive* pFile,
                                FX_FILESIZE& offset) {
  int32_t len = 0;
  if (!pObj) {
    if (pFile->AppendString(" null") < 0) {
      return -1;
    }
    offset += 5;
    return 1;
  }
  switch (pObj->GetType()) {
    case CPDF_Object::NULLOBJ:
      if (pFile->AppendString(" null") < 0) {
        return -1;
      }
      offset += 5;
      break;
    case CPDF_Object::BOOLEAN:
    case CPDF_Object::NUMBER:
      if (pFile->AppendString(" ") < 0) {
        return -1;
      }
      if ((len = pFile->AppendString(pObj->GetString().AsStringC())) < 0) {
        return -1;
      }
      offset += len + 1;
      break;
    case CPDF_Object::STRING: {
      CFX_ByteString str = pObj->GetString();
      bool bHex = pObj->AsString()->IsHex();
      if ((len = pFile->AppendString(PDF_EncodeString(str, bHex).AsStringC())) <
          0) {
        return -1;
      }
      offset += len;
      break;
    }
    case CPDF_Object::NAME: {
      if (pFile->AppendString("/") < 0) {
        return -1;
      }
      CFX_ByteString str = pObj->GetString();
      if ((len = pFile->AppendString(PDF_NameEncode(str).AsStringC())) < 0) {
        return -1;
      }
      offset += len + 1;
      break;
    }
    case CPDF_Object::REFERENCE: {
      if (pFile->AppendString(" ") < 0)
        return -1;
      if ((len = pFile->AppendDWord(pObj->AsReference()->GetRefObjNum())) < 0)
        return -1;
      if (pFile->AppendString(" 0 R ") < 0)
        return -1;
      offset += len + 6;
      break;
    }
    case CPDF_Object::ARRAY: {
      if (pFile->AppendString("[") < 0) {
        return -1;
      }
      offset += 1;
      const CPDF_Array* p = pObj->AsArray();
      for (size_t i = 0; i < p->GetCount(); i++) {
        CPDF_Object* pElement = p->GetObjectAt(i);
        if (!pElement->IsInline()) {
          if (pFile->AppendString(" ") < 0) {
            return -1;
          }
          if ((len = pFile->AppendDWord(pElement->GetObjNum())) < 0) {
            return -1;
          }
          if (pFile->AppendString(" 0 R") < 0) {
            return -1;
          }
          offset += len + 5;
        } else {
          if (PDF_CreatorAppendObject(pElement, pFile, offset) < 0) {
            return -1;
          }
        }
      }
      if (pFile->AppendString("]") < 0) {
        return -1;
      }
      offset += 1;
      break;
    }
    case CPDF_Object::DICTIONARY: {
      if (pFile->AppendString("<<") < 0) {
        return -1;
      }
      offset += 2;
      const CPDF_Dictionary* p = pObj->AsDictionary();
      for (const auto& it : *p) {
        const CFX_ByteString& key = it.first;
        CPDF_Object* pValue = it.second.get();
        if (pFile->AppendString("/") < 0) {
          return -1;
        }
        if ((len = pFile->AppendString(PDF_NameEncode(key).AsStringC())) < 0) {
          return -1;
        }
        offset += len + 1;
        if (!pValue->IsInline()) {
          if (pFile->AppendString(" ") < 0) {
            return -1;
          }
          if ((len = pFile->AppendDWord(pValue->GetObjNum())) < 0) {
            return -1;
          }
          if (pFile->AppendString(" 0 R") < 0) {
            return -1;
          }
          offset += len + 5;
        } else {
          if (PDF_CreatorAppendObject(pValue, pFile, offset) < 0) {
            return -1;
          }
        }
      }
      if (pFile->AppendString(">>") < 0) {
        return -1;
      }
      offset += 2;
      break;
    }
    case CPDF_Object::STREAM: {
      const CPDF_Stream* p = pObj->AsStream();
      if (PDF_CreatorAppendObject(p->GetDict(), pFile, offset) < 0) {
        return -1;
      }
      if (pFile->AppendString("stream\r\n") < 0) {
        return -1;
      }
      offset += 8;
      CPDF_StreamAcc acc;
      acc.LoadAllData(p, true);
      if (pFile->AppendBlock(acc.GetData(), acc.GetSize()) < 0) {
        return -1;
      }
      offset += acc.GetSize();
      if ((len = pFile->AppendString("\r\nendstream")) < 0) {
        return -1;
      }
      offset += len;
      break;
    }
    default:
      ASSERT(false);
      break;
  }
  return 1;
}

int32_t PDF_CreatorWriteTrailer(CPDF_Document* pDocument,
                                CFX_FileBufferArchive* pFile,
                                CPDF_Array* pIDArray) {
  FX_FILESIZE offset = 0;
  int32_t len = 0;
  CPDF_Parser* pParser = pDocument->GetParser();
  if (pParser) {
    CPDF_Dictionary* p = pParser->GetTrailer();
    for (const auto& it : *p) {
      const CFX_ByteString& key = it.first;
      CPDF_Object* pValue = it.second.get();
      if (key == "Encrypt" || key == "Size" || key == "Filter" ||
          key == "Index" || key == "Length" || key == "Prev" || key == "W" ||
          key == "XRefStm" || key == "Type" || key == "ID") {
        continue;
      }
      if (key == "DecodeParms")
        continue;

      if (pFile->AppendString(("/")) < 0) {
        return -1;
      }
      if ((len = pFile->AppendString(PDF_NameEncode(key).AsStringC())) < 0) {
        return -1;
      }
      offset += len + 1;
      if (!pValue->IsInline()) {
        if (pFile->AppendString(" ") < 0) {
          return -1;
        }
        if ((len = pFile->AppendDWord(pValue->GetObjNum())) < 0) {
          return -1;
        }
        if (pFile->AppendString(" 0 R ") < 0) {
          return -1;
        }
        offset += len + 6;
      } else {
        if (PDF_CreatorAppendObject(pValue, pFile, offset) < 0) {
          return -1;
        }
      }
    }
    if (pIDArray) {
      if (pFile->AppendString(("/ID")) < 0) {
        return -1;
      }
      offset += 3;
      if (PDF_CreatorAppendObject(pIDArray, pFile, offset) < 0) {
        return -1;
      }
    }
    return offset;
  }
  if (pFile->AppendString("\r\n/Root ") < 0) {
    return -1;
  }
  if ((len = pFile->AppendDWord(pDocument->GetRoot()->GetObjNum())) < 0) {
    return -1;
  }
  if (pFile->AppendString(" 0 R\r\n") < 0) {
    return -1;
  }
  offset += len + 14;
  if (pDocument->GetInfo()) {
    if (pFile->AppendString("/Info ") < 0) {
      return -1;
    }
    if ((len = pFile->AppendDWord(pDocument->GetInfo()->GetObjNum())) < 0) {
      return -1;
    }
    if (pFile->AppendString(" 0 R\r\n") < 0) {
      return -1;
    }
    offset += len + 12;
  }
  if (pIDArray) {
    if (pFile->AppendString(("/ID")) < 0) {
      return -1;
    }
    offset += 3;
    if (PDF_CreatorAppendObject(pIDArray, pFile, offset) < 0) {
      return -1;
    }
  }
  return offset;
}

int32_t PDF_CreatorWriteEncrypt(const CPDF_Dictionary* pEncryptDict,
                                uint32_t dwObjNum,
                                CFX_FileBufferArchive* pFile) {
  if (!pEncryptDict) {
    return 0;
  }
  ASSERT(pFile);
  FX_FILESIZE offset = 0;
  int32_t len = 0;
  if (pFile->AppendString("/Encrypt") < 0) {
    return -1;
  }
  offset += 8;
  if (pFile->AppendString(" ") < 0) {
    return -1;
  }
  if ((len = pFile->AppendDWord(dwObjNum)) < 0) {
    return -1;
  }
  if (pFile->AppendString(" 0 R ") < 0) {
    return -1;
  }
  offset += len + 6;
  return offset;
}

std::vector<uint8_t> PDF_GenerateFileID(uint32_t dwSeed1, uint32_t dwSeed2) {
  std::vector<uint8_t> buffer(sizeof(uint32_t) * 4);
  uint32_t* pBuffer = reinterpret_cast<uint32_t*>(buffer.data());
  void* pContext = FX_Random_MT_Start(dwSeed1);
  for (int i = 0; i < 2; ++i)
    *pBuffer++ = FX_Random_MT_Generate(pContext);
  FX_Random_MT_Close(pContext);
  pContext = FX_Random_MT_Start(dwSeed2);
  for (int i = 0; i < 2; ++i)
    *pBuffer++ = FX_Random_MT_Generate(pContext);
  FX_Random_MT_Close(pContext);
  return buffer;
}

void AppendIndex0(CFX_ByteTextBuf& buffer, bool bFirstObject) {
  buffer.AppendByte(0);
  buffer.AppendByte(0);
  buffer.AppendByte(0);
  buffer.AppendByte(0);
  buffer.AppendByte(0);
  const uint8_t byte = bFirstObject ? 0xFF : 0;
  buffer.AppendByte(byte);
  buffer.AppendByte(byte);
}

void AppendIndex1(CFX_ByteTextBuf& buffer, FX_FILESIZE offset) {
  buffer.AppendByte(1);
  buffer.AppendByte(FX_GETBYTEOFFSET24(offset));
  buffer.AppendByte(FX_GETBYTEOFFSET16(offset));
  buffer.AppendByte(FX_GETBYTEOFFSET8(offset));
  buffer.AppendByte(FX_GETBYTEOFFSET0(offset));
  buffer.AppendByte(0);
  buffer.AppendByte(0);
}

void AppendIndex2(CFX_ByteTextBuf& buffer, uint32_t objnum, int32_t index) {
  buffer.AppendByte(2);
  buffer.AppendByte(FX_GETBYTEOFFSET24(objnum));
  buffer.AppendByte(FX_GETBYTEOFFSET16(objnum));
  buffer.AppendByte(FX_GETBYTEOFFSET8(objnum));
  buffer.AppendByte(FX_GETBYTEOFFSET0(objnum));
  buffer.AppendByte(FX_GETBYTEOFFSET8(index));
  buffer.AppendByte(FX_GETBYTEOFFSET0(index));
}

bool IsXRefNeedEnd(CPDF_XRefStream* pXRef, uint32_t flag) {
  if (!(flag & FPDFCREATE_INCREMENTAL))
    return false;

  uint32_t iCount = 0;
  for (const auto& pair : pXRef->m_IndexArray)
    iCount += pair.count;

  return iCount >= PDF_XREFSTREAM_MAXSIZE;
}

int32_t OutputIndex(CFX_FileBufferArchive* pFile, FX_FILESIZE offset) {
  if (sizeof(offset) > 4) {
    if (FX_GETBYTEOFFSET32(offset)) {
      if (pFile->AppendByte(FX_GETBYTEOFFSET56(offset)) < 0)
        return -1;
      if (pFile->AppendByte(FX_GETBYTEOFFSET48(offset)) < 0)
        return -1;
      if (pFile->AppendByte(FX_GETBYTEOFFSET40(offset)) < 0)
        return -1;
      if (pFile->AppendByte(FX_GETBYTEOFFSET32(offset)) < 0)
        return -1;
    }
  }
  if (pFile->AppendByte(FX_GETBYTEOFFSET24(offset)) < 0)
    return -1;
  if (pFile->AppendByte(FX_GETBYTEOFFSET16(offset)) < 0)
    return -1;
  if (pFile->AppendByte(FX_GETBYTEOFFSET8(offset)) < 0)
    return -1;
  if (pFile->AppendByte(FX_GETBYTEOFFSET0(offset)) < 0)
    return -1;
  if (pFile->AppendByte(0) < 0)
    return -1;
  return 0;
}

class CPDF_FlateEncoder {
 public:
  CPDF_FlateEncoder(CPDF_Stream* pStream, bool bFlateEncode);
  CPDF_FlateEncoder(const uint8_t* pBuffer,
                    uint32_t size,
                    bool bFlateEncode,
                    bool bXRefStream);
  ~CPDF_FlateEncoder();

  void CloneDict();

  uint32_t m_dwSize;
  CFX_MaybeOwned<uint8_t, FxFreeDeleter> m_pData;
  CFX_MaybeOwned<CPDF_Dictionary> m_pDict;
  CPDF_StreamAcc m_Acc;
};

void CPDF_FlateEncoder::CloneDict() {
  if (m_pDict.IsOwned())
    return;
  m_pDict = ToDictionary(m_pDict->Clone());
  ASSERT(m_pDict.IsOwned());
}

CPDF_FlateEncoder::CPDF_FlateEncoder(CPDF_Stream* pStream, bool bFlateEncode)
    : m_dwSize(0) {
  m_Acc.LoadAllData(pStream, true);
  bool bHasFilter = pStream && pStream->HasFilter();
  if (bHasFilter && !bFlateEncode) {
    CPDF_StreamAcc destAcc;
    destAcc.LoadAllData(pStream);
    m_dwSize = destAcc.GetSize();
    m_pData = destAcc.DetachData();
    m_pDict = ToDictionary(pStream->GetDict()->Clone());
    m_pDict->RemoveFor("Filter");
    return;
  }
  if (bHasFilter || !bFlateEncode) {
    m_pData = const_cast<uint8_t*>(m_Acc.GetData());
    m_dwSize = m_Acc.GetSize();
    m_pDict = pStream->GetDict();
    return;
  }
  // TODO(thestig): Move to Init() and check return value.
  uint8_t* buffer = nullptr;
  ::FlateEncode(m_Acc.GetData(), m_Acc.GetSize(), &buffer, &m_dwSize);
  m_pData = std::unique_ptr<uint8_t, FxFreeDeleter>(buffer);
  m_pDict = ToDictionary(pStream->GetDict()->Clone());
  m_pDict->SetNewFor<CPDF_Number>("Length", static_cast<int>(m_dwSize));
  m_pDict->SetNewFor<CPDF_Name>("Filter", "FlateDecode");
  m_pDict->RemoveFor("DecodeParms");
}

CPDF_FlateEncoder::CPDF_FlateEncoder(const uint8_t* pBuffer,
                                     uint32_t size,
                                     bool bFlateEncode,
                                     bool bXRefStream)
    : m_dwSize(0) {
  if (!bFlateEncode) {
    m_pData = const_cast<uint8_t*>(pBuffer);
    m_dwSize = size;
    return;
  }
  uint8_t* buffer = nullptr;
  // TODO(thestig): Move to Init() and check return value.
  if (bXRefStream)
    ::PngEncode(pBuffer, size, &buffer, &m_dwSize);
  else
    ::FlateEncode(pBuffer, size, &buffer, &m_dwSize);
  m_pData = std::unique_ptr<uint8_t, FxFreeDeleter>(buffer);
}

CPDF_FlateEncoder::~CPDF_FlateEncoder() {}

class CPDF_Encryptor {
 public:
  CPDF_Encryptor(CPDF_CryptoHandler* pHandler,
                 int objnum,
                 uint8_t* src_data,
                 uint32_t src_size);
  ~CPDF_Encryptor();

  uint8_t* m_pData;
  uint32_t m_dwSize;
  bool m_bNewBuf;
};

CPDF_Encryptor::CPDF_Encryptor(CPDF_CryptoHandler* pHandler,
                               int objnum,
                               uint8_t* src_data,
                               uint32_t src_size)
    : m_pData(nullptr), m_dwSize(0), m_bNewBuf(false) {
  if (src_size == 0)
    return;

  if (!pHandler) {
    m_pData = (uint8_t*)src_data;
    m_dwSize = src_size;
    return;
  }
  m_dwSize = pHandler->EncryptGetSize(objnum, 0, src_data, src_size);
  m_pData = FX_Alloc(uint8_t, m_dwSize);
  pHandler->EncryptContent(objnum, 0, src_data, src_size, m_pData, m_dwSize);
  m_bNewBuf = true;
}

CPDF_Encryptor::~CPDF_Encryptor() {
  if (m_bNewBuf)
    FX_Free(m_pData);
}

}  // namespace

CPDF_ObjectStream::CPDF_ObjectStream() : m_dwObjNum(0), m_index(0) {}

CPDF_ObjectStream::~CPDF_ObjectStream() {}

void CPDF_ObjectStream::Start() {
  m_Items.clear();
  m_Buffer.Clear();
  m_dwObjNum = 0;
  m_index = 0;
}

void CPDF_ObjectStream::CompressIndirectObject(uint32_t dwObjNum,
                                               const CPDF_Object* pObj) {
  m_Items.push_back({dwObjNum, m_Buffer.GetLength()});
  m_Buffer << pObj;
}

void CPDF_ObjectStream::CompressIndirectObject(uint32_t dwObjNum,
                                               const uint8_t* pBuffer,
                                               uint32_t dwSize) {
  m_Items.push_back({dwObjNum, m_Buffer.GetLength()});
  m_Buffer.AppendBlock(pBuffer, dwSize);
}

FX_FILESIZE CPDF_ObjectStream::End(CPDF_Creator* pCreator) {
  ASSERT(pCreator);
  if (m_Items.empty())
    return 0;

  CFX_FileBufferArchive* pFile = &pCreator->m_File;
  FX_FILESIZE ObjOffset = pCreator->m_Offset;
  if (!m_dwObjNum) {
    m_dwObjNum = ++pCreator->m_dwLastObjNum;
  }
  CFX_ByteTextBuf tempBuffer;
  for (const auto& pair : m_Items)
    tempBuffer << pair.objnum << " " << pair.offset << " ";

  FX_FILESIZE& offset = pCreator->m_Offset;
  int32_t len = pFile->AppendDWord(m_dwObjNum);
  if (len < 0) {
    return -1;
  }
  offset += len;
  if ((len = pFile->AppendString(" 0 obj\r\n<</Type /ObjStm /N ")) < 0) {
    return -1;
  }
  offset += len;
  uint32_t iCount = pdfium::CollectionSize<uint32_t>(m_Items);
  if ((len = pFile->AppendDWord(iCount)) < 0) {
    return -1;
  }
  offset += len;
  if (pFile->AppendString("/First ") < 0) {
    return -1;
  }
  if ((len = pFile->AppendDWord((uint32_t)tempBuffer.GetLength())) < 0) {
    return -1;
  }
  if (pFile->AppendString("/Length ") < 0) {
    return -1;
  }
  offset += len + 15;

  tempBuffer << m_Buffer;
  CPDF_FlateEncoder encoder(tempBuffer.GetBuffer(), tempBuffer.GetLength(),
                            true, false);
  CPDF_Encryptor encryptor(pCreator->m_pCryptoHandler, m_dwObjNum,
                           encoder.m_pData.Get(), encoder.m_dwSize);
  if ((len = pFile->AppendDWord(encryptor.m_dwSize)) < 0) {
    return -1;
  }
  offset += len;
  if (pFile->AppendString("/Filter /FlateDecode") < 0) {
    return -1;
  }
  offset += 20;
  if ((len = pFile->AppendString(">>stream\r\n")) < 0) {
    return -1;
  }
  if (pFile->AppendBlock(encryptor.m_pData, encryptor.m_dwSize) < 0) {
    return -1;
  }
  offset += len + encryptor.m_dwSize;
  if ((len = pFile->AppendString("\r\nendstream\r\nendobj\r\n")) < 0) {
    return -1;
  }
  offset += len;
  return ObjOffset;
}

CPDF_XRefStream::CPDF_XRefStream()
    : m_PrevOffset(0), m_dwTempObjNum(0), m_iSeg(0) {}

CPDF_XRefStream::~CPDF_XRefStream() {}

bool CPDF_XRefStream::Start() {
  m_IndexArray.clear();
  m_Buffer.Clear();
  m_iSeg = 0;
  return true;
}
int32_t CPDF_XRefStream::CompressIndirectObject(uint32_t dwObjNum,
                                                const CPDF_Object* pObj,
                                                CPDF_Creator* pCreator) {
  if (!pCreator)
    return 0;

  m_ObjStream.CompressIndirectObject(dwObjNum, pObj);
  if (pdfium::CollectionSize<int32_t>(m_ObjStream.m_Items) <
          pCreator->m_ObjectStreamSize &&
      m_ObjStream.m_Buffer.GetLength() < PDF_OBJECTSTREAM_MAXLENGTH) {
    return 1;
  }
  return EndObjectStream(pCreator);
}
int32_t CPDF_XRefStream::CompressIndirectObject(uint32_t dwObjNum,
                                                const uint8_t* pBuffer,
                                                uint32_t dwSize,
                                                CPDF_Creator* pCreator) {
  if (!pCreator)
    return 0;

  m_ObjStream.CompressIndirectObject(dwObjNum, pBuffer, dwSize);
  if (pdfium::CollectionSize<int32_t>(m_ObjStream.m_Items) <
          pCreator->m_ObjectStreamSize &&
      m_ObjStream.m_Buffer.GetLength() < PDF_OBJECTSTREAM_MAXLENGTH) {
    return 1;
  }
  return EndObjectStream(pCreator);
}

int32_t CPDF_XRefStream::EndObjectStream(CPDF_Creator* pCreator, bool bEOF) {
  FX_FILESIZE objOffset = 0;
  if (bEOF) {
    objOffset = m_ObjStream.End(pCreator);
    if (objOffset < 0) {
      return -1;
    }
  }
  uint32_t& dwObjStmNum = m_ObjStream.m_dwObjNum;
  if (!dwObjStmNum) {
    dwObjStmNum = ++pCreator->m_dwLastObjNum;
  }
  int32_t iSize = pdfium::CollectionSize<int32_t>(m_ObjStream.m_Items);
  size_t iSeg = m_IndexArray.size();
  if (!(pCreator->m_dwFlags & FPDFCREATE_INCREMENTAL)) {
    if (m_dwTempObjNum == 0) {
      AppendIndex0(m_Buffer, true);
      m_dwTempObjNum++;
    }
    uint32_t end_num = m_IndexArray.back().objnum + m_IndexArray.back().count;
    int index = 0;
    for (; m_dwTempObjNum < end_num; m_dwTempObjNum++) {
      FX_FILESIZE* offset = pCreator->m_ObjectOffset.GetPtrAt(m_dwTempObjNum);
      if (offset) {
        if (index >= iSize ||
            m_dwTempObjNum != m_ObjStream.m_Items[index].objnum) {
          AppendIndex1(m_Buffer, *offset);
        } else {
          AppendIndex2(m_Buffer, dwObjStmNum, index++);
        }
      } else {
        AppendIndex0(m_Buffer, false);
      }
    }
    if (iSize > 0 && bEOF) {
      pCreator->m_ObjectOffset.Add(dwObjStmNum, 1);
      pCreator->m_ObjectOffset[dwObjStmNum] = objOffset;
    }
    m_iSeg = iSeg;
    if (bEOF) {
      m_ObjStream.Start();
    }
    return 1;
  }
  for (auto it = m_IndexArray.begin() + m_iSeg; it != m_IndexArray.end();
       ++it) {
    for (uint32_t m = it->objnum; m < it->objnum + it->count; ++m) {
      if (m_ObjStream.m_index >= iSize ||
          m != m_ObjStream.m_Items[it - m_IndexArray.begin()].objnum) {
        AppendIndex1(m_Buffer, pCreator->m_ObjectOffset[m]);
      } else {
        AppendIndex2(m_Buffer, dwObjStmNum, m_ObjStream.m_index++);
      }
    }
  }
  if (iSize > 0 && bEOF) {
    AppendIndex1(m_Buffer, objOffset);
    m_IndexArray.push_back({dwObjStmNum, 1});
    iSeg += 1;
  }
  m_iSeg = iSeg;
  if (bEOF) {
    m_ObjStream.Start();
  }
  return 1;
}
bool CPDF_XRefStream::GenerateXRefStream(CPDF_Creator* pCreator, bool bEOF) {
  FX_FILESIZE offset_tmp = pCreator->m_Offset;
  uint32_t objnum = ++pCreator->m_dwLastObjNum;
  CFX_FileBufferArchive* pFile = &pCreator->m_File;
  bool bIncremental = (pCreator->m_dwFlags & FPDFCREATE_INCREMENTAL) != 0;
  if (bIncremental) {
    AddObjectNumberToIndexArray(objnum);
  } else {
    for (; m_dwTempObjNum < pCreator->m_dwLastObjNum; m_dwTempObjNum++) {
      FX_FILESIZE* offset = pCreator->m_ObjectOffset.GetPtrAt(m_dwTempObjNum);
      if (offset) {
        AppendIndex1(m_Buffer, *offset);
      } else {
        AppendIndex0(m_Buffer, false);
      }
    }
  }
  AppendIndex1(m_Buffer, offset_tmp);
  FX_FILESIZE& offset = pCreator->m_Offset;
  int32_t len = pFile->AppendDWord(objnum);
  if (len < 0) {
    return false;
  }
  offset += len;
  if ((len = pFile->AppendString(" 0 obj\r\n<</Type /XRef/W[1 4 2]/Index[")) <
      0) {
    return false;
  }
  offset += len;
  if (!bIncremental) {
    if ((len = pFile->AppendDWord(0)) < 0) {
      return false;
    }
    if ((len = pFile->AppendString(" ")) < 0) {
      return false;
    }
    offset += len + 1;
    if ((len = pFile->AppendDWord(objnum + 1)) < 0) {
      return false;
    }
    offset += len;
  } else {
    for (const auto& pair : m_IndexArray) {
      if ((len = pFile->AppendDWord(pair.objnum)) < 0) {
        return false;
      }
      if (pFile->AppendString(" ") < 0) {
        return false;
      }
      offset += len + 1;
      if ((len = pFile->AppendDWord(pair.count)) < 0) {
        return false;
      }
      if (pFile->AppendString(" ") < 0) {
        return false;
      }
      offset += len + 1;
    }
  }
  if (pFile->AppendString("]/Size ") < 0) {
    return false;
  }
  if ((len = pFile->AppendDWord(objnum + 1)) < 0) {
    return false;
  }
  offset += len + 7;
  if (m_PrevOffset > 0) {
    if (pFile->AppendString("/Prev ") < 0) {
      return false;
    }
    FX_CHAR offset_buf[20];
    FXSYS_memset(offset_buf, 0, sizeof(offset_buf));
    FXSYS_i64toa(m_PrevOffset, offset_buf, 10);
    int32_t offset_len = (int32_t)FXSYS_strlen(offset_buf);
    if (pFile->AppendBlock(offset_buf, offset_len) < 0) {
      return false;
    }
    offset += offset_len + 6;
  }
  CPDF_FlateEncoder encoder(m_Buffer.GetBuffer(), m_Buffer.GetLength(), true,
                            true);
  if (pFile->AppendString("/Filter /FlateDecode") < 0)
    return false;

  offset += 20;
  if ((len = pFile->AppendString("/DecodeParms<</Columns 7/Predictor 12>>")) <
      0) {
    return false;
  }

  offset += len;
  if (pFile->AppendString("/Length ") < 0)
    return false;

  if ((len = pFile->AppendDWord(encoder.m_dwSize)) < 0)
    return false;

  offset += len + 8;
  if (bEOF) {
    if ((len = PDF_CreatorWriteTrailer(pCreator->m_pDocument, pFile,
                                       pCreator->m_pIDArray.get())) < 0) {
      return false;
    }
    offset += len;
    if (pCreator->m_pEncryptDict) {
      uint32_t dwEncryptObjNum = pCreator->m_pEncryptDict->GetObjNum();
      if (dwEncryptObjNum == 0) {
        dwEncryptObjNum = pCreator->m_dwEncryptObjNum;
      }
      if ((len = PDF_CreatorWriteEncrypt(pCreator->m_pEncryptDict,
                                         dwEncryptObjNum, pFile)) < 0) {
        return false;
      }
      offset += len;
    }
  }
  if ((len = pFile->AppendString(">>stream\r\n")) < 0)
    return false;

  offset += len;
  if (pFile->AppendBlock(encoder.m_pData.Get(), encoder.m_dwSize) < 0)
    return false;

  if ((len = pFile->AppendString("\r\nendstream\r\nendobj\r\n")) < 0)
    return false;

  offset += encoder.m_dwSize + len;
  m_PrevOffset = offset_tmp;
  return true;
}

bool CPDF_XRefStream::End(CPDF_Creator* pCreator, bool bEOF) {
  if (EndObjectStream(pCreator, bEOF) < 0) {
    return false;
  }
  return GenerateXRefStream(pCreator, bEOF);
}
bool CPDF_XRefStream::EndXRefStream(CPDF_Creator* pCreator) {
  if (!(pCreator->m_dwFlags & FPDFCREATE_INCREMENTAL)) {
    AppendIndex0(m_Buffer, true);
    for (uint32_t i = 1; i < pCreator->m_dwLastObjNum + 1; i++) {
      FX_FILESIZE* offset = pCreator->m_ObjectOffset.GetPtrAt(i);
      if (offset) {
        AppendIndex1(m_Buffer, *offset);
      } else {
        AppendIndex0(m_Buffer, false);
      }
    }
  } else {
    for (const auto& pair : m_IndexArray) {
      for (uint32_t j = pair.objnum; j < pair.objnum + pair.count; ++j)
        AppendIndex1(m_Buffer, pCreator->m_ObjectOffset[j]);
    }
  }
  return GenerateXRefStream(pCreator, false);
}
void CPDF_XRefStream::AddObjectNumberToIndexArray(uint32_t objnum) {
  if (m_IndexArray.empty()) {
    m_IndexArray.push_back({objnum, 1});
    return;
  }
  uint32_t next_objnum = m_IndexArray.back().objnum + m_IndexArray.back().count;
  if (objnum == next_objnum)
    m_IndexArray.back().count += 1;
  else
    m_IndexArray.push_back({objnum, 1});
}

CPDF_Creator::CPDF_Creator(CPDF_Document* pDoc)
    : m_pDocument(pDoc),
      m_pParser(pDoc->GetParser()),
      m_bSecurityChanged(false),
      m_pEncryptDict(m_pParser ? m_pParser->GetEncryptDict() : nullptr),
      m_dwEncryptObjNum(0),
      m_bEncryptCloned(false),
      m_pCryptoHandler(m_pParser ? m_pParser->GetCryptoHandler() : nullptr),
      m_bLocalCryptoHandler(false),
      m_pMetadata(nullptr),
      m_ObjectStreamSize(200),
      m_dwLastObjNum(m_pDocument->GetLastObjNum()),
      m_Offset(0),
      m_iStage(-1),
      m_dwFlags(0),
      m_Pos(nullptr),
      m_XrefStart(0),
      m_pIDArray(nullptr),
      m_FileVersion(0) {}

CPDF_Creator::~CPDF_Creator() {
  ResetStandardSecurity();
  if (m_bEncryptCloned) {
    delete m_pEncryptDict;
    m_pEncryptDict = nullptr;
  }
  Clear();
}

int32_t CPDF_Creator::WriteIndirectObjectToStream(const CPDF_Object* pObj) {
  if (!m_pXRefStream)
    return 1;

  uint32_t objnum = pObj->GetObjNum();
  if (m_pParser && m_pParser->GetObjectGenNum(objnum) > 0)
    return 1;

  if (pObj->IsNumber())
    return 1;

  CPDF_Dictionary* pDict = pObj->GetDict();
  if (pObj->IsStream()) {
    if (pDict && pDict->GetStringFor("Type") == "XRef")
      return 0;
    return 1;
  }

  if (pDict) {
    if (pDict == m_pDocument->GetRoot() || pDict == m_pEncryptDict)
      return 1;
    if (pDict->IsSignatureDict())
      return 1;
    if (pDict->GetStringFor("Type") == "Page")
      return 1;
  }

  m_pXRefStream->AddObjectNumberToIndexArray(objnum);
  if (m_pXRefStream->CompressIndirectObject(objnum, pObj, this) < 0)
    return -1;
  if (!IsXRefNeedEnd(m_pXRefStream.get(), m_dwFlags))
    return 0;
  if (!m_pXRefStream->End(this))
    return -1;
  if (!m_pXRefStream->Start())
    return -1;
  return 0;
}
int32_t CPDF_Creator::WriteIndirectObjectToStream(uint32_t objnum,
                                                  const uint8_t* pBuffer,
                                                  uint32_t dwSize) {
  if (!m_pXRefStream) {
    return 1;
  }
  m_pXRefStream->AddObjectNumberToIndexArray(objnum);
  int32_t iRet =
      m_pXRefStream->CompressIndirectObject(objnum, pBuffer, dwSize, this);
  if (iRet < 1) {
    return iRet;
  }
  if (!IsXRefNeedEnd(m_pXRefStream.get(), m_dwFlags)) {
    return 0;
  }
  if (!m_pXRefStream->End(this)) {
    return -1;
  }
  if (!m_pXRefStream->Start()) {
    return -1;
  }
  return 0;
}
int32_t CPDF_Creator::AppendObjectNumberToXRef(uint32_t objnum) {
  if (!m_pXRefStream) {
    return 1;
  }
  m_pXRefStream->AddObjectNumberToIndexArray(objnum);
  if (!IsXRefNeedEnd(m_pXRefStream.get(), m_dwFlags)) {
    return 0;
  }
  if (!m_pXRefStream->End(this)) {
    return -1;
  }
  if (!m_pXRefStream->Start()) {
    return -1;
  }
  return 0;
}
int32_t CPDF_Creator::WriteStream(const CPDF_Object* pStream,
                                  uint32_t objnum,
                                  CPDF_CryptoHandler* pCrypto) {
  CPDF_FlateEncoder encoder(const_cast<CPDF_Stream*>(pStream->AsStream()),
                            pStream != m_pMetadata);
  CPDF_Encryptor encryptor(pCrypto, objnum, encoder.m_pData.Get(),
                           encoder.m_dwSize);
  if (static_cast<uint32_t>(encoder.m_pDict->GetIntegerFor("Length")) !=
      encryptor.m_dwSize) {
    encoder.CloneDict();
    encoder.m_pDict->SetNewFor<CPDF_Number>(
        "Length", static_cast<int>(encryptor.m_dwSize));
  }
  if (WriteDirectObj(objnum, encoder.m_pDict.Get()) < 0)
    return -1;

  int len = m_File.AppendString("stream\r\n");
  if (len < 0)
    return -1;

  m_Offset += len;
  if (m_File.AppendBlock(encryptor.m_pData, encryptor.m_dwSize) < 0)
    return -1;

  m_Offset += encryptor.m_dwSize;
  if ((len = m_File.AppendString("\r\nendstream")) < 0)
    return -1;

  m_Offset += len;
  return 1;
}
int32_t CPDF_Creator::WriteIndirectObj(uint32_t objnum,
                                       const CPDF_Object* pObj) {
  int32_t len = m_File.AppendDWord(objnum);
  if (len < 0)
    return -1;

  m_Offset += len;
  if ((len = m_File.AppendString(" 0 obj\r\n")) < 0)
    return -1;

  m_Offset += len;
  if (pObj->IsStream()) {
    CPDF_CryptoHandler* pHandler =
        pObj != m_pMetadata ? m_pCryptoHandler : nullptr;
    if (WriteStream(pObj, objnum, pHandler) < 0)
      return -1;
  } else {
    if (WriteDirectObj(objnum, pObj) < 0)
      return -1;
  }
  if ((len = m_File.AppendString("\r\nendobj\r\n")) < 0)
    return -1;

  m_Offset += len;
  if (AppendObjectNumberToXRef(objnum) < 0)
    return -1;
  return 0;
}
int32_t CPDF_Creator::WriteIndirectObj(const CPDF_Object* pObj) {
  int32_t iRet = WriteIndirectObjectToStream(pObj);
  if (iRet < 1) {
    return iRet;
  }
  return WriteIndirectObj(pObj->GetObjNum(), pObj);
}
int32_t CPDF_Creator::WriteDirectObj(uint32_t objnum,
                                     const CPDF_Object* pObj,
                                     bool bEncrypt) {
  int32_t len = 0;
  if (!pObj) {
    if (m_File.AppendString(" null") < 0) {
      return -1;
    }
    m_Offset += 5;
    return 1;
  }
  switch (pObj->GetType()) {
    case CPDF_Object::NULLOBJ:
      if (m_File.AppendString(" null") < 0) {
        return -1;
      }
      m_Offset += 5;
      break;
    case CPDF_Object::BOOLEAN:
    case CPDF_Object::NUMBER:
      if (m_File.AppendString(" ") < 0) {
        return -1;
      }
      if ((len = m_File.AppendString(pObj->GetString().AsStringC())) < 0) {
        return -1;
      }
      m_Offset += len + 1;
      break;
    case CPDF_Object::STRING: {
      CFX_ByteString str = pObj->GetString();
      bool bHex = pObj->AsString()->IsHex();
      if (!m_pCryptoHandler || !bEncrypt) {
        CFX_ByteString content = PDF_EncodeString(str, bHex);
        if ((len = m_File.AppendString(content.AsStringC())) < 0) {
          return -1;
        }
        m_Offset += len;
        break;
      }
      CPDF_Encryptor encryptor(m_pCryptoHandler, objnum, (uint8_t*)str.c_str(),
                               str.GetLength());
      CFX_ByteString content = PDF_EncodeString(
          CFX_ByteString((const FX_CHAR*)encryptor.m_pData, encryptor.m_dwSize),
          bHex);
      if ((len = m_File.AppendString(content.AsStringC())) < 0) {
        return -1;
      }
      m_Offset += len;
      break;
    }
    case CPDF_Object::STREAM: {
      CPDF_FlateEncoder encoder(const_cast<CPDF_Stream*>(pObj->AsStream()),
                                true);
      CPDF_Encryptor encryptor(m_pCryptoHandler, objnum, encoder.m_pData.Get(),
                               encoder.m_dwSize);
      if (static_cast<uint32_t>(encoder.m_pDict->GetIntegerFor("Length")) !=
          encryptor.m_dwSize) {
        encoder.CloneDict();
        encoder.m_pDict->SetNewFor<CPDF_Number>(
            "Length", static_cast<int>(encryptor.m_dwSize));
      }
      if (WriteDirectObj(objnum, encoder.m_pDict.Get()) < 0)
        return -1;

      if ((len = m_File.AppendString("stream\r\n")) < 0)
        return -1;

      m_Offset += len;
      if (m_File.AppendBlock(encryptor.m_pData, encryptor.m_dwSize) < 0)
        return -1;

      m_Offset += encryptor.m_dwSize;
      if ((len = m_File.AppendString("\r\nendstream")) < 0)
        return -1;

      m_Offset += len;
      break;
    }
    case CPDF_Object::NAME: {
      if (m_File.AppendString("/") < 0) {
        return -1;
      }
      CFX_ByteString str = pObj->GetString();
      if ((len = m_File.AppendString(PDF_NameEncode(str).AsStringC())) < 0) {
        return -1;
      }
      m_Offset += len + 1;
      break;
    }
    case CPDF_Object::REFERENCE: {
      if (m_File.AppendString(" ") < 0)
        return -1;
      if ((len = m_File.AppendDWord(pObj->AsReference()->GetRefObjNum())) < 0)
        return -1;
      if (m_File.AppendString(" 0 R") < 0)
        return -1;
      m_Offset += len + 5;
      break;
    }
    case CPDF_Object::ARRAY: {
      if (m_File.AppendString("[") < 0) {
        return -1;
      }
      m_Offset += 1;
      const CPDF_Array* p = pObj->AsArray();
      for (size_t i = 0; i < p->GetCount(); i++) {
        CPDF_Object* pElement = p->GetObjectAt(i);
        if (!pElement->IsInline()) {
          if (m_File.AppendString(" ") < 0) {
            return -1;
          }
          if ((len = m_File.AppendDWord(pElement->GetObjNum())) < 0) {
            return -1;
          }
          if (m_File.AppendString(" 0 R") < 0) {
            return -1;
          }
          m_Offset += len + 5;
        } else {
          if (WriteDirectObj(objnum, pElement) < 0) {
            return -1;
          }
        }
      }
      if (m_File.AppendString("]") < 0) {
        return -1;
      }
      m_Offset += 1;
      break;
    }
    case CPDF_Object::DICTIONARY: {
      if (!m_pCryptoHandler || pObj == m_pEncryptDict)
        return PDF_CreatorAppendObject(pObj, &m_File, m_Offset);
      if (m_File.AppendString("<<") < 0)
        return -1;

      m_Offset += 2;
      const CPDF_Dictionary* p = pObj->AsDictionary();
      bool bSignDict = p->IsSignatureDict();
      for (const auto& it : *p) {
        bool bSignValue = false;
        const CFX_ByteString& key = it.first;
        CPDF_Object* pValue = it.second.get();
        if (m_File.AppendString("/") < 0) {
          return -1;
        }
        if ((len = m_File.AppendString(PDF_NameEncode(key).AsStringC())) < 0) {
          return -1;
        }
        m_Offset += len + 1;
        if (bSignDict && key == "Contents") {
          bSignValue = true;
        }
        if (!pValue->IsInline()) {
          if (m_File.AppendString(" ") < 0) {
            return -1;
          }
          if ((len = m_File.AppendDWord(pValue->GetObjNum())) < 0) {
            return -1;
          }
          if (m_File.AppendString(" 0 R ") < 0) {
            return -1;
          }
          m_Offset += len + 6;
        } else {
          if (WriteDirectObj(objnum, pValue, !bSignValue) < 0) {
            return -1;
          }
        }
      }
      if (m_File.AppendString(">>") < 0) {
        return -1;
      }
      m_Offset += 2;
      break;
    }
  }
  return 1;
}
int32_t CPDF_Creator::WriteOldIndirectObject(uint32_t objnum) {
  if (m_pParser->IsObjectFreeOrNull(objnum))
    return 0;

  m_ObjectOffset[objnum] = m_Offset;
  bool bExistInMap = !!m_pDocument->GetIndirectObject(objnum);
  const uint8_t object_type = m_pParser->GetObjectType(objnum);
  bool bObjStm = (object_type == 2) && m_pEncryptDict && !m_pXRefStream;
  if (m_pParser->IsVersionUpdated() || m_bSecurityChanged || bExistInMap ||
      bObjStm) {
    CPDF_Object* pObj = m_pDocument->GetOrParseIndirectObject(objnum);
    if (!pObj) {
      m_ObjectOffset[objnum] = 0;
      return 0;
    }
    if (WriteIndirectObj(pObj)) {
      return -1;
    }
    if (!bExistInMap) {
      m_pDocument->DeleteIndirectObject(objnum);
    }
  } else {
    uint8_t* pBuffer;
    uint32_t size;
    m_pParser->GetIndirectBinary(objnum, pBuffer, size);
    if (!pBuffer) {
      return 0;
    }
    if (object_type == 2) {
      if (m_pXRefStream) {
        if (WriteIndirectObjectToStream(objnum, pBuffer, size) < 0) {
          FX_Free(pBuffer);
          return -1;
        }
      } else {
        int32_t len = m_File.AppendDWord(objnum);
        if (len < 0) {
          return -1;
        }
        if (m_File.AppendString(" 0 obj ") < 0) {
          return -1;
        }
        m_Offset += len + 7;
        if (m_File.AppendBlock(pBuffer, size) < 0) {
          return -1;
        }
        m_Offset += size;
        if (m_File.AppendString("\r\nendobj\r\n") < 0) {
          return -1;
        }
        m_Offset += 10;
      }
    } else {
      if (m_File.AppendBlock(pBuffer, size) < 0) {
        return -1;
      }
      m_Offset += size;
      if (AppendObjectNumberToXRef(objnum) < 0) {
        return -1;
      }
    }
    FX_Free(pBuffer);
  }
  return 1;
}
int32_t CPDF_Creator::WriteOldObjs(IFX_Pause* pPause) {
  uint32_t nLastObjNum = m_pParser->GetLastObjNum();
  if (!m_pParser->IsValidObjectNumber(nLastObjNum))
    return 0;

  uint32_t objnum = (uint32_t)(uintptr_t)m_Pos;
  for (; objnum <= nLastObjNum; ++objnum) {
    int32_t iRet = WriteOldIndirectObject(objnum);
    if (iRet < 0)
      return iRet;

    if (!iRet)
      continue;

    if (pPause && pPause->NeedToPauseNow()) {
      m_Pos = (void*)(uintptr_t)(objnum + 1);
      return 1;
    }
  }
  return 0;
}

int32_t CPDF_Creator::WriteNewObjs(bool bIncremental, IFX_Pause* pPause) {
  size_t iCount = m_NewObjNumArray.size();
  size_t index = (size_t)(uintptr_t)m_Pos;
  while (index < iCount) {
    uint32_t objnum = m_NewObjNumArray[index];
    CPDF_Object* pObj = m_pDocument->GetIndirectObject(objnum);
    if (!pObj) {
      ++index;
      continue;
    }
    m_ObjectOffset[objnum] = m_Offset;
    if (WriteIndirectObj(pObj))
      return -1;

    index++;
    if (pPause && pPause->NeedToPauseNow()) {
      m_Pos = (FX_POSITION)(uintptr_t)index;
      return 1;
    }
  }
  return 0;
}

void CPDF_Creator::InitOldObjNumOffsets() {
  if (!m_pParser) {
    return;
  }
  uint32_t j = 0;
  uint32_t dwStart = 0;
  uint32_t dwEnd = m_pParser->GetLastObjNum();
  while (dwStart <= dwEnd) {
    while (dwStart <= dwEnd && m_pParser->IsObjectFreeOrNull(dwStart))
      dwStart++;

    if (dwStart > dwEnd)
      break;

    j = dwStart;
    while (j <= dwEnd && !m_pParser->IsObjectFreeOrNull(j))
      j++;

    m_ObjectOffset.Add(dwStart, j - dwStart);
    dwStart = j;
  }
}

void CPDF_Creator::InitNewObjNumOffsets() {
  bool bIncremental = (m_dwFlags & FPDFCREATE_INCREMENTAL) != 0;
  bool bNoOriginal = (m_dwFlags & FPDFCREATE_NO_ORIGINAL) != 0;
  for (const auto& pair : *m_pDocument) {
    const uint32_t objnum = pair.first;
    const CPDF_Object* pObj = pair.second.get();
    if (bIncremental || pObj->GetObjNum() == CPDF_Object::kInvalidObjNum)
      continue;
    if (m_pParser && m_pParser->IsValidObjectNumber(objnum) &&
        m_pParser->GetObjectType(objnum)) {
      continue;
    }
    AppendNewObjNum(objnum);
  }

  size_t iCount = m_NewObjNumArray.size();
  if (iCount == 0)
    return;

  size_t i = 0;
  uint32_t dwStartObjNum = 0;
  bool bCrossRefValid = m_pParser && m_pParser->GetLastXRefOffset() > 0;
  while (i < iCount) {
    dwStartObjNum = m_NewObjNumArray[i];
    if ((bIncremental && (bNoOriginal || bCrossRefValid)) ||
        !m_ObjectOffset.GetPtrAt(dwStartObjNum)) {
      break;
    }
    i++;
  }
  if (i >= iCount)
    return;

  uint32_t dwLastObjNum = dwStartObjNum;
  i++;
  bool bNewStart = false;
  for (; i < iCount; i++) {
    uint32_t dwCurObjNum = m_NewObjNumArray[i];
    bool bExist = m_pParser && m_pParser->IsValidObjectNumber(dwCurObjNum) &&
                  m_ObjectOffset.GetPtrAt(dwCurObjNum);
    if (bExist || dwCurObjNum - dwLastObjNum > 1) {
      if (!bNewStart)
        m_ObjectOffset.Add(dwStartObjNum, dwLastObjNum - dwStartObjNum + 1);
      dwStartObjNum = dwCurObjNum;
    }
    if (bNewStart)
      dwStartObjNum = dwCurObjNum;

    bNewStart = bExist;
    dwLastObjNum = dwCurObjNum;
  }
  m_ObjectOffset.Add(dwStartObjNum, dwLastObjNum - dwStartObjNum + 1);
}

void CPDF_Creator::AppendNewObjNum(uint32_t objnum) {
  m_NewObjNumArray.insert(std::lower_bound(m_NewObjNumArray.begin(),
                                           m_NewObjNumArray.end(), objnum),
                          objnum);
}

int32_t CPDF_Creator::WriteDoc_Stage1(IFX_Pause* pPause) {
  ASSERT(m_iStage > -1 || m_iStage < 20);
  if (m_iStage == 0) {
    if (!m_pParser) {
      m_dwFlags &= ~FPDFCREATE_INCREMENTAL;
    }
    if (m_bSecurityChanged && (m_dwFlags & FPDFCREATE_NO_ORIGINAL) == 0) {
      m_dwFlags &= ~FPDFCREATE_INCREMENTAL;
    }
    CPDF_Dictionary* pDict = m_pDocument->GetRoot();
    m_pMetadata = pDict ? pDict->GetDirectObjectFor("Metadata") : nullptr;
    if (m_dwFlags & FPDFCREATE_OBJECTSTREAM) {
      m_pXRefStream = pdfium::MakeUnique<CPDF_XRefStream>();
      m_pXRefStream->Start();
      if ((m_dwFlags & FPDFCREATE_INCREMENTAL) != 0 && m_pParser) {
        FX_FILESIZE prev = m_pParser->GetLastXRefOffset();
        m_pXRefStream->m_PrevOffset = prev;
      }
    }
    m_iStage = 10;
  }
  if (m_iStage == 10) {
    if ((m_dwFlags & FPDFCREATE_INCREMENTAL) == 0) {
      if (m_File.AppendString("%PDF-1.") < 0) {
        return -1;
      }
      m_Offset += 7;
      int32_t version = 7;
      if (m_FileVersion) {
        version = m_FileVersion;
      } else if (m_pParser) {
        version = m_pParser->GetFileVersion();
      }
      int32_t len = m_File.AppendDWord(version % 10);
      if (len < 0) {
        return -1;
      }
      m_Offset += len;
      if ((len = m_File.AppendString("\r\n%\xA1\xB3\xC5\xD7\r\n")) < 0) {
        return -1;
      }
      m_Offset += len;
      InitOldObjNumOffsets();
      m_iStage = 20;
    } else {
      CFX_RetainPtr<IFX_SeekableReadStream> pSrcFile =
          m_pParser->GetFileAccess();
      m_Offset = pSrcFile->GetSize();
      m_Pos = (void*)(uintptr_t)m_Offset;
      m_iStage = 15;
    }
  }
  if (m_iStage == 15) {
    if ((m_dwFlags & FPDFCREATE_NO_ORIGINAL) == 0 && m_Pos) {
      CFX_RetainPtr<IFX_SeekableReadStream> pSrcFile =
          m_pParser->GetFileAccess();
      uint8_t buffer[4096];  // TODO(tsepez): don't stack allocate.
      uint32_t src_size = (uint32_t)(uintptr_t)m_Pos;
      while (src_size) {
        uint32_t block_size = src_size > 4096 ? 4096 : src_size;
        if (!pSrcFile->ReadBlock(buffer, m_Offset - src_size, block_size)) {
          return -1;
        }
        if (m_File.AppendBlock(buffer, block_size) < 0) {
          return -1;
        }
        src_size -= block_size;
        if (pPause && pPause->NeedToPauseNow()) {
          m_Pos = (void*)(uintptr_t)src_size;
          return 1;
        }
      }
    }
    if ((m_dwFlags & FPDFCREATE_NO_ORIGINAL) == 0 &&
        m_pParser->GetLastXRefOffset() == 0) {
      InitOldObjNumOffsets();
      uint32_t dwEnd = m_pParser->GetLastObjNum();
      bool bObjStm = (m_dwFlags & FPDFCREATE_OBJECTSTREAM) != 0;
      for (uint32_t objnum = 0; objnum <= dwEnd; objnum++) {
        if (m_pParser->IsObjectFreeOrNull(objnum))
          continue;

        m_ObjectOffset[objnum] = m_pParser->GetObjectPositionOrZero(objnum);
        if (bObjStm) {
          m_pXRefStream->AddObjectNumberToIndexArray(objnum);
        }
      }
      if (bObjStm) {
        m_pXRefStream->EndXRefStream(this);
        m_pXRefStream->Start();
      }
    }
    m_iStage = 20;
  }
  InitNewObjNumOffsets();
  return m_iStage;
}
int32_t CPDF_Creator::WriteDoc_Stage2(IFX_Pause* pPause) {
  ASSERT(m_iStage >= 20 || m_iStage < 30);
  if (m_iStage == 20) {
    if ((m_dwFlags & FPDFCREATE_INCREMENTAL) == 0 && m_pParser) {
      m_Pos = (void*)(uintptr_t)0;
      m_iStage = 21;
    } else {
      m_iStage = 25;
    }
  }
  if (m_iStage == 21) {
    int32_t iRet = WriteOldObjs(pPause);
    if (iRet) {
      return iRet;
    }
    m_iStage = 25;
  }
  if (m_iStage == 25) {
    m_Pos = (void*)(uintptr_t)0;
    m_iStage = 26;
  }
  if (m_iStage == 26) {
    int32_t iRet =
        WriteNewObjs((m_dwFlags & FPDFCREATE_INCREMENTAL) != 0, pPause);
    if (iRet) {
      return iRet;
    }
    m_iStage = 27;
  }
  if (m_iStage == 27) {
    if (m_pEncryptDict && m_pEncryptDict->IsInline()) {
      m_dwLastObjNum += 1;
      FX_FILESIZE saveOffset = m_Offset;
      if (WriteIndirectObj(m_dwLastObjNum, m_pEncryptDict) < 0)
        return -1;

      m_ObjectOffset.Add(m_dwLastObjNum, 1);
      m_ObjectOffset[m_dwLastObjNum] = saveOffset;
      m_dwEncryptObjNum = m_dwLastObjNum;
      if (m_dwFlags & FPDFCREATE_INCREMENTAL)
        m_NewObjNumArray.push_back(m_dwLastObjNum);
    }
    m_iStage = 80;
  }
  return m_iStage;
}
int32_t CPDF_Creator::WriteDoc_Stage3(IFX_Pause* pPause) {
  ASSERT(m_iStage >= 80 || m_iStage < 90);
  uint32_t dwLastObjNum = m_dwLastObjNum;
  if (m_iStage == 80) {
    m_XrefStart = m_Offset;
    if (m_dwFlags & FPDFCREATE_OBJECTSTREAM) {
      m_pXRefStream->End(this, true);
      m_XrefStart = m_pXRefStream->m_PrevOffset;
      m_iStage = 90;
    } else if ((m_dwFlags & FPDFCREATE_INCREMENTAL) == 0 ||
               !m_pParser->IsXRefStream()) {
      if ((m_dwFlags & FPDFCREATE_INCREMENTAL) == 0 ||
          m_pParser->GetLastXRefOffset() == 0) {
        CFX_ByteString str;
        str = m_ObjectOffset.GetPtrAt(1)
                  ? "xref\r\n"
                  : "xref\r\n0 1\r\n0000000000 65535 f\r\n";
        if (m_File.AppendString(str.AsStringC()) < 0) {
          return -1;
        }
        m_Pos = (void*)(uintptr_t)1;
        m_iStage = 81;
      } else {
        if (m_File.AppendString("xref\r\n") < 0) {
          return -1;
        }
        m_Pos = (void*)(uintptr_t)0;
        m_iStage = 82;
      }
    } else {
      m_iStage = 90;
    }
  }
  if (m_iStage == 81) {
    CFX_ByteString str;
    uint32_t i = (uint32_t)(uintptr_t)m_Pos, j;
    while (i <= dwLastObjNum) {
      while (i <= dwLastObjNum && !m_ObjectOffset.GetPtrAt(i)) {
        i++;
      }
      if (i > dwLastObjNum) {
        break;
      }
      j = i;
      while (j <= dwLastObjNum && m_ObjectOffset.GetPtrAt(j)) {
        j++;
      }
      if (i == 1) {
        str.Format("0 %d\r\n0000000000 65535 f\r\n", j);
      } else {
        str.Format("%d %d\r\n", i, j - i);
      }
      if (m_File.AppendBlock(str.c_str(), str.GetLength()) < 0) {
        return -1;
      }
      while (i < j) {
        str.Format("%010d 00000 n\r\n", m_ObjectOffset[i++]);
        if (m_File.AppendBlock(str.c_str(), str.GetLength()) < 0) {
          return -1;
        }
      }
      if (i > dwLastObjNum) {
        break;
      }
      if (pPause && pPause->NeedToPauseNow()) {
        m_Pos = (void*)(uintptr_t)i;
        return 1;
      }
    }
    m_iStage = 90;
  }
  if (m_iStage == 82) {
    CFX_ByteString str;
    size_t iCount = m_NewObjNumArray.size();
    size_t i = (size_t)(uintptr_t)m_Pos;
    while (i < iCount) {
      size_t j = i;
      uint32_t objnum = m_NewObjNumArray[i];
      while (j < iCount) {
        if (++j == iCount)
          break;
        uint32_t dwCurrent = m_NewObjNumArray[j];
        if (dwCurrent - objnum > 1)
          break;
        objnum = dwCurrent;
      }
      objnum = m_NewObjNumArray[i];
      if (objnum == 1) {
        str.Format("0 %d\r\n0000000000 65535 f\r\n", j - i + 1);
      } else {
        str.Format("%d %d\r\n", objnum, j - i);
      }
      if (m_File.AppendBlock(str.c_str(), str.GetLength()) < 0) {
        return -1;
      }
      while (i < j) {
        objnum = m_NewObjNumArray[i++];
        str.Format("%010d 00000 n\r\n", m_ObjectOffset[objnum]);
        if (m_File.AppendBlock(str.c_str(), str.GetLength()) < 0)
          return -1;
      }
      if (pPause && (i % 100) == 0 && pPause->NeedToPauseNow()) {
        m_Pos = (void*)(uintptr_t)i;
        return 1;
      }
    }
    m_iStage = 90;
  }
  return m_iStage;
}

int32_t CPDF_Creator::WriteDoc_Stage4(IFX_Pause* pPause) {
  ASSERT(m_iStage >= 90);
  if ((m_dwFlags & FPDFCREATE_OBJECTSTREAM) == 0) {
    bool bXRefStream =
        (m_dwFlags & FPDFCREATE_INCREMENTAL) != 0 && m_pParser->IsXRefStream();
    if (!bXRefStream) {
      if (m_File.AppendString("trailer\r\n<<") < 0) {
        return -1;
      }
    } else {
      if (m_File.AppendDWord(m_pDocument->GetLastObjNum() + 1) < 0) {
        return -1;
      }
      if (m_File.AppendString(" 0 obj <<") < 0) {
        return -1;
      }
    }
    if (m_pParser) {
      CPDF_Dictionary* p = m_pParser->GetTrailer();
      for (const auto& it : *p) {
        const CFX_ByteString& key = it.first;
        CPDF_Object* pValue = it.second.get();
        // TODO(ochang): Consolidate with similar check in
        // PDF_CreatorWriteTrailer.
        if (key == "Encrypt" || key == "Size" || key == "Filter" ||
            key == "Index" || key == "Length" || key == "Prev" || key == "W" ||
            key == "XRefStm" || key == "ID") {
          continue;
        }
        if (m_File.AppendString(("/")) < 0) {
          return -1;
        }
        if (m_File.AppendString(PDF_NameEncode(key).AsStringC()) < 0) {
          return -1;
        }
        if (!pValue->IsInline()) {
          if (m_File.AppendString(" ") < 0) {
            return -1;
          }
          if (m_File.AppendDWord(pValue->GetObjNum()) < 0) {
            return -1;
          }
          if (m_File.AppendString(" 0 R ") < 0) {
            return -1;
          }
        } else {
          FX_FILESIZE offset = 0;
          if (PDF_CreatorAppendObject(pValue, &m_File, offset) < 0) {
            return -1;
          }
        }
      }
    } else {
      if (m_File.AppendString("\r\n/Root ") < 0) {
        return -1;
      }
      if (m_File.AppendDWord(m_pDocument->GetRoot()->GetObjNum()) < 0) {
        return -1;
      }
      if (m_File.AppendString(" 0 R\r\n") < 0) {
        return -1;
      }
      if (m_pDocument->GetInfo()) {
        if (m_File.AppendString("/Info ") < 0) {
          return -1;
        }
        if (m_File.AppendDWord(m_pDocument->GetInfo()->GetObjNum()) < 0) {
          return -1;
        }
        if (m_File.AppendString(" 0 R\r\n") < 0) {
          return -1;
        }
      }
    }
    if (m_pEncryptDict) {
      if (m_File.AppendString("/Encrypt") < 0) {
        return -1;
      }
      uint32_t dwObjNum = m_pEncryptDict->GetObjNum();
      if (dwObjNum == 0) {
        dwObjNum = m_pDocument->GetLastObjNum() + 1;
      }
      if (m_File.AppendString(" ") < 0) {
        return -1;
      }
      if (m_File.AppendDWord(dwObjNum) < 0) {
        return -1;
      }
      if (m_File.AppendString(" 0 R ") < 0) {
        return -1;
      }
    }
    if (m_File.AppendString("/Size ") < 0) {
      return -1;
    }
    if (m_File.AppendDWord(m_dwLastObjNum + (bXRefStream ? 2 : 1)) < 0) {
      return -1;
    }
    if ((m_dwFlags & FPDFCREATE_INCREMENTAL) != 0) {
      FX_FILESIZE prev = m_pParser->GetLastXRefOffset();
      if (prev) {
        if (m_File.AppendString("/Prev ") < 0) {
          return -1;
        }
        FX_CHAR offset_buf[20];
        FXSYS_memset(offset_buf, 0, sizeof(offset_buf));
        FXSYS_i64toa(prev, offset_buf, 10);
        if (m_File.AppendBlock(offset_buf, FXSYS_strlen(offset_buf)) < 0) {
          return -1;
        }
      }
    }
    if (m_pIDArray) {
      if (m_File.AppendString(("/ID")) < 0) {
        return -1;
      }
      FX_FILESIZE offset = 0;
      if (PDF_CreatorAppendObject(m_pIDArray.get(), &m_File, offset) < 0) {
        return -1;
      }
    }
    if (!bXRefStream) {
      if (m_File.AppendString(">>") < 0) {
        return -1;
      }
    } else {
      if (m_File.AppendString("/W[0 4 1]/Index[") < 0) {
        return -1;
      }
      if ((m_dwFlags & FPDFCREATE_INCREMENTAL) != 0 && m_pParser &&
          m_pParser->GetLastXRefOffset() == 0) {
        uint32_t i = 0;
        for (i = 0; i < m_dwLastObjNum; i++) {
          if (!m_ObjectOffset.GetPtrAt(i)) {
            continue;
          }
          if (m_File.AppendDWord(i) < 0) {
            return -1;
          }
          if (m_File.AppendString(" 1 ") < 0) {
            return -1;
          }
        }
        if (m_File.AppendString("]/Length ") < 0) {
          return -1;
        }
        if (m_File.AppendDWord(m_dwLastObjNum * 5) < 0) {
          return -1;
        }
        if (m_File.AppendString(">>stream\r\n") < 0) {
          return -1;
        }
        for (i = 0; i < m_dwLastObjNum; i++) {
          FX_FILESIZE* offset = m_ObjectOffset.GetPtrAt(i);
          if (!offset)
            continue;
          OutputIndex(&m_File, *offset);
        }
      } else {
        size_t count = m_NewObjNumArray.size();
        size_t i = 0;
        for (i = 0; i < count; i++) {
          if (m_File.AppendDWord(m_NewObjNumArray[i]) < 0)
            return -1;
          if (m_File.AppendString(" 1 ") < 0)
            return -1;
        }
        if (m_File.AppendString("]/Length ") < 0)
          return -1;
        if (m_File.AppendDWord(count * 5) < 0)
          return -1;
        if (m_File.AppendString(">>stream\r\n") < 0)
          return -1;
        for (i = 0; i < count; i++) {
          uint32_t objnum = m_NewObjNumArray[i];
          FX_FILESIZE offset = m_ObjectOffset[objnum];
          OutputIndex(&m_File, offset);
        }
      }
      if (m_File.AppendString("\r\nendstream") < 0)
        return -1;
    }
  }
  if (m_File.AppendString("\r\nstartxref\r\n") < 0) {
    return -1;
  }
  FX_CHAR offset_buf[20];
  FXSYS_memset(offset_buf, 0, sizeof(offset_buf));
  FXSYS_i64toa(m_XrefStart, offset_buf, 10);
  if (m_File.AppendBlock(offset_buf, FXSYS_strlen(offset_buf)) < 0) {
    return -1;
  }
  if (m_File.AppendString("\r\n%%EOF\r\n") < 0) {
    return -1;
  }
  m_File.Flush();
  return m_iStage = 100;
}

void CPDF_Creator::Clear() {
  m_pXRefStream.reset();
  m_File.Clear();
  m_NewObjNumArray.clear();
  m_pIDArray.reset();
}

bool CPDF_Creator::Create(const CFX_RetainPtr<IFX_WriteStream>& pFile,
                          uint32_t flags) {
  m_File.AttachFile(pFile);
  return Create(flags);
}

bool CPDF_Creator::Create(uint32_t flags) {
  m_dwFlags = flags;
  m_iStage = 0;
  m_Offset = 0;
  m_dwLastObjNum = m_pDocument->GetLastObjNum();
  m_ObjectOffset.Clear();
  m_NewObjNumArray.clear();
  InitID();
  if (flags & FPDFCREATE_PROGRESSIVE)
    return true;
  return Continue(nullptr) > -1;
}

void CPDF_Creator::InitID(bool bDefault) {
  CPDF_Array* pOldIDArray = m_pParser ? m_pParser->GetIDArray() : nullptr;
  bool bNewId = !m_pIDArray;
  if (bNewId) {
    m_pIDArray = pdfium::MakeUnique<CPDF_Array>();
    CPDF_Object* pID1 = pOldIDArray ? pOldIDArray->GetObjectAt(0) : nullptr;
    if (pID1) {
      m_pIDArray->Add(pID1->Clone());
    } else {
      std::vector<uint8_t> buffer =
          PDF_GenerateFileID((uint32_t)(uintptr_t) this, m_dwLastObjNum);
      CFX_ByteString bsBuffer(buffer.data(), buffer.size());
      m_pIDArray->AddNew<CPDF_String>(bsBuffer, true);
    }
  }
  if (!bDefault) {
    return;
  }
  if (pOldIDArray) {
    CPDF_Object* pID2 = pOldIDArray->GetObjectAt(1);
    if ((m_dwFlags & FPDFCREATE_INCREMENTAL) && m_pEncryptDict && pID2) {
      m_pIDArray->Add(pID2->Clone());
      return;
    }
    std::vector<uint8_t> buffer =
        PDF_GenerateFileID((uint32_t)(uintptr_t) this, m_dwLastObjNum);
    CFX_ByteString bsBuffer(buffer.data(), buffer.size());
    m_pIDArray->AddNew<CPDF_String>(bsBuffer, true);
    return;
  }
  m_pIDArray->Add(m_pIDArray->GetObjectAt(0)->Clone());
  if (m_pEncryptDict && !pOldIDArray && m_pParser && bNewId) {
    if (m_pEncryptDict->GetStringFor("Filter") == "Standard") {
      CFX_ByteString user_pass = m_pParser->GetPassword();
      uint32_t flag = PDF_ENCRYPT_CONTENT;
      CPDF_SecurityHandler handler;
      handler.OnCreate(m_pEncryptDict, m_pIDArray.get(), user_pass.raw_str(),
                       user_pass.GetLength(), flag);
      if (m_bLocalCryptoHandler)
        delete m_pCryptoHandler;
      m_pCryptoHandler = new CPDF_CryptoHandler;
      m_pCryptoHandler->Init(m_pEncryptDict, &handler);
      m_bLocalCryptoHandler = true;
      m_bSecurityChanged = true;
    }
  }
}
int32_t CPDF_Creator::Continue(IFX_Pause* pPause) {
  if (m_iStage < 0) {
    return m_iStage;
  }
  int32_t iRet = 0;
  while (m_iStage < 100) {
    if (m_iStage < 20) {
      iRet = WriteDoc_Stage1(pPause);
    } else if (m_iStage < 30) {
      iRet = WriteDoc_Stage2(pPause);
    } else if (m_iStage < 90) {
      iRet = WriteDoc_Stage3(pPause);
    } else {
      iRet = WriteDoc_Stage4(pPause);
    }
    if (iRet < m_iStage) {
      break;
    }
  }
  if (iRet < 1 || m_iStage == 100) {
    m_iStage = -1;
    Clear();
    return iRet > 99 ? 0 : (iRet < 1 ? -1 : iRet);
  }
  return m_iStage;
}
bool CPDF_Creator::SetFileVersion(int32_t fileVersion) {
  if (fileVersion < 10 || fileVersion > 17) {
    return false;
  }
  m_FileVersion = fileVersion;
  return true;
}
void CPDF_Creator::RemoveSecurity() {
  ResetStandardSecurity();
  m_bSecurityChanged = true;
  m_pEncryptDict = nullptr;
  m_pCryptoHandler = nullptr;
}
void CPDF_Creator::ResetStandardSecurity() {
  if (!m_bLocalCryptoHandler)
    return;

  delete m_pCryptoHandler;
  m_pCryptoHandler = nullptr;
  m_bLocalCryptoHandler = false;
}
