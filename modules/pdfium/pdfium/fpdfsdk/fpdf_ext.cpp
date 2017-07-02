// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "public/fpdf_ext.h"

#include <memory>

#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfdoc/cpdf_annot.h"
#include "core/fpdfdoc/cpdf_interform.h"
#include "core/fpdfdoc/cpdf_metadata.h"
#include "core/fxcrt/fx_basic.h"
#include "core/fxcrt/fx_memory.h"
#include "core/fxcrt/fx_xml.h"
#include "fpdfsdk/fsdk_define.h"
#include "third_party/base/ptr_util.h"

#ifdef PDF_ENABLE_XFA
#include "fpdfsdk/fpdfxfa/cpdfxfa_context.h"
#endif  // PDF_ENABLE_XFA

bool FPDF_UnSupportError(int nError) {
  CFSDK_UnsupportInfo_Adapter* pAdapter =
      CPDF_ModuleMgr::Get()->GetUnsupportInfoAdapter();
  if (!pAdapter)
    return false;

  UNSUPPORT_INFO* info = static_cast<UNSUPPORT_INFO*>(pAdapter->GetUnspInfo());
  if (info && info->FSDK_UnSupport_Handler)
    info->FSDK_UnSupport_Handler(info, nError);
  return true;
}

DLLEXPORT FPDF_BOOL STDCALL
FSDK_SetUnSpObjProcessHandler(UNSUPPORT_INFO* unsp_info) {
  if (!unsp_info || unsp_info->version != 1)
    return false;

  CPDF_ModuleMgr::Get()->SetUnsupportInfoAdapter(
      pdfium::MakeUnique<CFSDK_UnsupportInfo_Adapter>(unsp_info));
  return true;
}

void CheckUnSupportAnnot(CPDF_Document* pDoc, const CPDF_Annot* pPDFAnnot) {
  CPDF_Annot::Subtype nAnnotSubtype = pPDFAnnot->GetSubtype();
  if (nAnnotSubtype == CPDF_Annot::Subtype::THREED) {
    FPDF_UnSupportError(FPDF_UNSP_ANNOT_3DANNOT);
  } else if (nAnnotSubtype == CPDF_Annot::Subtype::SCREEN) {
    const CPDF_Dictionary* pAnnotDict = pPDFAnnot->GetAnnotDict();
    CFX_ByteString cbString;
    if (pAnnotDict->KeyExist("IT"))
      cbString = pAnnotDict->GetStringFor("IT");
    if (cbString.Compare("Img") != 0)
      FPDF_UnSupportError(FPDF_UNSP_ANNOT_SCREEN_MEDIA);
  } else if (nAnnotSubtype == CPDF_Annot::Subtype::MOVIE) {
    FPDF_UnSupportError(FPDF_UNSP_ANNOT_MOVIE);
  } else if (nAnnotSubtype == CPDF_Annot::Subtype::SOUND) {
    FPDF_UnSupportError(FPDF_UNSP_ANNOT_SOUND);
  } else if (nAnnotSubtype == CPDF_Annot::Subtype::RICHMEDIA) {
    FPDF_UnSupportError(FPDF_UNSP_ANNOT_SCREEN_RICHMEDIA);
  } else if (nAnnotSubtype == CPDF_Annot::Subtype::FILEATTACHMENT) {
    FPDF_UnSupportError(FPDF_UNSP_ANNOT_ATTACHMENT);
  } else if (nAnnotSubtype == CPDF_Annot::Subtype::WIDGET) {
    const CPDF_Dictionary* pAnnotDict = pPDFAnnot->GetAnnotDict();
    CFX_ByteString cbString;
    if (pAnnotDict->KeyExist("FT"))
      cbString = pAnnotDict->GetStringFor("FT");
    if (cbString.Compare("Sig") == 0)
      FPDF_UnSupportError(FPDF_UNSP_ANNOT_SIG);
  }
}

