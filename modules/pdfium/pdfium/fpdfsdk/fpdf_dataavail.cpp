// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "public/fpdf_dataavail.h"

#include <memory>
#include <utility>

#include "core/fpdfapi/parser/cpdf_data_avail.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fxcrt/cfx_retain_ptr.h"
#include "fpdfsdk/fsdk_define.h"
#include "public/fpdf_formfill.h"
#include "third_party/base/ptr_util.h"

// These checks are here because core/ and public/ cannot depend on each other.
static_assert(CPDF_DataAvail::DataError == PDF_DATA_ERROR,
              "CPDF_DataAvail::DataError value mismatch");
static_assert(CPDF_DataAvail::DataNotAvailable == PDF_DATA_NOTAVAIL,
              "CPDF_DataAvail::DataNotAvailable value mismatch");
static_assert(CPDF_DataAvail::DataAvailable == PDF_DATA_AVAIL,
              "CPDF_DataAvail::DataAvailable value mismatch");

static_assert(CPDF_DataAvail::LinearizationUnknown == PDF_LINEARIZATION_UNKNOWN,
              "CPDF_DataAvail::LinearizationUnknown value mismatch");
static_assert(CPDF_DataAvail::NotLinearized == PDF_NOT_LINEARIZED,
              "CPDF_DataAvail::NotLinearized value mismatch");
static_assert(CPDF_DataAvail::Linearized == PDF_LINEARIZED,
              "CPDF_DataAvail::Linearized value mismatch");

static_assert(CPDF_DataAvail::FormError == PDF_FORM_ERROR,
              "CPDF_DataAvail::FormError value mismatch");
static_assert(CPDF_DataAvail::FormNotAvailable == PDF_FORM_NOTAVAIL,
              "CPDF_DataAvail::FormNotAvailable value mismatch");
static_assert(CPDF_DataAvail::FormAvailable == PDF_FORM_AVAIL,
              "CPDF_DataAvail::FormAvailable value mismatch");
static_assert(CPDF_DataAvail::FormNotExist == PDF_FORM_NOTEXIST,
              "CPDF_DataAvail::FormNotExist value mismatch");

namespace {

class CFPDF_FileAvailWrap : public CPDF_DataAvail::FileAvail {
 public:
  CFPDF_FileAvailWrap() : m_pfileAvail(nullptr) {}
  ~CFPDF_FileAvailWrap() override {}

  void Set(FX_FILEAVAIL* pfileAvail) { m_pfileAvail = pfileAvail; }

  // CPDF_DataAvail::FileAvail:
  bool IsDataAvail(FX_FILESIZE offset, uint32_t size) override {
    return !!m_pfileAvail->IsDataAvail(m_pfileAvail, offset, size);
  }

 private:
  FX_FILEAVAIL* m_pfileAvail;
};

class CFPDF_FileAccessWrap : public IFX_SeekableReadStream {
 public:
  static CFX_RetainPtr<CFPDF_FileAccessWrap> Create() {
    return CFX_RetainPtr<CFPDF_FileAccessWrap>(new CFPDF_FileAccessWrap());
  }
  ~CFPDF_FileAccessWrap() override {}

  void Set(FPDF_FILEACCESS* pFile) { m_pFileAccess = pFile; }

  // IFX_SeekableReadStream
  FX_FILESIZE GetSize() override { return m_pFileAccess->m_FileLen; }

  bool ReadBlock(void* buffer, FX_FILESIZE offset, size_t size) override {
    return !!m_pFileAccess->m_GetBlock(m_pFileAccess->m_Param, offset,
                                       (uint8_t*)buffer, size);
  }

 private:
  CFPDF_FileAccessWrap() : m_pFileAccess(nullptr) {}

  FPDF_FILEACCESS* m_pFileAccess;
};

class CFPDF_DownloadHintsWrap : public CPDF_DataAvail::DownloadHints {
 public:
  explicit CFPDF_DownloadHintsWrap(FX_DOWNLOADHINTS* pDownloadHints) {
    m_pDownloadHints = pDownloadHints;
  }
  ~CFPDF_DownloadHintsWrap() override {}

 public:
  // IFX_DownloadHints
  void AddSegment(FX_FILESIZE offset, uint32_t size) override {
    m_pDownloadHints->AddSegment(m_pDownloadHints, offset, size);
  }

 private:
  FX_DOWNLOADHINTS* m_pDownloadHints;
};

class CFPDF_DataAvail {
 public:
  CFPDF_DataAvail()
      : m_FileAvail(new CFPDF_FileAvailWrap),
        m_FileRead(CFPDF_FileAccessWrap::Create()) {}
  ~CFPDF_DataAvail() {}

