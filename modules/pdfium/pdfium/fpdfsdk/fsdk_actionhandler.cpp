// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/fsdk_actionhandler.h"

#include <set>

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfdoc/cpdf_formfield.h"
#include "core/fpdfdoc/cpdf_interform.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_interform.h"
#include "fpdfsdk/fsdk_define.h"
#include "fpdfsdk/javascript/ijs_event_context.h"
#include "fpdfsdk/javascript/ijs_runtime.h"
#include "third_party/base/stl_util.h"

bool CPDFSDK_ActionHandler::DoAction_DocOpen(
    const CPDF_Action& action,
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  std::set<CPDF_Dictionary*> visited;
  return ExecuteDocumentOpenAction(action, pFormFillEnv, &visited);
}

bool CPDFSDK_ActionHandler::DoAction_JavaScript(
    const CPDF_Action& JsAction,
    CFX_WideString csJSName,
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  if (JsAction.GetType() == CPDF_Action::JavaScript) {
    CFX_WideString swJS = JsAction.GetJavaScript();
    if (!swJS.IsEmpty()) {
      RunDocumentOpenJavaScript(pFormFillEnv, csJSName, swJS);
      return true;
    }
  }

  return false;
}

bool CPDFSDK_ActionHandler::DoAction_FieldJavaScript(
    const CPDF_Action& JsAction,
    CPDF_AAction::AActionType type,
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    CPDF_FormField* pFormField,
    PDFSDK_FieldAction& data) {
  ASSERT(pFormFillEnv);
  if (pFormFillEnv->IsJSInitiated() &&
      JsAction.GetType() == CPDF_Action::JavaScript) {
    CFX_WideString swJS = JsAction.GetJavaScript();
    if (!swJS.IsEmpty()) {
      RunFieldJavaScript(pFormFillEnv, pFormField, type, data, swJS);
      return true;
    }
  }
  return false;
}

bool CPDFSDK_ActionHandler::DoAction_Page(
    const CPDF_Action& action,
    enum CPDF_AAction::AActionType eType,
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  std::set<CPDF_Dictionary*> visited;
  return ExecuteDocumentPageAction(action, eType, pFormFillEnv, &visited);
}

bool CPDFSDK_ActionHandler::DoAction_Document(
    const CPDF_Action& action,
    enum CPDF_AAction::AActionType eType,
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  std::set<CPDF_Dictionary*> visited;
  return ExecuteDocumentPageAction(action, eType, pFormFillEnv, &visited);
}

bool CPDFSDK_ActionHandler::DoAction_BookMark(
    CPDF_Bookmark* pBookMark,
    const CPDF_Action& action,
    CPDF_AAction::AActionType type,
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  std::set<CPDF_Dictionary*> visited;
  return ExecuteBookMark(action, pFormFillEnv, pBookMark, &visited);
}

bool CPDFSDK_ActionHandler::DoAction_Screen(
    const CPDF_Action& action,
    CPDF_AAction::AActionType type,
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    CPDFSDK_Annot* pScreen) {
  std::set<CPDF_Dictionary*> visited;
  return ExecuteScreenAction(action, type, pFormFillEnv, pScreen, &visited);
}

bool CPDFSDK_ActionHandler::DoAction_Link(
    const CPDF_Action& action,
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  std::set<CPDF_Dictionary*> visited;
  return ExecuteLinkAction(action, pFormFillEnv, &visited);
}

bool CPDFSDK_ActionHandler::DoAction_Field(
    const CPDF_Action& action,
    CPDF_AAction::AActionType type,
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    CPDF_FormField* pFormField,
    PDFSDK_FieldAction& data) {
  std::set<CPDF_Dictionary*> visited;
  return ExecuteFieldAction(action, type, pFormFillEnv, pFormField, data,
                            &visited);
}

bool CPDFSDK_ActionHandler::ExecuteDocumentOpenAction(
    const CPDF_Action& action,
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    std::set<CPDF_Dictionary*>* visited) {
  CPDF_Dictionary* pDict = action.GetDict();
  if (pdfium::ContainsKey(*visited, pDict))
    return false;

  visited->insert(pDict);

  ASSERT(pFormFillEnv);
  if (action.GetType() == CPDF_Action::JavaScript) {
    if (pFormFillEnv->IsJSInitiated()) {
      CFX_WideString swJS = action.GetJavaScript();
      if (!swJS.IsEmpty()) {
        RunDocumentOpenJavaScript(pFormFillEnv, L"", swJS);
      }
    }
  } else {
    DoAction_NoJs(action, pFormFillEnv);
  }

  for (int32_t i = 0, sz = action.GetSubActionsCount(); i < sz; i++) {
    CPDF_Action subaction = action.GetSubAction(i);
    if (!ExecuteDocumentOpenAction(subaction, pFormFillEnv, visited))
      return false;
  }

  return true;
}

