// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/parser/cpdf_data_avail.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_hint_tables.h"
#include "core/fpdfapi/parser/cpdf_linearized_header.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/fpdf_parser_utility.h"
#include "core/fxcrt/fx_ext.h"
#include "core/fxcrt/fx_safe_types.h"
#include "third_party/base/numerics/safe_conversions.h"
#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"

CPDF_DataAvail::FileAvail::~FileAvail() {}

CPDF_DataAvail::DownloadHints::~DownloadHints() {}

// static
int CPDF_DataAvail::s_CurrentDataAvailRecursionDepth = 0;

CPDF_DataAvail::CPDF_DataAvail(
    FileAvail* pFileAvail,
    const CFX_RetainPtr<IFX_SeekableReadStream>& pFileRead,
    bool bSupportHintTable)
    : m_pFileAvail(pFileAvail), m_pFileRead(pFileRead) {
  m_Pos = 0;
  m_dwFileLen = 0;
  if (m_pFileRead) {
    m_dwFileLen = (uint32_t)m_pFileRead->GetSize();
  }
  m_dwCurrentOffset = 0;
  m_dwXRefOffset = 0;
  m_dwTrailerOffset = 0;
  m_bufferOffset = 0;
  m_bufferSize = 0;
  m_PagesObjNum = 0;
  m_dwCurrentXRefSteam = 0;
  m_dwAcroFormObjNum = 0;
  m_dwInfoObjNum = 0;
  m_pDocument = 0;
  m_dwEncryptObjNum = 0;
  m_dwPrevXRefOffset = 0;
  m_dwLastXRefOffset = 0;
  m_bDocAvail = false;
  m_bMainXRefLoadTried = false;
  m_bDocAvail = false;
  m_bPagesLoad = false;
  m_bPagesTreeLoad = false;
  m_bMainXRefLoadedOK = false;
  m_bAnnotsLoad = false;
  m_bHaveAcroForm = false;
  m_bAcroFormLoad = false;
  m_bPageLoadedOK = false;
  m_bNeedDownLoadResource = false;
  m_bLinearizedFormParamLoad = false;
  m_pTrailer = nullptr;
  m_pCurrentParser = nullptr;
  m_pAcroForm = nullptr;
  m_pPageDict = nullptr;
  m_pPageResource = nullptr;
  m_docStatus = PDF_DATAAVAIL_HEADER;
  m_bTotalLoadPageTree = false;
  m_bCurPageDictLoadOK = false;
  m_bLinearedDataOK = false;
  m_bSupportHintTable = bSupportHintTable;
}

CPDF_DataAvail::~CPDF_DataAvail() {
  m_pHintTables.reset();
  for (CPDF_Object* pObject : m_arrayAcroforms)
    delete pObject;
}

void CPDF_DataAvail::SetDocument(CPDF_Document* pDoc) {
  m_pDocument = pDoc;
}

uint32_t CPDF_DataAvail::GetObjectSize(uint32_t objnum, FX_FILESIZE& offset) {
  CPDF_Parser* pParser = m_pDocument->GetParser();
  if (!pParser || !pParser->IsValidObjectNumber(objnum))
    return 0;

  if (pParser->GetObjectType(objnum) == 2)
    objnum = pParser->GetObjectPositionOrZero(objnum);

  if (pParser->GetObjectType(objnum) != 1 &&
      pParser->GetObjectType(objnum) != 255) {
    return 0;
  }

  offset = pParser->GetObjectPositionOrZero(objnum);
  if (offset == 0)
    return 0;

  auto it = pParser->m_SortedOffset.find(offset);
  if (it == pParser->m_SortedOffset.end() ||
      ++it == pParser->m_SortedOffset.end()) {
    return 0;
  }
  return *it - offset;
}

bool CPDF_DataAvail::AreObjectsAvailable(std::vector<CPDF_Object*>& obj_array,
                                         bool bParsePage,
                                         DownloadHints* pHints,
                                         std::vector<CPDF_Object*>& ret_array) {
  if (obj_array.empty())
    return true;

  uint32_t count = 0;
  std::vector<CPDF_Object*> new_obj_array;
  for (CPDF_Object* pObj : obj_array) {
    if (!pObj)
      continue;

    int32_t type = pObj->GetType();
    switch (type) {
      case CPDF_Object::ARRAY: {
        CPDF_Array* pArray = pObj->AsArray();
        for (size_t k = 0; k < pArray->GetCount(); ++k)
          new_obj_array.push_back(pArray->GetObjectAt(k));
      } break;
      case CPDF_Object::STREAM:
        pObj = pObj->GetDict();
      case CPDF_Object::DICTIONARY: {
        CPDF_Dictionary* pDict = pObj->GetDict();
        if (pDict && pDict->GetStringFor("Type") == "Page" && !bParsePage)
          continue;

        for (const auto& it : *pDict) {
          if (it.first != "Parent")
            new_obj_array.push_back(it.second.get());
        }
      } break;
      case CPDF_Object::REFERENCE: {
        CPDF_Reference* pRef = pObj->AsReference();
        uint32_t dwNum = pRef->GetRefObjNum();

        FX_FILESIZE offset;
        uint32_t size = GetObjectSize(dwNum, offset);
        if (size == 0 || offset < 0 || offset >= m_dwFileLen)
          break;

        if (!IsDataAvail(offset, size, pHints)) {
          ret_array.push_back(pObj);
          count++;
        } else if (!pdfium::ContainsKey(m_ObjectSet, dwNum)) {
          m_ObjectSet.insert(dwNum);
          CPDF_Object* pReferred =
              m_pDocument->GetOrParseIndirectObject(pRef->GetRefObjNum());
          if (pReferred)
            new_obj_array.push_back(pReferred);
        }
      } break;
    }
  }

  if (count > 0) {
    for (CPDF_Object* pObj : new_obj_array) {
      CPDF_Reference* pRef = pObj->AsReference();
      if (pRef && pdfium::ContainsKey(m_ObjectSet, pRef->GetRefObjNum()))
        continue;
      ret_array.push_back(pObj);
    }
    return false;
  }

  obj_array = new_obj_array;
  return AreObjectsAvailable(obj_array, false, pHints, ret_array);
}

CPDF_DataAvail::DocAvailStatus CPDF_DataAvail::IsDocAvail(
    DownloadHints* pHints) {
  if (!m_dwFileLen && m_pFileRead) {
    m_dwFileLen = (uint32_t)m_pFileRead->GetSize();
    if (!m_dwFileLen)
      return DataError;
  }

  while (!m_bDocAvail) {
    if (!CheckDocStatus(pHints))
      return DataNotAvailable;
  }

  return DataAvailable;
}

bool CPDF_DataAvail::CheckAcroFormSubObject(DownloadHints* pHints) {
  if (m_objs_array.empty()) {
    m_ObjectSet.clear();
    std::vector<CPDF_Object*> obj_array = m_arrayAcroforms;
    if (!AreObjectsAvailable(obj_array, false, pHints, m_objs_array))
      return false;

    m_objs_array.clear();
    return true;
  }

  std::vector<CPDF_Object*> new_objs_array;
  if (!AreObjectsAvailable(m_objs_array, false, pHints, new_objs_array)) {
    m_objs_array = new_objs_array;
    return false;
  }

  for (CPDF_Object* pObject : m_arrayAcroforms)
    delete pObject;

  m_arrayAcroforms.clear();
  return true;
}

