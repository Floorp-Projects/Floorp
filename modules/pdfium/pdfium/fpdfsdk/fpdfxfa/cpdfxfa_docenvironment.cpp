// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/fpdfxfa/cpdfxfa_docenvironment.h"

#include <memory>

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fxcrt/cfx_retain_ptr.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_interform.h"
#include "fpdfsdk/cpdfsdk_pageview.h"
#include "fpdfsdk/fpdfxfa/cpdfxfa_context.h"
#include "fpdfsdk/fpdfxfa/cpdfxfa_page.h"
#include "fpdfsdk/javascript/ijs_runtime.h"
#include "xfa/fxfa/xfa_ffdocview.h"
#include "xfa/fxfa/xfa_ffwidget.h"
#include "xfa/fxfa/xfa_ffwidgethandler.h"

#define IDS_XFA_Validate_Input                                          \
  "At least one required field was empty. Please fill in the required " \
  "fields\r\n(highlighted) before continuing."

// submit
#define FXFA_CONFIG 0x00000001
#define FXFA_TEMPLATE 0x00000010
#define FXFA_LOCALESET 0x00000100
#define FXFA_DATASETS 0x00001000
#define FXFA_XMPMETA 0x00010000
#define FXFA_XFDF 0x00100000
#define FXFA_FORM 0x01000000
#define FXFA_PDF 0x10000000
#define FXFA_XFA_ALL 0x01111111

CPDFXFA_DocEnvironment::CPDFXFA_DocEnvironment(CPDFXFA_Context* pContext)
    : m_pContext(pContext), m_pJSEventContext(nullptr) {
  ASSERT(m_pContext);
}

CPDFXFA_DocEnvironment::~CPDFXFA_DocEnvironment() {
  if (m_pJSEventContext && m_pContext->GetFormFillEnv()) {
    m_pContext->GetFormFillEnv()->GetJSRuntime()->ReleaseEventContext(
        m_pJSEventContext);
  }
}

void CPDFXFA_DocEnvironment::SetChangeMark(CXFA_FFDoc* hDoc) {
  if (hDoc == m_pContext->GetXFADoc() && m_pContext->GetFormFillEnv())
    m_pContext->GetFormFillEnv()->SetChangeMark();
}

void CPDFXFA_DocEnvironment::InvalidateRect(CXFA_FFPageView* pPageView,
                                            const CFX_RectF& rt,
                                            uint32_t dwFlags /* = 0 */) {
  if (!m_pContext->GetXFADoc() || !m_pContext->GetFormFillEnv())
    return;

  if (m_pContext->GetDocType() != DOCTYPE_DYNAMIC_XFA)
    return;

  CPDFXFA_Page* pPage = m_pContext->GetXFAPage(pPageView);
  if (!pPage)
    return;

  CPDFSDK_FormFillEnvironment* pFormFillEnv = m_pContext->GetFormFillEnv();
  if (!pFormFillEnv)
    return;

  pFormFillEnv->Invalidate(static_cast<FPDF_PAGE>(pPage),
                           CFX_FloatRect::FromCFXRectF(rt).ToFxRect());
}

void CPDFXFA_DocEnvironment::DisplayCaret(CXFA_FFWidget* hWidget,
                                          bool bVisible,
                                          const CFX_RectF* pRtAnchor) {
  if (!hWidget || !pRtAnchor || !m_pContext->GetXFADoc() ||
      !m_pContext->GetFormFillEnv() || !m_pContext->GetXFADocView())
    return;

  if (m_pContext->GetDocType() != DOCTYPE_DYNAMIC_XFA)
    return;

  CXFA_FFWidgetHandler* pWidgetHandler =
      m_pContext->GetXFADocView()->GetWidgetHandler();
  if (!pWidgetHandler)
    return;

  CXFA_FFPageView* pPageView = hWidget->GetPageView();
  if (!pPageView)
    return;

  CPDFXFA_Page* pPage = m_pContext->GetXFAPage(pPageView);
  if (!pPage)
    return;

  CPDFSDK_FormFillEnvironment* pFormFillEnv = m_pContext->GetFormFillEnv();
  if (!pFormFillEnv)
    return;

  CFX_FloatRect rcCaret = CFX_FloatRect::FromCFXRectF(*pRtAnchor);
  pFormFillEnv->DisplayCaret((FPDF_PAGE)pPage, bVisible, rcCaret.left,
                             rcCaret.top, rcCaret.right, rcCaret.bottom);
}

