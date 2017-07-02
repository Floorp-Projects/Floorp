// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/Document.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fpdfdoc/cpdf_interform.h"
#include "core/fpdfdoc/cpdf_nametree.h"
#include "fpdfsdk/cpdfsdk_annotiteration.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_interform.h"
#include "fpdfsdk/cpdfsdk_pageview.h"
#include "fpdfsdk/cpdfsdk_widget.h"
#include "fpdfsdk/javascript/Annot.h"
#include "fpdfsdk/javascript/Field.h"
#include "fpdfsdk/javascript/Icon.h"
#include "fpdfsdk/javascript/JS_Define.h"
#include "fpdfsdk/javascript/JS_EventHandler.h"
#include "fpdfsdk/javascript/JS_Object.h"
#include "fpdfsdk/javascript/JS_Value.h"
#include "fpdfsdk/javascript/app.h"
#include "fpdfsdk/javascript/cjs_event_context.h"
#include "fpdfsdk/javascript/cjs_runtime.h"
#include "fpdfsdk/javascript/resource.h"
#include "third_party/base/numerics/safe_math.h"
#include "third_party/base/ptr_util.h"

JSConstSpec CJS_PrintParamsObj::ConstSpecs[] = {{0, JSConstSpec::Number, 0, 0}};

JSPropertySpec CJS_PrintParamsObj::PropertySpecs[] = {{0, 0, 0}};

JSMethodSpec CJS_PrintParamsObj::MethodSpecs[] = {{0, 0}};

IMPLEMENT_JS_CLASS(CJS_PrintParamsObj, PrintParamsObj)

PrintParamsObj::PrintParamsObj(CJS_Object* pJSObject)
    : CJS_EmbedObj(pJSObject) {
  bUI = true;
  nStart = 0;
  nEnd = 0;
  bSilent = false;
  bShrinkToFit = false;
  bPrintAsImage = false;
  bReverse = false;
  bAnnotations = true;
}

#define MINWIDTH 5.0f
#define MINHEIGHT 5.0f

JSConstSpec CJS_Document::ConstSpecs[] = {{0, JSConstSpec::Number, 0, 0}};

JSPropertySpec CJS_Document::PropertySpecs[] = {
    {"ADBE", get_ADBE_static, set_ADBE_static},
    {"author", get_author_static, set_author_static},
    {"baseURL", get_baseURL_static, set_baseURL_static},
    {"bookmarkRoot", get_bookmarkRoot_static, set_bookmarkRoot_static},
    {"calculate", get_calculate_static, set_calculate_static},
    {"Collab", get_Collab_static, set_Collab_static},
    {"creationDate", get_creationDate_static, set_creationDate_static},
    {"creator", get_creator_static, set_creator_static},
    {"delay", get_delay_static, set_delay_static},
    {"dirty", get_dirty_static, set_dirty_static},
    {"documentFileName", get_documentFileName_static,
     set_documentFileName_static},
    {"external", get_external_static, set_external_static},
    {"filesize", get_filesize_static, set_filesize_static},
    {"icons", get_icons_static, set_icons_static},
    {"info", get_info_static, set_info_static},
    {"keywords", get_keywords_static, set_keywords_static},
    {"layout", get_layout_static, set_layout_static},
    {"media", get_media_static, set_media_static},
    {"modDate", get_modDate_static, set_modDate_static},
    {"mouseX", get_mouseX_static, set_mouseX_static},
    {"mouseY", get_mouseY_static, set_mouseY_static},
    {"numFields", get_numFields_static, set_numFields_static},
    {"numPages", get_numPages_static, set_numPages_static},
    {"pageNum", get_pageNum_static, set_pageNum_static},
    {"pageWindowRect", get_pageWindowRect_static, set_pageWindowRect_static},
    {"path", get_path_static, set_path_static},
    {"producer", get_producer_static, set_producer_static},
    {"subject", get_subject_static, set_subject_static},
    {"title", get_title_static, set_title_static},
    {"URL", get_URL_static, set_URL_static},
    {"zoom", get_zoom_static, set_zoom_static},
    {"zoomType", get_zoomType_static, set_zoomType_static},
    {0, 0, 0}};

JSMethodSpec CJS_Document::MethodSpecs[] = {
    {"addAnnot", addAnnot_static},
    {"addField", addField_static},
    {"addLink", addLink_static},
    {"addIcon", addIcon_static},
    {"calculateNow", calculateNow_static},
    {"closeDoc", closeDoc_static},
    {"createDataObject", createDataObject_static},
    {"deletePages", deletePages_static},
    {"exportAsText", exportAsText_static},
    {"exportAsFDF", exportAsFDF_static},
    {"exportAsXFDF", exportAsXFDF_static},
    {"extractPages", extractPages_static},
    {"getAnnot", getAnnot_static},
    {"getAnnots", getAnnots_static},
    {"getAnnot3D", getAnnot3D_static},
    {"getAnnots3D", getAnnots3D_static},
    {"getField", getField_static},
    {"getIcon", getIcon_static},
    {"getLinks", getLinks_static},
    {"getNthFieldName", getNthFieldName_static},
    {"getOCGs", getOCGs_static},
    {"getPageBox", getPageBox_static},
    {"getPageNthWord", getPageNthWord_static},
    {"getPageNthWordQuads", getPageNthWordQuads_static},
    {"getPageNumWords", getPageNumWords_static},
    {"getPrintParams", getPrintParams_static},
    {"getURL", getURL_static},
    {"gotoNamedDest", gotoNamedDest_static},
    {"importAnFDF", importAnFDF_static},
    {"importAnXFDF", importAnXFDF_static},
    {"importTextData", importTextData_static},
    {"insertPages", insertPages_static},
    {"mailForm", mailForm_static},
    {"print", print_static},
    {"removeField", removeField_static},
    {"replacePages", replacePages_static},
    {"resetForm", resetForm_static},
    {"removeIcon", removeIcon_static},
    {"saveAs", saveAs_static},
    {"submitForm", submitForm_static},
    {"syncAnnotScan", syncAnnotScan_static},
    {"mailDoc", mailDoc_static},
    {0, 0}};

IMPLEMENT_JS_CLASS(CJS_Document, Document)

void CJS_Document::InitInstance(IJS_Runtime* pIRuntime) {
  CJS_Runtime* pRuntime = static_cast<CJS_Runtime*>(pIRuntime);
  Document* pDoc = static_cast<Document*>(GetEmbedObject());
  pDoc->SetFormFillEnv(pRuntime->GetFormFillEnv());
}

Document::Document(CJS_Object* pJSObject)
    : CJS_EmbedObj(pJSObject),
      m_pFormFillEnv(nullptr),
      m_cwBaseURL(L""),
      m_bDelay(false) {}

Document::~Document() {
}

// the total number of fileds in document.
bool Document::numFields(CJS_Runtime* pRuntime,
                         CJS_PropValue& vp,
                         CFX_WideString& sError) {
  if (vp.IsSetting()) {
    sError = JSGetStringFromID(IDS_STRING_JSREADONLY);
    return false;
  }
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  CPDFSDK_InterForm* pInterForm = m_pFormFillEnv->GetInterForm();
  CPDF_InterForm* pPDFForm = pInterForm->GetInterForm();
  vp << static_cast<int>(pPDFForm->CountFields(CFX_WideString()));
  return true;
}