bool CPDF_DataAvail::CheckAcroForm(DownloadHints* pHints) {
  bool bExist = false;
  m_pAcroForm = GetObject(m_dwAcroFormObjNum, pHints, &bExist).release();
  if (!bExist) {
    m_docStatus = PDF_DATAAVAIL_PAGETREE;
    return true;
  }

  if (!m_pAcroForm) {
    if (m_docStatus == PDF_DATAAVAIL_ERROR) {
      m_docStatus = PDF_DATAAVAIL_LOADALLFILE;
      return true;
    }
    return false;
  }

  m_arrayAcroforms.push_back(m_pAcroForm);
  m_docStatus = PDF_DATAAVAIL_PAGETREE;
  return true;
}

bool CPDF_DataAvail::CheckDocStatus(DownloadHints* pHints) {
  switch (m_docStatus) {
    case PDF_DATAAVAIL_HEADER:
      return CheckHeader(pHints);
    case PDF_DATAAVAIL_FIRSTPAGE:
      return CheckFirstPage(pHints);
    case PDF_DATAAVAIL_HINTTABLE:
      return CheckHintTables(pHints);
    case PDF_DATAAVAIL_END:
      return CheckEnd(pHints);
    case PDF_DATAAVAIL_CROSSREF:
      return CheckCrossRef(pHints);
    case PDF_DATAAVAIL_CROSSREF_ITEM:
      return CheckCrossRefItem(pHints);
    case PDF_DATAAVAIL_CROSSREF_STREAM:
      return CheckAllCrossRefStream(pHints);
    case PDF_DATAAVAIL_TRAILER:
      return CheckTrailer(pHints);
    case PDF_DATAAVAIL_TRAILER_APPEND:
      return CheckTrailerAppend(pHints);
    case PDF_DATAAVAIL_LOADALLCROSSREF:
      return LoadAllXref(pHints);
    case PDF_DATAAVAIL_LOADALLFILE:
      return LoadAllFile(pHints);
    case PDF_DATAAVAIL_ROOT:
      return CheckRoot(pHints);
    case PDF_DATAAVAIL_INFO:
      return CheckInfo(pHints);
    case PDF_DATAAVAIL_ACROFORM:
      return CheckAcroForm(pHints);
    case PDF_DATAAVAIL_PAGETREE:
      if (m_bTotalLoadPageTree)
        return CheckPages(pHints);
      return LoadDocPages(pHints);
    case PDF_DATAAVAIL_PAGE:
      if (m_bTotalLoadPageTree)
        return CheckPage(pHints);
      m_docStatus = PDF_DATAAVAIL_PAGE_LATERLOAD;
      return true;
    case PDF_DATAAVAIL_ERROR:
      return LoadAllFile(pHints);
    case PDF_DATAAVAIL_PAGE_LATERLOAD:
      m_docStatus = PDF_DATAAVAIL_PAGE;
    default:
      m_bDocAvail = true;
      return true;
  }
}

bool CPDF_DataAvail::CheckPageStatus(DownloadHints* pHints) {
  switch (m_docStatus) {
    case PDF_DATAAVAIL_PAGETREE:
      return CheckPages(pHints);
    case PDF_DATAAVAIL_PAGE:
      return CheckPage(pHints);
    case PDF_DATAAVAIL_ERROR:
      return LoadAllFile(pHints);
    default:
      m_bPagesTreeLoad = true;
      m_bPagesLoad = true;
      return true;
  }
}

bool CPDF_DataAvail::LoadAllFile(DownloadHints* pHints) {
  if (m_pFileAvail->IsDataAvail(0, (uint32_t)m_dwFileLen)) {
    m_docStatus = PDF_DATAAVAIL_DONE;
    return true;
  }

  pHints->AddSegment(0, (uint32_t)m_dwFileLen);
  return false;
}

bool CPDF_DataAvail::LoadAllXref(DownloadHints* pHints) {
  m_parser.m_pSyntax->InitParser(m_pFileRead, (uint32_t)m_dwHeaderOffset);
  if (!m_parser.LoadAllCrossRefV4(m_dwLastXRefOffset) &&
      !m_parser.LoadAllCrossRefV5(m_dwLastXRefOffset)) {
    m_docStatus = PDF_DATAAVAIL_LOADALLFILE;
    return false;
  }

  m_dwRootObjNum = m_parser.GetRootObjNum();
  m_dwInfoObjNum = m_parser.GetInfoObjNum();
  m_pCurrentParser = &m_parser;
  m_docStatus = PDF_DATAAVAIL_ROOT;
  return true;
}

std::unique_ptr<CPDF_Object> CPDF_DataAvail::GetObject(uint32_t objnum,
                                                       DownloadHints* pHints,
                                                       bool* pExistInFile) {
  uint32_t size = 0;
  FX_FILESIZE offset = 0;
  CPDF_Parser* pParser = nullptr;

  if (pExistInFile)
    *pExistInFile = true;

  if (m_pDocument) {
    size = GetObjectSize(objnum, offset);
    pParser = m_pDocument->GetParser();
  } else {
    size = (uint32_t)m_parser.GetObjectSize(objnum);
    offset = m_parser.GetObjectOffset(objnum);
    pParser = &m_parser;
  }

  if (!IsDataAvail(offset, size, pHints))
    return nullptr;

  std::unique_ptr<CPDF_Object> pRet;
  if (pParser)
    pRet = pParser->ParseIndirectObject(nullptr, objnum);

  if (!pRet && pExistInFile)
    *pExistInFile = false;

  return pRet;
}

bool CPDF_DataAvail::CheckInfo(DownloadHints* pHints) {
  bool bExist = false;
  std::unique_ptr<CPDF_Object> pInfo =
      GetObject(m_dwInfoObjNum, pHints, &bExist);
  if (bExist && !pInfo) {
    if (m_docStatus == PDF_DATAAVAIL_ERROR) {
      m_docStatus = PDF_DATAAVAIL_LOADALLFILE;
      return true;
    }
    if (m_Pos == m_dwFileLen)
      m_docStatus = PDF_DATAAVAIL_ERROR;
    return false;
  }
  m_docStatus =
      m_bHaveAcroForm ? PDF_DATAAVAIL_ACROFORM : PDF_DATAAVAIL_PAGETREE;
  return true;
}

bool CPDF_DataAvail::CheckRoot(DownloadHints* pHints) {
  bool bExist = false;
  m_pRoot = GetObject(m_dwRootObjNum, pHints, &bExist);
  if (!bExist) {
    m_docStatus = PDF_DATAAVAIL_LOADALLFILE;
    return true;
  }

  if (!m_pRoot) {
    if (m_docStatus == PDF_DATAAVAIL_ERROR) {
      m_docStatus = PDF_DATAAVAIL_LOADALLFILE;
      return true;
    }
    return false;
  }

  CPDF_Dictionary* pDict = m_pRoot->GetDict();
  if (!pDict) {
    m_docStatus = PDF_DATAAVAIL_ERROR;
    return false;
  }

  CPDF_Reference* pRef = ToReference(pDict->GetObjectFor("Pages"));
  if (!pRef) {
    m_docStatus = PDF_DATAAVAIL_ERROR;
    return false;
  }

  m_PagesObjNum = pRef->GetRefObjNum();
  CPDF_Reference* pAcroFormRef =
      ToReference(m_pRoot->GetDict()->GetObjectFor("AcroForm"));
  if (pAcroFormRef) {
    m_bHaveAcroForm = true;
    m_dwAcroFormObjNum = pAcroFormRef->GetRefObjNum();
  }

  if (m_dwInfoObjNum) {
    m_docStatus = PDF_DATAAVAIL_INFO;
  } else {
    m_docStatus =
        m_bHaveAcroForm ? PDF_DATAAVAIL_ACROFORM : PDF_DATAAVAIL_PAGETREE;
  }
  return true;
}