bool CPDFXFA_DocEnvironment::GetPopupPos(CXFA_FFWidget* hWidget,
                                         FX_FLOAT fMinPopup,
                                         FX_FLOAT fMaxPopup,
                                         const CFX_RectF& rtAnchor,
                                         CFX_RectF& rtPopup) {
  if (!hWidget)
    return false;

  CXFA_FFPageView* pXFAPageView = hWidget->GetPageView();
  if (!pXFAPageView)
    return false;

  CPDFXFA_Page* pPage = m_pContext->GetXFAPage(pXFAPageView);
  if (!pPage)
    return false;

  CXFA_WidgetAcc* pWidgetAcc = hWidget->GetDataAcc();
  int nRotate = pWidgetAcc->GetRotate();
  CPDFSDK_FormFillEnvironment* pFormFillEnv = m_pContext->GetFormFillEnv();
  if (!pFormFillEnv)
    return false;

  FS_RECTF pageViewRect = {0.0f, 0.0f, 0.0f, 0.0f};
  pFormFillEnv->GetPageViewRect(pPage, pageViewRect);

  int t1;
  int t2;
  CFX_FloatRect rcAnchor = CFX_FloatRect::FromCFXRectF(rtAnchor);
  switch (nRotate) {
    case 90: {
      t1 = (int)(pageViewRect.right - rcAnchor.right);
      t2 = (int)(rcAnchor.left - pageViewRect.left);
      if (rcAnchor.bottom < pageViewRect.bottom)
        rtPopup.left += rcAnchor.bottom - pageViewRect.bottom;
      break;
    }
    case 180: {
      t2 = (int)(pageViewRect.top - rcAnchor.top);
      t1 = (int)(rcAnchor.bottom - pageViewRect.bottom);
      if (rcAnchor.left < pageViewRect.left)
        rtPopup.left += rcAnchor.left - pageViewRect.left;
      break;
    }
    case 270: {
      t1 = (int)(rcAnchor.left - pageViewRect.left);
      t2 = (int)(pageViewRect.right - rcAnchor.right);
      if (rcAnchor.top > pageViewRect.top)
        rtPopup.left -= rcAnchor.top - pageViewRect.top;
      break;
    }
    case 0:
    default: {
      t1 = (int)(pageViewRect.top - rcAnchor.top);
      t2 = (int)(rcAnchor.bottom - pageViewRect.bottom);
      if (rcAnchor.right > pageViewRect.right)
        rtPopup.left -= rcAnchor.right - pageViewRect.right;
      break;
    }
  }

  int t;
  uint32_t dwPos;
  if (t1 <= 0 && t2 <= 0)
    return false;
  if (t1 <= 0) {
    t = t2;
    dwPos = 1;
  } else if (t2 <= 0) {
    t = t1;
    dwPos = 0;
  } else if (t1 > t2) {
    t = t1;
    dwPos = 0;
  } else {
    t = t2;
    dwPos = 1;
  }

  FX_FLOAT fPopupHeight;
  if (t < fMinPopup)
    fPopupHeight = fMinPopup;
  else if (t > fMaxPopup)
    fPopupHeight = fMaxPopup;
  else
    fPopupHeight = static_cast<FX_FLOAT>(t);

  switch (nRotate) {
    case 0:
    case 180: {
      if (dwPos == 0) {
        rtPopup.top = rtAnchor.height;
        rtPopup.height = fPopupHeight;
      } else {
        rtPopup.top = -fPopupHeight;
        rtPopup.height = fPopupHeight;
      }
      break;
    }
    case 90:
    case 270: {
      if (dwPos == 0) {
        rtPopup.top = rtAnchor.width;
        rtPopup.height = fPopupHeight;
      } else {
        rtPopup.top = -fPopupHeight;
        rtPopup.height = fPopupHeight;
      }
      break;
    }
    default:
      break;
  }

  return true;
}

bool CPDFXFA_DocEnvironment::PopupMenu(CXFA_FFWidget* hWidget,
                                       CFX_PointF ptPopup) {
  if (!hWidget)
    return false;

  CXFA_FFPageView* pXFAPageView = hWidget->GetPageView();
  if (!pXFAPageView)
    return false;

  CPDFXFA_Page* pPage = m_pContext->GetXFAPage(pXFAPageView);
  if (!pPage)
    return false;

  CPDFSDK_FormFillEnvironment* pFormFillEnv = m_pContext->GetFormFillEnv();
  if (!pFormFillEnv)
    return false;

  int menuFlag = 0;
  if (hWidget->CanUndo())
    menuFlag |= FXFA_MENU_UNDO;
  if (hWidget->CanRedo())
    menuFlag |= FXFA_MENU_REDO;
  if (hWidget->CanPaste())
    menuFlag |= FXFA_MENU_PASTE;
  if (hWidget->CanCopy())
    menuFlag |= FXFA_MENU_COPY;
  if (hWidget->CanCut())
    menuFlag |= FXFA_MENU_CUT;
  if (hWidget->CanSelectAll())
    menuFlag |= FXFA_MENU_SELECTALL;

  return pFormFillEnv->PopupMenu(pPage, hWidget, menuFlag, ptPopup);
}

void CPDFXFA_DocEnvironment::PageViewEvent(CXFA_FFPageView* pPageView,
                                           uint32_t dwFlags) {
  CPDFSDK_FormFillEnvironment* pFormFillEnv = m_pContext->GetFormFillEnv();
  if (!pFormFillEnv)
    return;

  if (m_pContext->GetLoadStatus() == FXFA_LOADSTATUS_LOADING ||
      m_pContext->GetLoadStatus() == FXFA_LOADSTATUS_CLOSING ||
      XFA_PAGEVIEWEVENT_StopLayout != dwFlags)
    return;

  int nNewCount = m_pContext->GetPageCount();
  if (nNewCount == m_pContext->GetOriginalPageCount())
    return;

  CXFA_FFDocView* pXFADocView = m_pContext->GetXFADocView();
  if (!pXFADocView)
    return;

  for (int iPageIter = 0; iPageIter < m_pContext->GetOriginalPageCount();
       iPageIter++) {
    CPDFXFA_Page* pPage = (*m_pContext->GetXFAPageList())[iPageIter];
    if (!pPage)
      continue;

    m_pContext->GetFormFillEnv()->RemovePageView(pPage);
    pPage->SetXFAPageView(pXFADocView->GetPageView(iPageIter));
  }

  int flag = (nNewCount < m_pContext->GetOriginalPageCount())
                 ? FXFA_PAGEVIEWEVENT_POSTREMOVED
                 : FXFA_PAGEVIEWEVENT_POSTADDED;
  int count = FXSYS_abs(nNewCount - m_pContext->GetOriginalPageCount());
  m_pContext->SetOriginalPageCount(nNewCount);
  pFormFillEnv->PageEvent(count, flag);
}

