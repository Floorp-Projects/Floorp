// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PARSER_CPDF_SYNTAX_PARSER_H_
#define CORE_FPDFAPI_PARSER_CPDF_SYNTAX_PARSER_H_

#include <memory>

#include "core/fxcrt/cfx_string_pool_template.h"
#include "core/fxcrt/cfx_weak_ptr.h"
#include "core/fxcrt/fx_basic.h"

class CPDF_CryptoHandler;
class CPDF_Dictionary;
class CPDF_IndirectObjectHolder;
class CPDF_Object;
class CPDF_Stream;
class IFX_SeekableReadStream;

class CPDF_SyntaxParser {
 public:
  CPDF_SyntaxParser();
  explicit CPDF_SyntaxParser(const CFX_WeakPtr<CFX_ByteStringPool>& pPool);
  ~CPDF_SyntaxParser();

  void InitParser(const CFX_RetainPtr<IFX_SeekableReadStream>& pFileAccess,
                  uint32_t HeaderOffset);

  FX_FILESIZE SavePos() const { return m_Pos; }
  void RestorePos(FX_FILESIZE pos) { m_Pos = pos; }

  std::unique_ptr<CPDF_Object> GetObject(CPDF_IndirectObjectHolder* pObjList,
                                         uint32_t objnum,
                                         uint32_t gennum,
                                         bool bDecrypt);

  std::unique_ptr<CPDF_Object> GetObjectForStrict(
      CPDF_IndirectObjectHolder* pObjList,
      uint32_t objnum,
      uint32_t gennum);

  CFX_ByteString GetKeyword();
  void ToNextLine();
  void ToNextWord();
  bool SearchWord(const CFX_ByteStringC& word,
                  bool bWholeWord,
                  bool bForward,
                  FX_FILESIZE limit);

  FX_FILESIZE FindTag(const CFX_ByteStringC& tag, FX_FILESIZE limit);
  void SetEncrypt(std::unique_ptr<CPDF_CryptoHandler> pCryptoHandler);
  bool ReadBlock(uint8_t* pBuf, uint32_t size);
  bool GetCharAt(FX_FILESIZE pos, uint8_t& ch);
  CFX_ByteString GetNextWord(bool* bIsNumber);

 private:
  friend class CPDF_Parser;
  friend class CPDF_DataAvail;
  friend class cpdf_syntax_parser_ReadHexString_Test;

  static const int kParserMaxRecursionDepth = 64;
  static int s_CurrentRecursionDepth;

  uint32_t GetDirectNum();
  bool ReadChar(FX_FILESIZE read_pos, uint32_t read_size);
  bool GetNextChar(uint8_t& ch);
  bool GetCharAtBackward(FX_FILESIZE pos, uint8_t& ch);
  void GetNextWordInternal(bool* bIsNumber);
  bool IsWholeWord(FX_FILESIZE startpos,
                   FX_FILESIZE limit,
                   const CFX_ByteStringC& tag,
                   bool checkKeyword);

  CFX_ByteString ReadString();
  CFX_ByteString ReadHexString();
  unsigned int ReadEOLMarkers(FX_FILESIZE pos);
  std::unique_ptr<CPDF_Stream> ReadStream(
      std::unique_ptr<CPDF_Dictionary> pDict,
      uint32_t objnum,
      uint32_t gennum);

  inline bool CheckPosition(FX_FILESIZE pos) {
    return m_BufOffset >= pos ||
           static_cast<FX_FILESIZE>(m_BufOffset + m_BufSize) <= pos;
  }

  FX_FILESIZE m_Pos;
  uint32_t m_MetadataObjnum;
  CFX_RetainPtr<IFX_SeekableReadStream> m_pFileAccess;
  FX_FILESIZE m_HeaderOffset;
  FX_FILESIZE m_FileLen;
  uint8_t* m_pFileBuf;
  uint32_t m_BufSize;
  FX_FILESIZE m_BufOffset;
  std::unique_ptr<CPDF_CryptoHandler> m_pCryptoHandler;
  uint8_t m_WordBuffer[257];
  uint32_t m_WordSize;
  CFX_WeakPtr<CFX_ByteStringPool> m_pPool;
};

#endif  // CORE_FPDFAPI_PARSER_CPDF_SYNTAX_PARSER_H_