bool CPDF_DataAvail::PreparePageItem() {
  CPDF_Dictionary* pRoot = m_pDocument->GetRoot();
  CPDF_Reference* pRef =
      ToReference(pRoot ? pRoot->GetObjectFor("Pages") : nullptr);
  if (!pRef) {
    m_docStatus = PDF_DATAAVAIL_ERROR;
    return false;
  }

  m_PagesObjNum = pRef->GetRefObjNum();
  m_pCurrentParser = m_pDocument->GetParser();
  m_docStatus = PDF_DATAAVAIL_PAGETREE;
  return true;
}

bool CPDF_DataAvail::IsFirstCheck(uint32_t dwPage) {
  return m_pageMapCheckState.insert(dwPage).second;
}

void CPDF_DataAvail::ResetFirstCheck(uint32_t dwPage) {
  m_pageMapCheckState.erase(dwPage);
}

bool CPDF_DataAvail::CheckPage(DownloadHints* pHints) {
  std::vector<uint32_t> UnavailObjList;
  for (uint32_t dwPageObjNum : m_PageObjList) {
    bool bExists = false;
    std::unique_ptr<CPDF_Object> pObj =
        GetObject(dwPageObjNum, pHints, &bExists);
    if (!pObj) {
      if (bExists)
        UnavailObjList.push_back(dwPageObjNum);
      continue;
    }
    CPDF_Array* pArray = ToArray(pObj.get());
    if (pArray) {
      for (const auto& pArrayObj : *pArray) {
        if (CPDF_Reference* pRef = ToReference(pArrayObj.get()))
          UnavailObjList.push_back(pRef->GetRefObjNum());
      }
    }
    if (!pObj->IsDictionary())
      continue;

    CFX_ByteString type = pObj->GetDict()->GetStringFor("Type");
    if (type == "Pages") {
      m_PagesArray.push_back(std::move(pObj));
      continue;
    }
  }
  m_PageObjList.clear();
  if (!UnavailObjList.empty()) {
    m_PageObjList = std::move(UnavailObjList);
    return false;
  }
  size_t iPages = m_PagesArray.size();
  for (size_t i = 0; i < iPages; ++i) {
    std::unique_ptr<CPDF_Object> pPages = std::move(m_PagesArray[i]);
    if (pPages && !GetPageKids(m_pCurrentParser, pPages.get())) {
      m_PagesArray.clear();
      m_docStatus = PDF_DATAAVAIL_ERROR;
      return false;
    }
  }
  m_PagesArray.clear();
  if (m_PageObjList.empty())
    m_docStatus = PDF_DATAAVAIL_DONE;

  return true;
}

bool CPDF_DataAvail::GetPageKids(CPDF_Parser* pParser, CPDF_Object* pPages) {
  if (!pParser) {
    m_docStatus = PDF_DATAAVAIL_ERROR;
    return false;
  }

  CPDF_Dictionary* pDict = pPages->GetDict();
  CPDF_Object* pKids = pDict ? pDict->GetObjectFor("Kids") : nullptr;
  if (!pKids)
    return true;

  switch (pKids->GetType()) {
    case CPDF_Object::REFERENCE:
      m_PageObjList.push_back(pKids->AsReference()->GetRefObjNum());
      break;
    case CPDF_Object::ARRAY: {
      CPDF_Array* pKidsArray = pKids->AsArray();
      for (size_t i = 0; i < pKidsArray->GetCount(); ++i) {
        if (CPDF_Reference* pRef = ToReference(pKidsArray->GetObjectAt(i)))
          m_PageObjList.push_back(pRef->GetRefObjNum());
      }
    } break;
    default:
      m_docStatus = PDF_DATAAVAIL_ERROR;
      return false;
  }
  return true;
}

bool CPDF_DataAvail::CheckPages(DownloadHints* pHints) {
  bool bExists = false;
  std::unique_ptr<CPDF_Object> pPages =
      GetObject(m_PagesObjNum, pHints, &bExists);
  if (!bExists) {
    m_docStatus = PDF_DATAAVAIL_LOADALLFILE;
    return true;
  }

  if (!pPages) {
    if (m_docStatus == PDF_DATAAVAIL_ERROR) {
      m_docStatus = PDF_DATAAVAIL_LOADALLFILE;
      return true;
    }
    return false;
  }

  if (!GetPageKids(m_pCurrentParser, pPages.get())) {
    m_docStatus = PDF_DATAAVAIL_ERROR;
    return false;
  }

  m_docStatus = PDF_DATAAVAIL_PAGE;
  return true;
}

bool CPDF_DataAvail::CheckHeader(DownloadHints* pHints) {
  ASSERT(m_dwFileLen >= 0);
  const uint32_t kReqSize = std::min(static_cast<uint32_t>(m_dwFileLen), 1024U);

  if (m_pFileAvail->IsDataAvail(0, kReqSize)) {
    uint8_t buffer[1024];
    m_pFileRead->ReadBlock(buffer, 0, kReqSize);

    if (IsLinearizedFile(buffer, kReqSize)) {
      m_docStatus = PDF_DATAAVAIL_FIRSTPAGE;
    } else {
      if (m_docStatus == PDF_DATAAVAIL_ERROR)
        return false;
      m_docStatus = PDF_DATAAVAIL_END;
    }
    return true;
  }

  pHints->AddSegment(0, kReqSize);
  return false;
}

bool CPDF_DataAvail::CheckFirstPage(DownloadHints* pHints) {
  if (!m_pLinearized->GetFirstPageEndOffset() ||
      !m_pLinearized->GetFileSize() || !m_pLinearized->GetLastXRefOffset()) {
    m_docStatus = PDF_DATAAVAIL_ERROR;
    return false;
  }

  uint32_t dwEnd = m_pLinearized->GetFirstPageEndOffset();
  dwEnd += 512;
  if ((FX_FILESIZE)dwEnd > m_dwFileLen)
    dwEnd = (uint32_t)m_dwFileLen;

  int32_t iStartPos = (int32_t)(m_dwFileLen > 1024 ? 1024 : m_dwFileLen);
  int32_t iSize = dwEnd > 1024 ? dwEnd - 1024 : 0;
  if (!m_pFileAvail->IsDataAvail(iStartPos, iSize)) {
    pHints->AddSegment(iStartPos, iSize);
    return false;
  }

  m_docStatus =
      m_bSupportHintTable ? PDF_DATAAVAIL_HINTTABLE : PDF_DATAAVAIL_DONE;
  return true;
}

bool CPDF_DataAvail::IsDataAvail(FX_FILESIZE offset,
                                 uint32_t size,
                                 DownloadHints* pHints) {
  if (offset < 0 || offset > m_dwFileLen)
    return true;

  FX_SAFE_FILESIZE safeSize = offset;
  safeSize += size;
  safeSize += 512;
  if (!safeSize.IsValid() || safeSize.ValueOrDie() > m_dwFileLen)
    size = m_dwFileLen - offset;
  else
    size += 512;

  if (!m_pFileAvail->IsDataAvail(offset, size)) {
    if (pHints)
      pHints->AddSegment(offset, size);
    return false;
  }
  return true;
}