bool Document::dirty(CJS_Runtime* pRuntime,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  if (vp.IsGetting()) {
    vp << !!m_pFormFillEnv->GetChangeMark();
    return true;
  }
  bool bChanged = false;
  vp >> bChanged;
  if (bChanged)
    m_pFormFillEnv->SetChangeMark();
  else
    m_pFormFillEnv->ClearChangeMark();

  return true;
}

bool Document::ADBE(CJS_Runtime* pRuntime,
                    CJS_PropValue& vp,
                    CFX_WideString& sError) {
  if (vp.IsGetting())
    vp.GetJSValue()->SetNull(pRuntime);

  return true;
}

bool Document::pageNum(CJS_Runtime* pRuntime,
                       CJS_PropValue& vp,
                       CFX_WideString& sError) {
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  if (vp.IsGetting()) {
    if (CPDFSDK_PageView* pPageView = m_pFormFillEnv->GetCurrentView())
      vp << pPageView->GetPageIndex();
    return true;
  }
  int iPageCount = m_pFormFillEnv->GetPageCount();
  int iPageNum = 0;
  vp >> iPageNum;
  if (iPageNum >= 0 && iPageNum < iPageCount)
    m_pFormFillEnv->JS_docgotoPage(iPageNum);
  else if (iPageNum >= iPageCount)
    m_pFormFillEnv->JS_docgotoPage(iPageCount - 1);
  else if (iPageNum < 0)
    m_pFormFillEnv->JS_docgotoPage(0);

  return true;
}

bool Document::addAnnot(CJS_Runtime* pRuntime,
                        const std::vector<CJS_Value>& params,
                        CJS_Value& vRet,
                        CFX_WideString& sError) {
  // Not supported.
  return true;
}

bool Document::addField(CJS_Runtime* pRuntime,
                        const std::vector<CJS_Value>& params,
                        CJS_Value& vRet,
                        CFX_WideString& sError) {
  // Not supported.
  return true;
}

bool Document::exportAsText(CJS_Runtime* pRuntime,
                            const std::vector<CJS_Value>& params,
                            CJS_Value& vRet,
                            CFX_WideString& sError) {
  // Unsafe, not supported.
  return true;
}

bool Document::exportAsFDF(CJS_Runtime* pRuntime,
                           const std::vector<CJS_Value>& params,
                           CJS_Value& vRet,
                           CFX_WideString& sError) {
  // Unsafe, not supported.
  return true;
}

bool Document::exportAsXFDF(CJS_Runtime* pRuntime,
                            const std::vector<CJS_Value>& params,
                            CJS_Value& vRet,
                            CFX_WideString& sError) {
  // Unsafe, not supported.
  return true;
}

// Maps a field object in PDF document to a JavaScript variable
// comment:
// note: the paremter cName, this is clue how to treat if the cName is not a
// valiable filed name in this document

bool Document::getField(CJS_Runtime* pRuntime,
                        const std::vector<CJS_Value>& params,
                        CJS_Value& vRet,
                        CFX_WideString& sError) {
  if (params.size() < 1) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  CFX_WideString wideName = params[0].ToCFXWideString(pRuntime);
  CPDFSDK_InterForm* pInterForm = m_pFormFillEnv->GetInterForm();
  CPDF_InterForm* pPDFForm = pInterForm->GetInterForm();
  if (pPDFForm->CountFields(wideName) <= 0) {
    vRet.SetNull(pRuntime);
    return true;
  }

  v8::Local<v8::Object> pFieldObj =
      pRuntime->NewFxDynamicObj(CJS_Field::g_nObjDefnID);
  if (pFieldObj.IsEmpty())
    return false;

  CJS_Field* pJSField =
      static_cast<CJS_Field*>(pRuntime->GetObjectPrivate(pFieldObj));
  Field* pField = static_cast<Field*>(pJSField->GetEmbedObject());
  pField->AttachField(this, wideName);
  vRet = CJS_Value(pRuntime, pJSField);
  return true;
}

// Gets the name of the nth field in the document
bool Document::getNthFieldName(CJS_Runtime* pRuntime,
                               const std::vector<CJS_Value>& params,
                               CJS_Value& vRet,
                               CFX_WideString& sError) {
  if (params.size() != 1) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  int nIndex = params[0].ToInt(pRuntime);
  if (nIndex < 0) {
    sError = JSGetStringFromID(IDS_STRING_JSVALUEERROR);
    return false;
  }
  CPDFSDK_InterForm* pInterForm = m_pFormFillEnv->GetInterForm();
  CPDF_InterForm* pPDFForm = pInterForm->GetInterForm();
  CPDF_FormField* pField = pPDFForm->GetField(nIndex, CFX_WideString());
  if (!pField)
    return false;

  vRet = CJS_Value(pRuntime, pField->GetFullName().c_str());
  return true;
}

bool Document::importAnFDF(CJS_Runtime* pRuntime,
                           const std::vector<CJS_Value>& params,
                           CJS_Value& vRet,
                           CFX_WideString& sError) {
  // Unsafe, not supported.
  return true;
}

bool Document::importAnXFDF(CJS_Runtime* pRuntime,
                            const std::vector<CJS_Value>& params,
                            CJS_Value& vRet,
                            CFX_WideString& sError) {
  // Unsafe, not supported.
  return true;
}

bool Document::importTextData(CJS_Runtime* pRuntime,
                              const std::vector<CJS_Value>& params,
                              CJS_Value& vRet,
                              CFX_WideString& sError) {
  // Unsafe, not supported.
  return true;
}

// exports the form data and mails the resulting fdf file as an attachment to
// all recipients.
// comment: need reader supports
bool Document::mailForm(CJS_Runtime* pRuntime,
                        const std::vector<CJS_Value>& params,
                        CJS_Value& vRet,
                        CFX_WideString& sError) {
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  if (!m_pFormFillEnv->GetPermissions(FPDFPERM_EXTRACT_ACCESS)) {
    sError = JSGetStringFromID(IDS_STRING_JSNOPERMISSION);
    return false;
  }
  int iLength = params.size();
  bool bUI = iLength > 0 ? params[0].ToBool(pRuntime) : true;
  CFX_WideString cTo = iLength > 1 ? params[1].ToCFXWideString(pRuntime) : L"";
  CFX_WideString cCc = iLength > 2 ? params[2].ToCFXWideString(pRuntime) : L"";
  CFX_WideString cBcc = iLength > 3 ? params[3].ToCFXWideString(pRuntime) : L"";
  CFX_WideString cSubject =
      iLength > 4 ? params[4].ToCFXWideString(pRuntime) : L"";
  CFX_WideString cMsg = iLength > 5 ? params[5].ToCFXWideString(pRuntime) : L"";
  CPDFSDK_InterForm* pInterForm = m_pFormFillEnv->GetInterForm();
  CFX_ByteTextBuf textBuf;
  if (!pInterForm->ExportFormToFDFTextBuf(textBuf))
    return false;

  pRuntime->BeginBlock();
  CPDFSDK_FormFillEnvironment* pFormFillEnv = pRuntime->GetFormFillEnv();
  pFormFillEnv->JS_docmailForm(textBuf.GetBuffer(), textBuf.GetLength(), bUI,
                               cTo.c_str(), cSubject.c_str(), cCc.c_str(),
                               cBcc.c_str(), cMsg.c_str());
  pRuntime->EndBlock();
  return true;
}

