// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "public/fpdf_save.h"

#include <memory>
#include <utility>
#include <vector>

#include "core/fpdfapi/edit/cpdf_creator.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fxcrt/fx_ext.h"
#include "fpdfsdk/fsdk_define.h"
#include "public/fpdf_edit.h"

#ifdef PDF_ENABLE_XFA
#include "fpdfsdk/fpdfxfa/cpdfxfa_context.h"
#include "fpdfsdk/fpdfxfa/cxfa_fwladaptertimermgr.h"
#include "public/fpdf_formfill.h"
#include "xfa/fxfa/cxfa_eventparam.h"
#include "xfa/fxfa/xfa_checksum.h"
#include "xfa/fxfa/xfa_ffapp.h"
#include "xfa/fxfa/xfa_ffdocview.h"
#include "xfa/fxfa/xfa_ffwidgethandler.h"
#endif

#if _FX_OS_ == _FX_ANDROID_
#include <time.h>
#else
#include <ctime>
#endif

class CFX_IFileWrite final : public IFX_WriteStream {
 public:
  static CFX_RetainPtr<CFX_IFileWrite> Create();
  bool Init(FPDF_FILEWRITE* pFileWriteStruct);
  bool WriteBlock(const void* pData, size_t size) override;

 protected:
  CFX_IFileWrite();
  ~CFX_IFileWrite() override {}

  FPDF_FILEWRITE* m_pFileWriteStruct;
};

CFX_RetainPtr<CFX_IFileWrite> CFX_IFileWrite::Create() {
  return CFX_RetainPtr<CFX_IFileWrite>(new CFX_IFileWrite());
}

CFX_IFileWrite::CFX_IFileWrite() : m_pFileWriteStruct(nullptr) {}

bool CFX_IFileWrite::Init(FPDF_FILEWRITE* pFileWriteStruct) {
  if (!pFileWriteStruct)
    return false;

  m_pFileWriteStruct = pFileWriteStruct;
  return true;
}

bool CFX_IFileWrite::WriteBlock(const void* pData, size_t size) {
  if (!m_pFileWriteStruct)
    return false;

  m_pFileWriteStruct->WriteBlock(m_pFileWriteStruct, pData, size);
  return true;
}