bool CPDF_DataAvail::CheckHintTables(DownloadHints* pHints) {
  if (m_pLinearized->GetPageCount() <= 1) {
    m_docStatus = PDF_DATAAVAIL_DONE;
    return true;
  }
  if (!m_pLinearized->HasHintTable()) {
    m_docStatus = PDF_DATAAVAIL_ERROR;
    return false;
  }

  FX_FILESIZE szHintStart = m_pLinearized->GetHintStart();
  FX_FILESIZE szHintLength = m_pLinearized->GetHintLength();

  if (!IsDataAvail(szHintStart, szHintLength, pHints))
    return false;

  m_syntaxParser.InitParser(m_pFileRead, m_dwHeaderOffset);

  std::unique_ptr<CPDF_HintTables> pHintTables(
      new CPDF_HintTables(this, m_pLinearized.get()));
  std::unique_ptr<CPDF_Object> pHintStream(
      ParseIndirectObjectAt(szHintStart, 0));
  CPDF_Stream* pStream = ToStream(pHintStream.get());
  if (pStream && pHintTables->LoadHintStream(pStream))
    m_pHintTables = std::move(pHintTables);

  m_docStatus = PDF_DATAAVAIL_DONE;
  return true;
}

std::unique_ptr<CPDF_Object> CPDF_DataAvail::ParseIndirectObjectAt(
    FX_FILESIZE pos,
    uint32_t objnum,
    CPDF_IndirectObjectHolder* pObjList) {
  FX_FILESIZE SavedPos = m_syntaxParser.SavePos();
  m_syntaxParser.RestorePos(pos);

  bool bIsNumber;
  CFX_ByteString word = m_syntaxParser.GetNextWord(&bIsNumber);
  if (!bIsNumber)
    return nullptr;

  uint32_t parser_objnum = FXSYS_atoui(word.c_str());
  if (objnum && parser_objnum != objnum)
    return nullptr;

  word = m_syntaxParser.GetNextWord(&bIsNumber);
  if (!bIsNumber)
    return nullptr;

  uint32_t gennum = FXSYS_atoui(word.c_str());
  if (m_syntaxParser.GetKeyword() != "obj") {
    m_syntaxParser.RestorePos(SavedPos);
    return nullptr;
  }

  std::unique_ptr<CPDF_Object> pObj =
      m_syntaxParser.GetObject(pObjList, parser_objnum, gennum, true);
  m_syntaxParser.RestorePos(SavedPos);
  return pObj;
}

CPDF_DataAvail::DocLinearizationStatus CPDF_DataAvail::IsLinearizedPDF() {
  const uint32_t kReqSize = 1024;
  if (!m_pFileAvail->IsDataAvail(0, kReqSize))
    return LinearizationUnknown;

  if (!m_pFileRead)
    return NotLinearized;

  FX_FILESIZE dwSize = m_pFileRead->GetSize();
  if (dwSize < (FX_FILESIZE)kReqSize)
    return LinearizationUnknown;

  uint8_t buffer[1024];
  m_pFileRead->ReadBlock(buffer, 0, kReqSize);
  if (IsLinearizedFile(buffer, kReqSize))
    return Linearized;

  return NotLinearized;
}

bool CPDF_DataAvail::IsLinearized() {
  return !!m_pLinearized;
}

bool CPDF_DataAvail::IsLinearizedFile(uint8_t* pData, uint32_t dwLen) {
  if (m_pLinearized)
    return true;

  CFX_RetainPtr<IFX_MemoryStream> file =
      IFX_MemoryStream::Create(pData, (size_t)dwLen, false);
  int32_t offset = GetHeaderOffset(file);
  if (offset == -1) {
    m_docStatus = PDF_DATAAVAIL_ERROR;
    return false;
  }

  m_dwHeaderOffset = offset;
  m_syntaxParser.InitParser(file, offset);
  m_syntaxParser.RestorePos(m_syntaxParser.m_HeaderOffset + 9);

  bool bNumber;
  CFX_ByteString wordObjNum = m_syntaxParser.GetNextWord(&bNumber);
  if (!bNumber)
    return false;

  uint32_t objnum = FXSYS_atoui(wordObjNum.c_str());
  m_pLinearized = CPDF_LinearizedHeader::CreateForObject(
      ParseIndirectObjectAt(m_syntaxParser.m_HeaderOffset + 9, objnum));
  if (!m_pLinearized ||
      m_pLinearized->GetFileSize() != m_pFileRead->GetSize()) {
    m_pLinearized.reset();
    return false;
  }
  return true;
}

bool CPDF_DataAvail::CheckEnd(DownloadHints* pHints) {
  uint32_t req_pos = (uint32_t)(m_dwFileLen > 1024 ? m_dwFileLen - 1024 : 0);
  uint32_t dwSize = (uint32_t)(m_dwFileLen - req_pos);

  if (m_pFileAvail->IsDataAvail(req_pos, dwSize)) {
    uint8_t buffer[1024];
    m_pFileRead->ReadBlock(buffer, req_pos, dwSize);

    CFX_RetainPtr<IFX_MemoryStream> file =
        IFX_MemoryStream::Create(buffer, (size_t)dwSize, false);
    m_syntaxParser.InitParser(file, 0);
    m_syntaxParser.RestorePos(dwSize - 1);

    if (m_syntaxParser.SearchWord("startxref", true, false, dwSize)) {
      m_syntaxParser.GetNextWord(nullptr);

      bool bNumber;
      CFX_ByteString xrefpos_str = m_syntaxParser.GetNextWord(&bNumber);
      if (!bNumber) {
        m_docStatus = PDF_DATAAVAIL_ERROR;
        return false;
      }

      m_dwXRefOffset = (FX_FILESIZE)FXSYS_atoi64(xrefpos_str.c_str());
      if (!m_dwXRefOffset || m_dwXRefOffset > m_dwFileLen) {
        m_docStatus = PDF_DATAAVAIL_LOADALLFILE;
        return true;
      }

      m_dwLastXRefOffset = m_dwXRefOffset;
      SetStartOffset(m_dwXRefOffset);
      m_docStatus = PDF_DATAAVAIL_CROSSREF;
      return true;
    }

    m_docStatus = PDF_DATAAVAIL_LOADALLFILE;
    return true;
  }

  pHints->AddSegment(req_pos, dwSize);
  return false;
}

int32_t CPDF_DataAvail::CheckCrossRefStream(DownloadHints* pHints,
                                            FX_FILESIZE& xref_offset) {
  xref_offset = 0;
  uint32_t req_size =
      (uint32_t)(m_Pos + 512 > m_dwFileLen ? m_dwFileLen - m_Pos : 512);

  if (m_pFileAvail->IsDataAvail(m_Pos, req_size)) {
    int32_t iSize = (int32_t)(m_Pos + req_size - m_dwCurrentXRefSteam);
    CFX_BinaryBuf buf(iSize);
    uint8_t* pBuf = buf.GetBuffer();

    m_pFileRead->ReadBlock(pBuf, m_dwCurrentXRefSteam, iSize);

    CFX_RetainPtr<IFX_MemoryStream> file =
        IFX_MemoryStream::Create(pBuf, (size_t)iSize, false);
    m_parser.m_pSyntax->InitParser(file, 0);

    bool bNumber;
    CFX_ByteString objnum = m_parser.m_pSyntax->GetNextWord(&bNumber);
    if (!bNumber)
      return -1;

    uint32_t objNum = FXSYS_atoui(objnum.c_str());
    std::unique_ptr<CPDF_Object> pObj =
        m_parser.ParseIndirectObjectAt(nullptr, 0, objNum);

    if (!pObj) {
      m_Pos += m_parser.m_pSyntax->SavePos();
      return 0;
    }

    CPDF_Dictionary* pDict = pObj->GetDict();
    CPDF_Name* pName = ToName(pDict ? pDict->GetObjectFor("Type") : nullptr);
    if (pName && pName->GetString() == "XRef") {
      m_Pos += m_parser.m_pSyntax->SavePos();
      xref_offset = pObj->GetDict()->GetIntegerFor("Prev");
      return 1;
    }
    return -1;
  }
  pHints->AddSegment(m_Pos, req_size);
  return 0;
}

