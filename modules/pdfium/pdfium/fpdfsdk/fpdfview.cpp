// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "public/fpdfview.h"

#include <memory>
#include <utility>

#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/cpdf_pagerendercontext.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fpdfapi/render/cpdf_progressiverenderer.h"
#include "core/fpdfapi/render/cpdf_renderoptions.h"
#include "core/fpdfdoc/cpdf_annotlist.h"
#include "core/fpdfdoc/cpdf_nametree.h"
#include "core/fpdfdoc/cpdf_occontext.h"
#include "core/fpdfdoc/cpdf_viewerpreferences.h"
#include "core/fxcodec/fx_codec.h"
#include "core/fxcrt/fx_memory.h"
#include "core/fxcrt/fx_safe_types.h"
#include "core/fxge/cfx_fxgedevice.h"
#include "core/fxge/cfx_gemodule.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_pageview.h"
#include "fpdfsdk/fsdk_define.h"
#include "fpdfsdk/fsdk_pauseadapter.h"
#include "fpdfsdk/javascript/ijs_runtime.h"
#include "public/fpdf_ext.h"
#include "public/fpdf_progressive.h"
#include "third_party/base/numerics/safe_conversions_impl.h"
#include "third_party/base/ptr_util.h"

#ifdef PDF_ENABLE_XFA
#include "fpdfsdk/fpdfxfa/cpdfxfa_context.h"
#include "fpdfsdk/fpdfxfa/cpdfxfa_page.h"
#include "fpdfsdk/fpdfxfa/cxfa_fwladaptertimermgr.h"
#include "public/fpdf_formfill.h"
#include "xfa/fxbarcode/BC_Library.h"
#endif  // PDF_ENABLE_XFA

#ifdef PDF_ENABLE_XFA_BMP
#include "core/fxcodec/codec/ccodec_bmpmodule.h"
#endif

#ifdef PDF_ENABLE_XFA_GIF
#include "core/fxcodec/codec/ccodec_gifmodule.h"
#endif

#ifdef PDF_ENABLE_XFA_PNG
#include "core/fxcodec/codec/ccodec_pngmodule.h"
#endif

#ifdef PDF_ENABLE_XFA_TIFF
#include "core/fxcodec/codec/ccodec_tiffmodule.h"
#endif

#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
#include "core/fxge/cfx_windowsdevice.h"
#endif

namespace {

// Also indicates whether library is currently initialized.
CCodec_ModuleMgr* g_pCodecModule = nullptr;

void RenderPageImpl(CPDF_PageRenderContext* pContext,
                    CPDF_Page* pPage,
                    const CFX_Matrix& matrix,
                    const FX_RECT& clipping_rect,
                    int flags,
                    bool bNeedToRestore,
                    IFSDK_PAUSE_Adapter* pause) {
  if (!pContext->m_pOptions)
    pContext->m_pOptions = pdfium::MakeUnique<CPDF_RenderOptions>();

  if (flags & FPDF_LCD_TEXT)
    pContext->m_pOptions->m_Flags |= RENDER_CLEARTYPE;
  else
    pContext->m_pOptions->m_Flags &= ~RENDER_CLEARTYPE;

  if (flags & FPDF_NO_NATIVETEXT)
    pContext->m_pOptions->m_Flags |= RENDER_NO_NATIVETEXT;
  if (flags & FPDF_RENDER_LIMITEDIMAGECACHE)
    pContext->m_pOptions->m_Flags |= RENDER_LIMITEDIMAGECACHE;
  if (flags & FPDF_RENDER_FORCEHALFTONE)
    pContext->m_pOptions->m_Flags |= RENDER_FORCE_HALFTONE;
#ifndef PDF_ENABLE_XFA
  if (flags & FPDF_RENDER_NO_SMOOTHTEXT)
    pContext->m_pOptions->m_Flags |= RENDER_NOTEXTSMOOTH;
  if (flags & FPDF_RENDER_NO_SMOOTHIMAGE)
    pContext->m_pOptions->m_Flags |= RENDER_NOIMAGESMOOTH;
  if (flags & FPDF_RENDER_NO_SMOOTHPATH)
    pContext->m_pOptions->m_Flags |= RENDER_NOPATHSMOOTH;
#endif  // PDF_ENABLE_XFA

  // Grayscale output
  if (flags & FPDF_GRAYSCALE) {
    pContext->m_pOptions->m_ColorMode = RENDER_COLOR_GRAY;
    pContext->m_pOptions->m_ForeColor = 0;
    pContext->m_pOptions->m_BackColor = 0xffffff;
  }

  const CPDF_OCContext::UsageType usage =
      (flags & FPDF_PRINTING) ? CPDF_OCContext::Print : CPDF_OCContext::View;
  pContext->m_pOptions->m_AddFlags = flags >> 8;
  pContext->m_pOptions->m_pOCContext =
      pdfium::MakeRetain<CPDF_OCContext>(pPage->m_pDocument, usage);

  pContext->m_pDevice->SaveState();
  pContext->m_pDevice->SetClip_Rect(clipping_rect);

  pContext->m_pContext = pdfium::MakeUnique<CPDF_RenderContext>(pPage);
  pContext->m_pContext->AppendLayer(pPage, &matrix);

  if (flags & FPDF_ANNOT) {
    pContext->m_pAnnots = pdfium::MakeUnique<CPDF_AnnotList>(pPage);
    bool bPrinting = pContext->m_pDevice->GetDeviceClass() != FXDC_DISPLAY;
    pContext->m_pAnnots->DisplayAnnots(pPage, pContext->m_pContext.get(),
                                       bPrinting, &matrix, false, nullptr);
  }

  pContext->m_pRenderer = pdfium::MakeUnique<CPDF_ProgressiveRenderer>(
      pContext->m_pContext.get(), pContext->m_pDevice.get(),
      pContext->m_pOptions.get());
  pContext->m_pRenderer->Start(pause);
  if (bNeedToRestore)
    pContext->m_pDevice->RestoreState(false);
}

class CPDF_CustomAccess final : public IFX_SeekableReadStream {
 public:
  static CFX_RetainPtr<CPDF_CustomAccess> Create(FPDF_FILEACCESS* pFileAccess) {
    return CFX_RetainPtr<CPDF_CustomAccess>(new CPDF_CustomAccess(pFileAccess));
  }