bool Document::print(CJS_Runtime* pRuntime,
                     const std::vector<CJS_Value>& params,
                     CJS_Value& vRet,
                     CFX_WideString& sError) {
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  bool bUI = true;
  int nStart = 0;
  int nEnd = 0;
  bool bSilent = false;
  bool bShrinkToFit = false;
  bool bPrintAsImage = false;
  bool bReverse = false;
  bool bAnnotations = false;
  int nlength = params.size();
  if (nlength == 9) {
    if (params[8].GetType() == CJS_Value::VT_object) {
      v8::Local<v8::Object> pObj = params[8].ToV8Object(pRuntime);
      if (CFXJS_Engine::GetObjDefnID(pObj) ==
          CJS_PrintParamsObj::g_nObjDefnID) {
        if (CJS_Object* pJSObj = params[8].ToCJSObject(pRuntime)) {
          if (PrintParamsObj* pprintparamsObj =
                  static_cast<PrintParamsObj*>(pJSObj->GetEmbedObject())) {
            bUI = pprintparamsObj->bUI;
            nStart = pprintparamsObj->nStart;
            nEnd = pprintparamsObj->nEnd;
            bSilent = pprintparamsObj->bSilent;
            bShrinkToFit = pprintparamsObj->bShrinkToFit;
            bPrintAsImage = pprintparamsObj->bPrintAsImage;
            bReverse = pprintparamsObj->bReverse;
            bAnnotations = pprintparamsObj->bAnnotations;
          }
        }
      }
    }
  } else {
    if (nlength >= 1)
      bUI = params[0].ToBool(pRuntime);
    if (nlength >= 2)
      nStart = params[1].ToInt(pRuntime);
    if (nlength >= 3)
      nEnd = params[2].ToInt(pRuntime);
    if (nlength >= 4)
      bSilent = params[3].ToBool(pRuntime);
    if (nlength >= 5)
      bShrinkToFit = params[4].ToBool(pRuntime);
    if (nlength >= 6)
      bPrintAsImage = params[5].ToBool(pRuntime);
    if (nlength >= 7)
      bReverse = params[6].ToBool(pRuntime);
    if (nlength >= 8)
      bAnnotations = params[7].ToBool(pRuntime);
  }

  if (m_pFormFillEnv) {
    m_pFormFillEnv->JS_docprint(bUI, nStart, nEnd, bSilent, bShrinkToFit,
                                bPrintAsImage, bReverse, bAnnotations);
    return true;
  }
  return false;
}

// removes the specified field from the document.
// comment:
// note: if the filed name is not rational, adobe is dumb for it.

bool Document::removeField(CJS_Runtime* pRuntime,
                           const std::vector<CJS_Value>& params,
                           CJS_Value& vRet,
                           CFX_WideString& sError) {
  if (params.size() != 1) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  if (!(m_pFormFillEnv->GetPermissions(FPDFPERM_MODIFY) ||
        m_pFormFillEnv->GetPermissions(FPDFPERM_ANNOT_FORM))) {
    sError = JSGetStringFromID(IDS_STRING_JSNOPERMISSION);
    return false;
  }
  CFX_WideString sFieldName = params[0].ToCFXWideString(pRuntime);
  CPDFSDK_InterForm* pInterForm = m_pFormFillEnv->GetInterForm();
  std::vector<CPDFSDK_Annot::ObservedPtr> widgets;
  pInterForm->GetWidgets(sFieldName, &widgets);
  if (widgets.empty())
    return true;

  for (const auto& pAnnot : widgets) {
    CPDFSDK_Widget* pWidget = static_cast<CPDFSDK_Widget*>(pAnnot.Get());
    if (!pWidget)
      continue;

    CFX_FloatRect rcAnnot = pWidget->GetRect();
    --rcAnnot.left;
    --rcAnnot.bottom;
    ++rcAnnot.right;
    ++rcAnnot.top;

    std::vector<CFX_FloatRect> aRefresh(1, rcAnnot);
    UnderlyingPageType* pPage = pWidget->GetUnderlyingPage();
    ASSERT(pPage);

    // If there is currently no pageview associated with the page being used
    // do not create one. We may be in the process of tearing down the document
    // and creating a new pageview at this point will cause bad things.
    CPDFSDK_PageView* pPageView = m_pFormFillEnv->GetPageView(pPage, false);
    if (pPageView) {
#if PDF_ENABLE_XFA
      pPageView->DeleteAnnot(pWidget);
#endif  // PDF_ENABLE_XFA
      pPageView->UpdateRects(aRefresh);
    }
  }
  m_pFormFillEnv->SetChangeMark();

  return true;
}

// reset filed values within a document.
// comment:
// note: if the fields names r not rational, aodbe is dumb for it.

bool Document::resetForm(CJS_Runtime* pRuntime,
                         const std::vector<CJS_Value>& params,
                         CJS_Value& vRet,
                         CFX_WideString& sError) {
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  if (!(m_pFormFillEnv->GetPermissions(FPDFPERM_MODIFY) ||
        m_pFormFillEnv->GetPermissions(FPDFPERM_ANNOT_FORM) ||
        m_pFormFillEnv->GetPermissions(FPDFPERM_FILL_FORM))) {
    sError = JSGetStringFromID(IDS_STRING_JSNOPERMISSION);
    return false;
  }

  CPDFSDK_InterForm* pInterForm = m_pFormFillEnv->GetInterForm();
  CPDF_InterForm* pPDFForm = pInterForm->GetInterForm();
  CJS_Array aName;

  if (params.empty()) {
    pPDFForm->ResetForm(true);
    m_pFormFillEnv->SetChangeMark();
    return true;
  }

  switch (params[0].GetType()) {
    default:
      aName.Attach(params[0].ToV8Array(pRuntime));
      break;
    case CJS_Value::VT_string:
      aName.SetElement(pRuntime, 0, params[0]);
      break;
  }

  std::vector<CPDF_FormField*> aFields;
  for (int i = 0, isz = aName.GetLength(pRuntime); i < isz; ++i) {
    CJS_Value valElement(pRuntime);
    aName.GetElement(pRuntime, i, valElement);
    CFX_WideString swVal = valElement.ToCFXWideString(pRuntime);
    for (int j = 0, jsz = pPDFForm->CountFields(swVal); j < jsz; ++j)
      aFields.push_back(pPDFForm->GetField(j, swVal));
  }

  if (!aFields.empty()) {
    pPDFForm->ResetForm(aFields, true, true);
    m_pFormFillEnv->SetChangeMark();
  }

  return true;
}

bool Document::saveAs(CJS_Runtime* pRuntime,
                      const std::vector<CJS_Value>& params,
                      CJS_Value& vRet,
                      CFX_WideString& sError) {
  // Unsafe, not supported.
  return true;
}

bool Document::syncAnnotScan(CJS_Runtime* pRuntime,
                             const std::vector<CJS_Value>& params,
                             CJS_Value& vRet,
                             CFX_WideString& sError) {
  return true;
}

