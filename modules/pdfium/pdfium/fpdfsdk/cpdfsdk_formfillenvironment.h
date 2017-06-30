// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_CPDFSDK_FORMFILLENVIRONMENT_H_
#define FPDFSDK_CPDFSDK_FORMFILLENVIRONMENT_H_

#include <map>
#include <memory>
#include <vector>

#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfdoc/cpdf_occontext.h"
#include "core/fxcrt/cfx_observable.h"
#include "fpdfsdk/cfx_systemhandler.h"
#include "fpdfsdk/cpdfsdk_annot.h"
#include "fpdfsdk/fsdk_define.h"
#include "public/fpdf_formfill.h"
#include "public/fpdf_fwlevent.h"

class CFFL_InteractiveFormFiller;
class CFX_SystemHandler;
class CPDFSDK_ActionHandler;
class CPDFSDK_AnnotHandlerMgr;
class CPDFSDK_InterForm;
class CPDFSDK_PageView;
class IJS_Runtime;

class CPDFSDK_FormFillEnvironment
    : public CFX_Observable<CPDFSDK_FormFillEnvironment> {
 public:
  CPDFSDK_FormFillEnvironment(UnderlyingDocumentType* pDoc,
                              FPDF_FORMFILLINFO* pFFinfo);
  ~CPDFSDK_FormFillEnvironment();

  CPDFSDK_PageView* GetPageView(UnderlyingPageType* pPage, bool renew);
  CPDFSDK_PageView* GetPageView(int nIndex);
  CPDFSDK_PageView* GetCurrentView();
  void RemovePageView(UnderlyingPageType* pPage);
  void UpdateAllViews(CPDFSDK_PageView* pSender, CPDFSDK_Annot* pAnnot);

  CPDFSDK_Annot* GetFocusAnnot() { return m_pFocusAnnot.Get(); }
  bool SetFocusAnnot(CPDFSDK_Annot::ObservedPtr* pAnnot);
  bool KillFocusAnnot(uint32_t nFlag);
  void ClearAllFocusedAnnots();

  bool ExtractPages(const std::vector<uint16_t>& arrExtraPages,
                    CPDF_Document* pDstDoc);
  bool InsertPages(int nInsertAt,
                   const CPDF_Document* pSrcDoc,
                   const std::vector<uint16_t>& arrSrcPages);
  bool ReplacePages(int nPage,
                    const CPDF_Document* pSrcDoc,
                    const std::vector<uint16_t>& arrSrcPages);

  int GetPageCount() { return m_pUnderlyingDoc->GetPageCount(); }
  bool GetPermissions(int nFlag);

  bool GetChangeMark() const { return m_bChangeMask; }
  void SetChangeMark() { m_bChangeMask = true; }
  void ClearChangeMark() { m_bChangeMask = false; }

  UnderlyingPageType* GetPage(int nIndex);

  void ProcJavascriptFun();
  bool ProcOpenAction();

  void Invalidate(FPDF_PAGE page, const FX_RECT& rect);
  void OutputSelectedRect(FPDF_PAGE page, const CFX_FloatRect& rect);

  void SetCursor(int nCursorType);
  int SetTimer(int uElapse, TimerCallback lpTimerFunc);
  void KillTimer(int nTimerID);
  FX_SYSTEMTIME GetLocalTime() const;

  void OnChange();
  bool IsSHIFTKeyDown(uint32_t nFlag) const;
  bool IsCTRLKeyDown(uint32_t nFlag) const;
  bool IsALTKeyDown(uint32_t nFlag) const;

  FPDF_PAGE GetPage(FPDF_DOCUMENT document, int nPageIndex);
  FPDF_PAGE GetCurrentPage(FPDF_DOCUMENT document);

  void ExecuteNamedAction(const FX_CHAR* namedAction);
  void OnSetFieldInputFocus(FPDF_WIDESTRING focusText,
                            FPDF_DWORD nTextLen,
                            bool bFocus);
  void DoURIAction(const FX_CHAR* bsURI);
  void DoGoToAction(int nPageIndex,
                    int zoomMode,
                    float* fPosArray,
                    int sizeOfArray);

  UnderlyingDocumentType* GetUnderlyingDocument() const {
    return m_pUnderlyingDoc;
  }

#ifdef PDF_ENABLE_XFA
  CPDF_Document* GetPDFDocument() const {
    return m_pUnderlyingDoc ? m_pUnderlyingDoc->GetPDFDoc() : nullptr;
  }

  CPDFXFA_Context* GetXFAContext() const { return m_pUnderlyingDoc; }
  void ResetXFADocument() { m_pUnderlyingDoc = nullptr; }

  int GetPageViewCount() const { return m_PageMap.size(); }

  void DisplayCaret(FPDF_PAGE page,
                    FPDF_BOOL bVisible,
                    double left,
                    double top,
                    double right,
                    double bottom);
  int GetCurrentPageIndex(FPDF_DOCUMENT document);
  void SetCurrentPage(FPDF_DOCUMENT document, int iCurPage);

  // TODO(dsinclair): This should probably change to PDFium?
  CFX_WideString FFI_GetAppName() const { return CFX_WideString(L"Acrobat"); }

  CFX_WideString GetPlatform();
  void GotoURL(FPDF_DOCUMENT document, const CFX_WideStringC& wsURL);
  void GetPageViewRect(FPDF_PAGE page, FS_RECTF& dstRect);
  bool PopupMenu(FPDF_PAGE page,
                 FPDF_WIDGET hWidget,
                 int menuFlag,
                 CFX_PointF pt);

  void Alert(FPDF_WIDESTRING Msg, FPDF_WIDESTRING Title, int Type, int Icon);
  void EmailTo(FPDF_FILEHANDLER* fileHandler,
               FPDF_WIDESTRING pTo,
               FPDF_WIDESTRING pSubject,
               FPDF_WIDESTRING pCC,
               FPDF_WIDESTRING pBcc,
               FPDF_WIDESTRING pMsg);
  void UploadTo(FPDF_FILEHANDLER* fileHandler,
                int fileFlag,
                FPDF_WIDESTRING uploadTo);
  FPDF_FILEHANDLER* OpenFile(int fileType,
                             FPDF_WIDESTRING wsURL,
                             const char* mode);
  CFX_RetainPtr<IFX_SeekableReadStream> DownloadFromURL(const FX_WCHAR* url);
  CFX_WideString PostRequestURL(const FX_WCHAR* wsURL,
                                const FX_WCHAR* wsData,
                                const FX_WCHAR* wsContentType,
                                const FX_WCHAR* wsEncode,
                                const FX_WCHAR* wsHeader);
  FPDF_BOOL PutRequestURL(const FX_WCHAR* wsURL,
                          const FX_WCHAR* wsData,
                          const FX_WCHAR* wsEncode);
  CFX_WideString GetLanguage();

  void PageEvent(int iPageCount, uint32_t dwEventType) const;
#else   // PDF_ENABLE_XFA
  CPDF_Document* GetPDFDocument() const { return m_pUnderlyingDoc; }
#endif  // PDF_ENABLE_XFA

  int JS_appAlert(const FX_WCHAR* Msg,
                  const FX_WCHAR* Title,
                  uint32_t Type,
                  uint32_t Icon);
  int JS_appResponse(const FX_WCHAR* Question,
                     const FX_WCHAR* Title,
                     const FX_WCHAR* Default,
                     const FX_WCHAR* cLabel,
                     FPDF_BOOL bPassword,
                     void* response,
                     int length);
  void JS_appBeep(int nType);
  CFX_WideString JS_fieldBrowse();
  CFX_WideString JS_docGetFilePath();
  void JS_docSubmitForm(void* formData, int length, const FX_WCHAR* URL);
  void JS_docmailForm(void* mailData,
                      int length,
                      FPDF_BOOL bUI,
                      const FX_WCHAR* To,
                      const FX_WCHAR* Subject,
                      const FX_WCHAR* CC,
                      const FX_WCHAR* BCC,
                      const FX_WCHAR* Msg);
  void JS_docprint(FPDF_BOOL bUI,
                   int nStart,
                   int nEnd,
                   FPDF_BOOL bSilent,
                   FPDF_BOOL bShrinkToFit,
                   FPDF_BOOL bPrintAsImage,
                   FPDF_BOOL bReverse,
                   FPDF_BOOL bAnnotations);
  void JS_docgotoPage(int nPageNum);

  bool IsJSInitiated() const { return m_pInfo && m_pInfo->m_pJsPlatform; }
  CFX_ByteString GetAppName() const { return ""; }
  CFX_SystemHandler* GetSysHandler() const { return m_pSysHandler.get(); }
  FPDF_FORMFILLINFO* GetFormFillInfo() const { return m_pInfo; }

  // Creates if not present.
  CFFL_InteractiveFormFiller* GetInteractiveFormFiller();
  CPDFSDK_AnnotHandlerMgr* GetAnnotHandlerMgr();  // Creates if not present.
  IJS_Runtime* GetJSRuntime();                    // Creates if not present.
  CPDFSDK_ActionHandler* GetActionHander();       // Creates if not present.
  CPDFSDK_InterForm* GetInterForm();              // Creates if not present.

 private:
  std::unique_ptr<CPDFSDK_AnnotHandlerMgr> m_pAnnotHandlerMgr;
  std::unique_ptr<CPDFSDK_ActionHandler> m_pActionHandler;
  std::unique_ptr<IJS_Runtime> m_pJSRuntime;
  FPDF_FORMFILLINFO* const m_pInfo;
  std::map<UnderlyingPageType*, std::unique_ptr<CPDFSDK_PageView>> m_PageMap;
  std::unique_ptr<CPDFSDK_InterForm> m_pInterForm;
  CPDFSDK_Annot::ObservedPtr m_pFocusAnnot;
  UnderlyingDocumentType* m_pUnderlyingDoc;
  std::unique_ptr<CFFL_InteractiveFormFiller> m_pFormFiller;
  std::unique_ptr<CFX_SystemHandler> m_pSysHandler;
  bool m_bChangeMask;
  bool m_bBeingDestroyed;
};

#endif  // FPDFSDK_CPDFSDK_FORMFILLENVIRONMENT_H_