  // IFX_SeekableReadStream
  FX_FILESIZE GetSize() override;
  bool ReadBlock(void* buffer, FX_FILESIZE offset, size_t size) override;

 private:
  explicit CPDF_CustomAccess(FPDF_FILEACCESS* pFileAccess);

  FPDF_FILEACCESS m_FileAccess;
};

CPDF_CustomAccess::CPDF_CustomAccess(FPDF_FILEACCESS* pFileAccess)
    : m_FileAccess(*pFileAccess) {}

FX_FILESIZE CPDF_CustomAccess::GetSize() {
  return m_FileAccess.m_FileLen;
}

bool CPDF_CustomAccess::ReadBlock(void* buffer,
                                  FX_FILESIZE offset,
                                  size_t size) {
  if (offset < 0)
    return false;

  FX_SAFE_FILESIZE newPos = pdfium::base::checked_cast<FX_FILESIZE>(size);
  newPos += offset;
  if (!newPos.IsValid() ||
      newPos.ValueOrDie() > static_cast<FX_FILESIZE>(m_FileAccess.m_FileLen)) {
    return false;
  }
  return !!m_FileAccess.m_GetBlock(m_FileAccess.m_Param, offset,
                                   reinterpret_cast<uint8_t*>(buffer), size);
}

#ifdef PDF_ENABLE_XFA
class CFPDF_FileStream : public IFX_SeekableStream {
 public:
  static CFX_RetainPtr<CFPDF_FileStream> Create(FPDF_FILEHANDLER* pFS) {
    return CFX_RetainPtr<CFPDF_FileStream>(new CFPDF_FileStream(pFS));
  }
  ~CFPDF_FileStream() override;

  // IFX_SeekableStream:
  FX_FILESIZE GetSize() override;
  bool IsEOF() override;
  FX_FILESIZE GetPosition() override;
  bool ReadBlock(void* buffer, FX_FILESIZE offset, size_t size) override;
  size_t ReadBlock(void* buffer, size_t size) override;
  bool WriteBlock(const void* buffer, FX_FILESIZE offset, size_t size) override;
  bool Flush() override;

  void SetPosition(FX_FILESIZE pos) { m_nCurPos = pos; }

 protected:
  explicit CFPDF_FileStream(FPDF_FILEHANDLER* pFS);

  FPDF_FILEHANDLER* m_pFS;
  FX_FILESIZE m_nCurPos;
};

CFPDF_FileStream::CFPDF_FileStream(FPDF_FILEHANDLER* pFS) {
  m_pFS = pFS;
  m_nCurPos = 0;
}

CFPDF_FileStream::~CFPDF_FileStream() {
  if (m_pFS && m_pFS->Release)
    m_pFS->Release(m_pFS->clientData);
}

FX_FILESIZE CFPDF_FileStream::GetSize() {
  if (m_pFS && m_pFS->GetSize)
    return (FX_FILESIZE)m_pFS->GetSize(m_pFS->clientData);
  return 0;
}

bool CFPDF_FileStream::IsEOF() {
  return m_nCurPos >= GetSize();
}

FX_FILESIZE CFPDF_FileStream::GetPosition() {
  return m_nCurPos;
}

bool CFPDF_FileStream::ReadBlock(void* buffer,
                                 FX_FILESIZE offset,
                                 size_t size) {
  if (!buffer || !size || !m_pFS->ReadBlock)
    return false;

  if (m_pFS->ReadBlock(m_pFS->clientData, (FPDF_DWORD)offset, buffer,
                       (FPDF_DWORD)size) == 0) {
    m_nCurPos = offset + size;
    return true;
  }
  return false;
}

size_t CFPDF_FileStream::ReadBlock(void* buffer, size_t size) {
  if (!buffer || !size || !m_pFS->ReadBlock)
    return 0;

  FX_FILESIZE nSize = GetSize();
  if (m_nCurPos >= nSize)
    return 0;
  FX_FILESIZE dwAvail = nSize - m_nCurPos;
  if (dwAvail < (FX_FILESIZE)size)
    size = (size_t)dwAvail;
  if (m_pFS->ReadBlock(m_pFS->clientData, (FPDF_DWORD)m_nCurPos, buffer,
                       (FPDF_DWORD)size) == 0) {
    m_nCurPos += size;
    return size;
  }

  return 0;
}

bool CFPDF_FileStream::WriteBlock(const void* buffer,
                                  FX_FILESIZE offset,
                                  size_t size) {
  if (!m_pFS || !m_pFS->WriteBlock)
    return false;

  if (m_pFS->WriteBlock(m_pFS->clientData, (FPDF_DWORD)offset, buffer,
                        (FPDF_DWORD)size) == 0) {
    m_nCurPos = offset + size;
    return true;
  }
  return false;
}

bool CFPDF_FileStream::Flush() {
  if (!m_pFS || !m_pFS->Flush)
    return true;

  return m_pFS->Flush(m_pFS->clientData) == 0;
}
#endif  // PDF_ENABLE_XFA

}  // namespace

UnderlyingDocumentType* UnderlyingFromFPDFDocument(FPDF_DOCUMENT doc) {
  return static_cast<UnderlyingDocumentType*>(doc);
}

FPDF_DOCUMENT FPDFDocumentFromUnderlying(UnderlyingDocumentType* doc) {
  return static_cast<FPDF_DOCUMENT>(doc);
}

UnderlyingPageType* UnderlyingFromFPDFPage(FPDF_PAGE page) {
  return static_cast<UnderlyingPageType*>(page);
}

CPDF_Document* CPDFDocumentFromFPDFDocument(FPDF_DOCUMENT doc) {
#ifdef PDF_ENABLE_XFA
  return doc ? UnderlyingFromFPDFDocument(doc)->GetPDFDoc() : nullptr;
#else   // PDF_ENABLE_XFA
  return UnderlyingFromFPDFDocument(doc);
#endif  // PDF_ENABLE_XFA
}

FPDF_DOCUMENT FPDFDocumentFromCPDFDocument(CPDF_Document* doc) {
#ifdef PDF_ENABLE_XFA
  return doc ? FPDFDocumentFromUnderlying(
                   new CPDFXFA_Context(pdfium::WrapUnique(doc)))
             : nullptr;
#else   // PDF_ENABLE_XFA
  return FPDFDocumentFromUnderlying(doc);
#endif  // PDF_ENABLE_XFA
}