bool Document::submitForm(CJS_Runtime* pRuntime,
                          const std::vector<CJS_Value>& params,
                          CJS_Value& vRet,
                          CFX_WideString& sError) {
  int nSize = params.size();
  if (nSize < 1) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }

  CJS_Array aFields;
  CFX_WideString strURL;
  bool bFDF = true;
  bool bEmpty = false;
  CJS_Value v = params[0];
  if (v.GetType() == CJS_Value::VT_string) {
    strURL = params[0].ToCFXWideString(pRuntime);
    if (nSize > 1)
      bFDF = params[1].ToBool(pRuntime);
    if (nSize > 2)
      bEmpty = params[2].ToBool(pRuntime);
    if (nSize > 3)
      aFields.Attach(params[3].ToV8Array(pRuntime));
  } else if (v.GetType() == CJS_Value::VT_object) {
    v8::Local<v8::Object> pObj = params[0].ToV8Object(pRuntime);
    v8::Local<v8::Value> pValue = pRuntime->GetObjectProperty(pObj, L"cURL");
    if (!pValue.IsEmpty())
      strURL = CJS_Value(pRuntime, pValue).ToCFXWideString(pRuntime);

    pValue = pRuntime->GetObjectProperty(pObj, L"bFDF");
    bFDF = CJS_Value(pRuntime, pValue).ToBool(pRuntime);

    pValue = pRuntime->GetObjectProperty(pObj, L"bEmpty");
    bEmpty = CJS_Value(pRuntime, pValue).ToBool(pRuntime);

    pValue = pRuntime->GetObjectProperty(pObj, L"aFields");
    aFields.Attach(CJS_Value(pRuntime, pValue).ToV8Array(pRuntime));
  }

  CPDFSDK_InterForm* pInterForm = m_pFormFillEnv->GetInterForm();
  CPDF_InterForm* pPDFInterForm = pInterForm->GetInterForm();
  if (aFields.GetLength(pRuntime) == 0 && bEmpty) {
    if (pPDFInterForm->CheckRequiredFields(nullptr, true)) {
      pRuntime->BeginBlock();
      pInterForm->SubmitForm(strURL, false);
      pRuntime->EndBlock();
    }
    return true;
  }

  std::vector<CPDF_FormField*> fieldObjects;
  for (int i = 0, sz = aFields.GetLength(pRuntime); i < sz; ++i) {
    CJS_Value valName(pRuntime);
    aFields.GetElement(pRuntime, i, valName);

    CFX_WideString sName = valName.ToCFXWideString(pRuntime);
    CPDF_InterForm* pPDFForm = pInterForm->GetInterForm();
    for (int j = 0, jsz = pPDFForm->CountFields(sName); j < jsz; ++j) {
      CPDF_FormField* pField = pPDFForm->GetField(j, sName);
      if (!bEmpty && pField->GetValue().IsEmpty())
        continue;

      fieldObjects.push_back(pField);
    }
  }

  if (pPDFInterForm->CheckRequiredFields(&fieldObjects, true)) {
    pRuntime->BeginBlock();
    pInterForm->SubmitFields(strURL, fieldObjects, true, !bFDF);
    pRuntime->EndBlock();
  }
  return true;
}

void Document::SetFormFillEnv(CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  m_pFormFillEnv.Reset(pFormFillEnv);
}

bool Document::bookmarkRoot(CJS_Runtime* pRuntime,
                            CJS_PropValue& vp,
                            CFX_WideString& sError) {
  return true;
}

bool Document::mailDoc(CJS_Runtime* pRuntime,
                       const std::vector<CJS_Value>& params,
                       CJS_Value& vRet,
                       CFX_WideString& sError) {
  // TODO(tsepez): Check maximum number of allowed params.
  bool bUI = true;
  CFX_WideString cTo = L"";
  CFX_WideString cCc = L"";
  CFX_WideString cBcc = L"";
  CFX_WideString cSubject = L"";
  CFX_WideString cMsg = L"";

  if (params.size() >= 1)
    bUI = params[0].ToBool(pRuntime);
  if (params.size() >= 2)
    cTo = params[1].ToCFXWideString(pRuntime);
  if (params.size() >= 3)
    cCc = params[2].ToCFXWideString(pRuntime);
  if (params.size() >= 4)
    cBcc = params[3].ToCFXWideString(pRuntime);
  if (params.size() >= 5)
    cSubject = params[4].ToCFXWideString(pRuntime);
  if (params.size() >= 6)
    cMsg = params[5].ToCFXWideString(pRuntime);

  if (params.size() >= 1 && params[0].GetType() == CJS_Value::VT_object) {
    v8::Local<v8::Object> pObj = params[0].ToV8Object(pRuntime);

    v8::Local<v8::Value> pValue = pRuntime->GetObjectProperty(pObj, L"bUI");
    bUI = CJS_Value(pRuntime, pValue).ToBool(pRuntime);

    pValue = pRuntime->GetObjectProperty(pObj, L"cTo");
    cTo = CJS_Value(pRuntime, pValue).ToCFXWideString(pRuntime);

    pValue = pRuntime->GetObjectProperty(pObj, L"cCc");
    cCc = CJS_Value(pRuntime, pValue).ToCFXWideString(pRuntime);

    pValue = pRuntime->GetObjectProperty(pObj, L"cBcc");
    cBcc = CJS_Value(pRuntime, pValue).ToCFXWideString(pRuntime);

    pValue = pRuntime->GetObjectProperty(pObj, L"cSubject");
    cSubject = CJS_Value(pRuntime, pValue).ToCFXWideString(pRuntime);

    pValue = pRuntime->GetObjectProperty(pObj, L"cMsg");
    cMsg = CJS_Value(pRuntime, pValue).ToCFXWideString(pRuntime);
  }

  pRuntime->BeginBlock();
  CPDFSDK_FormFillEnvironment* pFormFillEnv = pRuntime->GetFormFillEnv();
  pFormFillEnv->JS_docmailForm(nullptr, 0, bUI, cTo.c_str(), cSubject.c_str(),
                               cCc.c_str(), cBcc.c_str(), cMsg.c_str());
  pRuntime->EndBlock();
  return true;
}

bool Document::author(CJS_Runtime* pRuntime,
                      CJS_PropValue& vp,
                      CFX_WideString& sError) {
  return getPropertyInternal(pRuntime, vp, "Author", sError);
}