bool CheckSharedForm(const CXML_Element* pElement, CFX_ByteString cbName) {
  int count = pElement->CountAttrs();
  int i = 0;
  for (i = 0; i < count; i++) {
    CFX_ByteString space, name;
    CFX_WideString value;
    pElement->GetAttrByIndex(i, space, name, value);
    if (space == "xmlns" && name == "adhocwf" &&
        value == L"http://ns.adobe.com/AcrobatAdhocWorkflow/1.0/") {
      CXML_Element* pVersion =
          pElement->GetElement("adhocwf", cbName.AsStringC());
      if (!pVersion)
        continue;
      CFX_WideString wsContent = pVersion->GetContent(0);
      int nType = wsContent.GetInteger();
      switch (nType) {
        case 1:
          FPDF_UnSupportError(FPDF_UNSP_DOC_SHAREDFORM_ACROBAT);
          break;
        case 2:
          FPDF_UnSupportError(FPDF_UNSP_DOC_SHAREDFORM_FILESYSTEM);
          break;
        case 0:
          FPDF_UnSupportError(FPDF_UNSP_DOC_SHAREDFORM_EMAIL);
          break;
      }
    }
  }

  uint32_t nCount = pElement->CountChildren();
  for (i = 0; i < (int)nCount; i++) {
    CXML_Element::ChildType childType = pElement->GetChildType(i);
    if (childType == CXML_Element::Element) {
      CXML_Element* pChild = pElement->GetElement(i);
      if (CheckSharedForm(pChild, cbName))
        return true;
    }
  }
  return false;
}

void CheckUnSupportError(CPDF_Document* pDoc, uint32_t err_code) {
  // Security
  if (err_code == FPDF_ERR_SECURITY) {
    FPDF_UnSupportError(FPDF_UNSP_DOC_SECURITY);
    return;
  }
  if (!pDoc)
    return;

  // Portfolios and Packages
  CPDF_Dictionary* pRootDict = pDoc->GetRoot();
  if (pRootDict) {
    CFX_ByteString cbString;
    if (pRootDict->KeyExist("Collection")) {
      FPDF_UnSupportError(FPDF_UNSP_DOC_PORTABLECOLLECTION);
      return;
    }
    if (pRootDict->KeyExist("Names")) {
      CPDF_Dictionary* pNameDict = pRootDict->GetDictFor("Names");
      if (pNameDict && pNameDict->KeyExist("EmbeddedFiles")) {
        FPDF_UnSupportError(FPDF_UNSP_DOC_ATTACHMENT);
        return;
      }
      if (pNameDict && pNameDict->KeyExist("JavaScript")) {
        CPDF_Dictionary* pJSDict = pNameDict->GetDictFor("JavaScript");
        CPDF_Array* pArray = pJSDict ? pJSDict->GetArrayFor("Names") : nullptr;
        if (pArray) {
          for (size_t i = 0; i < pArray->GetCount(); i++) {
            CFX_ByteString cbStr = pArray->GetStringAt(i);
            if (cbStr.Compare("com.adobe.acrobat.SharedReview.Register") == 0) {
              FPDF_UnSupportError(FPDF_UNSP_DOC_SHAREDREVIEW);
              return;
            }
          }
        }
      }
    }
  }

  // SharedForm
  CPDF_Metadata metaData(pDoc);
  const CXML_Element* pElement = metaData.GetRoot();
  if (pElement)
    CheckSharedForm(pElement, "workflowType");

#ifndef PDF_ENABLE_XFA
  // XFA Forms
  CPDF_InterForm interform(pDoc);
  if (interform.HasXFAForm())
    FPDF_UnSupportError(FPDF_UNSP_DOC_XFAFORM);
#endif  // PDF_ENABLE_XFA
}

DLLEXPORT int STDCALL FPDFDoc_GetPageMode(FPDF_DOCUMENT document) {
  CPDF_Document* pDoc = CPDFDocumentFromFPDFDocument(document);
  if (!pDoc)
    return PAGEMODE_UNKNOWN;

  CPDF_Dictionary* pRoot = pDoc->GetRoot();
  if (!pRoot)
    return PAGEMODE_UNKNOWN;

  CPDF_Object* pName = pRoot->GetObjectFor("PageMode");
  if (!pName)
    return PAGEMODE_USENONE;

  CFX_ByteString strPageMode = pName->GetString();
  if (strPageMode.IsEmpty() || strPageMode.EqualNoCase("UseNone"))
    return PAGEMODE_USENONE;
  if (strPageMode.EqualNoCase("UseOutlines"))
    return PAGEMODE_USEOUTLINES;
  if (strPageMode.EqualNoCase("UseThumbs"))
    return PAGEMODE_USETHUMBS;
  if (strPageMode.EqualNoCase("FullScreen"))
    return PAGEMODE_FULLSCREEN;
  if (strPageMode.EqualNoCase("UseOC"))
    return PAGEMODE_USEOC;
  if (strPageMode.EqualNoCase("UseAttachments"))
    return PAGEMODE_USEATTACHMENTS;

  return PAGEMODE_UNKNOWN;
}