CPDF_Page* CPDFPageFromFPDFPage(FPDF_PAGE page) {
#ifdef PDF_ENABLE_XFA
  return page ? UnderlyingFromFPDFPage(page)->GetPDFPage() : nullptr;
#else   // PDF_ENABLE_XFA
  return UnderlyingFromFPDFPage(page);
#endif  // PDF_ENABLE_XFA
}

CFX_DIBitmap* CFXBitmapFromFPDFBitmap(FPDF_BITMAP bitmap) {
  return static_cast<CFX_DIBitmap*>(bitmap);
}

CFX_RetainPtr<IFX_SeekableReadStream> MakeSeekableReadStream(
    FPDF_FILEACCESS* pFileAccess) {
  return CPDF_CustomAccess::Create(pFileAccess);
}

#ifdef PDF_ENABLE_XFA
CFX_RetainPtr<IFX_SeekableStream> MakeSeekableStream(
    FPDF_FILEHANDLER* pFilehandler) {
  return CFPDF_FileStream::Create(pFilehandler);
}
#endif  // PDF_ENABLE_XFA

// 0 bit: FPDF_POLICY_MACHINETIME_ACCESS
static uint32_t foxit_sandbox_policy = 0xFFFFFFFF;

void FSDK_SetSandBoxPolicy(FPDF_DWORD policy, FPDF_BOOL enable) {
  switch (policy) {
    case FPDF_POLICY_MACHINETIME_ACCESS: {
      if (enable)
        foxit_sandbox_policy |= 0x01;
      else
        foxit_sandbox_policy &= 0xFFFFFFFE;
    } break;
    default:
      break;
  }
}

FPDF_BOOL FSDK_IsSandBoxPolicyEnabled(FPDF_DWORD policy) {
  switch (policy) {
    case FPDF_POLICY_MACHINETIME_ACCESS:
      return !!(foxit_sandbox_policy & 0x01);
    default:
      return false;
  }
}

DLLEXPORT void STDCALL FPDF_InitLibrary() {
  FPDF_InitLibraryWithConfig(nullptr);
}

DLLEXPORT void STDCALL
FPDF_InitLibraryWithConfig(const FPDF_LIBRARY_CONFIG* cfg) {
  if (g_pCodecModule)
    return;

  g_pCodecModule = new CCodec_ModuleMgr();

  CFX_GEModule* pModule = CFX_GEModule::Get();
  pModule->Init(cfg ? cfg->m_pUserFontPaths : nullptr, g_pCodecModule);

  CPDF_ModuleMgr* pModuleMgr = CPDF_ModuleMgr::Get();
  pModuleMgr->SetCodecModule(g_pCodecModule);
  pModuleMgr->InitPageModule();
  pModuleMgr->LoadEmbeddedGB1CMaps();
  pModuleMgr->LoadEmbeddedJapan1CMaps();
  pModuleMgr->LoadEmbeddedCNS1CMaps();
  pModuleMgr->LoadEmbeddedKorea1CMaps();

#ifdef PDF_ENABLE_XFA_BMP
  pModuleMgr->GetCodecModule()->SetBmpModule(
      pdfium::MakeUnique<CCodec_BmpModule>());
#endif

#ifdef PDF_ENABLE_XFA_GIF
  pModuleMgr->GetCodecModule()->SetGifModule(
      pdfium::MakeUnique<CCodec_GifModule>());
#endif

#ifdef PDF_ENABLE_XFA_PNG
  pModuleMgr->GetCodecModule()->SetPngModule(
      pdfium::MakeUnique<CCodec_PngModule>());
#endif

#ifdef PDF_ENABLE_XFA_TIFF
  pModuleMgr->GetCodecModule()->SetTiffModule(
      pdfium::MakeUnique<CCodec_TiffModule>());
#endif

#ifdef PDF_ENABLE_XFA
  FXJSE_Initialize();
  BC_Library_Init();
#endif  // PDF_ENABLE_XFA
  if (cfg && cfg->version >= 2)
    IJS_Runtime::Initialize(cfg->m_v8EmbedderSlot, cfg->m_pIsolate);
}

DLLEXPORT void STDCALL FPDF_DestroyLibrary() {
  if (!g_pCodecModule)
    return;

#ifdef PDF_ENABLE_XFA
  BC_Library_Destory();
  FXJSE_Finalize();
#endif  // PDF_ENABLE_XFA

  CPDF_ModuleMgr::Destroy();
  CFX_GEModule::Destroy();

  delete g_pCodecModule;
  g_pCodecModule = nullptr;

  IJS_Runtime::Destroy();
}

#ifndef _WIN32
int g_LastError;
void SetLastError(int err) {
  g_LastError = err;
}

int GetLastError() {
  return g_LastError;
}
#endif  // _WIN32

void ProcessParseError(CPDF_Parser::Error err) {
  uint32_t err_code = FPDF_ERR_SUCCESS;
  // Translate FPDFAPI error code to FPDFVIEW error code
  switch (err) {
    case CPDF_Parser::SUCCESS:
      err_code = FPDF_ERR_SUCCESS;
      break;
    case CPDF_Parser::FILE_ERROR:
      err_code = FPDF_ERR_FILE;
      break;
    case CPDF_Parser::FORMAT_ERROR:
      err_code = FPDF_ERR_FORMAT;
      break;
    case CPDF_Parser::PASSWORD_ERROR:
      err_code = FPDF_ERR_PASSWORD;
      break;
    case CPDF_Parser::HANDLER_ERROR:
      err_code = FPDF_ERR_SECURITY;
      break;
  }
  SetLastError(err_code);
}

DLLEXPORT void STDCALL FPDF_SetSandBoxPolicy(FPDF_DWORD policy,
                                             FPDF_BOOL enable) {
  return FSDK_SetSandBoxPolicy(policy, enable);
}

#if defined(_WIN32)
#if defined(PDFIUM_PRINT_TEXT_WITH_GDI)
DLLEXPORT void STDCALL
FPDF_SetTypefaceAccessibleFunc(PDFiumEnsureTypefaceCharactersAccessible func) {
  g_pdfium_typeface_accessible_func = func;
}