void CPDFXFA_DocEnvironment::WidgetPostAdd(CXFA_FFWidget* hWidget,
                                           CXFA_WidgetAcc* pWidgetData) {
  if (m_pContext->GetDocType() != DOCTYPE_DYNAMIC_XFA || !hWidget)
    return;

  CXFA_FFPageView* pPageView = hWidget->GetPageView();
  if (!pPageView)
    return;

  CPDFXFA_Page* pXFAPage = m_pContext->GetXFAPage(pPageView);
  if (!pXFAPage)
    return;

  m_pContext->GetFormFillEnv()->GetPageView(pXFAPage, true)->AddAnnot(hWidget);
}

void CPDFXFA_DocEnvironment::WidgetPreRemove(CXFA_FFWidget* hWidget,
                                             CXFA_WidgetAcc* pWidgetData) {
  if (m_pContext->GetDocType() != DOCTYPE_DYNAMIC_XFA || !hWidget)
    return;

  CXFA_FFPageView* pPageView = hWidget->GetPageView();
  if (!pPageView)
    return;

  CPDFXFA_Page* pXFAPage = m_pContext->GetXFAPage(pPageView);
  if (!pXFAPage)
    return;

  CPDFSDK_PageView* pSdkPageView =
      m_pContext->GetFormFillEnv()->GetPageView(pXFAPage, true);
  if (CPDFSDK_Annot* pAnnot = pSdkPageView->GetAnnotByXFAWidget(hWidget))
    pSdkPageView->DeleteAnnot(pAnnot);
}

int32_t CPDFXFA_DocEnvironment::CountPages(CXFA_FFDoc* hDoc) {
  if (hDoc == m_pContext->GetXFADoc() && m_pContext->GetFormFillEnv())
    return m_pContext->GetPageCount();
  return 0;
}

int32_t CPDFXFA_DocEnvironment::GetCurrentPage(CXFA_FFDoc* hDoc) {
  if (hDoc != m_pContext->GetXFADoc() || !m_pContext->GetFormFillEnv())
    return -1;
  if (m_pContext->GetDocType() != DOCTYPE_DYNAMIC_XFA)
    return -1;

  CPDFSDK_FormFillEnvironment* pFormFillEnv = m_pContext->GetFormFillEnv();
  if (!pFormFillEnv)
    return -1;

  return pFormFillEnv->GetCurrentPageIndex(this);
}

void CPDFXFA_DocEnvironment::SetCurrentPage(CXFA_FFDoc* hDoc,
                                            int32_t iCurPage) {
  if (hDoc != m_pContext->GetXFADoc() || !m_pContext->GetFormFillEnv() ||
      m_pContext->GetDocType() != DOCTYPE_DYNAMIC_XFA || iCurPage < 0 ||
      iCurPage >= m_pContext->GetFormFillEnv()->GetPageCount()) {
    return;
  }

  CPDFSDK_FormFillEnvironment* pFormFillEnv = m_pContext->GetFormFillEnv();
  if (!pFormFillEnv)
    return;
  pFormFillEnv->SetCurrentPage(this, iCurPage);
}

bool CPDFXFA_DocEnvironment::IsCalculationsEnabled(CXFA_FFDoc* hDoc) {
  if (hDoc != m_pContext->GetXFADoc() || !m_pContext->GetFormFillEnv())
    return false;
  if (m_pContext->GetFormFillEnv()->GetInterForm()) {
    return m_pContext->GetFormFillEnv()
        ->GetInterForm()
        ->IsXfaCalculateEnabled();
  }
  return false;
}

void CPDFXFA_DocEnvironment::SetCalculationsEnabled(CXFA_FFDoc* hDoc,
                                                    bool bEnabled) {
  if (hDoc != m_pContext->GetXFADoc() || !m_pContext->GetFormFillEnv())
    return;
  if (m_pContext->GetFormFillEnv()->GetInterForm()) {
    m_pContext->GetFormFillEnv()->GetInterForm()->XfaEnableCalculate(bEnabled);
  }
}

void CPDFXFA_DocEnvironment::GetTitle(CXFA_FFDoc* hDoc,
                                      CFX_WideString& wsTitle) {
  if (hDoc != m_pContext->GetXFADoc() || !m_pContext->GetPDFDoc())
    return;

  CPDF_Dictionary* pInfoDict = m_pContext->GetPDFDoc()->GetInfo();
  if (!pInfoDict)
    return;

  CFX_ByteString csTitle = pInfoDict->GetStringFor("Title");
  wsTitle = wsTitle.FromLocal(csTitle.GetBuffer(csTitle.GetLength()));
  csTitle.ReleaseBuffer(csTitle.GetLength());
}

void CPDFXFA_DocEnvironment::SetTitle(CXFA_FFDoc* hDoc,
                                      const CFX_WideString& wsTitle) {
  if (hDoc != m_pContext->GetXFADoc() || !m_pContext->GetPDFDoc())
    return;

  if (CPDF_Dictionary* pInfoDict = m_pContext->GetPDFDoc()->GetInfo())
    pInfoDict->SetNewFor<CPDF_String>("Title", wsTitle);
}