bool CPDFSDK_ActionHandler::ExecuteLinkAction(
    const CPDF_Action& action,
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    std::set<CPDF_Dictionary*>* visited) {
  CPDF_Dictionary* pDict = action.GetDict();
  if (pdfium::ContainsKey(*visited, pDict))
    return false;

  visited->insert(pDict);

  ASSERT(pFormFillEnv);
  if (action.GetType() == CPDF_Action::JavaScript) {
    if (pFormFillEnv->IsJSInitiated()) {
      CFX_WideString swJS = action.GetJavaScript();
      if (!swJS.IsEmpty()) {
        IJS_Runtime* pRuntime = pFormFillEnv->GetJSRuntime();
        IJS_EventContext* pContext = pRuntime->NewEventContext();
        pContext->OnLink_MouseUp(pFormFillEnv);

        CFX_WideString csInfo;
        bool bRet = pContext->RunScript(swJS, &csInfo);
        pRuntime->ReleaseEventContext(pContext);
        if (!bRet) {
          // FIXME: return error.
        }
      }
    }
  } else {
    DoAction_NoJs(action, pFormFillEnv);
  }

  for (int32_t i = 0, sz = action.GetSubActionsCount(); i < sz; i++) {
    CPDF_Action subaction = action.GetSubAction(i);
    if (!ExecuteLinkAction(subaction, pFormFillEnv, visited))
      return false;
  }

  return true;
}

bool CPDFSDK_ActionHandler::ExecuteDocumentPageAction(
    const CPDF_Action& action,
    CPDF_AAction::AActionType type,
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    std::set<CPDF_Dictionary*>* visited) {
  CPDF_Dictionary* pDict = action.GetDict();
  if (pdfium::ContainsKey(*visited, pDict))
    return false;

  visited->insert(pDict);

  ASSERT(pFormFillEnv);
  if (action.GetType() == CPDF_Action::JavaScript) {
    if (pFormFillEnv->IsJSInitiated()) {
      CFX_WideString swJS = action.GetJavaScript();
      if (!swJS.IsEmpty()) {
        RunDocumentPageJavaScript(pFormFillEnv, type, swJS);
      }
    }
  } else {
    DoAction_NoJs(action, pFormFillEnv);
  }

  if (!IsValidDocView(pFormFillEnv))
    return false;

  for (int32_t i = 0, sz = action.GetSubActionsCount(); i < sz; i++) {
    CPDF_Action subaction = action.GetSubAction(i);
    if (!ExecuteDocumentPageAction(subaction, type, pFormFillEnv, visited))
      return false;
  }

  return true;
}

bool CPDFSDK_ActionHandler::IsValidField(
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    CPDF_Dictionary* pFieldDict) {
  ASSERT(pFieldDict);

  CPDFSDK_InterForm* pInterForm = pFormFillEnv->GetInterForm();
  CPDF_InterForm* pPDFInterForm = pInterForm->GetInterForm();
  return !!pPDFInterForm->GetFieldByDict(pFieldDict);
}

bool CPDFSDK_ActionHandler::ExecuteFieldAction(
    const CPDF_Action& action,
    CPDF_AAction::AActionType type,
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    CPDF_FormField* pFormField,
    PDFSDK_FieldAction& data,
    std::set<CPDF_Dictionary*>* visited) {
  CPDF_Dictionary* pDict = action.GetDict();
  if (pdfium::ContainsKey(*visited, pDict))
    return false;

  visited->insert(pDict);

  ASSERT(pFormFillEnv);
  if (action.GetType() == CPDF_Action::JavaScript) {
    if (pFormFillEnv->IsJSInitiated()) {
      CFX_WideString swJS = action.GetJavaScript();
      if (!swJS.IsEmpty()) {
        RunFieldJavaScript(pFormFillEnv, pFormField, type, data, swJS);
        if (!IsValidField(pFormFillEnv, pFormField->GetFieldDict()))
          return false;
      }
    }
  } else {
    DoAction_NoJs(action, pFormFillEnv);
  }

  for (int32_t i = 0, sz = action.GetSubActionsCount(); i < sz; i++) {
    CPDF_Action subaction = action.GetSubAction(i);
    if (!ExecuteFieldAction(subaction, type, pFormFillEnv, pFormField, data,
                            visited))
      return false;
  }

  return true;
}

