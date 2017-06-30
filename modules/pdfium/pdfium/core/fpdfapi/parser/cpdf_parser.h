// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PARSER_CPDF_PARSER_H_
#define CORE_FPDFAPI_PARSER_CPDF_PARSER_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "core/fxcrt/fx_basic.h"

class CPDF_Array;
class CPDF_CryptoHandler;
class CPDF_Dictionary;
class CPDF_Document;
class CPDF_IndirectObjectHolder;
class CPDF_LinearizedHeader;
class CPDF_Object;
class CPDF_SecurityHandler;
class CPDF_StreamAcc;
class CPDF_SyntaxParser;
class IFX_SeekableReadStream;

class CPDF_Parser {
 public:
  enum Error {
    SUCCESS = 0,
    FILE_ERROR,
    FORMAT_ERROR,
    PASSWORD_ERROR,
    HANDLER_ERROR
  };

  // A limit on the maximum object number in the xref table. Theoretical limits
  // are higher, but this may be large enough in practice.
  static const uint32_t kMaxObjectNumber = 1048576;

  CPDF_Parser();
  ~CPDF_Parser();

  Error StartParse(const CFX_RetainPtr<IFX_SeekableReadStream>& pFile,
                   CPDF_Document* pDocument);
  Error StartLinearizedParse(const CFX_RetainPtr<IFX_SeekableReadStream>& pFile,
                             CPDF_Document* pDocument);

  void SetPassword(const FX_CHAR* password) { m_Password = password; }
  CFX_ByteString GetPassword() { return m_Password; }
  CPDF_Dictionary* GetTrailer() const { return m_pTrailer.get(); }
  FX_FILESIZE GetLastXRefOffset() const { return m_LastXRefOffset; }

  uint32_t GetPermissions() const;
  uint32_t GetRootObjNum();
  uint32_t GetInfoObjNum();
  CPDF_Array* GetIDArray();

  CPDF_Dictionary* GetEncryptDict() const { return m_pEncryptDict; }

  std::unique_ptr<CPDF_Object> ParseIndirectObject(
      CPDF_IndirectObjectHolder* pObjList,
      uint32_t objnum);

  uint32_t GetLastObjNum() const;
  bool IsValidObjectNumber(uint32_t objnum) const;
  FX_FILESIZE GetObjectPositionOrZero(uint32_t objnum) const;
  uint8_t GetObjectType(uint32_t objnum) const;
  uint16_t GetObjectGenNum(uint32_t objnum) const;
  bool IsVersionUpdated() const { return m_bVersionUpdated; }
  bool IsObjectFreeOrNull(uint32_t objnum) const;
  CPDF_CryptoHandler* GetCryptoHandler();
  CFX_RetainPtr<IFX_SeekableReadStream> GetFileAccess() const;

  FX_FILESIZE GetObjectOffset(uint32_t objnum) const;
  FX_FILESIZE GetObjectSize(uint32_t objnum) const;

  void GetIndirectBinary(uint32_t objnum, uint8_t*& pBuffer, uint32_t& size);
  int GetFileVersion() const { return m_FileVersion; }
  bool IsXRefStream() const { return m_bXRefStream; }

  std::unique_ptr<CPDF_Object> ParseIndirectObjectAt(
      CPDF_IndirectObjectHolder* pObjList,
      FX_FILESIZE pos,
      uint32_t objnum);

  std::unique_ptr<CPDF_Object> ParseIndirectObjectAtByStrict(
      CPDF_IndirectObjectHolder* pObjList,
      FX_FILESIZE pos,
      uint32_t objnum,
      FX_FILESIZE* pResultPos);

  uint32_t GetFirstPageNo() const;

 protected:
  struct ObjectInfo {
    ObjectInfo() : pos(0), type(0), gennum(0) {}

    FX_FILESIZE pos;
    uint8_t type;
    uint16_t gennum;
  };

  std::unique_ptr<CPDF_SyntaxParser> m_pSyntax;
  std::map<uint32_t, ObjectInfo> m_ObjectInfo;

  bool LoadCrossRefV4(FX_FILESIZE pos, FX_FILESIZE streampos, bool bSkip);
  bool RebuildCrossRef();

 private:
  friend class CPDF_DataAvail;

  enum class ParserState {
    kDefault,
    kComment,
    kWhitespace,
    kString,
    kHexString,
    kEscapedString,
    kXref,
    kObjNum,
    kPostObjNum,
    kGenNum,
    kPostGenNum,
    kTrailer,
    kBeginObj,
    kEndObj
  };

  CPDF_Object* ParseDirect(CPDF_Object* pObj);
  bool LoadAllCrossRefV4(FX_FILESIZE pos);
  bool LoadAllCrossRefV5(FX_FILESIZE pos);
  bool LoadCrossRefV5(FX_FILESIZE* pos, bool bMainXRef);
  std::unique_ptr<CPDF_Dictionary> LoadTrailerV4();
  Error SetEncryptHandler();
  void ReleaseEncryptHandler();
  bool LoadLinearizedAllCrossRefV4(FX_FILESIZE pos, uint32_t dwObjCount);
  bool LoadLinearizedCrossRefV4(FX_FILESIZE pos, uint32_t dwObjCount);
  bool LoadLinearizedAllCrossRefV5(FX_FILESIZE pos);
  Error LoadLinearizedMainXRefTable();
  CPDF_StreamAcc* GetObjectStream(uint32_t number);
  bool IsLinearizedFile(
      const CFX_RetainPtr<IFX_SeekableReadStream>& pFileAccess,
      uint32_t offset);
  void SetEncryptDictionary(CPDF_Dictionary* pDict);
  void ShrinkObjectMap(uint32_t size);
  // A simple check whether the cross reference table matches with
  // the objects.
  bool VerifyCrossRefV4();

  CPDF_Document* m_pDocument;  // not owned
  bool m_bHasParsed;
  bool m_bXRefStream;
  bool m_bVersionUpdated;
  int m_FileVersion;
  CPDF_Dictionary* m_pEncryptDict;
  FX_FILESIZE m_LastXRefOffset;
  std::unique_ptr<CPDF_SecurityHandler> m_pSecurityHandler;
  CFX_ByteString m_Password;
  std::set<FX_FILESIZE> m_SortedOffset;
  std::unique_ptr<CPDF_Dictionary> m_pTrailer;
  std::vector<std::unique_ptr<CPDF_Dictionary>> m_Trailers;
  std::unique_ptr<CPDF_LinearizedHeader> m_pLinearized;
  uint32_t m_dwXrefStartObjNum;

  // A map of object numbers to indirect streams. Map owns the streams.
  std::map<uint32_t, std::unique_ptr<CPDF_StreamAcc>> m_ObjectStreamMap;

  // Mapping of object numbers to offsets. The offsets are relative to the first
  // object in the stream.
  using StreamObjectCache = std::map<uint32_t, uint32_t>;

  // Mapping of streams to their object caches. This is valid as long as the
  // streams in |m_ObjectStreamMap| are valid.
  std::map<CPDF_StreamAcc*, StreamObjectCache> m_ObjCache;

  // All indirect object numbers that are being parsed.
  std::set<uint32_t> m_ParsingObjNums;
};

#endif  // CORE_FPDFAPI_PARSER_CPDF_PARSER_H_
