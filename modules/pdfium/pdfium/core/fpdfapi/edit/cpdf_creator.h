// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_EDIT_CPDF_CREATOR_H_
#define CORE_FPDFAPI_EDIT_CPDF_CREATOR_H_

#include <memory>
#include <vector>

#include "core/fxcrt/cfx_retain_ptr.h"
#include "core/fxcrt/fx_basic.h"

class CPDF_Array;
class CPDF_CryptoHandler;
class CPDF_Dictionary;
class CPDF_Document;
class CPDF_Object;
class CPDF_Parser;
class CPDF_XRefStream;

#define FPDFCREATE_INCREMENTAL 1
#define FPDFCREATE_NO_ORIGINAL 2
#define FPDFCREATE_PROGRESSIVE 4
#define FPDFCREATE_OBJECTSTREAM 8

CFX_ByteTextBuf& operator<<(CFX_ByteTextBuf& buf, const CPDF_Object* pObj);

class CPDF_Creator {
 public:
  explicit CPDF_Creator(CPDF_Document* pDoc);
  ~CPDF_Creator();

  void RemoveSecurity();
  bool Create(const CFX_RetainPtr<IFX_WriteStream>& pFile, uint32_t flags = 0);
  int32_t Continue(IFX_Pause* pPause = nullptr);
  bool SetFileVersion(int32_t fileVersion = 17);

 private:
  friend class CPDF_ObjectStream;
  friend class CPDF_XRefStream;

  bool Create(uint32_t flags);
  void ResetStandardSecurity();
  void Clear();

  void InitOldObjNumOffsets();
  void InitNewObjNumOffsets();
  void InitID(bool bDefault = true);

  void AppendNewObjNum(uint32_t objbum);
  int32_t AppendObjectNumberToXRef(uint32_t objnum);

  int32_t WriteDoc_Stage1(IFX_Pause* pPause);
  int32_t WriteDoc_Stage2(IFX_Pause* pPause);
  int32_t WriteDoc_Stage3(IFX_Pause* pPause);
  int32_t WriteDoc_Stage4(IFX_Pause* pPause);

  int32_t WriteOldIndirectObject(uint32_t objnum);
  int32_t WriteOldObjs(IFX_Pause* pPause);
  int32_t WriteNewObjs(bool bIncremental, IFX_Pause* pPause);
  int32_t WriteIndirectObj(const CPDF_Object* pObj);
  int32_t WriteDirectObj(uint32_t objnum,
                         const CPDF_Object* pObj,
                         bool bEncrypt = true);
  int32_t WriteIndirectObjectToStream(const CPDF_Object* pObj);
  int32_t WriteIndirectObj(uint32_t objnum, const CPDF_Object* pObj);
  int32_t WriteIndirectObjectToStream(uint32_t objnum,
                                      const uint8_t* pBuffer,
                                      uint32_t dwSize);

  int32_t WriteStream(const CPDF_Object* pStream,
                      uint32_t objnum,
                      CPDF_CryptoHandler* pCrypto);

  CPDF_Document* const m_pDocument;
  CPDF_Parser* const m_pParser;
  bool m_bSecurityChanged;
  CPDF_Dictionary* m_pEncryptDict;
  uint32_t m_dwEncryptObjNum;
  bool m_bEncryptCloned;
  CPDF_CryptoHandler* m_pCryptoHandler;
  // Whether this owns the crypto handler |m_pCryptoHandler|.
  bool m_bLocalCryptoHandler;
  CPDF_Object* m_pMetadata;
  std::unique_ptr<CPDF_XRefStream> m_pXRefStream;
  int32_t m_ObjectStreamSize;
  uint32_t m_dwLastObjNum;
  CFX_FileBufferArchive m_File;
  FX_FILESIZE m_Offset;
  int32_t m_iStage;
  uint32_t m_dwFlags;
  FX_POSITION m_Pos;
  FX_FILESIZE m_XrefStart;
  CFX_FileSizeListArray m_ObjectOffset;
  std::vector<uint32_t> m_NewObjNumArray;  // Sorted, ascending.
  std::unique_ptr<CPDF_Array> m_pIDArray;
  int32_t m_FileVersion;
};

#endif  // CORE_FPDFAPI_EDIT_CPDF_CREATOR_H_