void CPDF_DataAvail::SetStartOffset(FX_FILESIZE dwOffset) {
  m_Pos = dwOffset;
}

bool CPDF_DataAvail::GetNextToken(CFX_ByteString& token) {
  uint8_t ch;
  if (!GetNextChar(ch))
    return false;

  while (1) {
    while (PDFCharIsWhitespace(ch)) {
      if (!GetNextChar(ch))
        return false;
    }

    if (ch != '%')
      break;

    while (1) {
      if (!GetNextChar(ch))
        return false;
      if (PDFCharIsLineEnding(ch))
        break;
    }
  }

  uint8_t buffer[256];
  uint32_t index = 0;
  if (PDFCharIsDelimiter(ch)) {
    buffer[index++] = ch;
    if (ch == '/') {
      while (1) {
        if (!GetNextChar(ch))
          return false;

        if (!PDFCharIsOther(ch) && !PDFCharIsNumeric(ch)) {
          m_Pos--;
          CFX_ByteString ret(buffer, index);
          token = ret;
          return true;
        }

        if (index < sizeof(buffer))
          buffer[index++] = ch;
      }
    } else if (ch == '<') {
      if (!GetNextChar(ch))
        return false;

      if (ch == '<')
        buffer[index++] = ch;
      else
        m_Pos--;
    } else if (ch == '>') {
      if (!GetNextChar(ch))
        return false;

      if (ch == '>')
        buffer[index++] = ch;
      else
        m_Pos--;
    }

    CFX_ByteString ret(buffer, index);
    token = ret;
    return true;
  }

  while (1) {
    if (index < sizeof(buffer))
      buffer[index++] = ch;

    if (!GetNextChar(ch))
      return false;

    if (PDFCharIsDelimiter(ch) || PDFCharIsWhitespace(ch)) {
      m_Pos--;
      break;
    }
  }

  token = CFX_ByteString(buffer, index);
  return true;
}

bool CPDF_DataAvail::GetNextChar(uint8_t& ch) {
  FX_FILESIZE pos = m_Pos;
  if (pos >= m_dwFileLen)
    return false;

  if (m_bufferOffset >= pos ||
      (FX_FILESIZE)(m_bufferOffset + m_bufferSize) <= pos) {
    FX_FILESIZE read_pos = pos;
    uint32_t read_size = 512;
    if ((FX_FILESIZE)read_size > m_dwFileLen)
      read_size = (uint32_t)m_dwFileLen;

    if ((FX_FILESIZE)(read_pos + read_size) > m_dwFileLen)
      read_pos = m_dwFileLen - read_size;

    if (!m_pFileRead->ReadBlock(m_bufferData, read_pos, read_size))
      return false;

    m_bufferOffset = read_pos;
    m_bufferSize = read_size;
  }
  ch = m_bufferData[pos - m_bufferOffset];
  m_Pos++;
  return true;
}

bool CPDF_DataAvail::CheckCrossRefItem(DownloadHints* pHints) {
  int32_t iSize = 0;
  CFX_ByteString token;
  while (1) {
    if (!GetNextToken(token)) {
      iSize = (int32_t)(m_Pos + 512 > m_dwFileLen ? m_dwFileLen - m_Pos : 512);
      pHints->AddSegment(m_Pos, iSize);
      return false;
    }

    if (token == "trailer") {
      m_dwTrailerOffset = m_Pos;
      m_docStatus = PDF_DATAAVAIL_TRAILER;
      return true;
    }
  }
}

bool CPDF_DataAvail::CheckAllCrossRefStream(DownloadHints* pHints) {
  FX_FILESIZE xref_offset = 0;

  int32_t nRet = CheckCrossRefStream(pHints, xref_offset);
  if (nRet == 1) {
    if (!xref_offset) {
      m_docStatus = PDF_DATAAVAIL_LOADALLCROSSREF;
    } else {
      m_dwCurrentXRefSteam = xref_offset;
      m_Pos = xref_offset;
    }
    return true;
  }

  if (nRet == -1)
    m_docStatus = PDF_DATAAVAIL_ERROR;
  return false;
}

bool CPDF_DataAvail::CheckCrossRef(DownloadHints* pHints) {
  int32_t iSize = 0;
  CFX_ByteString token;
  if (!GetNextToken(token)) {
    iSize = (int32_t)(m_Pos + 512 > m_dwFileLen ? m_dwFileLen - m_Pos : 512);
    pHints->AddSegment(m_Pos, iSize);
    return false;
  }

  if (token == "xref") {
    while (1) {
      if (!GetNextToken(token)) {
        iSize =
            (int32_t)(m_Pos + 512 > m_dwFileLen ? m_dwFileLen - m_Pos : 512);
        pHints->AddSegment(m_Pos, iSize);
        m_docStatus = PDF_DATAAVAIL_CROSSREF_ITEM;
        return false;
      }

      if (token == "trailer") {
        m_dwTrailerOffset = m_Pos;
        m_docStatus = PDF_DATAAVAIL_TRAILER;
        return true;
      }
    }
  } else {
    m_docStatus = PDF_DATAAVAIL_LOADALLFILE;
    return true;
  }
  return false;
}

bool CPDF_DataAvail::CheckTrailerAppend(DownloadHints* pHints) {
  if (m_Pos < m_dwFileLen) {
    FX_FILESIZE dwAppendPos = m_Pos + m_syntaxParser.SavePos();
    int32_t iSize = (int32_t)(
        dwAppendPos + 512 > m_dwFileLen ? m_dwFileLen - dwAppendPos : 512);

    if (!m_pFileAvail->IsDataAvail(dwAppendPos, iSize)) {
      pHints->AddSegment(dwAppendPos, iSize);
      return false;
    }
  }

  if (m_dwPrevXRefOffset) {
    SetStartOffset(m_dwPrevXRefOffset);
    m_docStatus = PDF_DATAAVAIL_CROSSREF;
  } else {
    m_docStatus = PDF_DATAAVAIL_LOADALLCROSSREF;
  }
  return true;
}

bool CPDF_DataAvail::CheckTrailer(DownloadHints* pHints) {
  int32_t iTrailerSize =
      (int32_t)(m_Pos + 512 > m_dwFileLen ? m_dwFileLen - m_Pos : 512);
  if (m_pFileAvail->IsDataAvail(m_Pos, iTrailerSize)) {
    int32_t iSize = (int32_t)(m_Pos + iTrailerSize - m_dwTrailerOffset);
    CFX_BinaryBuf buf(iSize);
    uint8_t* pBuf = buf.GetBuffer();
    if (!pBuf) {
      m_docStatus = PDF_DATAAVAIL_ERROR;
      return false;
    }

    if (!m_pFileRead->ReadBlock(pBuf, m_dwTrailerOffset, iSize))
      return false;

    CFX_RetainPtr<IFX_MemoryStream> file =
        IFX_MemoryStream::Create(pBuf, (size_t)iSize, false);
    m_syntaxParser.InitParser(file, 0);

    std::unique_ptr<CPDF_Object> pTrailer(
        m_syntaxParser.GetObject(nullptr, 0, 0, true));
    if (!pTrailer) {
      m_Pos += m_syntaxParser.SavePos();
      pHints->AddSegment(m_Pos, iTrailerSize);
      return false;
    }

    if (!pTrailer->IsDictionary())
      return false;

    CPDF_Dictionary* pTrailerDict = pTrailer->GetDict();
    CPDF_Object* pEncrypt = pTrailerDict->GetObjectFor("Encrypt");
    if (ToReference(pEncrypt)) {
      m_docStatus = PDF_DATAAVAIL_LOADALLFILE;
      return true;
    }

    uint32_t xrefpos = GetDirectInteger(pTrailerDict, "Prev");
    if (xrefpos) {
      m_dwPrevXRefOffset = GetDirectInteger(pTrailerDict, "XRefStm");
      if (m_dwPrevXRefOffset) {
        m_docStatus = PDF_DATAAVAIL_LOADALLFILE;
      } else {
        m_dwPrevXRefOffset = xrefpos;
        if (m_dwPrevXRefOffset >= m_dwFileLen) {
          m_docStatus = PDF_DATAAVAIL_LOADALLFILE;
        } else {
          SetStartOffset(m_dwPrevXRefOffset);
          m_docStatus = PDF_DATAAVAIL_TRAILER_APPEND;
        }
      }
      return true;
    }
    m_dwPrevXRefOffset = 0;
    m_docStatus = PDF_DATAAVAIL_TRAILER_APPEND;
    return true;
  }
  pHints->AddSegment(m_Pos, iTrailerSize);
  return false;
}