void CPDFXFA_DocEnvironment::ExportData(CXFA_FFDoc* hDoc,
                                        const CFX_WideString& wsFilePath,
                                        bool bXDP) {
  if (hDoc != m_pContext->GetXFADoc())
    return;

  if (m_pContext->GetDocType() != DOCTYPE_DYNAMIC_XFA &&
      m_pContext->GetDocType() != DOCTYPE_STATIC_XFA) {
    return;
  }

  CPDFSDK_FormFillEnvironment* pFormFillEnv = m_pContext->GetFormFillEnv();
  if (!pFormFillEnv)
    return;

  int fileType = bXDP ? FXFA_SAVEAS_XDP : FXFA_SAVEAS_XML;
  CFX_ByteString bs = wsFilePath.UTF16LE_Encode();
  if (wsFilePath.IsEmpty()) {
    if (!pFormFillEnv->GetFormFillInfo() ||
        !pFormFillEnv->GetFormFillInfo()->m_pJsPlatform) {
      return;
    }

    CFX_WideString filepath = pFormFillEnv->JS_fieldBrowse();
    bs = filepath.UTF16LE_Encode();
  }
  int len = bs.GetLength();
  FPDF_FILEHANDLER* pFileHandler =
      pFormFillEnv->OpenFile(bXDP ? FXFA_SAVEAS_XDP : FXFA_SAVEAS_XML,
                             (FPDF_WIDESTRING)bs.GetBuffer(len), "wb");
  bs.ReleaseBuffer(len);
  if (!pFileHandler)
    return;

  CFX_RetainPtr<IFX_SeekableStream> fileWrite =
      MakeSeekableStream(pFileHandler);
  CFX_ByteString content;
  if (fileType == FXFA_SAVEAS_XML) {
    content = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n";
    fileWrite->WriteBlock(content.c_str(), fileWrite->GetSize(),
                          content.GetLength());
    m_pContext->GetXFADocView()->GetDoc()->SavePackage(XFA_HASHCODE_Data,
                                                       fileWrite, nullptr);
  } else if (fileType == FXFA_SAVEAS_XDP) {
    if (!m_pContext->GetPDFDoc())
      return;

    CPDF_Dictionary* pRoot = m_pContext->GetPDFDoc()->GetRoot();
    if (!pRoot)
      return;

    CPDF_Dictionary* pAcroForm = pRoot->GetDictFor("AcroForm");
    if (!pAcroForm)
      return;

    CPDF_Array* pArray = ToArray(pAcroForm->GetObjectFor("XFA"));
    if (!pArray)
      return;

    int size = pArray->GetCount();
    for (int i = 1; i < size; i += 2) {
      CPDF_Object* pPDFObj = pArray->GetObjectAt(i);
      CPDF_Object* pPrePDFObj = pArray->GetObjectAt(i - 1);
      if (!pPrePDFObj->IsString())
        continue;
      if (!pPDFObj->IsReference())
        continue;

      CPDF_Stream* pStream = ToStream(pPDFObj->GetDirect());
      if (!pStream)
        continue;
      if (pPrePDFObj->GetString() == "form") {
        m_pContext->GetXFADocView()->GetDoc()->SavePackage(XFA_HASHCODE_Form,
                                                           fileWrite, nullptr);
        continue;
      }
      if (pPrePDFObj->GetString() == "datasets") {
        m_pContext->GetXFADocView()->GetDoc()->SavePackage(
            XFA_HASHCODE_Datasets, fileWrite, nullptr);
        continue;
      }
      if (i == size - 1) {
        CFX_WideString wPath = CFX_WideString::FromUTF16LE(
            reinterpret_cast<const unsigned short*>(bs.c_str()),
            bs.GetLength() / sizeof(unsigned short));
        CFX_ByteString bPath = wPath.UTF8Encode();
        const char* szFormat =
            "\n<pdf href=\"%s\" xmlns=\"http://ns.adobe.com/xdp/pdf/\"/>";
        content.Format(szFormat, bPath.c_str());
        fileWrite->WriteBlock(content.c_str(), fileWrite->GetSize(),
                              content.GetLength());
      }
      std::unique_ptr<CPDF_StreamAcc> pAcc(new CPDF_StreamAcc);
      pAcc->LoadAllData(pStream);
      fileWrite->WriteBlock(pAcc->GetData(), fileWrite->GetSize(),
                            pAcc->GetSize());
    }
  }
  fileWrite->Flush();
}

void CPDFXFA_DocEnvironment::GotoURL(CXFA_FFDoc* hDoc,
                                     const CFX_WideString& bsURL) {
  if (hDoc != m_pContext->GetXFADoc())
    return;

  if (m_pContext->GetDocType() != DOCTYPE_DYNAMIC_XFA)
    return;

  CPDFSDK_FormFillEnvironment* pFormFillEnv = m_pContext->GetFormFillEnv();
  if (!pFormFillEnv)
    return;

  CFX_WideStringC str(bsURL.c_str());
  pFormFillEnv->GotoURL(this, str);
}

bool CPDFXFA_DocEnvironment::IsValidationsEnabled(CXFA_FFDoc* hDoc) {
  if (hDoc != m_pContext->GetXFADoc() || !m_pContext->GetFormFillEnv())
    return false;
  if (m_pContext->GetFormFillEnv()->GetInterForm()) {
    return m_pContext->GetFormFillEnv()
        ->GetInterForm()
        ->IsXfaValidationsEnabled();
  }
  return true;
}