bool Document::info(CJS_Runtime* pRuntime,
                    CJS_PropValue& vp,
                    CFX_WideString& sError) {
  if (vp.IsSetting()) {
    sError = JSGetStringFromID(IDS_STRING_JSREADONLY);
    return false;
  }
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  CPDF_Dictionary* pDictionary = m_pFormFillEnv->GetPDFDocument()->GetInfo();
  if (!pDictionary)
    return false;

  CFX_WideString cwAuthor = pDictionary->GetUnicodeTextFor("Author");
  CFX_WideString cwTitle = pDictionary->GetUnicodeTextFor("Title");
  CFX_WideString cwSubject = pDictionary->GetUnicodeTextFor("Subject");
  CFX_WideString cwKeywords = pDictionary->GetUnicodeTextFor("Keywords");
  CFX_WideString cwCreator = pDictionary->GetUnicodeTextFor("Creator");
  CFX_WideString cwProducer = pDictionary->GetUnicodeTextFor("Producer");
  CFX_WideString cwCreationDate =
      pDictionary->GetUnicodeTextFor("CreationDate");
  CFX_WideString cwModDate = pDictionary->GetUnicodeTextFor("ModDate");
  CFX_WideString cwTrapped = pDictionary->GetUnicodeTextFor("Trapped");

  v8::Local<v8::Object> pObj = pRuntime->NewFxDynamicObj(-1);
  pRuntime->PutObjectProperty(pObj, L"Author",
                              pRuntime->NewString(cwAuthor.AsStringC()));
  pRuntime->PutObjectProperty(pObj, L"Title",
                              pRuntime->NewString(cwTitle.AsStringC()));
  pRuntime->PutObjectProperty(pObj, L"Subject",
                              pRuntime->NewString(cwSubject.AsStringC()));
  pRuntime->PutObjectProperty(pObj, L"Keywords",
                              pRuntime->NewString(cwKeywords.AsStringC()));
  pRuntime->PutObjectProperty(pObj, L"Creator",
                              pRuntime->NewString(cwCreator.AsStringC()));
  pRuntime->PutObjectProperty(pObj, L"Producer",
                              pRuntime->NewString(cwProducer.AsStringC()));
  pRuntime->PutObjectProperty(pObj, L"CreationDate",
                              pRuntime->NewString(cwCreationDate.AsStringC()));
  pRuntime->PutObjectProperty(pObj, L"ModDate",
                              pRuntime->NewString(cwModDate.AsStringC()));
  pRuntime->PutObjectProperty(pObj, L"Trapped",
                              pRuntime->NewString(cwTrapped.AsStringC()));

  // It's to be compatible to non-standard info dictionary.
  for (const auto& it : *pDictionary) {
    const CFX_ByteString& bsKey = it.first;
    CPDF_Object* pValueObj = it.second.get();
    CFX_WideString wsKey = CFX_WideString::FromUTF8(bsKey.AsStringC());
    if (pValueObj->IsString() || pValueObj->IsName()) {
      pRuntime->PutObjectProperty(
          pObj, wsKey,
          pRuntime->NewString(pValueObj->GetUnicodeText().AsStringC()));
    } else if (pValueObj->IsNumber()) {
      pRuntime->PutObjectProperty(pObj, wsKey,
                                  pRuntime->NewNumber(pValueObj->GetNumber()));
    } else if (pValueObj->IsBoolean()) {
      pRuntime->PutObjectProperty(
          pObj, wsKey, pRuntime->NewBoolean(!!pValueObj->GetInteger()));
    }
  }
  vp << pObj;
  return true;
}

bool Document::getPropertyInternal(CJS_Runtime* pRuntime,
                                   CJS_PropValue& vp,
                                   const CFX_ByteString& propName,
                                   CFX_WideString& sError) {
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  CPDF_Dictionary* pDictionary = m_pFormFillEnv->GetPDFDocument()->GetInfo();
  if (!pDictionary)
    return false;

  if (vp.IsGetting()) {
    vp << pDictionary->GetUnicodeTextFor(propName);
  } else {
    if (!m_pFormFillEnv->GetPermissions(FPDFPERM_MODIFY)) {
      sError = JSGetStringFromID(IDS_STRING_JSNOPERMISSION);
      return false;
    }
    CFX_WideString csProperty;
    vp >> csProperty;
    pDictionary->SetNewFor<CPDF_String>(propName, PDF_EncodeText(csProperty),
                                        false);
    m_pFormFillEnv->SetChangeMark();
  }
  return true;
}

bool Document::creationDate(CJS_Runtime* pRuntime,
                            CJS_PropValue& vp,
                            CFX_WideString& sError) {
  return getPropertyInternal(pRuntime, vp, "CreationDate", sError);
}

bool Document::creator(CJS_Runtime* pRuntime,
                       CJS_PropValue& vp,
                       CFX_WideString& sError) {
  return getPropertyInternal(pRuntime, vp, "Creator", sError);
}

bool Document::delay(CJS_Runtime* pRuntime,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  if (vp.IsGetting()) {
    vp << m_bDelay;
    return true;
  }
  if (!m_pFormFillEnv->GetPermissions(FPDFPERM_MODIFY)) {
    sError = JSGetStringFromID(IDS_STRING_JSNOPERMISSION);
    return false;
  }
  vp >> m_bDelay;
  if (m_bDelay) {
    m_DelayData.clear();
    return true;
  }
  std::list<std::unique_ptr<CJS_DelayData>> DelayDataToProcess;
  DelayDataToProcess.swap(m_DelayData);
  for (const auto& pData : DelayDataToProcess)
    Field::DoDelay(m_pFormFillEnv.Get(), pData.get());

  return true;
}

bool Document::keywords(CJS_Runtime* pRuntime,
                        CJS_PropValue& vp,
                        CFX_WideString& sError) {
  return getPropertyInternal(pRuntime, vp, "Keywords", sError);
}

bool Document::modDate(CJS_Runtime* pRuntime,
                       CJS_PropValue& vp,
                       CFX_WideString& sError) {
  return getPropertyInternal(pRuntime, vp, "ModDate", sError);
}

bool Document::producer(CJS_Runtime* pRuntime,
                        CJS_PropValue& vp,
                        CFX_WideString& sError) {
  return getPropertyInternal(pRuntime, vp, "Producer", sError);
}

bool Document::subject(CJS_Runtime* pRuntime,
                       CJS_PropValue& vp,
                       CFX_WideString& sError) {
  return getPropertyInternal(pRuntime, vp, "Subject", sError);
}

bool Document::title(CJS_Runtime* pRuntime,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  if (!m_pFormFillEnv || !m_pFormFillEnv->GetUnderlyingDocument()) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  return getPropertyInternal(pRuntime, vp, "Title", sError);
}

bool Document::numPages(CJS_Runtime* pRuntime,
                        CJS_PropValue& vp,
                        CFX_WideString& sError) {
  if (vp.IsSetting()) {
    sError = JSGetStringFromID(IDS_STRING_JSREADONLY);
    return false;
  }
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  vp << m_pFormFillEnv->GetPageCount();
  return true;
}

bool Document::external(CJS_Runtime* pRuntime,
                        CJS_PropValue& vp,
                        CFX_WideString& sError) {
  // In Chrome case, should always return true.
  if (vp.IsGetting()) {
    vp << true;
  }
  return true;
}

bool Document::filesize(CJS_Runtime* pRuntime,
                        CJS_PropValue& vp,
                        CFX_WideString& sError) {
  if (vp.IsSetting()) {
    sError = JSGetStringFromID(IDS_STRING_JSREADONLY);
    return false;
  }
  vp << 0;
  return true;
}

bool Document::mouseX(CJS_Runtime* pRuntime,
                      CJS_PropValue& vp,
                      CFX_WideString& sError) {
  return true;
}

bool Document::mouseY(CJS_Runtime* pRuntime,
                      CJS_PropValue& vp,
                      CFX_WideString& sError) {
  return true;
}

bool Document::URL(CJS_Runtime* pRuntime,
                   CJS_PropValue& vp,
                   CFX_WideString& sError) {
  if (vp.IsSetting()) {
    sError = JSGetStringFromID(IDS_STRING_JSREADONLY);
    return false;
  }
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  vp << m_pFormFillEnv->JS_docGetFilePath();
  return true;
}

bool Document::baseURL(CJS_Runtime* pRuntime,
                       CJS_PropValue& vp,
                       CFX_WideString& sError) {
  if (vp.IsGetting()) {
    vp << m_cwBaseURL;
  } else {
    vp >> m_cwBaseURL;
  }
  return true;
}