bool CPDFSDK_ActionHandler::ExecuteScreenAction(
    const CPDF_Action& action,
    CPDF_AAction::AActionType type,
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    CPDFSDK_Annot* pScreen,
    std::set<CPDF_Dictionary*>* visited) {
  CPDF_Dictionary* pDict = action.GetDict();
  if (pdfium::ContainsKey(*visited, pDict))
    return false;

  visited->insert(pDict);

  ASSERT(pFormFillEnv);
  if (action.GetType() == CPDF_Action::JavaScript) {
    if (pFormFillEnv->IsJSInitiated()) {
      CFX_WideString swJS = action.GetJavaScript();
      if (!swJS.IsEmpty()) {
        IJS_Runtime* pRuntime = pFormFillEnv->GetJSRuntime();
        IJS_EventContext* pContext = pRuntime->NewEventContext();
        CFX_WideString csInfo;
        bool bRet = pContext->RunScript(swJS, &csInfo);
        pRuntime->ReleaseEventContext(pContext);
        if (!bRet) {
          // FIXME: return error.
        }
      }
    }
  } else {
    DoAction_NoJs(action, pFormFillEnv);
  }

  for (int32_t i = 0, sz = action.GetSubActionsCount(); i < sz; i++) {
    CPDF_Action subaction = action.GetSubAction(i);
    if (!ExecuteScreenAction(subaction, type, pFormFillEnv, pScreen, visited))
      return false;
  }

  return true;
}

bool CPDFSDK_ActionHandler::ExecuteBookMark(
    const CPDF_Action& action,
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    CPDF_Bookmark* pBookmark,
    std::set<CPDF_Dictionary*>* visited) {
  CPDF_Dictionary* pDict = action.GetDict();
  if (pdfium::ContainsKey(*visited, pDict))
    return false;

  visited->insert(pDict);

  ASSERT(pFormFillEnv);
  if (action.GetType() == CPDF_Action::JavaScript) {
    if (pFormFillEnv->IsJSInitiated()) {
      CFX_WideString swJS = action.GetJavaScript();
      if (!swJS.IsEmpty()) {
        IJS_Runtime* pRuntime = pFormFillEnv->GetJSRuntime();
        IJS_EventContext* pContext = pRuntime->NewEventContext();
        pContext->OnBookmark_MouseUp(pBookmark);

        CFX_WideString csInfo;
        bool bRet = pContext->RunScript(swJS, &csInfo);
        pRuntime->ReleaseEventContext(pContext);
        if (!bRet) {
          // FIXME: return error.
        }
      }
    }
  } else {
    DoAction_NoJs(action, pFormFillEnv);
  }

  for (int32_t i = 0, sz = action.GetSubActionsCount(); i < sz; i++) {
    CPDF_Action subaction = action.GetSubAction(i);
    if (!ExecuteBookMark(subaction, pFormFillEnv, pBookmark, visited))
      return false;
  }

  return true;
}

void CPDFSDK_ActionHandler::DoAction_NoJs(
    const CPDF_Action& action,
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  ASSERT(pFormFillEnv);

  switch (action.GetType()) {
    case CPDF_Action::GoTo:
      DoAction_GoTo(pFormFillEnv, action);
      break;
    case CPDF_Action::GoToR:
      DoAction_GoToR(pFormFillEnv, action);
      break;
    case CPDF_Action::GoToE:
      break;
    case CPDF_Action::Launch:
      DoAction_Launch(pFormFillEnv, action);
      break;
    case CPDF_Action::Thread:
      break;
    case CPDF_Action::URI:
      DoAction_URI(pFormFillEnv, action);
      break;
    case CPDF_Action::Sound:
      break;
    case CPDF_Action::Movie:
      break;
    case CPDF_Action::Hide:
      DoAction_Hide(action, pFormFillEnv);
      break;
    case CPDF_Action::Named:
      DoAction_Named(pFormFillEnv, action);
      break;
    case CPDF_Action::SubmitForm:
      DoAction_SubmitForm(action, pFormFillEnv);
      break;
    case CPDF_Action::ResetForm:
      DoAction_ResetForm(action, pFormFillEnv);
      break;
    case CPDF_Action::ImportData:
      DoAction_ImportData(action, pFormFillEnv);
      break;
    case CPDF_Action::JavaScript:
      ASSERT(false);
      break;
    case CPDF_Action::SetOCGState:
      DoAction_SetOCGState(pFormFillEnv, action);
      break;
    case CPDF_Action::Rendition:
      break;
    case CPDF_Action::Trans:
      break;
    case CPDF_Action::GoTo3DView:
      break;
    default:
      break;
  }
}