void CPDFXFA_DocEnvironment::SetValidationsEnabled(CXFA_FFDoc* hDoc,
                                                   bool bEnabled) {
  if (hDoc != m_pContext->GetXFADoc() || !m_pContext->GetFormFillEnv())
    return;
  if (m_pContext->GetFormFillEnv()->GetInterForm()) {
    m_pContext->GetFormFillEnv()->GetInterForm()->XfaSetValidationsEnabled(
        bEnabled);
  }
}

void CPDFXFA_DocEnvironment::SetFocusWidget(CXFA_FFDoc* hDoc,
                                            CXFA_FFWidget* hWidget) {
  if (hDoc != m_pContext->GetXFADoc())
    return;

  if (!hWidget) {
    CPDFSDK_Annot::ObservedPtr pNull;
    m_pContext->GetFormFillEnv()->SetFocusAnnot(&pNull);
    return;
  }

  int pageViewCount = m_pContext->GetFormFillEnv()->GetPageViewCount();
  for (int i = 0; i < pageViewCount; i++) {
    CPDFSDK_PageView* pPageView = m_pContext->GetFormFillEnv()->GetPageView(i);
    if (!pPageView)
      continue;

    CPDFSDK_Annot::ObservedPtr pAnnot(pPageView->GetAnnotByXFAWidget(hWidget));
    if (pAnnot) {
      m_pContext->GetFormFillEnv()->SetFocusAnnot(&pAnnot);
      break;
    }
  }
}

void CPDFXFA_DocEnvironment::Print(CXFA_FFDoc* hDoc,
                                   int32_t nStartPage,
                                   int32_t nEndPage,
                                   uint32_t dwOptions) {
  if (hDoc != m_pContext->GetXFADoc())
    return;

  CPDFSDK_FormFillEnvironment* pFormFillEnv = m_pContext->GetFormFillEnv();
  if (!pFormFillEnv || !pFormFillEnv->GetFormFillInfo() ||
      !pFormFillEnv->GetFormFillInfo()->m_pJsPlatform ||
      !pFormFillEnv->GetFormFillInfo()->m_pJsPlatform->Doc_print) {
    return;
  }

  pFormFillEnv->GetFormFillInfo()->m_pJsPlatform->Doc_print(
      pFormFillEnv->GetFormFillInfo()->m_pJsPlatform,
      dwOptions & XFA_PRINTOPT_ShowDialog, nStartPage, nEndPage,
      dwOptions & XFA_PRINTOPT_CanCancel, dwOptions & XFA_PRINTOPT_ShrinkPage,
      dwOptions & XFA_PRINTOPT_AsImage, dwOptions & XFA_PRINTOPT_ReverseOrder,
      dwOptions & XFA_PRINTOPT_PrintAnnot);
}

FX_ARGB CPDFXFA_DocEnvironment::GetHighlightColor(CXFA_FFDoc* hDoc) {
  if (hDoc != m_pContext->GetXFADoc() || !m_pContext->GetFormFillEnv())
    return 0;

  CPDFSDK_InterForm* pInterForm = m_pContext->GetFormFillEnv()->GetInterForm();
  if (!pInterForm)
    return 0;

  return ArgbEncode(pInterForm->GetHighlightAlpha(),
                    pInterForm->GetHighlightColor(FPDF_FORMFIELD_XFA));
}

bool CPDFXFA_DocEnvironment::NotifySubmit(bool bPrevOrPost) {
  if (bPrevOrPost)
    return OnBeforeNotifySubmit();

  OnAfterNotifySubmit();
  return true;
}

bool CPDFXFA_DocEnvironment::OnBeforeNotifySubmit() {
  if (m_pContext->GetDocType() != DOCTYPE_DYNAMIC_XFA &&
      m_pContext->GetDocType() != DOCTYPE_STATIC_XFA) {
    return true;
  }

  if (!m_pContext->GetXFADocView())
    return true;

  CXFA_FFWidgetHandler* pWidgetHandler =
      m_pContext->GetXFADocView()->GetWidgetHandler();
  if (!pWidgetHandler)
    return true;

  std::unique_ptr<CXFA_WidgetAccIterator> pWidgetAccIterator(
      m_pContext->GetXFADocView()->CreateWidgetAccIterator());
  if (pWidgetAccIterator) {
    CXFA_EventParam Param;
    Param.m_eType = XFA_EVENT_PreSubmit;
    while (CXFA_WidgetAcc* pWidgetAcc = pWidgetAccIterator->MoveToNext())
      pWidgetHandler->ProcessEvent(pWidgetAcc, &Param);
  }

  pWidgetAccIterator.reset(
      m_pContext->GetXFADocView()->CreateWidgetAccIterator());
  if (!pWidgetAccIterator)
    return true;

  CXFA_WidgetAcc* pWidgetAcc = pWidgetAccIterator->MoveToNext();
  pWidgetAcc = pWidgetAccIterator->MoveToNext();
  while (pWidgetAcc) {
    int fRet = pWidgetAcc->ProcessValidate(-1);
    if (fRet == XFA_EVENTERROR_Error) {
      CPDFSDK_FormFillEnvironment* pFormFillEnv = m_pContext->GetFormFillEnv();
      if (!pFormFillEnv)
        return false;

      CFX_WideString ws;
      ws.FromLocal(IDS_XFA_Validate_Input);
      CFX_ByteString bs = ws.UTF16LE_Encode();
      int len = bs.GetLength();
      pFormFillEnv->Alert((FPDF_WIDESTRING)bs.GetBuffer(len),
                          (FPDF_WIDESTRING)L"", 0, 1);
      bs.ReleaseBuffer(len);
      return false;
    }
    pWidgetAcc = pWidgetAccIterator->MoveToNext();
  }
  m_pContext->GetXFADocView()->UpdateDocView();

  return true;
}

