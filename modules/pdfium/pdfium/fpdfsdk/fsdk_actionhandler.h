// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_FSDK_ACTIONHANDLER_H_
#define FPDFSDK_FSDK_ACTIONHANDLER_H_

#include <memory>
#include <set>

#include "core/fpdfdoc/cpdf_aaction.h"
#include "core/fpdfdoc/cpdf_action.h"
#include "core/fxcrt/fx_string.h"
#include "fpdfsdk/pdfsdk_fieldaction.h"

class CPDFSDK_Annot;
class CPDFSDK_FormFillEnvironment;
class CPDF_Bookmark;
class CPDF_Dictionary;
class CPDF_FormField;

class CPDFSDK_ActionHandler {
 public:
  bool DoAction_DocOpen(const CPDF_Action& action,
                        CPDFSDK_FormFillEnvironment* pFormFillEnv);
  bool DoAction_JavaScript(const CPDF_Action& JsAction,
                           CFX_WideString csJSName,
                           CPDFSDK_FormFillEnvironment* pFormFillEnv);
  bool DoAction_Page(const CPDF_Action& action,
                     enum CPDF_AAction::AActionType eType,
                     CPDFSDK_FormFillEnvironment* pFormFillEnv);
  bool DoAction_Document(const CPDF_Action& action,
                         enum CPDF_AAction::AActionType eType,
                         CPDFSDK_FormFillEnvironment* pFormFillEnv);
  bool DoAction_BookMark(CPDF_Bookmark* pBookMark,
                         const CPDF_Action& action,
                         CPDF_AAction::AActionType type,
                         CPDFSDK_FormFillEnvironment* pFormFillEnv);
  bool DoAction_Screen(const CPDF_Action& action,
                       CPDF_AAction::AActionType type,
                       CPDFSDK_FormFillEnvironment* pFormFillEnv,
                       CPDFSDK_Annot* pScreen);
  bool DoAction_Link(const CPDF_Action& action,
                     CPDFSDK_FormFillEnvironment* pFormFillEnv);
  bool DoAction_Field(const CPDF_Action& action,
                      CPDF_AAction::AActionType type,
                      CPDFSDK_FormFillEnvironment* pFormFillEnv,
                      CPDF_FormField* pFormField,
                      PDFSDK_FieldAction& data);
  bool DoAction_FieldJavaScript(const CPDF_Action& JsAction,
                                CPDF_AAction::AActionType type,
                                CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                CPDF_FormField* pFormField,
                                PDFSDK_FieldAction& data);

 private:
  bool ExecuteDocumentOpenAction(const CPDF_Action& action,
                                 CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                 std::set<CPDF_Dictionary*>* visited);
  bool ExecuteDocumentPageAction(const CPDF_Action& action,
                                 CPDF_AAction::AActionType type,
                                 CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                 std::set<CPDF_Dictionary*>* visited);
  bool ExecuteFieldAction(const CPDF_Action& action,
                          CPDF_AAction::AActionType type,
                          CPDFSDK_FormFillEnvironment* pFormFillEnv,
                          CPDF_FormField* pFormField,
                          PDFSDK_FieldAction& data,
                          std::set<CPDF_Dictionary*>* visited);
  bool ExecuteScreenAction(const CPDF_Action& action,
                           CPDF_AAction::AActionType type,
                           CPDFSDK_FormFillEnvironment* pFormFillEnv,
                           CPDFSDK_Annot* pScreen,
                           std::set<CPDF_Dictionary*>* visited);
  bool ExecuteBookMark(const CPDF_Action& action,
                       CPDFSDK_FormFillEnvironment* pFormFillEnv,
                       CPDF_Bookmark* pBookmark,
                       std::set<CPDF_Dictionary*>* visited);
  bool ExecuteLinkAction(const CPDF_Action& action,
                         CPDFSDK_FormFillEnvironment* pFormFillEnv,
                         std::set<CPDF_Dictionary*>* visited);

  void DoAction_NoJs(const CPDF_Action& action,
                     CPDFSDK_FormFillEnvironment* pFormFillEnv);
  void RunDocumentPageJavaScript(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                 CPDF_AAction::AActionType type,
                                 const CFX_WideString& script);
  void RunDocumentOpenJavaScript(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                 const CFX_WideString& sScriptName,
                                 const CFX_WideString& script);
  void RunFieldJavaScript(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                          CPDF_FormField* pFormField,
                          CPDF_AAction::AActionType type,
                          PDFSDK_FieldAction& data,
                          const CFX_WideString& script);

  bool IsValidField(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                    CPDF_Dictionary* pFieldDict);
  bool IsValidDocView(CPDFSDK_FormFillEnvironment* pFormFillEnv);

  void DoAction_GoTo(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                     const CPDF_Action& action);
  void DoAction_GoToR(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                      const CPDF_Action& action);
  void DoAction_Launch(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                       const CPDF_Action& action);
  void DoAction_URI(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                    const CPDF_Action& action);
  void DoAction_Named(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                      const CPDF_Action& action);
  void DoAction_SetOCGState(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                            const CPDF_Action& action);

  bool DoAction_Hide(const CPDF_Action& action,
                     CPDFSDK_FormFillEnvironment* pFormFillEnv);
  bool DoAction_SubmitForm(const CPDF_Action& action,
                           CPDFSDK_FormFillEnvironment* pFormFillEnv);
  bool DoAction_ResetForm(const CPDF_Action& action,
                          CPDFSDK_FormFillEnvironment* pFormFillEnv);
  bool DoAction_ImportData(const CPDF_Action& action,
                           CPDFSDK_FormFillEnvironment* pFormFillEnv);
};

#endif  // FPDFSDK_FSDK_ACTIONHANDLER_H_