bool Document::calculate(CJS_Runtime* pRuntime,
                         CJS_PropValue& vp,
                         CFX_WideString& sError) {
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  CPDFSDK_InterForm* pInterForm = m_pFormFillEnv->GetInterForm();
  if (vp.IsGetting()) {
    vp << !!pInterForm->IsCalculateEnabled();
    return true;
  }
  bool bCalculate;
  vp >> bCalculate;
  pInterForm->EnableCalculate(bCalculate);
  return true;
}

bool Document::documentFileName(CJS_Runtime* pRuntime,
                                CJS_PropValue& vp,
                                CFX_WideString& sError) {
  if (vp.IsSetting()) {
    sError = JSGetStringFromID(IDS_STRING_JSREADONLY);
    return false;
  }
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  CFX_WideString wsFilePath = m_pFormFillEnv->JS_docGetFilePath();
  int32_t i = wsFilePath.GetLength() - 1;
  for (; i >= 0; i--) {
    if (wsFilePath.GetAt(i) == L'\\' || wsFilePath.GetAt(i) == L'/')
      break;
  }
  if (i >= 0 && i < wsFilePath.GetLength() - 1) {
    vp << (wsFilePath.GetBuffer(wsFilePath.GetLength()) + i + 1);
  } else {
    vp << L"";
  }
  return true;
}

bool Document::path(CJS_Runtime* pRuntime,
                    CJS_PropValue& vp,
                    CFX_WideString& sError) {
  if (vp.IsSetting()) {
    sError = JSGetStringFromID(IDS_STRING_JSREADONLY);
    return false;
  }
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  vp << app::SysPathToPDFPath(m_pFormFillEnv->JS_docGetFilePath());
  return true;
}

bool Document::pageWindowRect(CJS_Runtime* pRuntime,
                              CJS_PropValue& vp,
                              CFX_WideString& sError) {
  return true;
}

bool Document::layout(CJS_Runtime* pRuntime,
                      CJS_PropValue& vp,
                      CFX_WideString& sError) {
  return true;
}

bool Document::addLink(CJS_Runtime* pRuntime,
                       const std::vector<CJS_Value>& params,
                       CJS_Value& vRet,
                       CFX_WideString& sError) {
  return true;
}

bool Document::closeDoc(CJS_Runtime* pRuntime,
                        const std::vector<CJS_Value>& params,
                        CJS_Value& vRet,
                        CFX_WideString& sError) {
  return true;
}

bool Document::getPageBox(CJS_Runtime* pRuntime,
                          const std::vector<CJS_Value>& params,
                          CJS_Value& vRet,
                          CFX_WideString& sError) {
  return true;
}

bool Document::getAnnot(CJS_Runtime* pRuntime,
                        const std::vector<CJS_Value>& params,
                        CJS_Value& vRet,
                        CFX_WideString& sError) {
  if (params.size() != 2) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  int nPageNo = params[0].ToInt(pRuntime);
  CFX_WideString swAnnotName = params[1].ToCFXWideString(pRuntime);
  CPDFSDK_PageView* pPageView = m_pFormFillEnv->GetPageView(nPageNo);
  if (!pPageView)
    return false;

  CPDFSDK_AnnotIteration annotIteration(pPageView, false);
  CPDFSDK_BAAnnot* pSDKBAAnnot = nullptr;
  for (const auto& pSDKAnnotCur : annotIteration) {
    CPDFSDK_BAAnnot* pBAAnnot =
        static_cast<CPDFSDK_BAAnnot*>(pSDKAnnotCur.Get());
    if (pBAAnnot && pBAAnnot->GetAnnotName() == swAnnotName) {
      pSDKBAAnnot = pBAAnnot;
      break;
    }
  }
  if (!pSDKBAAnnot)
    return false;

  v8::Local<v8::Object> pObj =
      pRuntime->NewFxDynamicObj(CJS_Annot::g_nObjDefnID);
  if (pObj.IsEmpty())
    return false;

  CJS_Annot* pJS_Annot =
      static_cast<CJS_Annot*>(pRuntime->GetObjectPrivate(pObj));
  Annot* pAnnot = static_cast<Annot*>(pJS_Annot->GetEmbedObject());
  pAnnot->SetSDKAnnot(pSDKBAAnnot);
  vRet = CJS_Value(pRuntime, pJS_Annot);
  return true;
}

bool Document::getAnnots(CJS_Runtime* pRuntime,
                         const std::vector<CJS_Value>& params,
                         CJS_Value& vRet,
                         CFX_WideString& sError) {
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  // TODO(tonikitoo): Add support supported parameters as per
  // the PDF spec.

  int nPageNo = m_pFormFillEnv->GetPageCount();
  CJS_Array annots;

  for (int i = 0; i < nPageNo; ++i) {
    CPDFSDK_PageView* pPageView = m_pFormFillEnv->GetPageView(i);
    if (!pPageView)
      return false;

    CPDFSDK_AnnotIteration annotIteration(pPageView, false);
    for (const auto& pSDKAnnotCur : annotIteration) {
      if (!pSDKAnnotCur) {
        sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
        return false;
      }
      v8::Local<v8::Object> pObj =
          pRuntime->NewFxDynamicObj(CJS_Annot::g_nObjDefnID);
      if (pObj.IsEmpty())
        return false;

      CJS_Annot* pJS_Annot =
          static_cast<CJS_Annot*>(pRuntime->GetObjectPrivate(pObj));
      Annot* pAnnot = static_cast<Annot*>(pJS_Annot->GetEmbedObject());
      pAnnot->SetSDKAnnot(static_cast<CPDFSDK_BAAnnot*>(pSDKAnnotCur.Get()));
      annots.SetElement(pRuntime, i, CJS_Value(pRuntime, pJS_Annot));
    }
  }
  vRet = CJS_Value(pRuntime, annots);
  return true;
}

bool Document::getAnnot3D(CJS_Runtime* pRuntime,
                          const std::vector<CJS_Value>& params,
                          CJS_Value& vRet,
                          CFX_WideString& sError) {
  vRet.SetNull(pRuntime);
  return true;
}

bool Document::getAnnots3D(CJS_Runtime* pRuntime,
                           const std::vector<CJS_Value>& params,
                           CJS_Value& vRet,
                           CFX_WideString& sError) {
  return true;
}

bool Document::getOCGs(CJS_Runtime* pRuntime,
                       const std::vector<CJS_Value>& params,
                       CJS_Value& vRet,
                       CFX_WideString& sError) {
  return true;
}

bool Document::getLinks(CJS_Runtime* pRuntime,
                        const std::vector<CJS_Value>& params,
                        CJS_Value& vRet,
                        CFX_WideString& sError) {
  return true;
}

bool Document::IsEnclosedInRect(CFX_FloatRect rect, CFX_FloatRect LinkRect) {
  return (rect.left <= LinkRect.left && rect.top <= LinkRect.top &&
          rect.right >= LinkRect.right && rect.bottom >= LinkRect.bottom);
}