void CPDFXFA_DocEnvironment::OnAfterNotifySubmit() {
  if (m_pContext->GetDocType() != DOCTYPE_DYNAMIC_XFA &&
      m_pContext->GetDocType() != DOCTYPE_STATIC_XFA)
    return;

  if (!m_pContext->GetXFADocView())
    return;

  CXFA_FFWidgetHandler* pWidgetHandler =
      m_pContext->GetXFADocView()->GetWidgetHandler();
  if (!pWidgetHandler)
    return;

  std::unique_ptr<CXFA_WidgetAccIterator> pWidgetAccIterator(
      m_pContext->GetXFADocView()->CreateWidgetAccIterator());
  if (!pWidgetAccIterator)
    return;

  CXFA_EventParam Param;
  Param.m_eType = XFA_EVENT_PostSubmit;
  CXFA_WidgetAcc* pWidgetAcc = pWidgetAccIterator->MoveToNext();
  while (pWidgetAcc) {
    pWidgetHandler->ProcessEvent(pWidgetAcc, &Param);
    pWidgetAcc = pWidgetAccIterator->MoveToNext();
  }
  m_pContext->GetXFADocView()->UpdateDocView();
}

bool CPDFXFA_DocEnvironment::SubmitData(CXFA_FFDoc* hDoc, CXFA_Submit submit) {
  if (!NotifySubmit(true) || !m_pContext->GetXFADocView())
    return false;

  m_pContext->GetXFADocView()->UpdateDocView();
  bool ret = SubmitDataInternal(hDoc, submit);
  NotifySubmit(false);
  return ret;
}

CFX_RetainPtr<IFX_SeekableReadStream> CPDFXFA_DocEnvironment::OpenLinkedFile(
    CXFA_FFDoc* hDoc,
    const CFX_WideString& wsLink) {
  CPDFSDK_FormFillEnvironment* pFormFillEnv = m_pContext->GetFormFillEnv();
  if (!pFormFillEnv)
    return nullptr;

  CFX_ByteString bs = wsLink.UTF16LE_Encode();
  int len = bs.GetLength();
  FPDF_FILEHANDLER* pFileHandler =
      pFormFillEnv->OpenFile(0, (FPDF_WIDESTRING)bs.GetBuffer(len), "rb");
  bs.ReleaseBuffer(len);
  if (!pFileHandler)
    return nullptr;

  return MakeSeekableStream(pFileHandler);
}

