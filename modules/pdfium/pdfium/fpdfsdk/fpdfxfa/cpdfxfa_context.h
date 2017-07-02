// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_FPDFXFA_CPDFXFA_CONTEXT_H_
#define FPDFSDK_FPDFXFA_CPDFXFA_CONTEXT_H_

#include <memory>
#include <vector>

#include "fpdfsdk/fpdfxfa/cpdfxfa_docenvironment.h"
#include "xfa/fxfa/xfa_ffdoc.h"

class CJS_Runtime;
class CPDFSDK_FormFillEnvironment;
class CPDFXFA_Page;
class CXFA_FFDocHandler;
class IJS_EventContext;
class IJS_Runtime;

enum LoadStatus {
  FXFA_LOADSTATUS_PRELOAD = 0,
  FXFA_LOADSTATUS_LOADING,
  FXFA_LOADSTATUS_LOADED,
  FXFA_LOADSTATUS_CLOSING,
  FXFA_LOADSTATUS_CLOSED
};

class CPDFXFA_Context : public IXFA_AppProvider {
 public:
  explicit CPDFXFA_Context(std::unique_ptr<CPDF_Document> pPDFDoc);
  ~CPDFXFA_Context() override;

  bool LoadXFADoc();
  CPDF_Document* GetPDFDoc() { return m_pPDFDoc.get(); }
  CXFA_FFDoc* GetXFADoc() { return m_pXFADoc.get(); }
  CXFA_FFDocView* GetXFADocView() { return m_pXFADocView; }
  int GetDocType() const { return m_iDocType; }
  v8::Isolate* GetJSERuntime() const;
  CXFA_FFApp* GetXFAApp() { return m_pXFAApp.get(); }

  CPDFSDK_FormFillEnvironment* GetFormFillEnv() const { return m_pFormFillEnv; }
  void SetFormFillEnv(CPDFSDK_FormFillEnvironment* pFormFillEnv);

  void DeletePage(int page_index);
  int GetPageCount() const;

  CPDFXFA_Page* GetXFAPage(int page_index);
  CPDFXFA_Page* GetXFAPage(CXFA_FFPageView* pPage) const;

  void RemovePage(CPDFXFA_Page* page);

  void ClearChangeMark();

  // IFXA_AppProvider:
  CFX_WideString GetLanguage() override;
  CFX_WideString GetPlatform() override;
  CFX_WideString GetAppName() override;
  CFX_WideString GetAppTitle() const override;

  void Beep(uint32_t dwType) override;
  int32_t MsgBox(const CFX_WideString& wsMessage,
                 const CFX_WideString& wsTitle,
                 uint32_t dwIconType,
                 uint32_t dwButtonType) override;
  CFX_WideString Response(const CFX_WideString& wsQuestion,
                          const CFX_WideString& wsTitle,
                          const CFX_WideString& wsDefaultAnswer,
                          bool bMark) override;
  CFX_RetainPtr<IFX_SeekableReadStream> DownloadURL(
      const CFX_WideString& wsURL) override;
  bool PostRequestURL(const CFX_WideString& wsURL,
                      const CFX_WideString& wsData,
                      const CFX_WideString& wsContentType,
                      const CFX_WideString& wsEncode,
                      const CFX_WideString& wsHeader,
                      CFX_WideString& wsResponse) override;
  bool PutRequestURL(const CFX_WideString& wsURL,
                     const CFX_WideString& wsData,
                     const CFX_WideString& wsEncode) override;

  IFWL_AdapterTimerMgr* GetTimerMgr() override;

 protected:
  friend class CPDFXFA_DocEnvironment;

  int GetOriginalPageCount() const { return m_nPageCount; }
  void SetOriginalPageCount(int count) {
    m_nPageCount = count;
    m_XFAPageList.resize(count);
  }

  LoadStatus GetLoadStatus() const { return m_nLoadStatus; }
  std::vector<CPDFXFA_Page*>* GetXFAPageList() { return &m_XFAPageList; }

 private:
  void CloseXFADoc();

  int m_iDocType;

  std::unique_ptr<CPDF_Document> m_pPDFDoc;
  std::unique_ptr<CXFA_FFDoc> m_pXFADoc;
  CPDFSDK_FormFillEnvironment* m_pFormFillEnv;  // not owned.
  CXFA_FFDocView* m_pXFADocView;                // not owned.
  std::unique_ptr<CXFA_FFApp> m_pXFAApp;
  std::unique_ptr<CJS_Runtime> m_pRuntime;
  std::vector<CPDFXFA_Page*> m_XFAPageList;
  LoadStatus m_nLoadStatus;
  int m_nPageCount;

  // Must be destroyed before |m_pFormFillEnv|.
  CPDFXFA_DocEnvironment m_DocEnv;
};

#endif  // FPDFSDK_FPDFXFA_CPDFXFA_CONTEXT_H_