bool CPDF_DataAvail::CheckPage(uint32_t dwPage, DownloadHints* pHints) {
  while (true) {
    switch (m_docStatus) {
      case PDF_DATAAVAIL_PAGETREE:
        if (!LoadDocPages(pHints))
          return false;
        break;
      case PDF_DATAAVAIL_PAGE:
        if (!LoadDocPage(dwPage, pHints))
          return false;
        break;
      case PDF_DATAAVAIL_ERROR:
        return LoadAllFile(pHints);
      default:
        m_bPagesTreeLoad = true;
        m_bPagesLoad = true;
        m_bCurPageDictLoadOK = true;
        m_docStatus = PDF_DATAAVAIL_PAGE;
        return true;
    }
  }
}

bool CPDF_DataAvail::CheckArrayPageNode(uint32_t dwPageNo,
                                        PageNode* pPageNode,
                                        DownloadHints* pHints) {
  bool bExists = false;
  std::unique_ptr<CPDF_Object> pPages = GetObject(dwPageNo, pHints, &bExists);
  if (!bExists) {
    m_docStatus = PDF_DATAAVAIL_ERROR;
    return false;
  }

  if (!pPages)
    return false;

  CPDF_Array* pArray = pPages->AsArray();
  if (!pArray) {
    m_docStatus = PDF_DATAAVAIL_ERROR;
    return false;
  }

  pPageNode->m_type = PDF_PAGENODE_PAGES;
  for (size_t i = 0; i < pArray->GetCount(); ++i) {
    CPDF_Reference* pKid = ToReference(pArray->GetObjectAt(i));
    if (!pKid)
      continue;

    auto pNode = pdfium::MakeUnique<PageNode>();
    pNode->m_dwPageNo = pKid->GetRefObjNum();
    pPageNode->m_ChildNodes.push_back(std::move(pNode));
  }
  return true;
}

bool CPDF_DataAvail::CheckUnknownPageNode(uint32_t dwPageNo,
                                          PageNode* pPageNode,
                                          DownloadHints* pHints) {
  bool bExists = false;
  std::unique_ptr<CPDF_Object> pPage = GetObject(dwPageNo, pHints, &bExists);
  if (!bExists) {
    m_docStatus = PDF_DATAAVAIL_ERROR;
    return false;
  }

  if (!pPage)
    return false;

  if (pPage->IsArray()) {
    pPageNode->m_dwPageNo = dwPageNo;
    pPageNode->m_type = PDF_PAGENODE_ARRAY;
    return true;
  }

  if (!pPage->IsDictionary()) {
    m_docStatus = PDF_DATAAVAIL_ERROR;
    return false;
  }

  pPageNode->m_dwPageNo = dwPageNo;
  CPDF_Dictionary* pDict = pPage->GetDict();
  CFX_ByteString type = pDict->GetStringFor("Type");
  if (type == "Pages") {
    pPageNode->m_type = PDF_PAGENODE_PAGES;
    CPDF_Object* pKids = pDict->GetObjectFor("Kids");
    if (!pKids) {
      m_docStatus = PDF_DATAAVAIL_PAGE;
      return true;
    }

    switch (pKids->GetType()) {
      case CPDF_Object::REFERENCE: {
        CPDF_Reference* pKid = pKids->AsReference();
        auto pNode = pdfium::MakeUnique<PageNode>();
        pNode->m_dwPageNo = pKid->GetRefObjNum();
        pPageNode->m_ChildNodes.push_back(std::move(pNode));
      } break;
      case CPDF_Object::ARRAY: {
        CPDF_Array* pKidsArray = pKids->AsArray();
        for (size_t i = 0; i < pKidsArray->GetCount(); ++i) {
          CPDF_Reference* pKid = ToReference(pKidsArray->GetObjectAt(i));
          if (!pKid)
            continue;

          auto pNode = pdfium::MakeUnique<PageNode>();
          pNode->m_dwPageNo = pKid->GetRefObjNum();
          pPageNode->m_ChildNodes.push_back(std::move(pNode));
        }
      } break;
      default:
        break;
    }
  } else if (type == "Page") {
    pPageNode->m_type = PDF_PAGENODE_PAGE;
  } else {
    m_docStatus = PDF_DATAAVAIL_ERROR;
    return false;
  }
  return true;
}

bool CPDF_DataAvail::CheckPageNode(const CPDF_DataAvail::PageNode& pageNode,
                                   int32_t iPage,
                                   int32_t& iCount,
                                   DownloadHints* pHints,
                                   int level) {
  if (level >= kMaxPageRecursionDepth)
    return false;

  int32_t iSize = pdfium::CollectionSize<int32_t>(pageNode.m_ChildNodes);
  if (iSize <= 0 || iPage >= iSize) {
    m_docStatus = PDF_DATAAVAIL_ERROR;
    return false;
  }
  for (int32_t i = 0; i < iSize; ++i) {
    PageNode* pNode = pageNode.m_ChildNodes[i].get();
    if (!pNode)
      continue;

    if (pNode->m_type == PDF_PAGENODE_UNKNOWN) {
      // Updates the type for the unknown page node.
      if (!CheckUnknownPageNode(pNode->m_dwPageNo, pNode, pHints))
        return false;
    }
    if (pNode->m_type == PDF_PAGENODE_ARRAY) {
      // Updates a more specific type for the array page node.
      if (!CheckArrayPageNode(pNode->m_dwPageNo, pNode, pHints))
        return false;
    }
    switch (pNode->m_type) {
      case PDF_PAGENODE_PAGE:
        iCount++;
        if (iPage == iCount && m_pDocument)
          m_pDocument->SetPageObjNum(iPage, pNode->m_dwPageNo);
        break;
      case PDF_PAGENODE_PAGES:
        if (!CheckPageNode(*pNode, iPage, iCount, pHints, level + 1))
          return false;
        break;
      case PDF_PAGENODE_UNKNOWN:
      case PDF_PAGENODE_ARRAY:
        // Already converted above, error if we get here.
        return false;
    }
    if (iPage == iCount) {
      m_docStatus = PDF_DATAAVAIL_DONE;
      return true;
    }
  }
  return true;
}