bool CPDFSDK_ActionHandler::IsValidDocView(
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  ASSERT(pFormFillEnv);
  return true;
}

void CPDFSDK_ActionHandler::DoAction_GoTo(
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    const CPDF_Action& action) {
  ASSERT(action.GetDict());

  CPDF_Document* pPDFDocument = pFormFillEnv->GetPDFDocument();
  ASSERT(pPDFDocument);

  CPDF_Dest MyDest = action.GetDest(pPDFDocument);
  int nPageIndex = MyDest.GetPageIndex(pPDFDocument);
  int nFitType = MyDest.GetZoomMode();
  const CPDF_Array* pMyArray = ToArray(MyDest.GetObject());
  float* pPosAry = nullptr;
  int sizeOfAry = 0;
  if (pMyArray) {
    pPosAry = new float[pMyArray->GetCount()];
    int j = 0;
    for (size_t i = 2; i < pMyArray->GetCount(); i++) {
      pPosAry[j++] = pMyArray->GetFloatAt(i);
    }
    sizeOfAry = j;
  }

  pFormFillEnv->DoGoToAction(nPageIndex, nFitType, pPosAry, sizeOfAry);
  delete[] pPosAry;
}

void CPDFSDK_ActionHandler::DoAction_GoToR(
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    const CPDF_Action& action) {}

void CPDFSDK_ActionHandler::DoAction_Launch(
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    const CPDF_Action& action) {}

void CPDFSDK_ActionHandler::DoAction_URI(
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    const CPDF_Action& action) {
  ASSERT(action.GetDict());

  CFX_ByteString sURI = action.GetURI(pFormFillEnv->GetPDFDocument());
  pFormFillEnv->DoURIAction(sURI.c_str());
}

void CPDFSDK_ActionHandler::DoAction_Named(
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    const CPDF_Action& action) {
  ASSERT(action.GetDict());

  CFX_ByteString csName = action.GetNamedAction();
  pFormFillEnv->ExecuteNamedAction(csName.c_str());
}

void CPDFSDK_ActionHandler::DoAction_SetOCGState(
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    const CPDF_Action& action) {}

void CPDFSDK_ActionHandler::RunFieldJavaScript(
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    CPDF_FormField* pFormField,
    CPDF_AAction::AActionType type,
    PDFSDK_FieldAction& data,
    const CFX_WideString& script) {
  ASSERT(type != CPDF_AAction::Calculate);
  ASSERT(type != CPDF_AAction::Format);

  IJS_Runtime* pRuntime = pFormFillEnv->GetJSRuntime();
  IJS_EventContext* pContext = pRuntime->NewEventContext();
  switch (type) {
    case CPDF_AAction::CursorEnter:
      pContext->OnField_MouseEnter(data.bModifier, data.bShift, pFormField);
      break;
    case CPDF_AAction::CursorExit:
      pContext->OnField_MouseExit(data.bModifier, data.bShift, pFormField);
      break;
    case CPDF_AAction::ButtonDown:
      pContext->OnField_MouseDown(data.bModifier, data.bShift, pFormField);
      break;
    case CPDF_AAction::ButtonUp:
      pContext->OnField_MouseUp(data.bModifier, data.bShift, pFormField);
      break;
    case CPDF_AAction::GetFocus:
      pContext->OnField_Focus(data.bModifier, data.bShift, pFormField,
                              data.sValue);
      break;
    case CPDF_AAction::LoseFocus:
      pContext->OnField_Blur(data.bModifier, data.bShift, pFormField,
                             data.sValue);
      break;
    case CPDF_AAction::KeyStroke:
      pContext->OnField_Keystroke(data.sChange, data.sChangeEx, data.bKeyDown,
                                  data.bModifier, data.nSelEnd, data.nSelStart,
                                  data.bShift, pFormField, data.sValue,
                                  data.bWillCommit, data.bFieldFull, data.bRC);
      break;
    case CPDF_AAction::Validate:
      pContext->OnField_Validate(data.sChange, data.sChangeEx, data.bKeyDown,
                                 data.bModifier, data.bShift, pFormField,
                                 data.sValue, data.bRC);
      break;
    default:
      ASSERT(false);
      break;
  }

  CFX_WideString csInfo;
  bool bRet = pContext->RunScript(script, &csInfo);
  pRuntime->ReleaseEventContext(pContext);
  if (!bRet) {
    // FIXME: return error.
  }
}