DLLEXPORT void STDCALL FPDF_SetPrintTextWithGDI(FPDF_BOOL use_gdi) {
  g_pdfium_print_text_with_gdi = !!use_gdi;
}
#endif  // PDFIUM_PRINT_TEXT_WITH_GDI

DLLEXPORT FPDF_BOOL STDCALL FPDF_SetPrintPostscriptLevel(int postscript_level) {
  if (postscript_level != 0 && postscript_level != 2 && postscript_level != 3)
    return FALSE;
  g_pdfium_print_postscript_level = postscript_level;
  return TRUE;
}
#endif  // defined(_WIN32)

DLLEXPORT FPDF_DOCUMENT STDCALL FPDF_LoadDocument(FPDF_STRING file_path,
                                                  FPDF_BYTESTRING password) {
  // NOTE: the creation of the file needs to be by the embedder on the
  // other side of this API.
  CFX_RetainPtr<IFX_SeekableReadStream> pFileAccess =
      IFX_SeekableReadStream::CreateFromFilename((const FX_CHAR*)file_path);
  if (!pFileAccess)
    return nullptr;

  auto pParser = pdfium::MakeUnique<CPDF_Parser>();
  pParser->SetPassword(password);

  auto pDocument = pdfium::MakeUnique<CPDF_Document>(std::move(pParser));
  CPDF_Parser::Error error =
      pDocument->GetParser()->StartParse(pFileAccess, pDocument.get());
  if (error != CPDF_Parser::SUCCESS) {
    ProcessParseError(error);
    return nullptr;
  }
  return FPDFDocumentFromCPDFDocument(pDocument.release());
}

#ifdef PDF_ENABLE_XFA
DLLEXPORT FPDF_BOOL STDCALL FPDF_HasXFAField(FPDF_DOCUMENT document,
                                             int* docType) {
  if (!document)
    return false;

  CPDF_Document* pdfDoc =
      (static_cast<CPDFXFA_Context*>(document))->GetPDFDoc();
  if (!pdfDoc)
    return false;

  CPDF_Dictionary* pRoot = pdfDoc->GetRoot();
  if (!pRoot)
    return false;

  CPDF_Dictionary* pAcroForm = pRoot->GetDictFor("AcroForm");
  if (!pAcroForm)
    return false;

  CPDF_Object* pXFA = pAcroForm->GetObjectFor("XFA");
  if (!pXFA)
    return false;

  bool bDynamicXFA = pRoot->GetBooleanFor("NeedsRendering", false);
  *docType = bDynamicXFA ? DOCTYPE_DYNAMIC_XFA : DOCTYPE_STATIC_XFA;
  return true;
}

DLLEXPORT FPDF_BOOL STDCALL FPDF_LoadXFA(FPDF_DOCUMENT document) {
  return document && (static_cast<CPDFXFA_Context*>(document))->LoadXFADoc();
}
#endif  // PDF_ENABLE_XFA

class CMemFile final : public IFX_SeekableReadStream {
 public:
  static CFX_RetainPtr<CMemFile> Create(uint8_t* pBuf, FX_FILESIZE size) {
    return CFX_RetainPtr<CMemFile>(new CMemFile(pBuf, size));
  }

  FX_FILESIZE GetSize() override { return m_size; }
  bool ReadBlock(void* buffer, FX_FILESIZE offset, size_t size) override {
    if (offset < 0)
      return false;

    FX_SAFE_FILESIZE newPos = pdfium::base::checked_cast<FX_FILESIZE>(size);
    newPos += offset;
    if (!newPos.IsValid() || newPos.ValueOrDie() > m_size)
      return false;

    FXSYS_memcpy(buffer, m_pBuf + offset, size);
    return true;
  }

 private:
  CMemFile(uint8_t* pBuf, FX_FILESIZE size) : m_pBuf(pBuf), m_size(size) {}

  uint8_t* const m_pBuf;
  const FX_FILESIZE m_size;
};

DLLEXPORT FPDF_DOCUMENT STDCALL FPDF_LoadMemDocument(const void* data_buf,
                                                     int size,
                                                     FPDF_BYTESTRING password) {
  CFX_RetainPtr<CMemFile> pMemFile = CMemFile::Create((uint8_t*)data_buf, size);
  auto pParser = pdfium::MakeUnique<CPDF_Parser>();
  pParser->SetPassword(password);

  auto pDocument = pdfium::MakeUnique<CPDF_Document>(std::move(pParser));
  CPDF_Parser::Error error =
      pDocument->GetParser()->StartParse(pMemFile, pDocument.get());
  if (error != CPDF_Parser::SUCCESS) {
    ProcessParseError(error);
    return nullptr;
  }
  CheckUnSupportError(pDocument.get(), error);
  return FPDFDocumentFromCPDFDocument(pDocument.release());
}

DLLEXPORT FPDF_DOCUMENT STDCALL
FPDF_LoadCustomDocument(FPDF_FILEACCESS* pFileAccess,
                        FPDF_BYTESTRING password) {
  CFX_RetainPtr<CPDF_CustomAccess> pFile =
      CPDF_CustomAccess::Create(pFileAccess);
  auto pParser = pdfium::MakeUnique<CPDF_Parser>();
  pParser->SetPassword(password);

  auto pDocument = pdfium::MakeUnique<CPDF_Document>(std::move(pParser));
  CPDF_Parser::Error error =
      pDocument->GetParser()->StartParse(pFile, pDocument.get());
  if (error != CPDF_Parser::SUCCESS) {
    ProcessParseError(error);
    return nullptr;
  }
  CheckUnSupportError(pDocument.get(), error);
  return FPDFDocumentFromCPDFDocument(pDocument.release());
}

DLLEXPORT FPDF_BOOL STDCALL FPDF_GetFileVersion(FPDF_DOCUMENT doc,
                                                int* fileVersion) {
  if (!fileVersion)
    return false;

  *fileVersion = 0;
  CPDF_Document* pDoc = CPDFDocumentFromFPDFDocument(doc);
  if (!pDoc)
    return false;

  CPDF_Parser* pParser = pDoc->GetParser();
  if (!pParser)
    return false;

  *fileVersion = pParser->GetFileVersion();
  return true;
}