bool Document::addIcon(CJS_Runtime* pRuntime,
                       const std::vector<CJS_Value>& params,
                       CJS_Value& vRet,
                       CFX_WideString& sError) {
  if (params.size() != 2) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }

  CFX_WideString swIconName = params[0].ToCFXWideString(pRuntime);
  if (params[1].GetType() != CJS_Value::VT_object) {
    sError = JSGetStringFromID(IDS_STRING_JSTYPEERROR);
    return false;
  }

  v8::Local<v8::Object> pJSIcon = params[1].ToV8Object(pRuntime);
  if (pRuntime->GetObjDefnID(pJSIcon) != CJS_Icon::g_nObjDefnID) {
    sError = JSGetStringFromID(IDS_STRING_JSTYPEERROR);
    return false;
  }

  if (!params[1].ToCJSObject(pRuntime)->GetEmbedObject()) {
    sError = JSGetStringFromID(IDS_STRING_JSTYPEERROR);
    return false;
  }

  m_IconNames.push_back(swIconName);
  return true;
}

bool Document::icons(CJS_Runtime* pRuntime,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  if (vp.IsSetting()) {
    sError = JSGetStringFromID(IDS_STRING_JSREADONLY);
    return false;
  }
  if (m_IconNames.empty()) {
    vp.GetJSValue()->SetNull(pRuntime);
    return true;
  }

  CJS_Array Icons;
  int i = 0;
  for (const auto& name : m_IconNames) {
    v8::Local<v8::Object> pObj =
        pRuntime->NewFxDynamicObj(CJS_Icon::g_nObjDefnID);
    if (pObj.IsEmpty())
      return false;

    CJS_Icon* pJS_Icon =
        static_cast<CJS_Icon*>(pRuntime->GetObjectPrivate(pObj));
    Icon* pIcon = static_cast<Icon*>(pJS_Icon->GetEmbedObject());
    pIcon->SetIconName(name);
    Icons.SetElement(pRuntime, i++, CJS_Value(pRuntime, pJS_Icon));
  }

  vp << Icons;
  return true;
}

bool Document::getIcon(CJS_Runtime* pRuntime,
                       const std::vector<CJS_Value>& params,
                       CJS_Value& vRet,
                       CFX_WideString& sError) {
  if (params.size() != 1) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }

  CFX_WideString swIconName = params[0].ToCFXWideString(pRuntime);
  auto it = std::find(m_IconNames.begin(), m_IconNames.end(), swIconName);
  if (it == m_IconNames.end())
    return false;

  v8::Local<v8::Object> pObj =
      pRuntime->NewFxDynamicObj(CJS_Icon::g_nObjDefnID);
  if (pObj.IsEmpty())
    return false;

  CJS_Icon* pJS_Icon = static_cast<CJS_Icon*>(pRuntime->GetObjectPrivate(pObj));
  Icon* pIcon = static_cast<Icon*>(pJS_Icon->GetEmbedObject());
  pIcon->SetIconName(*it);
  vRet = CJS_Value(pRuntime, pJS_Icon);
  return true;
}

bool Document::removeIcon(CJS_Runtime* pRuntime,
                          const std::vector<CJS_Value>& params,
                          CJS_Value& vRet,
                          CFX_WideString& sError) {
  // Unsafe, no supported.
  return true;
}

bool Document::createDataObject(CJS_Runtime* pRuntime,
                                const std::vector<CJS_Value>& params,
                                CJS_Value& vRet,
                                CFX_WideString& sError) {
  // Unsafe, not implemented.
  return true;
}

bool Document::media(CJS_Runtime* pRuntime,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  return true;
}

bool Document::calculateNow(CJS_Runtime* pRuntime,
                            const std::vector<CJS_Value>& params,
                            CJS_Value& vRet,
                            CFX_WideString& sError) {
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  if (!(m_pFormFillEnv->GetPermissions(FPDFPERM_MODIFY) ||
        m_pFormFillEnv->GetPermissions(FPDFPERM_ANNOT_FORM) ||
        m_pFormFillEnv->GetPermissions(FPDFPERM_FILL_FORM))) {
    sError = JSGetStringFromID(IDS_STRING_JSNOPERMISSION);
    return false;
  }
  m_pFormFillEnv->GetInterForm()->OnCalculate();
  return true;
}

bool Document::Collab(CJS_Runtime* pRuntime,
                      CJS_PropValue& vp,
                      CFX_WideString& sError) {
  return true;
}

bool Document::getPageNthWord(CJS_Runtime* pRuntime,
                              const std::vector<CJS_Value>& params,
                              CJS_Value& vRet,
                              CFX_WideString& sError) {
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  if (!m_pFormFillEnv->GetPermissions(FPDFPERM_EXTRACT_ACCESS)) {
    sError = JSGetStringFromID(IDS_STRING_JSNOPERMISSION);
    return false;
  }

  // TODO(tsepez): check maximum allowable params.

  int nPageNo = params.size() > 0 ? params[0].ToInt(pRuntime) : 0;
  int nWordNo = params.size() > 1 ? params[1].ToInt(pRuntime) : 0;
  bool bStrip = params.size() > 2 ? params[2].ToBool(pRuntime) : true;

  CPDF_Document* pDocument = m_pFormFillEnv->GetPDFDocument();
  if (!pDocument)
    return false;

  if (nPageNo < 0 || nPageNo >= pDocument->GetPageCount()) {
    sError = JSGetStringFromID(IDS_STRING_JSVALUEERROR);
    return false;
  }

  CPDF_Dictionary* pPageDict = pDocument->GetPage(nPageNo);
  if (!pPageDict)
    return false;

  CPDF_Page page(pDocument, pPageDict, true);
  page.ParseContent();

  int nWords = 0;
  CFX_WideString swRet;
  for (auto& pPageObj : *page.GetPageObjectList()) {
    if (pPageObj->IsText()) {
      CPDF_TextObject* pTextObj = pPageObj->AsText();
      int nObjWords = CountWords(pTextObj);
      if (nWords + nObjWords >= nWordNo) {
        swRet = GetObjWordStr(pTextObj, nWordNo - nWords);
        break;
      }
      nWords += nObjWords;
    }
  }

  if (bStrip) {
    swRet.TrimLeft();
    swRet.TrimRight();
  }

  vRet = CJS_Value(pRuntime, swRet.c_str());
  return true;
}

bool Document::getPageNthWordQuads(CJS_Runtime* pRuntime,
                                   const std::vector<CJS_Value>& params,
                                   CJS_Value& vRet,
                                   CFX_WideString& sError) {
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  if (!m_pFormFillEnv->GetPermissions(FPDFPERM_EXTRACT_ACCESS)) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  return false;
}

bool Document::getPageNumWords(CJS_Runtime* pRuntime,
                               const std::vector<CJS_Value>& params,
                               CJS_Value& vRet,
                               CFX_WideString& sError) {
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  if (!m_pFormFillEnv->GetPermissions(FPDFPERM_EXTRACT_ACCESS)) {
    sError = JSGetStringFromID(IDS_STRING_JSNOPERMISSION);
    return false;
  }
  int nPageNo = params.size() > 0 ? params[0].ToInt(pRuntime) : 0;
  CPDF_Document* pDocument = m_pFormFillEnv->GetPDFDocument();
  if (nPageNo < 0 || nPageNo >= pDocument->GetPageCount()) {
    sError = JSGetStringFromID(IDS_STRING_JSVALUEERROR);
    return false;
  }

  CPDF_Dictionary* pPageDict = pDocument->GetPage(nPageNo);
  if (!pPageDict)
    return false;

  CPDF_Page page(pDocument, pPageDict, true);
  page.ParseContent();

  int nWords = 0;
  for (auto& pPageObj : *page.GetPageObjectList()) {
    if (pPageObj->IsText())
      nWords += CountWords(pPageObj->AsText());
  }

  vRet = CJS_Value(pRuntime, nWords);
  return true;
}