bool CPDFXFA_DocEnvironment::ExportSubmitFile(FPDF_FILEHANDLER* pFileHandler,
                                              int fileType,
                                              FPDF_DWORD encodeType,
                                              FPDF_DWORD flag) {
  if (!m_pContext->GetXFADocView())
    return false;

  CFX_ByteString content;
  CPDFSDK_FormFillEnvironment* pFormFillEnv = m_pContext->GetFormFillEnv();
  if (!pFormFillEnv)
    return false;

  CFX_RetainPtr<IFX_SeekableStream> fileStream =
      MakeSeekableStream(pFileHandler);

  if (fileType == FXFA_SAVEAS_XML) {
    const char kContent[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n";
    fileStream->WriteBlock(kContent, 0, strlen(kContent));
    m_pContext->GetXFADoc()->SavePackage(XFA_HASHCODE_Data, fileStream,
                                         nullptr);
    return true;
  }

  if (fileType != FXFA_SAVEAS_XDP)
    return true;

  if (!flag) {
    flag = FXFA_CONFIG | FXFA_TEMPLATE | FXFA_LOCALESET | FXFA_DATASETS |
           FXFA_XMPMETA | FXFA_XFDF | FXFA_FORM;
  }
  if (!m_pContext->GetPDFDoc()) {
    fileStream->Flush();
    return false;
  }

  CPDF_Dictionary* pRoot = m_pContext->GetPDFDoc()->GetRoot();
  if (!pRoot) {
    fileStream->Flush();
    return false;
  }

  CPDF_Dictionary* pAcroForm = pRoot->GetDictFor("AcroForm");
  if (!pAcroForm) {
    fileStream->Flush();
    return false;
  }

  CPDF_Array* pArray = ToArray(pAcroForm->GetObjectFor("XFA"));
  if (!pArray) {
    fileStream->Flush();
    return false;
  }

  int size = pArray->GetCount();
  for (int i = 1; i < size; i += 2) {
    CPDF_Object* pPDFObj = pArray->GetObjectAt(i);
    CPDF_Object* pPrePDFObj = pArray->GetObjectAt(i - 1);
    if (!pPrePDFObj->IsString())
      continue;
    if (!pPDFObj->IsReference())
      continue;

    CPDF_Object* pDirectObj = pPDFObj->GetDirect();
    if (!pDirectObj->IsStream())
      continue;
    if (pPrePDFObj->GetString() == "config" && !(flag & FXFA_CONFIG))
      continue;
    if (pPrePDFObj->GetString() == "template" && !(flag & FXFA_TEMPLATE))
      continue;
    if (pPrePDFObj->GetString() == "localeSet" && !(flag & FXFA_LOCALESET))
      continue;
    if (pPrePDFObj->GetString() == "datasets" && !(flag & FXFA_DATASETS))
      continue;
    if (pPrePDFObj->GetString() == "xmpmeta" && !(flag & FXFA_XMPMETA))
      continue;
    if (pPrePDFObj->GetString() == "xfdf" && !(flag & FXFA_XFDF))
      continue;
    if (pPrePDFObj->GetString() == "form" && !(flag & FXFA_FORM))
      continue;
    if (pPrePDFObj->GetString() == "form") {
      m_pContext->GetXFADoc()->SavePackage(XFA_HASHCODE_Form, fileStream,
                                           nullptr);
    } else if (pPrePDFObj->GetString() == "datasets") {
      m_pContext->GetXFADoc()->SavePackage(XFA_HASHCODE_Datasets, fileStream,
                                           nullptr);
    } else {
      // PDF,creator.
    }
  }
  return true;
}

void CPDFXFA_DocEnvironment::ToXFAContentFlags(CFX_WideString csSrcContent,
                                               FPDF_DWORD& flag) {
  if (csSrcContent.Find(L" config ", 0) != -1)
    flag |= FXFA_CONFIG;
  if (csSrcContent.Find(L" template ", 0) != -1)
    flag |= FXFA_TEMPLATE;
  if (csSrcContent.Find(L" localeSet ", 0) != -1)
    flag |= FXFA_LOCALESET;
  if (csSrcContent.Find(L" datasets ", 0) != -1)
    flag |= FXFA_DATASETS;
  if (csSrcContent.Find(L" xmpmeta ", 0) != -1)
    flag |= FXFA_XMPMETA;
  if (csSrcContent.Find(L" xfdf ", 0) != -1)
    flag |= FXFA_XFDF;
  if (csSrcContent.Find(L" form ", 0) != -1)
    flag |= FXFA_FORM;
  if (flag == 0) {
    flag = FXFA_CONFIG | FXFA_TEMPLATE | FXFA_LOCALESET | FXFA_DATASETS |
           FXFA_XMPMETA | FXFA_XFDF | FXFA_FORM;
  }
}

bool CPDFXFA_DocEnvironment::MailToInfo(CFX_WideString& csURL,
                                        CFX_WideString& csToAddress,
                                        CFX_WideString& csCCAddress,
                                        CFX_WideString& csBCCAddress,
                                        CFX_WideString& csSubject,
                                        CFX_WideString& csMsg) {
  CFX_WideString srcURL = csURL;
  srcURL.TrimLeft();
  if (srcURL.Left(7).CompareNoCase(L"mailto:") != 0)
    return false;

  int pos = srcURL.Find(L'?', 0);
  CFX_WideString tmp;
  if (pos == -1) {
    pos = srcURL.Find(L'@', 0);
    if (pos == -1)
      return false;

    tmp = srcURL.Right(csURL.GetLength() - 7);
  } else {
    tmp = srcURL.Left(pos);
    tmp = tmp.Right(tmp.GetLength() - 7);
  }
  tmp.TrimLeft();
  tmp.TrimRight();

  csToAddress = tmp;

  srcURL = srcURL.Right(srcURL.GetLength() - (pos + 1));
  while (!srcURL.IsEmpty()) {
    srcURL.TrimLeft();
    srcURL.TrimRight();
    pos = srcURL.Find(L'&', 0);

    tmp = (pos == -1) ? srcURL : srcURL.Left(pos);
    tmp.TrimLeft();
    tmp.TrimRight();
    if (tmp.GetLength() >= 3 && tmp.Left(3).CompareNoCase(L"cc=") == 0) {
      tmp = tmp.Right(tmp.GetLength() - 3);
      if (!csCCAddress.IsEmpty())
        csCCAddress += L';';
      csCCAddress += tmp;
    } else if (tmp.GetLength() >= 4 &&
               tmp.Left(4).CompareNoCase(L"bcc=") == 0) {
      tmp = tmp.Right(tmp.GetLength() - 4);
      if (!csBCCAddress.IsEmpty())
        csBCCAddress += L';';
      csBCCAddress += tmp;
    } else if (tmp.GetLength() >= 8 &&
               tmp.Left(8).CompareNoCase(L"subject=") == 0) {
      tmp = tmp.Right(tmp.GetLength() - 8);
      csSubject += tmp;
    } else if (tmp.GetLength() >= 5 &&
               tmp.Left(5).CompareNoCase(L"body=") == 0) {
      tmp = tmp.Right(tmp.GetLength() - 5);
      csMsg += tmp;
    }
    srcURL = (pos == -1) ? L"" : srcURL.Right(csURL.GetLength() - (pos + 1));
  }
  csToAddress.Replace(L",", L";");
  csCCAddress.Replace(L",", L";");
  csBCCAddress.Replace(L",", L";");
  return true;
}

bool CPDFXFA_DocEnvironment::SubmitDataInternal(CXFA_FFDoc* hDoc,
                                                CXFA_Submit submit) {
  CPDFSDK_FormFillEnvironment* pFormFillEnv = m_pContext->GetFormFillEnv();
  if (!pFormFillEnv)
    return false;

  CFX_WideStringC csURLC;
  submit.GetSubmitTarget(csURLC);
  CFX_WideString csURL(csURLC);
  if (csURL.IsEmpty()) {
    CFX_WideString ws;
    ws.FromLocal("Submit cancelled.");
    CFX_ByteString bs = ws.UTF16LE_Encode();
    int len = bs.GetLength();
    pFormFillEnv->Alert((FPDF_WIDESTRING)bs.GetBuffer(len),
                        (FPDF_WIDESTRING)L"", 0, 4);
    bs.ReleaseBuffer(len);
    return false;
  }

  FPDF_FILEHANDLER* pFileHandler = nullptr;
  int fileFlag = -1;
  switch (submit.GetSubmitFormat()) {
    case XFA_ATTRIBUTEENUM_Xdp: {
      CFX_WideStringC csContentC;
      submit.GetSubmitXDPContent(csContentC);
      CFX_WideString csContent;
      csContent = csContentC;
      csContent.TrimLeft();
      csContent.TrimRight();
      CFX_WideString space;
      space.FromLocal(" ");
      csContent = space + csContent + space;
      FPDF_DWORD flag = 0;
      if (submit.IsSubmitEmbedPDF())
        flag |= FXFA_PDF;

      ToXFAContentFlags(csContent, flag);
      pFileHandler = pFormFillEnv->OpenFile(FXFA_SAVEAS_XDP, nullptr, "wb");
      fileFlag = FXFA_SAVEAS_XDP;
      ExportSubmitFile(pFileHandler, FXFA_SAVEAS_XDP, 0, flag);
      break;
    }
    case XFA_ATTRIBUTEENUM_Xml:
      pFileHandler = pFormFillEnv->OpenFile(FXFA_SAVEAS_XML, nullptr, "wb");
      fileFlag = FXFA_SAVEAS_XML;
      ExportSubmitFile(pFileHandler, FXFA_SAVEAS_XML, 0, FXFA_XFA_ALL);
      break;
    case XFA_ATTRIBUTEENUM_Pdf:
      break;
    case XFA_ATTRIBUTEENUM_Urlencoded:
      pFileHandler = pFormFillEnv->OpenFile(FXFA_SAVEAS_XML, nullptr, "wb");
      fileFlag = FXFA_SAVEAS_XML;
      ExportSubmitFile(pFileHandler, FXFA_SAVEAS_XML, 0, FXFA_XFA_ALL);
      break;
    default:
      return false;
  }
  if (!pFileHandler)
    return false;
  if (csURL.Left(7).CompareNoCase(L"mailto:") == 0) {
    CFX_WideString csToAddress;
    CFX_WideString csCCAddress;
    CFX_WideString csBCCAddress;
    CFX_WideString csSubject;
    CFX_WideString csMsg;
    if (!MailToInfo(csURL, csToAddress, csCCAddress, csBCCAddress, csSubject,
                    csMsg)) {
      return false;
    }
    CFX_ByteString bsTo = CFX_WideString(csToAddress).UTF16LE_Encode();
    CFX_ByteString bsCC = CFX_WideString(csCCAddress).UTF16LE_Encode();
    CFX_ByteString bsBcc = CFX_WideString(csBCCAddress).UTF16LE_Encode();
    CFX_ByteString bsSubject = CFX_WideString(csSubject).UTF16LE_Encode();
    CFX_ByteString bsMsg = CFX_WideString(csMsg).UTF16LE_Encode();
    FPDF_WIDESTRING pTo = (FPDF_WIDESTRING)bsTo.GetBuffer(bsTo.GetLength());
    FPDF_WIDESTRING pCC = (FPDF_WIDESTRING)bsCC.GetBuffer(bsCC.GetLength());
    FPDF_WIDESTRING pBcc = (FPDF_WIDESTRING)bsBcc.GetBuffer(bsBcc.GetLength());
    FPDF_WIDESTRING pSubject =
        (FPDF_WIDESTRING)bsSubject.GetBuffer(bsSubject.GetLength());
    FPDF_WIDESTRING pMsg = (FPDF_WIDESTRING)bsMsg.GetBuffer(bsMsg.GetLength());
    pFormFillEnv->EmailTo(pFileHandler, pTo, pSubject, pCC, pBcc, pMsg);
    bsTo.ReleaseBuffer();
    bsCC.ReleaseBuffer();
    bsBcc.ReleaseBuffer();
    bsSubject.ReleaseBuffer();
    bsMsg.ReleaseBuffer();
  } else {
    // HTTP or FTP
    CFX_WideString ws;
    CFX_ByteString bs = csURL.UTF16LE_Encode();
    int len = bs.GetLength();
    pFormFillEnv->UploadTo(pFileHandler, fileFlag,
                           (FPDF_WIDESTRING)bs.GetBuffer(len));
    bs.ReleaseBuffer(len);
  }
  return true;
}

bool CPDFXFA_DocEnvironment::SetGlobalProperty(
    CXFA_FFDoc* hDoc,
    const CFX_ByteStringC& szPropName,
    CFXJSE_Value* pValue) {
  if (hDoc != m_pContext->GetXFADoc())
    return false;

  if (m_pContext->GetFormFillEnv() &&
      m_pContext->GetFormFillEnv()->GetJSRuntime()) {
    return m_pContext->GetFormFillEnv()->GetJSRuntime()->SetValueByName(
        szPropName, pValue);
  }
  return false;
}

bool CPDFXFA_DocEnvironment::GetGlobalProperty(
    CXFA_FFDoc* hDoc,
    const CFX_ByteStringC& szPropName,
    CFXJSE_Value* pValue) {
  if (hDoc != m_pContext->GetXFADoc())
    return false;
  if (!m_pContext->GetFormFillEnv() ||
      !m_pContext->GetFormFillEnv()->GetJSRuntime()) {
    return false;
  }

  CPDFSDK_FormFillEnvironment* pFormFillEnv = m_pContext->GetFormFillEnv();
  if (!m_pJSEventContext)
    m_pJSEventContext = pFormFillEnv->GetJSRuntime()->NewEventContext();

  return pFormFillEnv->GetJSRuntime()->GetValueByName(szPropName, pValue);
}