bool CPDF_DataAvail::LoadDocPage(uint32_t dwPage, DownloadHints* pHints) {
  FX_SAFE_INT32 safePage = pdfium::base::checked_cast<int32_t>(dwPage);
  int32_t iPage = safePage.ValueOrDie();
  if (m_pDocument->GetPageCount() <= iPage ||
      m_pDocument->IsPageLoaded(iPage)) {
    m_docStatus = PDF_DATAAVAIL_DONE;
    return true;
  }
  if (m_PageNode.m_type == PDF_PAGENODE_PAGE) {
    m_docStatus = iPage == 0 ? PDF_DATAAVAIL_DONE : PDF_DATAAVAIL_ERROR;
    return true;
  }
  int32_t iCount = -1;
  return CheckPageNode(m_PageNode, iPage, iCount, pHints, 0);
}

bool CPDF_DataAvail::CheckPageCount(DownloadHints* pHints) {
  bool bExists = false;
  std::unique_ptr<CPDF_Object> pPages =
      GetObject(m_PagesObjNum, pHints, &bExists);
  if (!bExists) {
    m_docStatus = PDF_DATAAVAIL_ERROR;
    return false;
  }
  if (!pPages)
    return false;

  CPDF_Dictionary* pPagesDict = pPages->GetDict();
  if (!pPagesDict) {
    m_docStatus = PDF_DATAAVAIL_ERROR;
    return false;
  }
  if (!pPagesDict->KeyExist("Kids"))
    return true;

  return pPagesDict->GetIntegerFor("Count") > 0;
}

bool CPDF_DataAvail::LoadDocPages(DownloadHints* pHints) {
  if (!CheckUnknownPageNode(m_PagesObjNum, &m_PageNode, pHints))
    return false;

  if (CheckPageCount(pHints)) {
    m_docStatus = PDF_DATAAVAIL_PAGE;
    return true;
  }

  m_bTotalLoadPageTree = true;
  return false;
}

bool CPDF_DataAvail::LoadPages(DownloadHints* pHints) {
  while (!m_bPagesTreeLoad) {
    if (!CheckPageStatus(pHints))
      return false;
  }

  if (m_bPagesLoad)
    return true;

  m_pDocument->LoadPages();
  return false;
}

CPDF_DataAvail::DocAvailStatus CPDF_DataAvail::CheckLinearizedData(
    DownloadHints* pHints) {
  if (m_bLinearedDataOK)
    return DataAvailable;
  ASSERT(m_pLinearized);
  if (!m_pLinearized->GetLastXRefOffset())
    return DataError;

  if (!m_bMainXRefLoadTried) {
    FX_SAFE_UINT32 data_size = m_dwFileLen;
    data_size -= m_pLinearized->GetLastXRefOffset();
    if (!data_size.IsValid())
      return DataError;

    if (!m_pFileAvail->IsDataAvail(m_pLinearized->GetLastXRefOffset(),
                                   data_size.ValueOrDie())) {
      pHints->AddSegment(m_pLinearized->GetLastXRefOffset(),
                         data_size.ValueOrDie());
      return DataNotAvailable;
    }

    CPDF_Parser::Error eRet =
        m_pDocument->GetParser()->LoadLinearizedMainXRefTable();
    m_bMainXRefLoadTried = true;
    if (eRet != CPDF_Parser::SUCCESS)
      return DataError;

    if (!PreparePageItem())
      return DataNotAvailable;

    m_bMainXRefLoadedOK = true;
    m_bLinearedDataOK = true;
  }

  return m_bLinearedDataOK ? DataAvailable : DataNotAvailable;
}

bool CPDF_DataAvail::CheckPageAnnots(uint32_t dwPage, DownloadHints* pHints) {
  if (m_objs_array.empty()) {
    m_ObjectSet.clear();

    FX_SAFE_INT32 safePage = pdfium::base::checked_cast<int32_t>(dwPage);
    CPDF_Dictionary* pPageDict = m_pDocument->GetPage(safePage.ValueOrDie());
    if (!pPageDict)
      return true;

    CPDF_Object* pAnnots = pPageDict->GetObjectFor("Annots");
    if (!pAnnots)
      return true;

    std::vector<CPDF_Object*> obj_array;
    obj_array.push_back(pAnnots);
    if (!AreObjectsAvailable(obj_array, false, pHints, m_objs_array))
      return false;

    m_objs_array.clear();
    return true;
  }

  std::vector<CPDF_Object*> new_objs_array;
  if (!AreObjectsAvailable(m_objs_array, false, pHints, new_objs_array)) {
    m_objs_array = new_objs_array;
    return false;
  }
  m_objs_array.clear();
  return true;
}

CPDF_DataAvail::DocAvailStatus CPDF_DataAvail::CheckLinearizedFirstPage(
    uint32_t dwPage,
    DownloadHints* pHints) {
  if (!m_bAnnotsLoad) {
    if (!CheckPageAnnots(dwPage, pHints))
      return DataNotAvailable;
    m_bAnnotsLoad = true;
  }
  const bool is_page_valid = ValidatePage(dwPage);
  (void)is_page_valid;
  ASSERT(is_page_valid);
  return DataAvailable;
}

bool CPDF_DataAvail::HaveResourceAncestor(CPDF_Dictionary* pDict) {
  CFX_AutoRestorer<int> restorer(&s_CurrentDataAvailRecursionDepth);
  if (++s_CurrentDataAvailRecursionDepth > kMaxDataAvailRecursionDepth)
    return false;

  CPDF_Object* pParent = pDict->GetObjectFor("Parent");
  if (!pParent)
    return false;

  CPDF_Dictionary* pParentDict = pParent->GetDict();
  if (!pParentDict)
    return false;

  CPDF_Object* pRet = pParentDict->GetObjectFor("Resources");
  if (pRet) {
    m_pPageResource = pRet;
    return true;
  }

  return HaveResourceAncestor(pParentDict);
}