namespace {

#ifdef PDF_ENABLE_XFA
bool SaveXFADocumentData(
    CPDFXFA_Context* pContext,
    std::vector<CFX_RetainPtr<IFX_SeekableStream>>* fileList) {
  if (!pContext)
    return false;

  if (pContext->GetDocType() != DOCTYPE_DYNAMIC_XFA &&
      pContext->GetDocType() != DOCTYPE_STATIC_XFA)
    return true;

  CXFA_FFDocView* pXFADocView = pContext->GetXFADocView();
  if (!pXFADocView)
    return true;

  CPDF_Document* pPDFDocument = pContext->GetPDFDoc();
  if (!pPDFDocument)
    return false;

  CPDF_Dictionary* pRoot = pPDFDocument->GetRoot();
  if (!pRoot)
    return false;

  CPDF_Dictionary* pAcroForm = pRoot->GetDictFor("AcroForm");
  if (!pAcroForm)
    return false;

  CPDF_Object* pXFA = pAcroForm->GetObjectFor("XFA");
  if (!pXFA)
    return true;

  CPDF_Array* pArray = pXFA->AsArray();
  if (!pArray)
    return false;

  int size = pArray->GetCount();
  int iFormIndex = -1;
  int iDataSetsIndex = -1;
  int iTemplate = -1;
  int iLast = size - 2;
  for (int i = 0; i < size - 1; i++) {
    CPDF_Object* pPDFObj = pArray->GetObjectAt(i);
    if (!pPDFObj->IsString())
      continue;
    if (pPDFObj->GetString() == "form")
      iFormIndex = i + 1;
    else if (pPDFObj->GetString() == "datasets")
      iDataSetsIndex = i + 1;
    else if (pPDFObj->GetString() == "template")
      iTemplate = i + 1;
  }
  std::unique_ptr<CXFA_ChecksumContext> pChecksum(new CXFA_ChecksumContext);
  pChecksum->StartChecksum();

  // template
  if (iTemplate > -1) {
    CPDF_Stream* pTemplateStream = pArray->GetStreamAt(iTemplate);
    CPDF_StreamAcc streamAcc;
    streamAcc.LoadAllData(pTemplateStream);
    uint8_t* pData = (uint8_t*)streamAcc.GetData();
    uint32_t dwSize2 = streamAcc.GetSize();
    CFX_RetainPtr<IFX_SeekableStream> pTemplate =
        IFX_MemoryStream::Create(pData, dwSize2);
    pChecksum->UpdateChecksum(pTemplate);
  }
  CPDF_Stream* pFormStream = nullptr;
  CPDF_Stream* pDataSetsStream = nullptr;
  if (iFormIndex != -1) {
    // Get form CPDF_Stream
    CPDF_Object* pFormPDFObj = pArray->GetObjectAt(iFormIndex);
    if (pFormPDFObj->IsReference()) {
      CPDF_Object* pFormDirectObj = pFormPDFObj->GetDirect();
      if (pFormDirectObj && pFormDirectObj->IsStream()) {
        pFormStream = (CPDF_Stream*)pFormDirectObj;
      }
    } else if (pFormPDFObj->IsStream()) {
      pFormStream = (CPDF_Stream*)pFormPDFObj;
    }
  }

  if (iDataSetsIndex != -1) {
    // Get datasets CPDF_Stream
    CPDF_Object* pDataSetsPDFObj = pArray->GetObjectAt(iDataSetsIndex);
    if (pDataSetsPDFObj->IsReference()) {
      CPDF_Reference* pDataSetsRefObj = (CPDF_Reference*)pDataSetsPDFObj;
      CPDF_Object* pDataSetsDirectObj = pDataSetsRefObj->GetDirect();
      if (pDataSetsDirectObj && pDataSetsDirectObj->IsStream()) {
        pDataSetsStream = (CPDF_Stream*)pDataSetsDirectObj;
      }
    } else if (pDataSetsPDFObj->IsStream()) {
      pDataSetsStream = (CPDF_Stream*)pDataSetsPDFObj;
    }
  }
  // L"datasets"
  {
    CFX_RetainPtr<IFX_SeekableStream> pDsfileWrite = IFX_MemoryStream::Create();
    if (pXFADocView->GetDoc()->SavePackage(XFA_HASHCODE_Datasets, pDsfileWrite,
                                           nullptr) &&
        pDsfileWrite->GetSize() > 0) {
      // Datasets
      pChecksum->UpdateChecksum(pDsfileWrite);
      pChecksum->FinishChecksum();
      auto pDataDict = pdfium::MakeUnique<CPDF_Dictionary>(
          pPDFDocument->GetByteStringPool());
      if (iDataSetsIndex != -1) {
        if (pDataSetsStream) {
          pDataSetsStream->InitStreamFromFile(pDsfileWrite,
                                              std::move(pDataDict));
        }
      } else {
        CPDF_Stream* pData = pPDFDocument->NewIndirect<CPDF_Stream>();
        pData->InitStreamFromFile(pDsfileWrite, std::move(pDataDict));
        iLast = pArray->GetCount() - 2;
        pArray->InsertNewAt<CPDF_String>(iLast, "datasets", false);
        pArray->InsertNewAt<CPDF_Reference>(iLast + 1, pPDFDocument,
                                            pData->GetObjNum());
      }
      fileList->push_back(std::move(pDsfileWrite));
    }
  }
  // L"form"
  {
    CFX_RetainPtr<IFX_SeekableStream> pfileWrite = IFX_MemoryStream::Create();
    if (pXFADocView->GetDoc()->SavePackage(XFA_HASHCODE_Form, pfileWrite,
                                           pChecksum.get()) &&
        pfileWrite->GetSize() > 0) {
      auto pDataDict = pdfium::MakeUnique<CPDF_Dictionary>(
          pPDFDocument->GetByteStringPool());
      if (iFormIndex != -1) {
        if (pFormStream)
          pFormStream->InitStreamFromFile(pfileWrite, std::move(pDataDict));
      } else {
        CPDF_Stream* pData = pPDFDocument->NewIndirect<CPDF_Stream>();
        pData->InitStreamFromFile(pfileWrite, std::move(pDataDict));
        iLast = pArray->GetCount() - 2;
        pArray->InsertNewAt<CPDF_String>(iLast, "form", false);
        pArray->InsertNewAt<CPDF_Reference>(iLast + 1, pPDFDocument,
                                            pData->GetObjNum());
      }
      fileList->push_back(std::move(pfileWrite));
    }
  }
  return true;
}

bool SendPostSaveToXFADoc(CPDFXFA_Context* pContext) {
  if (!pContext)
    return false;

  if (pContext->GetDocType() != DOCTYPE_DYNAMIC_XFA &&
      pContext->GetDocType() != DOCTYPE_STATIC_XFA)
    return true;

  CXFA_FFDocView* pXFADocView = pContext->GetXFADocView();
  if (!pXFADocView)
    return false;

  CXFA_FFWidgetHandler* pWidgetHander = pXFADocView->GetWidgetHandler();
  std::unique_ptr<CXFA_WidgetAccIterator> pWidgetAccIterator(
      pXFADocView->CreateWidgetAccIterator());
  while (CXFA_WidgetAcc* pWidgetAcc = pWidgetAccIterator->MoveToNext()) {
    CXFA_EventParam preParam;
    preParam.m_eType = XFA_EVENT_PostSave;
    pWidgetHander->ProcessEvent(pWidgetAcc, &preParam);
  }
  pXFADocView->UpdateDocView();
  pContext->ClearChangeMark();
  return true;
}

bool SendPreSaveToXFADoc(
    CPDFXFA_Context* pContext,
    std::vector<CFX_RetainPtr<IFX_SeekableStream>>* fileList) {
  if (pContext->GetDocType() != DOCTYPE_DYNAMIC_XFA &&
      pContext->GetDocType() != DOCTYPE_STATIC_XFA)
    return true;

  CXFA_FFDocView* pXFADocView = pContext->GetXFADocView();
  if (!pXFADocView)
    return true;

  CXFA_FFWidgetHandler* pWidgetHander = pXFADocView->GetWidgetHandler();
  std::unique_ptr<CXFA_WidgetAccIterator> pWidgetAccIterator(
      pXFADocView->CreateWidgetAccIterator());
  while (CXFA_WidgetAcc* pWidgetAcc = pWidgetAccIterator->MoveToNext()) {
    CXFA_EventParam preParam;
    preParam.m_eType = XFA_EVENT_PreSave;
    pWidgetHander->ProcessEvent(pWidgetAcc, &preParam);
  }
  pXFADocView->UpdateDocView();
  return SaveXFADocumentData(pContext, fileList);
}
#endif  // PDF_ENABLE_XFA

bool FPDF_Doc_Save(FPDF_DOCUMENT document,
                   FPDF_FILEWRITE* pFileWrite,
                   FPDF_DWORD flags,
                   FPDF_BOOL bSetVersion,
                   int fileVerion) {
  CPDF_Document* pPDFDoc = CPDFDocumentFromFPDFDocument(document);
  if (!pPDFDoc)
    return 0;

#ifdef PDF_ENABLE_XFA
  CPDFXFA_Context* pContext = static_cast<CPDFXFA_Context*>(document);
  std::vector<CFX_RetainPtr<IFX_SeekableStream>> fileList;
  SendPreSaveToXFADoc(pContext, &fileList);
#endif  // PDF_ENABLE_XFA

  if (flags < FPDF_INCREMENTAL || flags > FPDF_REMOVE_SECURITY)
    flags = 0;

  CPDF_Creator FileMaker(pPDFDoc);
  if (bSetVersion)
    FileMaker.SetFileVersion(fileVerion);
  if (flags == FPDF_REMOVE_SECURITY) {
    flags = 0;
    FileMaker.RemoveSecurity();
  }

  CFX_RetainPtr<CFX_IFileWrite> pStreamWrite = CFX_IFileWrite::Create();
  pStreamWrite->Init(pFileWrite);
  bool bRet = FileMaker.Create(pStreamWrite, flags);
#ifdef PDF_ENABLE_XFA
  SendPostSaveToXFADoc(pContext);
#endif  // PDF_ENABLE_XFA
  return bRet;
}

}  // namespace

DLLEXPORT FPDF_BOOL STDCALL FPDF_SaveAsCopy(FPDF_DOCUMENT document,
                                            FPDF_FILEWRITE* pFileWrite,
                                            FPDF_DWORD flags) {
  return FPDF_Doc_Save(document, pFileWrite, flags, false, 0);
}

DLLEXPORT FPDF_BOOL STDCALL FPDF_SaveWithVersion(FPDF_DOCUMENT document,
                                                 FPDF_FILEWRITE* pFileWrite,
                                                 FPDF_DWORD flags,
                                                 int fileVersion) {
  return FPDF_Doc_Save(document, pFileWrite, flags, true, fileVersion);
}