// jabdelmalek: changed return type from uint32_t to build on Linux (and match
// header).
DLLEXPORT unsigned long STDCALL FPDF_GetDocPermissions(FPDF_DOCUMENT document) {
  CPDF_Document* pDoc = CPDFDocumentFromFPDFDocument(document);
  // https://bugs.chromium.org/p/pdfium/issues/detail?id=499
  if (!pDoc) {
#ifndef PDF_ENABLE_XFA
    return 0;
#else   // PDF_ENABLE_XFA
    return 0xFFFFFFFF;
#endif  // PDF_ENABLE_XFA
  }

  return pDoc->GetUserPermissions();
}

DLLEXPORT int STDCALL FPDF_GetSecurityHandlerRevision(FPDF_DOCUMENT document) {
  CPDF_Document* pDoc = CPDFDocumentFromFPDFDocument(document);
  if (!pDoc || !pDoc->GetParser())
    return -1;

  CPDF_Dictionary* pDict = pDoc->GetParser()->GetEncryptDict();
  return pDict ? pDict->GetIntegerFor("R") : -1;
}

DLLEXPORT int STDCALL FPDF_GetPageCount(FPDF_DOCUMENT document) {
  UnderlyingDocumentType* pDoc = UnderlyingFromFPDFDocument(document);
  return pDoc ? pDoc->GetPageCount() : 0;
}

DLLEXPORT FPDF_PAGE STDCALL FPDF_LoadPage(FPDF_DOCUMENT document,
                                          int page_index) {
  UnderlyingDocumentType* pDoc = UnderlyingFromFPDFDocument(document);
  if (!pDoc)
    return nullptr;

  if (page_index < 0 || page_index >= pDoc->GetPageCount())
    return nullptr;

#ifdef PDF_ENABLE_XFA
  return pDoc->GetXFAPage(page_index);
#else   // PDF_ENABLE_XFA
  CPDF_Dictionary* pDict = pDoc->GetPage(page_index);
  if (!pDict)
    return nullptr;

  CPDF_Page* pPage = new CPDF_Page(pDoc, pDict, true);
  pPage->ParseContent();
  return pPage;
#endif  // PDF_ENABLE_XFA
}

DLLEXPORT double STDCALL FPDF_GetPageWidth(FPDF_PAGE page) {
  UnderlyingPageType* pPage = UnderlyingFromFPDFPage(page);
  return pPage ? pPage->GetPageWidth() : 0.0;
}

DLLEXPORT double STDCALL FPDF_GetPageHeight(FPDF_PAGE page) {
  UnderlyingPageType* pPage = UnderlyingFromFPDFPage(page);
  return pPage ? pPage->GetPageHeight() : 0.0;
}

#if defined(_WIN32)
DLLEXPORT void STDCALL FPDF_RenderPage(HDC dc,
                                       FPDF_PAGE page,
                                       int start_x,
                                       int start_y,
                                       int size_x,
                                       int size_y,
                                       int rotate,
                                       int flags) {
  CPDF_Page* pPage = CPDFPageFromFPDFPage(page);
  if (!pPage)
    return;

  CPDF_PageRenderContext* pContext = new CPDF_PageRenderContext;
  pPage->SetRenderContext(pdfium::WrapUnique(pContext));

  std::unique_ptr<CFX_DIBitmap> pBitmap;
  // TODO: This results in unnecessary rasterization of some PDFs due to
  // HasImageMask() returning true. If any image on the page is a mask, the
  // entire page gets rasterized and the spool size gets huge.
  const bool bNewBitmap =
      pPage->BackgroundAlphaNeeded() || pPage->HasImageMask();
  if (bNewBitmap) {
    pBitmap = pdfium::MakeUnique<CFX_DIBitmap>();
    pBitmap->Create(size_x, size_y, FXDIB_Argb);
    pBitmap->Clear(0x00ffffff);
    CFX_FxgeDevice* pDevice = new CFX_FxgeDevice;
    pContext->m_pDevice = pdfium::WrapUnique(pDevice);
    pDevice->Attach(pBitmap.get(), false, nullptr, false);
  } else {
    pContext->m_pDevice = pdfium::MakeUnique<CFX_WindowsDevice>(dc);
  }

  FPDF_RenderPage_Retail(pContext, page, start_x, start_y, size_x, size_y,
                         rotate, flags, true, nullptr);

  if (bNewBitmap) {
    CFX_WindowsDevice WinDC(dc);
    if (WinDC.GetDeviceCaps(FXDC_DEVICE_CLASS) == FXDC_PRINTER) {
      std::unique_ptr<CFX_DIBitmap> pDst = pdfium::MakeUnique<CFX_DIBitmap>();
      int pitch = pBitmap->GetPitch();
      pDst->Create(size_x, size_y, FXDIB_Rgb32);
      FXSYS_memset(pDst->GetBuffer(), -1, pitch * size_y);
      pDst->CompositeBitmap(0, 0, size_x, size_y, pBitmap.get(), 0, 0,
                            FXDIB_BLEND_NORMAL, nullptr, false, nullptr);
      WinDC.StretchDIBits(pDst.get(), 0, 0, size_x, size_y);
    } else {
      WinDC.SetDIBits(pBitmap.get(), 0, 0);
    }
  }

  pPage->SetRenderContext(nullptr);
}
#endif  // defined(_WIN32)

DLLEXPORT void STDCALL FPDF_RenderPageBitmap(FPDF_BITMAP bitmap,
                                             FPDF_PAGE page,
                                             int start_x,
                                             int start_y,
                                             int size_x,
                                             int size_y,
                                             int rotate,
                                             int flags) {
  if (!bitmap)
    return;

  CPDF_Page* pPage = CPDFPageFromFPDFPage(page);
  if (!pPage)
    return;

  CPDF_PageRenderContext* pContext = new CPDF_PageRenderContext;
  pPage->SetRenderContext(pdfium::WrapUnique(pContext));
  CFX_FxgeDevice* pDevice = new CFX_FxgeDevice;
  pContext->m_pDevice.reset(pDevice);
  CFX_DIBitmap* pBitmap = CFXBitmapFromFPDFBitmap(bitmap);
  pDevice->Attach(pBitmap, !!(flags & FPDF_REVERSE_BYTE_ORDER), nullptr, false);

  FPDF_RenderPage_Retail(pContext, page, start_x, start_y, size_x, size_y,
                         rotate, flags, true, nullptr);

#ifdef _SKIA_SUPPORT_PATHS_
  pDevice->Flush();
  pBitmap->UnPreMultiply();
#endif
  pPage->SetRenderContext(nullptr);
}