CPDF_DataAvail::DocAvailStatus CPDF_DataAvail::IsPageAvail(
    uint32_t dwPage,
    DownloadHints* pHints) {
  if (!m_pDocument)
    return DataError;

  if (IsFirstCheck(dwPage)) {
    m_bCurPageDictLoadOK = false;
    m_bPageLoadedOK = false;
    m_bAnnotsLoad = false;
    m_bNeedDownLoadResource = false;
    m_objs_array.clear();
    m_ObjectSet.clear();
  }

  if (pdfium::ContainsKey(m_pagesLoadState, dwPage))
    return DataAvailable;

  if (m_pLinearized) {
    if (dwPage == m_pLinearized->GetFirstPageNo()) {
      DocAvailStatus nRet = CheckLinearizedFirstPage(dwPage, pHints);
      if (nRet == DataAvailable)
        m_pagesLoadState.insert(dwPage);
      return nRet;
    }

    DocAvailStatus nResult = CheckLinearizedData(pHints);
    if (nResult != DataAvailable)
      return nResult;

    if (m_pHintTables) {
      nResult = m_pHintTables->CheckPage(dwPage, pHints);
      if (nResult != DataAvailable)
        return nResult;
      m_pagesLoadState.insert(dwPage);
      return GetPage(dwPage) ? DataAvailable : DataError;
    }

    if (m_bMainXRefLoadedOK) {
      if (m_bTotalLoadPageTree) {
        if (!LoadPages(pHints))
          return DataNotAvailable;
      } else {
        if (!m_bCurPageDictLoadOK && !CheckPage(dwPage, pHints))
          return DataNotAvailable;
      }
    } else {
      if (!LoadAllFile(pHints))
        return DataNotAvailable;
      m_pDocument->GetParser()->RebuildCrossRef();
      ResetFirstCheck(dwPage);
      return DataAvailable;
    }
  } else {
    if (!m_bTotalLoadPageTree && !m_bCurPageDictLoadOK &&
        !CheckPage(dwPage, pHints)) {
      return DataNotAvailable;
    }
  }

  if (m_bHaveAcroForm && !m_bAcroFormLoad) {
    if (!CheckAcroFormSubObject(pHints))
      return DataNotAvailable;
    m_bAcroFormLoad = true;
  }

  if (!m_bPageLoadedOK) {
    if (m_objs_array.empty()) {
      m_ObjectSet.clear();

      FX_SAFE_INT32 safePage = pdfium::base::checked_cast<int32_t>(dwPage);
      m_pPageDict = m_pDocument->GetPage(safePage.ValueOrDie());
      if (!m_pPageDict) {
        ResetFirstCheck(dwPage);
        // This is XFA page.
        return DataAvailable;
      }

      std::vector<CPDF_Object*> obj_array;
      obj_array.push_back(m_pPageDict);
      if (!AreObjectsAvailable(obj_array, true, pHints, m_objs_array))
        return DataNotAvailable;

      m_objs_array.clear();
    } else {
      std::vector<CPDF_Object*> new_objs_array;
      if (!AreObjectsAvailable(m_objs_array, false, pHints, new_objs_array)) {
        m_objs_array = new_objs_array;
        return DataNotAvailable;
      }
    }
    m_objs_array.clear();
    m_bPageLoadedOK = true;
  }

  if (!m_bAnnotsLoad) {
    if (!CheckPageAnnots(dwPage, pHints))
      return DataNotAvailable;
    m_bAnnotsLoad = true;
  }

  if (m_pPageDict && !m_bNeedDownLoadResource) {
    m_pPageResource = m_pPageDict->GetObjectFor("Resources");
    m_bNeedDownLoadResource =
        m_pPageResource || HaveResourceAncestor(m_pPageDict);
  }

  if (m_bNeedDownLoadResource) {
    if (!CheckResources(pHints))
      return DataNotAvailable;
    m_bNeedDownLoadResource = false;
  }

  m_bPageLoadedOK = false;
  m_bAnnotsLoad = false;
  m_bCurPageDictLoadOK = false;

  ResetFirstCheck(dwPage);
  m_pagesLoadState.insert(dwPage);
  const bool is_page_valid = ValidatePage(dwPage);
  (void)is_page_valid;
  ASSERT(is_page_valid);
  return DataAvailable;
}

bool CPDF_DataAvail::CheckResources(DownloadHints* pHints) {
  if (m_objs_array.empty()) {
    std::vector<CPDF_Object*> obj_array;
    obj_array.push_back(m_pPageResource);
    if (!AreObjectsAvailable(obj_array, true, pHints, m_objs_array))
      return false;

    m_objs_array.clear();
    return true;
  }
  std::vector<CPDF_Object*> new_objs_array;
  if (!AreObjectsAvailable(m_objs_array, false, pHints, new_objs_array)) {
    m_objs_array = new_objs_array;
    return false;
  }
  m_objs_array.clear();
  return true;
}

void CPDF_DataAvail::GetLinearizedMainXRefInfo(FX_FILESIZE* pPos,
                                               uint32_t* pSize) {
  if (pPos)
    *pPos = m_dwLastXRefOffset;
  if (pSize)
    *pSize = (uint32_t)(m_dwFileLen - m_dwLastXRefOffset);
}

int CPDF_DataAvail::GetPageCount() const {
  if (m_pLinearized)
    return m_pLinearized->GetPageCount();
  return m_pDocument ? m_pDocument->GetPageCount() : 0;
}

CPDF_Dictionary* CPDF_DataAvail::GetPage(int index) {
  if (!m_pDocument || index < 0 || index >= GetPageCount())
    return nullptr;
  CPDF_Dictionary* page = m_pDocument->GetPage(index);
  if (page)
    return page;
  if (!m_pLinearized || !m_pHintTables)
    return nullptr;

  if (index == static_cast<int>(m_pLinearized->GetFirstPageNo()))
    return nullptr;
  FX_FILESIZE szPageStartPos = 0;
  FX_FILESIZE szPageLength = 0;
  uint32_t dwObjNum = 0;
  const bool bPagePosGot = m_pHintTables->GetPagePos(index, &szPageStartPos,
                                                     &szPageLength, &dwObjNum);
  if (!bPagePosGot || !dwObjNum)
    return nullptr;
  // We should say to the document, which object is the page.
  m_pDocument->SetPageObjNum(index, dwObjNum);
  // Page object already can be parsed in document.
  if (!m_pDocument->GetIndirectObject(dwObjNum)) {
    m_syntaxParser.InitParser(
        m_pFileRead, pdfium::base::checked_cast<uint32_t>(szPageStartPos));
    m_pDocument->ReplaceIndirectObjectIfHigherGeneration(
        dwObjNum, ParseIndirectObjectAt(0, dwObjNum, m_pDocument));
  }
  const bool is_page_valid = ValidatePage(index);
  (void)is_page_valid;
  ASSERT(is_page_valid);
  return m_pDocument->GetPage(index);
}

CPDF_DataAvail::DocFormStatus CPDF_DataAvail::IsFormAvail(
    DownloadHints* pHints) {
  if (!m_pDocument)
    return FormAvailable;
  if (m_pLinearized) {
    DocAvailStatus nDocStatus = CheckLinearizedData(pHints);
    if (nDocStatus == DataError)
      return FormError;
    if (nDocStatus == DataNotAvailable)
      return FormNotAvailable;
  }

  if (!m_bLinearizedFormParamLoad) {
    CPDF_Dictionary* pRoot = m_pDocument->GetRoot();
    if (!pRoot)
      return FormAvailable;

    CPDF_Object* pAcroForm = pRoot->GetObjectFor("AcroForm");
    if (!pAcroForm)
      return FormNotExist;

    m_objs_array.push_back(pAcroForm->GetDict());
    m_bLinearizedFormParamLoad = true;
  }

  std::vector<CPDF_Object*> new_objs_array;
  if (!AreObjectsAvailable(m_objs_array, false, pHints, new_objs_array)) {
    m_objs_array = new_objs_array;
    return FormNotAvailable;
  }

  m_objs_array.clear();
  const bool is_form_valid = ValidateForm();
  (void)is_form_valid;
  ASSERT(is_form_valid);
  return FormAvailable;
}

bool CPDF_DataAvail::ValidatePage(uint32_t dwPage) {
  FX_SAFE_INT32 safePage = pdfium::base::checked_cast<int32_t>(dwPage);
  CPDF_Dictionary* pPageDict = m_pDocument->GetPage(safePage.ValueOrDie());
  if (!pPageDict)
    return false;
  std::vector<CPDF_Object*> obj_array;
  obj_array.push_back(pPageDict);
  std::vector<CPDF_Object*> dummy;
  return AreObjectsAvailable(obj_array, true, nullptr, dummy);
}

bool CPDF_DataAvail::ValidateForm() {
  CPDF_Dictionary* pRoot = m_pDocument->GetRoot();
  if (!pRoot)
    return true;
  CPDF_Object* pAcroForm = pRoot->GetObjectFor("AcroForm");
  if (!pAcroForm)
    return false;
  std::vector<CPDF_Object*> obj_array;
  obj_array.push_back(pAcroForm);
  std::vector<CPDF_Object*> dummy;
  return AreObjectsAvailable(obj_array, true, nullptr, dummy);
}

CPDF_DataAvail::PageNode::PageNode() : m_type(PDF_PAGENODE_UNKNOWN) {}

CPDF_DataAvail::PageNode::~PageNode() {}