  std::unique_ptr<CPDF_DataAvail> m_pDataAvail;
  std::unique_ptr<CFPDF_FileAvailWrap> m_FileAvail;
  CFX_RetainPtr<CFPDF_FileAccessWrap> m_FileRead;
};

CFPDF_DataAvail* CFPDFDataAvailFromFPDFAvail(FPDF_AVAIL avail) {
  return static_cast<CFPDF_DataAvail*>(avail);
}

}  // namespace

DLLEXPORT FPDF_AVAIL STDCALL FPDFAvail_Create(FX_FILEAVAIL* file_avail,
                                              FPDF_FILEACCESS* file) {
  CFPDF_DataAvail* pAvail = new CFPDF_DataAvail;
  pAvail->m_FileAvail->Set(file_avail);
  pAvail->m_FileRead->Set(file);
  pAvail->m_pDataAvail = pdfium::MakeUnique<CPDF_DataAvail>(
      pAvail->m_FileAvail.get(), pAvail->m_FileRead, true);
  return pAvail;
}

DLLEXPORT void STDCALL FPDFAvail_Destroy(FPDF_AVAIL avail) {
  delete (CFPDF_DataAvail*)avail;
}

DLLEXPORT int STDCALL FPDFAvail_IsDocAvail(FPDF_AVAIL avail,
                                           FX_DOWNLOADHINTS* hints) {
  if (!avail || !hints)
    return PDF_DATA_ERROR;
  CFPDF_DownloadHintsWrap hints_wrap(hints);
  return CFPDFDataAvailFromFPDFAvail(avail)->m_pDataAvail->IsDocAvail(
      &hints_wrap);
}

DLLEXPORT FPDF_DOCUMENT STDCALL
FPDFAvail_GetDocument(FPDF_AVAIL avail, FPDF_BYTESTRING password) {
  CFPDF_DataAvail* pDataAvail = static_cast<CFPDF_DataAvail*>(avail);
  if (!pDataAvail)
    return nullptr;

  std::unique_ptr<CPDF_Parser> pParser(new CPDF_Parser);
  pParser->SetPassword(password);

  std::unique_ptr<CPDF_Document> pDocument(
      new CPDF_Document(std::move(pParser)));
  CPDF_Parser::Error error = pDocument->GetParser()->StartLinearizedParse(
      pDataAvail->m_pDataAvail->GetFileRead(), pDocument.get());
  if (error != CPDF_Parser::SUCCESS) {
    ProcessParseError(error);
    return nullptr;
  }
  pDataAvail->m_pDataAvail->SetDocument(pDocument.get());
  CheckUnSupportError(pDocument.get(), FPDF_ERR_SUCCESS);
  return FPDFDocumentFromCPDFDocument(pDocument.release());
}

DLLEXPORT int STDCALL FPDFAvail_GetFirstPageNum(FPDF_DOCUMENT doc) {
  CPDF_Document* pDoc = CPDFDocumentFromFPDFDocument(doc);
  return pDoc ? pDoc->GetParser()->GetFirstPageNo() : 0;
}

DLLEXPORT int STDCALL FPDFAvail_IsPageAvail(FPDF_AVAIL avail,
                                            int page_index,
                                            FX_DOWNLOADHINTS* hints) {
  if (!avail || !hints)
    return PDF_DATA_ERROR;
  if (page_index < 0)
    return PDF_DATA_NOTAVAIL;
  CFPDF_DownloadHintsWrap hints_wrap(hints);
  return CFPDFDataAvailFromFPDFAvail(avail)->m_pDataAvail->IsPageAvail(
      page_index, &hints_wrap);
}

DLLEXPORT int STDCALL FPDFAvail_IsFormAvail(FPDF_AVAIL avail,
                                            FX_DOWNLOADHINTS* hints) {
  if (!avail || !hints)
    return PDF_FORM_ERROR;
  CFPDF_DownloadHintsWrap hints_wrap(hints);
  return CFPDFDataAvailFromFPDFAvail(avail)->m_pDataAvail->IsFormAvail(
      &hints_wrap);
}

DLLEXPORT int STDCALL FPDFAvail_IsLinearized(FPDF_AVAIL avail) {
  if (!avail)
    return PDF_LINEARIZATION_UNKNOWN;
  return CFPDFDataAvailFromFPDFAvail(avail)->m_pDataAvail->IsLinearizedPDF();
}