DLLEXPORT void STDCALL FPDF_RenderPageBitmapWithMatrix(FPDF_BITMAP bitmap,
                                                       FPDF_PAGE page,
                                                       const FS_MATRIX* matrix,
                                                       const FS_RECTF* clipping,
                                                       int flags) {
  if (!bitmap)
    return;

  CPDF_Page* pPage = CPDFPageFromFPDFPage(page);
  if (!pPage)
    return;

  CPDF_PageRenderContext* pContext = new CPDF_PageRenderContext;
  pPage->SetRenderContext(pdfium::WrapUnique(pContext));
  CFX_FxgeDevice* pDevice = new CFX_FxgeDevice;
  pContext->m_pDevice.reset(pDevice);
  CFX_DIBitmap* pBitmap = CFXBitmapFromFPDFBitmap(bitmap);
  pDevice->Attach(pBitmap, !!(flags & FPDF_REVERSE_BYTE_ORDER), nullptr, false);

  CFX_Matrix transform_matrix = pPage->GetPageMatrix();
  if (matrix) {
    transform_matrix.Concat(CFX_Matrix(matrix->a, matrix->b, matrix->c,
                                       matrix->d, matrix->e, matrix->f));
  }

  CFX_FloatRect clipping_rect;
  if (clipping) {
    clipping_rect.left = clipping->left;
    clipping_rect.bottom = clipping->bottom;
    clipping_rect.right = clipping->right;
    clipping_rect.top = clipping->top;
  }
  RenderPageImpl(pContext, pPage, transform_matrix, clipping_rect.ToFxRect(),
                 flags, true, nullptr);

  pPage->SetRenderContext(nullptr);
}

#ifdef _SKIA_SUPPORT_
DLLEXPORT FPDF_RECORDER STDCALL FPDF_RenderPageSkp(FPDF_PAGE page,
                                                   int size_x,
                                                   int size_y) {
  CPDF_Page* pPage = CPDFPageFromFPDFPage(page);
  if (!pPage)
    return nullptr;

  CPDF_PageRenderContext* pContext = new CPDF_PageRenderContext;
  pPage->SetRenderContext(pdfium::WrapUnique(pContext));
  CFX_FxgeDevice* skDevice = new CFX_FxgeDevice;
  FPDF_RECORDER recorder = skDevice->CreateRecorder(size_x, size_y);
  pContext->m_pDevice.reset(skDevice);
  FPDF_RenderPage_Retail(pContext, page, 0, 0, size_x, size_y, 0, 0, true,
                         nullptr);
  pPage->SetRenderContext(nullptr);
  return recorder;
}
#endif

DLLEXPORT void STDCALL FPDF_ClosePage(FPDF_PAGE page) {
  UnderlyingPageType* pPage = UnderlyingFromFPDFPage(page);
  if (!page)
    return;
#ifdef PDF_ENABLE_XFA
  pPage->Release();
#else   // PDF_ENABLE_XFA
  CPDFSDK_PageView* pPageView =
      static_cast<CPDFSDK_PageView*>(pPage->GetView());
  if (pPageView) {
    // We're already destroying the pageview, so bail early.
    if (pPageView->IsBeingDestroyed())
      return;

    if (pPageView->IsLocked()) {
      pPageView->TakePageOwnership();
      return;
    }

    bool owned = pPageView->OwnsPage();
    // This will delete the |pPageView| object. We must cleanup the PageView
    // first because it will attempt to reset the View on the |pPage| during
    // destruction.
    pPageView->GetFormFillEnv()->RemovePageView(pPage);
    // If the page was owned then the pageview will have deleted the page.
    if (owned)
      return;
  }
  delete pPage;
#endif  // PDF_ENABLE_XFA
}

DLLEXPORT void STDCALL FPDF_CloseDocument(FPDF_DOCUMENT document) {
  delete UnderlyingFromFPDFDocument(document);
}

DLLEXPORT unsigned long STDCALL FPDF_GetLastError() {
  return GetLastError();
}

DLLEXPORT void STDCALL FPDF_DeviceToPage(FPDF_PAGE page,
                                         int start_x,
                                         int start_y,
                                         int size_x,
                                         int size_y,
                                         int rotate,
                                         int device_x,
                                         int device_y,
                                         double* page_x,
                                         double* page_y) {
  if (!page || !page_x || !page_y)
    return;
  UnderlyingPageType* pPage = UnderlyingFromFPDFPage(page);
#ifdef PDF_ENABLE_XFA
  pPage->DeviceToPage(start_x, start_y, size_x, size_y, rotate, device_x,
                      device_y, page_x, page_y);
#else   // PDF_ENABLE_XFA
  CFX_Matrix page2device =
      pPage->GetDisplayMatrix(start_x, start_y, size_x, size_y, rotate);
  CFX_Matrix device2page;
  device2page.SetReverse(page2device);

  CFX_PointF pos = device2page.Transform(CFX_PointF(
      static_cast<FX_FLOAT>(device_x), static_cast<FX_FLOAT>(device_y)));

  *page_x = pos.x;
  *page_y = pos.y;
#endif  // PDF_ENABLE_XFA
}

DLLEXPORT void STDCALL FPDF_PageToDevice(FPDF_PAGE page,
                                         int start_x,
                                         int start_y,
                                         int size_x,
                                         int size_y,
                                         int rotate,
                                         double page_x,
                                         double page_y,
                                         int* device_x,
                                         int* device_y) {
  if (!device_x || !device_y)
    return;
  UnderlyingPageType* pPage = UnderlyingFromFPDFPage(page);
  if (!pPage)
    return;
#ifdef PDF_ENABLE_XFA
  pPage->PageToDevice(start_x, start_y, size_x, size_y, rotate, page_x, page_y,
                      device_x, device_y);
#else   // PDF_ENABLE_XFA
  CFX_Matrix page2device =
      pPage->GetDisplayMatrix(start_x, start_y, size_x, size_y, rotate);
  CFX_PointF pos = page2device.Transform(
      CFX_PointF(static_cast<FX_FLOAT>(page_x), static_cast<FX_FLOAT>(page_y)));

  *device_x = FXSYS_round(pos.x);
  *device_y = FXSYS_round(pos.y);
#endif  // PDF_ENABLE_XFA
}