bool Document::getPrintParams(CJS_Runtime* pRuntime,
                              const std::vector<CJS_Value>& params,
                              CJS_Value& vRet,
                              CFX_WideString& sError) {
  v8::Local<v8::Object> pRetObj =
      pRuntime->NewFxDynamicObj(CJS_PrintParamsObj::g_nObjDefnID);
  if (pRetObj.IsEmpty())
    return false;

  // Not implemented yet.

  vRet = CJS_Value(pRuntime, pRetObj);
  return true;
}

#define ISLATINWORD(u) (u != 0x20 && u <= 0x28FF)

int Document::CountWords(CPDF_TextObject* pTextObj) {
  if (!pTextObj)
    return 0;

  int nWords = 0;

  CPDF_Font* pFont = pTextObj->GetFont();
  if (!pFont)
    return 0;

  bool bIsLatin = false;

  for (int i = 0, sz = pTextObj->CountChars(); i < sz; i++) {
    uint32_t charcode = CPDF_Font::kInvalidCharCode;
    FX_FLOAT kerning;

    pTextObj->GetCharInfo(i, &charcode, &kerning);
    CFX_WideString swUnicode = pFont->UnicodeFromCharCode(charcode);

    uint16_t unicode = 0;
    if (swUnicode.GetLength() > 0)
      unicode = swUnicode[0];

    if (ISLATINWORD(unicode) && bIsLatin)
      continue;

    bIsLatin = ISLATINWORD(unicode);
    if (unicode != 0x20)
      nWords++;
  }

  return nWords;
}

CFX_WideString Document::GetObjWordStr(CPDF_TextObject* pTextObj,
                                       int nWordIndex) {
  CFX_WideString swRet;

  CPDF_Font* pFont = pTextObj->GetFont();
  if (!pFont)
    return L"";

  int nWords = 0;
  bool bIsLatin = false;

  for (int i = 0, sz = pTextObj->CountChars(); i < sz; i++) {
    uint32_t charcode = CPDF_Font::kInvalidCharCode;
    FX_FLOAT kerning;

    pTextObj->GetCharInfo(i, &charcode, &kerning);
    CFX_WideString swUnicode = pFont->UnicodeFromCharCode(charcode);

    uint16_t unicode = 0;
    if (swUnicode.GetLength() > 0)
      unicode = swUnicode[0];

    if (ISLATINWORD(unicode) && bIsLatin) {
    } else {
      bIsLatin = ISLATINWORD(unicode);
      if (unicode != 0x20)
        nWords++;
    }

    if (nWords - 1 == nWordIndex)
      swRet += unicode;
  }

  return swRet;
}

bool Document::zoom(CJS_Runtime* pRuntime,
                    CJS_PropValue& vp,
                    CFX_WideString& sError) {
  return true;
}

/**
(none,  NoVary)
(fitP,  FitPage)
(fitW,  FitWidth)
(fitH,  FitHeight)
(fitV,  FitVisibleWidth)
(pref,  Preferred)
(refW,  ReflowWidth)
*/

bool Document::zoomType(CJS_Runtime* pRuntime,
                        CJS_PropValue& vp,
                        CFX_WideString& sError) {
  return true;
}

bool Document::deletePages(CJS_Runtime* pRuntime,
                           const std::vector<CJS_Value>& params,
                           CJS_Value& vRet,
                           CFX_WideString& sError) {
  // Unsafe, no supported.
  return true;
}

bool Document::extractPages(CJS_Runtime* pRuntime,
                            const std::vector<CJS_Value>& params,
                            CJS_Value& vRet,
                            CFX_WideString& sError) {
  // Unsafe, not supported.
  return true;
}

bool Document::insertPages(CJS_Runtime* pRuntime,
                           const std::vector<CJS_Value>& params,
                           CJS_Value& vRet,
                           CFX_WideString& sError) {
  // Unsafe, not supported.
  return true;
}

bool Document::replacePages(CJS_Runtime* pRuntime,
                            const std::vector<CJS_Value>& params,
                            CJS_Value& vRet,
                            CFX_WideString& sError) {
  // Unsafe, not supported.
  return true;
}

bool Document::getURL(CJS_Runtime* pRuntime,
                      const std::vector<CJS_Value>& params,
                      CJS_Value& vRet,
                      CFX_WideString& sError) {
  // Unsafe, not supported.
  return true;
}

bool Document::gotoNamedDest(CJS_Runtime* pRuntime,
                             const std::vector<CJS_Value>& params,
                             CJS_Value& vRet,
                             CFX_WideString& sError) {
  if (params.size() != 1) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }
  if (!m_pFormFillEnv) {
    sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
    return false;
  }
  CFX_WideString wideName = params[0].ToCFXWideString(pRuntime);
  CFX_ByteString utf8Name = wideName.UTF8Encode();
  CPDF_Document* pDocument = m_pFormFillEnv->GetPDFDocument();
  if (!pDocument)
    return false;

  CPDF_NameTree nameTree(pDocument, "Dests");
  CPDF_Array* destArray = nameTree.LookupNamedDest(pDocument, utf8Name);
  if (!destArray)
    return false;

  CPDF_Dest dest(destArray);
  const CPDF_Array* arrayObject = ToArray(dest.GetObject());

  std::unique_ptr<float[]> scrollPositionArray;
  int scrollPositionArraySize = 0;

  if (arrayObject) {
    scrollPositionArray.reset(new float[arrayObject->GetCount()]);
    int j = 0;
    for (size_t i = 2; i < arrayObject->GetCount(); i++)
      scrollPositionArray[j++] = arrayObject->GetFloatAt(i);
    scrollPositionArraySize = j;
  }

  pRuntime->BeginBlock();
  m_pFormFillEnv->DoGoToAction(dest.GetPageIndex(pDocument), dest.GetZoomMode(),
                               scrollPositionArray.get(),
                               scrollPositionArraySize);
  pRuntime->EndBlock();

  return true;
}

void Document::AddDelayData(CJS_DelayData* pData) {
  m_DelayData.push_back(std::unique_ptr<CJS_DelayData>(pData));
}

void Document::DoFieldDelay(const CFX_WideString& sFieldName,
                            int nControlIndex) {
  std::vector<std::unique_ptr<CJS_DelayData>> DelayDataForFieldAndControlIndex;
  auto iter = m_DelayData.begin();
  while (iter != m_DelayData.end()) {
    auto old = iter++;
    if ((*old)->sFieldName == sFieldName &&
        (*old)->nControlIndex == nControlIndex) {
      DelayDataForFieldAndControlIndex.push_back(std::move(*old));
      m_DelayData.erase(old);
    }
  }

  for (const auto& pData : DelayDataForFieldAndControlIndex)
    Field::DoDelay(m_pFormFillEnv.Get(), pData.get());
}

CJS_Document* Document::GetCJSDoc() const {
  return static_cast<CJS_Document*>(m_pJSObject);
}