void CPDFSDK_ActionHandler::RunDocumentOpenJavaScript(
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    const CFX_WideString& sScriptName,
    const CFX_WideString& script) {
  IJS_Runtime* pRuntime = pFormFillEnv->GetJSRuntime();
  IJS_EventContext* pContext = pRuntime->NewEventContext();
  pContext->OnDoc_Open(pFormFillEnv, sScriptName);

  CFX_WideString csInfo;
  bool bRet = pContext->RunScript(script, &csInfo);
  pRuntime->ReleaseEventContext(pContext);
  if (!bRet) {
    // FIXME: return error.
  }
}

void CPDFSDK_ActionHandler::RunDocumentPageJavaScript(
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    CPDF_AAction::AActionType type,
    const CFX_WideString& script) {
  IJS_Runtime* pRuntime = pFormFillEnv->GetJSRuntime();
  IJS_EventContext* pContext = pRuntime->NewEventContext();
  switch (type) {
    case CPDF_AAction::OpenPage:
      pContext->OnPage_Open(pFormFillEnv);
      break;
    case CPDF_AAction::ClosePage:
      pContext->OnPage_Close(pFormFillEnv);
      break;
    case CPDF_AAction::CloseDocument:
      pContext->OnDoc_WillClose(pFormFillEnv);
      break;
    case CPDF_AAction::SaveDocument:
      pContext->OnDoc_WillSave(pFormFillEnv);
      break;
    case CPDF_AAction::DocumentSaved:
      pContext->OnDoc_DidSave(pFormFillEnv);
      break;
    case CPDF_AAction::PrintDocument:
      pContext->OnDoc_WillPrint(pFormFillEnv);
      break;
    case CPDF_AAction::DocumentPrinted:
      pContext->OnDoc_DidPrint(pFormFillEnv);
      break;
    case CPDF_AAction::PageVisible:
      pContext->OnPage_InView(pFormFillEnv);
      break;
    case CPDF_AAction::PageInvisible:
      pContext->OnPage_OutView(pFormFillEnv);
      break;
    default:
      ASSERT(false);
      break;
  }

  CFX_WideString csInfo;
  bool bRet = pContext->RunScript(script, &csInfo);
  pRuntime->ReleaseEventContext(pContext);
  if (!bRet) {
    // FIXME: return error.
  }
}

bool CPDFSDK_ActionHandler::DoAction_Hide(
    const CPDF_Action& action,
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  CPDFSDK_InterForm* pInterForm = pFormFillEnv->GetInterForm();
  if (pInterForm->DoAction_Hide(action)) {
    pFormFillEnv->SetChangeMark();
    return true;
  }

  return false;
}

bool CPDFSDK_ActionHandler::DoAction_SubmitForm(
    const CPDF_Action& action,
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  CPDFSDK_InterForm* pInterForm = pFormFillEnv->GetInterForm();
  return pInterForm->DoAction_SubmitForm(action);
}

bool CPDFSDK_ActionHandler::DoAction_ResetForm(
    const CPDF_Action& action,
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  CPDFSDK_InterForm* pInterForm = pFormFillEnv->GetInterForm();
  return pInterForm->DoAction_ResetForm(action);
}

bool CPDFSDK_ActionHandler::DoAction_ImportData(
    const CPDF_Action& action,
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  CPDFSDK_InterForm* pInterForm = pFormFillEnv->GetInterForm();
  if (pInterForm->DoAction_ImportData(action)) {
    pFormFillEnv->SetChangeMark();
    return true;
  }

  return false;
}