DLLEXPORT FPDF_BITMAP STDCALL FPDFBitmap_Create(int width,
                                                int height,
                                                int alpha) {
  auto pBitmap = pdfium::MakeUnique<CFX_DIBitmap>();
  if (!pBitmap->Create(width, height, alpha ? FXDIB_Argb : FXDIB_Rgb32))
    return nullptr;

  return pBitmap.release();
}

DLLEXPORT FPDF_BITMAP STDCALL FPDFBitmap_CreateEx(int width,
                                                  int height,
                                                  int format,
                                                  void* first_scan,
                                                  int stride) {
  FXDIB_Format fx_format;
  switch (format) {
    case FPDFBitmap_Gray:
      fx_format = FXDIB_8bppRgb;
      break;
    case FPDFBitmap_BGR:
      fx_format = FXDIB_Rgb;
      break;
    case FPDFBitmap_BGRx:
      fx_format = FXDIB_Rgb32;
      break;
    case FPDFBitmap_BGRA:
      fx_format = FXDIB_Argb;
      break;
    default:
      return nullptr;
  }
  CFX_DIBitmap* pBitmap = new CFX_DIBitmap;
  pBitmap->Create(width, height, fx_format, (uint8_t*)first_scan, stride);
  return pBitmap;
}

DLLEXPORT void STDCALL FPDFBitmap_FillRect(FPDF_BITMAP bitmap,
                                           int left,
                                           int top,
                                           int width,
                                           int height,
                                           FPDF_DWORD color) {
  if (!bitmap)
    return;

  CFX_FxgeDevice device;
  CFX_DIBitmap* pBitmap = CFXBitmapFromFPDFBitmap(bitmap);
  device.Attach(pBitmap, false, nullptr, false);
  if (!pBitmap->HasAlpha())
    color |= 0xFF000000;
  FX_RECT rect(left, top, left + width, top + height);
  device.FillRect(&rect, color);
}

DLLEXPORT void* STDCALL FPDFBitmap_GetBuffer(FPDF_BITMAP bitmap) {
  return bitmap ? CFXBitmapFromFPDFBitmap(bitmap)->GetBuffer() : nullptr;
}

DLLEXPORT int STDCALL FPDFBitmap_GetWidth(FPDF_BITMAP bitmap) {
  return bitmap ? CFXBitmapFromFPDFBitmap(bitmap)->GetWidth() : 0;
}

DLLEXPORT int STDCALL FPDFBitmap_GetHeight(FPDF_BITMAP bitmap) {
  return bitmap ? CFXBitmapFromFPDFBitmap(bitmap)->GetHeight() : 0;
}

DLLEXPORT int STDCALL FPDFBitmap_GetStride(FPDF_BITMAP bitmap) {
  return bitmap ? CFXBitmapFromFPDFBitmap(bitmap)->GetPitch() : 0;
}

DLLEXPORT void STDCALL FPDFBitmap_Destroy(FPDF_BITMAP bitmap) {
  delete CFXBitmapFromFPDFBitmap(bitmap);
}

void FPDF_RenderPage_Retail(CPDF_PageRenderContext* pContext,
                            FPDF_PAGE page,
                            int start_x,
                            int start_y,
                            int size_x,
                            int size_y,
                            int rotate,
                            int flags,
                            bool bNeedToRestore,
                            IFSDK_PAUSE_Adapter* pause) {
  CPDF_Page* pPage = CPDFPageFromFPDFPage(page);
  if (!pPage)
    return;

  RenderPageImpl(pContext, pPage, pPage->GetDisplayMatrix(
                                      start_x, start_y, size_x, size_y, rotate),
                 FX_RECT(start_x, start_y, start_x + size_x, start_y + size_y),
                 flags, bNeedToRestore, pause);
}

DLLEXPORT int STDCALL FPDF_GetPageSizeByIndex(FPDF_DOCUMENT document,
                                              int page_index,
                                              double* width,
                                              double* height) {
  UnderlyingDocumentType* pDoc = UnderlyingFromFPDFDocument(document);
  if (!pDoc)
    return false;

#ifdef PDF_ENABLE_XFA
  int count = pDoc->GetPageCount();
  if (page_index < 0 || page_index >= count)
    return false;
  CPDFXFA_Page* pPage = pDoc->GetXFAPage(page_index);
  if (!pPage)
    return false;
  *width = pPage->GetPageWidth();
  *height = pPage->GetPageHeight();
#else   // PDF_ENABLE_XFA
  CPDF_Dictionary* pDict = pDoc->GetPage(page_index);
  if (!pDict)
    return false;

  CPDF_Page page(pDoc, pDict, true);
  *width = page.GetPageWidth();
  *height = page.GetPageHeight();
#endif  // PDF_ENABLE_XFA

  return true;
}

DLLEXPORT FPDF_BOOL STDCALL
FPDF_VIEWERREF_GetPrintScaling(FPDF_DOCUMENT document) {
  CPDF_Document* pDoc = CPDFDocumentFromFPDFDocument(document);
  if (!pDoc)
    return true;
  CPDF_ViewerPreferences viewRef(pDoc);
  return viewRef.PrintScaling();
}

DLLEXPORT int STDCALL FPDF_VIEWERREF_GetNumCopies(FPDF_DOCUMENT document) {
  CPDF_Document* pDoc = CPDFDocumentFromFPDFDocument(document);
  if (!pDoc)
    return 1;
  CPDF_ViewerPreferences viewRef(pDoc);
  return viewRef.NumCopies();
}

DLLEXPORT FPDF_PAGERANGE STDCALL
FPDF_VIEWERREF_GetPrintPageRange(FPDF_DOCUMENT document) {
  CPDF_Document* pDoc = CPDFDocumentFromFPDFDocument(document);
  if (!pDoc)
    return nullptr;
  CPDF_ViewerPreferences viewRef(pDoc);
  return viewRef.PrintPageRange();
}

