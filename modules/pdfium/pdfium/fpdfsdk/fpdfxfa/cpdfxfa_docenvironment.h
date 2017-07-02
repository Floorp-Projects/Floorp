// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_FPDFXFA_CPDFXFA_DOCENVIRONMENT_H_
#define FPDFSDK_FPDFXFA_CPDFXFA_DOCENVIRONMENT_H_

#include "core/fxcrt/cfx_retain_ptr.h"
#include "public/fpdfview.h"
#include "xfa/fxfa/fxfa.h"

class CPDFXFA_Context;
class IJS_EventContext;

class CPDFXFA_DocEnvironment : public IXFA_DocEnvironment {
 public:
  explicit CPDFXFA_DocEnvironment(CPDFXFA_Context*);
  ~CPDFXFA_DocEnvironment() override;

  // IXFA_DocEnvironment
  void SetChangeMark(CXFA_FFDoc* hDoc) override;
  // used in dynamic xfa, dwFlags refer to XFA_INVALIDATE_XXX macros.
  void InvalidateRect(CXFA_FFPageView* pPageView,
                      const CFX_RectF& rt,
                      uint32_t dwFlags) override;
  // show or hide caret
  void DisplayCaret(CXFA_FFWidget* hWidget,
                    bool bVisible,
                    const CFX_RectF* pRtAnchor) override;
  // dwPos: (0:bottom 1:top)
  bool GetPopupPos(CXFA_FFWidget* hWidget,
                   FX_FLOAT fMinPopup,
                   FX_FLOAT fMaxPopup,
                   const CFX_RectF& rtAnchor,
                   CFX_RectF& rtPopup) override;
  bool PopupMenu(CXFA_FFWidget* hWidget, CFX_PointF ptPopup) override;

  // dwFlags XFA_PAGEVIEWEVENT_Added, XFA_PAGEVIEWEVENT_Removing
  void PageViewEvent(CXFA_FFPageView* pPageView, uint32_t dwFlags) override;
  void WidgetPostAdd(CXFA_FFWidget* hWidget,
                     CXFA_WidgetAcc* pWidgetData) override;
  void WidgetPreRemove(CXFA_FFWidget* hWidget,
                       CXFA_WidgetAcc* pWidgetData) override;

  // Host method
  int32_t CountPages(CXFA_FFDoc* hDoc) override;
  int32_t GetCurrentPage(CXFA_FFDoc* hDoc) override;
  void SetCurrentPage(CXFA_FFDoc* hDoc, int32_t iCurPage) override;
  bool IsCalculationsEnabled(CXFA_FFDoc* hDoc) override;
  void SetCalculationsEnabled(CXFA_FFDoc* hDoc, bool bEnabled) override;
  void GetTitle(CXFA_FFDoc* hDoc, CFX_WideString& wsTitle) override;
  void SetTitle(CXFA_FFDoc* hDoc, const CFX_WideString& wsTitle) override;
  void ExportData(CXFA_FFDoc* hDoc,
                  const CFX_WideString& wsFilePath,
                  bool bXDP) override;
  void GotoURL(CXFA_FFDoc* hDoc, const CFX_WideString& bsURL) override;
  bool IsValidationsEnabled(CXFA_FFDoc* hDoc) override;
  void SetValidationsEnabled(CXFA_FFDoc* hDoc, bool bEnabled) override;
  void SetFocusWidget(CXFA_FFDoc* hDoc, CXFA_FFWidget* hWidget) override;
  void Print(CXFA_FFDoc* hDoc,
             int32_t nStartPage,
             int32_t nEndPage,
             uint32_t dwOptions) override;
  FX_ARGB GetHighlightColor(CXFA_FFDoc* hDoc) override;

  /**
   *Submit data to email, http, ftp.
   * @param[in] hDoc The document handler.
   * @param[in] eFormat Determines the format in which the data will be
   *submitted. XFA_ATTRIBUTEENUM_Xdp, XFA_ATTRIBUTEENUM_Xml...
   * @param[in] wsTarget The URL to which the data will be submitted.
   * @param[in] eEncoding The encoding of text content.
   * @param[in] pXDPContent Controls what subset of the data is submitted, used
   *only when the format property is xdp.
   * @param[in] bEmbedPDF, specifies whether PDF is embedded in the submitted
   *content or not.
   */
  bool SubmitData(CXFA_FFDoc* hDoc, CXFA_Submit submit) override;

  bool GetGlobalProperty(CXFA_FFDoc* hDoc,
                         const CFX_ByteStringC& szPropName,
                         CFXJSE_Value* pValue) override;
  bool SetGlobalProperty(CXFA_FFDoc* hDoc,
                         const CFX_ByteStringC& szPropName,
                         CFXJSE_Value* pValue) override;

  CFX_RetainPtr<IFX_SeekableReadStream> OpenLinkedFile(
      CXFA_FFDoc* hDoc,
      const CFX_WideString& wsLink) override;

 private:
  bool OnBeforeNotifySubmit();
  void OnAfterNotifySubmit();
  bool NotifySubmit(bool bPrevOrPost);
  bool SubmitDataInternal(CXFA_FFDoc* hDoc, CXFA_Submit submit);
  bool MailToInfo(CFX_WideString& csURL,
                  CFX_WideString& csToAddress,
                  CFX_WideString& csCCAddress,
                  CFX_WideString& csBCCAddress,
                  CFX_WideString& csSubject,
                  CFX_WideString& csMsg);
  bool ExportSubmitFile(FPDF_FILEHANDLER* ppFileHandler,
                        int fileType,
                        FPDF_DWORD encodeType,
                        FPDF_DWORD flag);
  void ToXFAContentFlags(CFX_WideString csSrcContent, FPDF_DWORD& flag);

  CPDFXFA_Context* const m_pContext;    // Not owned.
  IJS_EventContext* m_pJSEventContext;  // Not owned.
};

#endif  // FPDFSDK_FPDFXFA_CPDFXFA_DOCENVIRONMENT_H_