DLLEXPORT FPDF_DUPLEXTYPE STDCALL
FPDF_VIEWERREF_GetDuplex(FPDF_DOCUMENT document) {
  CPDF_Document* pDoc = CPDFDocumentFromFPDFDocument(document);
  if (!pDoc)
    return DuplexUndefined;
  CPDF_ViewerPreferences viewRef(pDoc);
  CFX_ByteString duplex = viewRef.Duplex();
  if ("Simplex" == duplex)
    return Simplex;
  if ("DuplexFlipShortEdge" == duplex)
    return DuplexFlipShortEdge;
  if ("DuplexFlipLongEdge" == duplex)
    return DuplexFlipLongEdge;
  return DuplexUndefined;
}

DLLEXPORT unsigned long STDCALL FPDF_VIEWERREF_GetName(FPDF_DOCUMENT document,
                                                       FPDF_BYTESTRING key,
                                                       char* buffer,
                                                       unsigned long length) {
  CPDF_Document* pDoc = CPDFDocumentFromFPDFDocument(document);
  if (!pDoc)
    return 0;

  CPDF_ViewerPreferences viewRef(pDoc);
  CFX_ByteString bsVal;
  if (!viewRef.GenericName(key, &bsVal))
    return 0;

  unsigned long dwStringLen = bsVal.GetLength() + 1;
  if (buffer && length >= dwStringLen)
    memcpy(buffer, bsVal.c_str(), dwStringLen);
  return dwStringLen;
}

DLLEXPORT FPDF_DWORD STDCALL FPDF_CountNamedDests(FPDF_DOCUMENT document) {
  CPDF_Document* pDoc = CPDFDocumentFromFPDFDocument(document);
  if (!pDoc)
    return 0;

  CPDF_Dictionary* pRoot = pDoc->GetRoot();
  if (!pRoot)
    return 0;

  CPDF_NameTree nameTree(pDoc, "Dests");
  pdfium::base::CheckedNumeric<FPDF_DWORD> count = nameTree.GetCount();
  CPDF_Dictionary* pDest = pRoot->GetDictFor("Dests");
  if (pDest)
    count += pDest->GetCount();

  if (!count.IsValid())
    return 0;

  return count.ValueOrDie();
}

DLLEXPORT FPDF_DEST STDCALL FPDF_GetNamedDestByName(FPDF_DOCUMENT document,
                                                    FPDF_BYTESTRING name) {
  if (!name || name[0] == 0)
    return nullptr;

  CPDF_Document* pDoc = CPDFDocumentFromFPDFDocument(document);
  if (!pDoc)
    return nullptr;

  CPDF_NameTree name_tree(pDoc, "Dests");
  return name_tree.LookupNamedDest(pDoc, name);
}

#ifdef PDF_ENABLE_XFA
DLLEXPORT FPDF_RESULT STDCALL FPDF_BStr_Init(FPDF_BSTR* str) {
  if (!str)
    return -1;

  FXSYS_memset(str, 0, sizeof(FPDF_BSTR));
  return 0;
}

DLLEXPORT FPDF_RESULT STDCALL FPDF_BStr_Set(FPDF_BSTR* str,
                                            FPDF_LPCSTR bstr,
                                            int length) {
  if (!str)
    return -1;
  if (!bstr || !length)
    return -1;
  if (length == -1)
    length = FXSYS_strlen(bstr);

  if (length == 0) {
    if (str->str) {
      FX_Free(str->str);
      str->str = nullptr;
    }
    str->len = 0;
    return 0;
  }

  if (str->str && str->len < length)
    str->str = FX_Realloc(char, str->str, length + 1);
  else if (!str->str)
    str->str = FX_Alloc(char, length + 1);

  str->str[length] = 0;
  FXSYS_memcpy(str->str, bstr, length);
  str->len = length;

  return 0;
}

DLLEXPORT FPDF_RESULT STDCALL FPDF_BStr_Clear(FPDF_BSTR* str) {
  if (!str)
    return -1;

  if (str->str) {
    FX_Free(str->str);
    str->str = nullptr;
  }
  str->len = 0;
  return 0;
}
#endif  // PDF_ENABLE_XFA

DLLEXPORT FPDF_DEST STDCALL FPDF_GetNamedDest(FPDF_DOCUMENT document,
                                              int index,
                                              void* buffer,
                                              long* buflen) {
  if (!buffer)
    *buflen = 0;

  if (index < 0)
    return nullptr;

  CPDF_Document* pDoc = CPDFDocumentFromFPDFDocument(document);
  if (!pDoc)
    return nullptr;

  CPDF_Dictionary* pRoot = pDoc->GetRoot();
  if (!pRoot)
    return nullptr;

  CPDF_Object* pDestObj = nullptr;
  CFX_ByteString bsName;
  CPDF_NameTree nameTree(pDoc, "Dests");
  int count = nameTree.GetCount();
  if (index >= count) {
    CPDF_Dictionary* pDest = pRoot->GetDictFor("Dests");
    if (!pDest)
      return nullptr;

    pdfium::base::CheckedNumeric<int> checked_count = count;
    checked_count += pDest->GetCount();
    if (!checked_count.IsValid() || index >= checked_count.ValueOrDie())
      return nullptr;

    index -= count;
    int i = 0;
    for (const auto& it : *pDest) {
      bsName = it.first;
      pDestObj = it.second.get();
      if (!pDestObj)
        continue;
      if (i == index)
        break;
      i++;
    }
  } else {
    pDestObj = nameTree.LookupValue(index, bsName);
  }
  if (!pDestObj)
    return nullptr;
  if (CPDF_Dictionary* pDict = pDestObj->AsDictionary()) {
    pDestObj = pDict->GetArrayFor("D");
    if (!pDestObj)
      return nullptr;
  }
  if (!pDestObj->IsArray())
    return nullptr;

  CFX_WideString wsName = PDF_DecodeText(bsName);
  CFX_ByteString utf16Name = wsName.UTF16LE_Encode();
  int len = utf16Name.GetLength();
  if (!buffer) {
    *buflen = len;
  } else if (len <= *buflen) {
    memcpy(buffer, utf16Name.c_str(), len);
    *buflen = len;
  } else {
    *buflen = -1;
  }
  return (FPDF_DEST)pDestObj;
}
