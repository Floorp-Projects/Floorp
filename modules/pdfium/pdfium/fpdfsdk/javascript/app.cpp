// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/app.h"

#include <map>
#include <memory>
#include <vector>

#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_interform.h"
#include "fpdfsdk/javascript/Document.h"
#include "fpdfsdk/javascript/JS_Define.h"
#include "fpdfsdk/javascript/JS_EventHandler.h"
#include "fpdfsdk/javascript/JS_Object.h"
#include "fpdfsdk/javascript/JS_Value.h"
#include "fpdfsdk/javascript/cjs_event_context.h"
#include "fpdfsdk/javascript/cjs_runtime.h"
#include "fpdfsdk/javascript/resource.h"
#include "third_party/base/stl_util.h"

class GlobalTimer {
 public:
  GlobalTimer(app* pObj,
              CPDFSDK_FormFillEnvironment* pFormFillEnv,
              CJS_Runtime* pRuntime,
              int nType,
              const CFX_WideString& script,
              uint32_t dwElapse,
              uint32_t dwTimeOut);
  ~GlobalTimer();

  static void Trigger(int nTimerID);
  static void Cancel(int nTimerID);

  bool IsOneShot() const { return m_nType == 1; }
  uint32_t GetTimeOut() const { return m_dwTimeOut; }
  int GetTimerID() const { return m_nTimerID; }
  CJS_Runtime* GetRuntime() const { return m_pRuntime.Get(); }
  CFX_WideString GetJScript() const { return m_swJScript; }

 private:
  using TimerMap = std::map<uint32_t, GlobalTimer*>;
  static TimerMap* GetGlobalTimerMap();

  uint32_t m_nTimerID;
  app* const m_pEmbedObj;
  bool m_bProcessing;

  // data
  const int m_nType;  // 0:Interval; 1:TimeOut
  const uint32_t m_dwTimeOut;
  const CFX_WideString m_swJScript;
  CJS_Runtime::ObservedPtr m_pRuntime;
  CPDFSDK_FormFillEnvironment::ObservedPtr m_pFormFillEnv;
};

GlobalTimer::GlobalTimer(app* pObj,
                         CPDFSDK_FormFillEnvironment* pFormFillEnv,
                         CJS_Runtime* pRuntime,
                         int nType,
                         const CFX_WideString& script,
                         uint32_t dwElapse,
                         uint32_t dwTimeOut)
    : m_nTimerID(0),
      m_pEmbedObj(pObj),
      m_bProcessing(false),
      m_nType(nType),
      m_dwTimeOut(dwTimeOut),
      m_swJScript(script),
      m_pRuntime(pRuntime),
      m_pFormFillEnv(pFormFillEnv) {
  CFX_SystemHandler* pHandler = m_pFormFillEnv->GetSysHandler();
  m_nTimerID = pHandler->SetTimer(dwElapse, Trigger);
  if (m_nTimerID)
    (*GetGlobalTimerMap())[m_nTimerID] = this;
}

GlobalTimer::~GlobalTimer() {
  if (!m_nTimerID)
    return;

  if (GetRuntime())
    m_pFormFillEnv->GetSysHandler()->KillTimer(m_nTimerID);

  GetGlobalTimerMap()->erase(m_nTimerID);
}

// static
void GlobalTimer::Trigger(int nTimerID) {
  auto it = GetGlobalTimerMap()->find(nTimerID);
  if (it == GetGlobalTimerMap()->end())
    return;

  GlobalTimer* pTimer = it->second;
  if (pTimer->m_bProcessing)
    return;

  pTimer->m_bProcessing = true;
  if (pTimer->m_pEmbedObj)
    pTimer->m_pEmbedObj->TimerProc(pTimer);

  // Timer proc may have destroyed timer, find it again.
  it = GetGlobalTimerMap()->find(nTimerID);
  if (it == GetGlobalTimerMap()->end())
    return;

  pTimer = it->second;
  pTimer->m_bProcessing = false;
  if (pTimer->IsOneShot())
    pTimer->m_pEmbedObj->CancelProc(pTimer);
}

// static
void GlobalTimer::Cancel(int nTimerID) {
  auto it = GetGlobalTimerMap()->find(nTimerID);
  if (it == GetGlobalTimerMap()->end())
    return;

  GlobalTimer* pTimer = it->second;
  pTimer->m_pEmbedObj->CancelProc(pTimer);
}

// static
GlobalTimer::TimerMap* GlobalTimer::GetGlobalTimerMap() {
  // Leak the timer array at shutdown.
  static auto* s_TimerMap = new TimerMap;
  return s_TimerMap;
}

JSConstSpec CJS_TimerObj::ConstSpecs[] = {{0, JSConstSpec::Number, 0, 0}};

JSPropertySpec CJS_TimerObj::PropertySpecs[] = {{0, 0, 0}};

JSMethodSpec CJS_TimerObj::MethodSpecs[] = {{0, 0}};

IMPLEMENT_JS_CLASS(CJS_TimerObj, TimerObj)

TimerObj::TimerObj(CJS_Object* pJSObject)
    : CJS_EmbedObj(pJSObject), m_nTimerID(0) {}

TimerObj::~TimerObj() {}

void TimerObj::SetTimer(GlobalTimer* pTimer) {
  m_nTimerID = pTimer->GetTimerID();
}

#define JS_STR_VIEWERTYPE L"pdfium"
#define JS_STR_VIEWERVARIATION L"Full"
#define JS_STR_PLATFORM L"WIN"
#define JS_STR_LANGUAGE L"ENU"
#define JS_NUM_VIEWERVERSION 8
#ifdef PDF_ENABLE_XFA
#define JS_NUM_VIEWERVERSION_XFA 11
#endif  // PDF_ENABLE_XFA
#define JS_NUM_FORMSVERSION 7

JSConstSpec CJS_App::ConstSpecs[] = {{0, JSConstSpec::Number, 0, 0}};

JSPropertySpec CJS_App::PropertySpecs[] = {
    {"activeDocs", get_activeDocs_static, set_activeDocs_static},
    {"calculate", get_calculate_static, set_calculate_static},
    {"formsVersion", get_formsVersion_static, set_formsVersion_static},
    {"fs", get_fs_static, set_fs_static},
    {"fullscreen", get_fullscreen_static, set_fullscreen_static},
    {"language", get_language_static, set_language_static},
    {"media", get_media_static, set_media_static},
    {"platform", get_platform_static, set_platform_static},
    {"runtimeHighlight", get_runtimeHighlight_static,
     set_runtimeHighlight_static},
    {"viewerType", get_viewerType_static, set_viewerType_static},
    {"viewerVariation", get_viewerVariation_static, set_viewerVariation_static},
    {"viewerVersion", get_viewerVersion_static, set_viewerVersion_static},
    {0, 0, 0}};

JSMethodSpec CJS_App::MethodSpecs[] = {{"alert", alert_static},
                                       {"beep", beep_static},
                                       {"browseForDoc", browseForDoc_static},
                                       {"clearInterval", clearInterval_static},
                                       {"clearTimeOut", clearTimeOut_static},
                                       {"execDialog", execDialog_static},
                                       {"execMenuItem", execMenuItem_static},
                                       {"findComponent", findComponent_static},
                                       {"goBack", goBack_static},
                                       {"goForward", goForward_static},
                                       {"launchURL", launchURL_static},
                                       {"mailMsg", mailMsg_static},
                                       {"newFDF", newFDF_static},
                                       {"newDoc", newDoc_static},
                                       {"openDoc", openDoc_static},
                                       {"openFDF", openFDF_static},
                                       {"popUpMenuEx", popUpMenuEx_static},
                                       {"popUpMenu", popUpMenu_static},
                                       {"response", response_static},
                                       {"setInterval", setInterval_static},
                                       {"setTimeOut", setTimeOut_static},
                                       {0, 0}};

IMPLEMENT_JS_CLASS(CJS_App, app)

app::app(CJS_Object* pJSObject)
    : CJS_EmbedObj(pJSObject), m_bCalculate(true), m_bRuntimeHighLight(false) {}

app::~app() {
}

bool app::activeDocs(CJS_Runtime* pRuntime,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  if (!vp.IsGetting())
    return false;

  CJS_Document* pJSDocument = nullptr;
  v8::Local<v8::Object> pObj = pRuntime->GetThisObj();
  if (CFXJS_Engine::GetObjDefnID(pObj) == CJS_Document::g_nObjDefnID)
    pJSDocument = static_cast<CJS_Document*>(pRuntime->GetObjectPrivate(pObj));

  CJS_Array aDocs;
  aDocs.SetElement(pRuntime, 0, CJS_Value(pRuntime, pJSDocument));
  if (aDocs.GetLength(pRuntime) > 0)
    vp << aDocs;
  else
    vp.GetJSValue()->SetNull(pRuntime);

  return true;
}

bool app::calculate(CJS_Runtime* pRuntime,
                    CJS_PropValue& vp,
                    CFX_WideString& sError) {
  if (vp.IsSetting()) {
    bool bVP;
    vp >> bVP;
    m_bCalculate = (bool)bVP;
    pRuntime->GetFormFillEnv()->GetInterForm()->EnableCalculate(
        (bool)m_bCalculate);
  } else {
    vp << (bool)m_bCalculate;
  }
  return true;
}

bool app::formsVersion(CJS_Runtime* pRuntime,
                       CJS_PropValue& vp,
                       CFX_WideString& sError) {
  if (vp.IsGetting()) {
    vp << JS_NUM_FORMSVERSION;
    return true;
  }

  return false;
}

bool app::viewerType(CJS_Runtime* pRuntime,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  if (vp.IsGetting()) {
    vp << JS_STR_VIEWERTYPE;
    return true;
  }

  return false;
}

bool app::viewerVariation(CJS_Runtime* pRuntime,
                          CJS_PropValue& vp,
                          CFX_WideString& sError) {
  if (vp.IsGetting()) {
    vp << JS_STR_VIEWERVARIATION;
    return true;
  }

  return false;
}

bool app::viewerVersion(CJS_Runtime* pRuntime,
                        CJS_PropValue& vp,
                        CFX_WideString& sError) {
  if (!vp.IsGetting())
    return false;
#ifdef PDF_ENABLE_XFA
  CPDFXFA_Context* pXFAContext = pRuntime->GetFormFillEnv()->GetXFAContext();
  if (pXFAContext->GetDocType() == 1 || pXFAContext->GetDocType() == 2) {
    vp << JS_NUM_VIEWERVERSION_XFA;
    return true;
  }
#endif  // PDF_ENABLE_XFA
  vp << JS_NUM_VIEWERVERSION;
  return true;
}

bool app::platform(CJS_Runtime* pRuntime,
                   CJS_PropValue& vp,
                   CFX_WideString& sError) {
  if (!vp.IsGetting())
    return false;
#ifdef PDF_ENABLE_XFA
  CPDFSDK_FormFillEnvironment* pFormFillEnv = pRuntime->GetFormFillEnv();
  if (!pFormFillEnv)
    return false;
  CFX_WideString platfrom = pFormFillEnv->GetPlatform();
  if (!platfrom.IsEmpty()) {
    vp << platfrom;
    return true;
  }
#endif
  vp << JS_STR_PLATFORM;
  return true;
}

bool app::language(CJS_Runtime* pRuntime,
                   CJS_PropValue& vp,
                   CFX_WideString& sError) {
  if (!vp.IsGetting())
    return false;
#ifdef PDF_ENABLE_XFA
  CPDFSDK_FormFillEnvironment* pFormFillEnv = pRuntime->GetFormFillEnv();
  if (!pFormFillEnv)
    return false;
  CFX_WideString language = pFormFillEnv->GetLanguage();
  if (!language.IsEmpty()) {
    vp << language;
    return true;
  }
#endif
  vp << JS_STR_LANGUAGE;
  return true;
}

// creates a new fdf object that contains no data
// comment: need reader support
// note:
// CFDF_Document * CPDFSDK_FormFillEnvironment::NewFDF();
bool app::newFDF(CJS_Runtime* pRuntime,
                 const std::vector<CJS_Value>& params,
                 CJS_Value& vRet,
                 CFX_WideString& sError) {
  return true;
}
// opens a specified pdf document and returns its document object
// comment:need reader support
// note: as defined in js reference, the proto of this function's fourth
// parmeters, how old an fdf document while do not show it.
// CFDF_Document * CPDFSDK_FormFillEnvironment::OpenFDF(string strPath,bool
// bUserConv);

bool app::openFDF(CJS_Runtime* pRuntime,
                  const std::vector<CJS_Value>& params,
                  CJS_Value& vRet,
                  CFX_WideString& sError) {
  return true;
}

bool app::alert(CJS_Runtime* pRuntime,
                const std::vector<CJS_Value>& params,
                CJS_Value& vRet,
                CFX_WideString& sError) {
  std::vector<CJS_Value> newParams = JS_ExpandKeywordParams(
      pRuntime, params, 4, L"cMsg", L"nIcon", L"nType", L"cTitle");

  if (newParams[0].GetType() == CJS_Value::VT_unknown) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }

  CPDFSDK_FormFillEnvironment* pFormFillEnv = pRuntime->GetFormFillEnv();
  if (!pFormFillEnv) {
    vRet = CJS_Value(pRuntime, 0);
    return true;
  }

  CFX_WideString swMsg;
  if (newParams[0].GetType() == CJS_Value::VT_object) {
    CJS_Array carray;
    if (newParams[0].ConvertToArray(pRuntime, carray)) {
      swMsg = L"[";
      CJS_Value element(pRuntime);
      for (int i = 0; i < carray.GetLength(pRuntime); ++i) {
        if (i)
          swMsg += L", ";
        carray.GetElement(pRuntime, i, element);
        swMsg += element.ToCFXWideString(pRuntime);
      }
      swMsg += L"]";
    } else {
      swMsg = newParams[0].ToCFXWideString(pRuntime);
    }
  } else {
    swMsg = newParams[0].ToCFXWideString(pRuntime);
  }

  int iIcon = 0;
  if (newParams[1].GetType() != CJS_Value::VT_unknown)
    iIcon = newParams[1].ToInt(pRuntime);

  int iType = 0;
  if (newParams[2].GetType() != CJS_Value::VT_unknown)
    iType = newParams[2].ToInt(pRuntime);

  CFX_WideString swTitle;
  if (newParams[3].GetType() != CJS_Value::VT_unknown)
    swTitle = newParams[3].ToCFXWideString(pRuntime);
  else
    swTitle = JSGetStringFromID(IDS_STRING_JSALERT);

  pRuntime->BeginBlock();
  pFormFillEnv->KillFocusAnnot(0);

  vRet = CJS_Value(pRuntime, pFormFillEnv->JS_appAlert(
                                 swMsg.c_str(), swTitle.c_str(), iType, iIcon));
  pRuntime->EndBlock();
  return true;
}

bool app::beep(CJS_Runtime* pRuntime,
               const std::vector<CJS_Value>& params,
               CJS_Value& vRet,
               CFX_WideString& sError) {
  if (params.size() == 1) {
    pRuntime->GetFormFillEnv()->JS_appBeep(params[0].ToInt(pRuntime));
    return true;
  }

  sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
  return false;
}

bool app::findComponent(CJS_Runtime* pRuntime,
                        const std::vector<CJS_Value>& params,
                        CJS_Value& vRet,
                        CFX_WideString& sError) {
  return true;
}

bool app::popUpMenuEx(CJS_Runtime* pRuntime,
                      const std::vector<CJS_Value>& params,
                      CJS_Value& vRet,
                      CFX_WideString& sError) {
  return false;
}

bool app::fs(CJS_Runtime* pRuntime, CJS_PropValue& vp, CFX_WideString& sError) {
  return false;
}

bool app::setInterval(CJS_Runtime* pRuntime,
                      const std::vector<CJS_Value>& params,
                      CJS_Value& vRet,
                      CFX_WideString& sError) {
  if (params.size() > 2 || params.size() == 0) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }

  CFX_WideString script =
      params.size() > 0 ? params[0].ToCFXWideString(pRuntime) : L"";
  if (script.IsEmpty()) {
    sError = JSGetStringFromID(IDS_STRING_JSAFNUMBER_KEYSTROKE);
    return true;
  }

  uint32_t dwInterval = params.size() > 1 ? params[1].ToInt(pRuntime) : 1000;

  GlobalTimer* timerRef = new GlobalTimer(this, pRuntime->GetFormFillEnv(),
                                          pRuntime, 0, script, dwInterval, 0);
  m_Timers.insert(std::unique_ptr<GlobalTimer>(timerRef));

  v8::Local<v8::Object> pRetObj =
      pRuntime->NewFxDynamicObj(CJS_TimerObj::g_nObjDefnID);
  if (pRetObj.IsEmpty())
    return false;

  CJS_TimerObj* pJS_TimerObj =
      static_cast<CJS_TimerObj*>(pRuntime->GetObjectPrivate(pRetObj));
  TimerObj* pTimerObj = static_cast<TimerObj*>(pJS_TimerObj->GetEmbedObject());
  pTimerObj->SetTimer(timerRef);

  vRet = CJS_Value(pRuntime, pRetObj);
  return true;
}

bool app::setTimeOut(CJS_Runtime* pRuntime,
                     const std::vector<CJS_Value>& params,
                     CJS_Value& vRet,
                     CFX_WideString& sError) {
  if (params.size() > 2 || params.size() == 0) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }

  CFX_WideString script = params[0].ToCFXWideString(pRuntime);
  if (script.IsEmpty()) {
    sError = JSGetStringFromID(IDS_STRING_JSAFNUMBER_KEYSTROKE);
    return true;
  }

  uint32_t dwTimeOut = params.size() > 1 ? params[1].ToInt(pRuntime) : 1000;
  GlobalTimer* timerRef =
      new GlobalTimer(this, pRuntime->GetFormFillEnv(), pRuntime, 1, script,
                      dwTimeOut, dwTimeOut);
  m_Timers.insert(std::unique_ptr<GlobalTimer>(timerRef));

  v8::Local<v8::Object> pRetObj =
      pRuntime->NewFxDynamicObj(CJS_TimerObj::g_nObjDefnID);
  if (pRetObj.IsEmpty())
    return false;

  CJS_TimerObj* pJS_TimerObj =
      static_cast<CJS_TimerObj*>(pRuntime->GetObjectPrivate(pRetObj));
  TimerObj* pTimerObj = static_cast<TimerObj*>(pJS_TimerObj->GetEmbedObject());
  pTimerObj->SetTimer(timerRef);
  vRet = CJS_Value(pRuntime, pRetObj);
  return true;
}

bool app::clearTimeOut(CJS_Runtime* pRuntime,
                       const std::vector<CJS_Value>& params,
                       CJS_Value& vRet,
                       CFX_WideString& sError) {
  if (params.size() != 1) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }

  app::ClearTimerCommon(pRuntime, params[0]);
  return true;
}

bool app::clearInterval(CJS_Runtime* pRuntime,
                        const std::vector<CJS_Value>& params,
                        CJS_Value& vRet,
                        CFX_WideString& sError) {
  if (params.size() != 1) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }

  app::ClearTimerCommon(pRuntime, params[0]);
  return true;
}

void app::ClearTimerCommon(CJS_Runtime* pRuntime, const CJS_Value& param) {
  if (param.GetType() != CJS_Value::VT_object)
    return;

  v8::Local<v8::Object> pObj = param.ToV8Object(pRuntime);
  if (CFXJS_Engine::GetObjDefnID(pObj) != CJS_TimerObj::g_nObjDefnID)
    return;

  CJS_Object* pJSObj = param.ToCJSObject(pRuntime);
  if (!pJSObj)
    return;

  TimerObj* pTimerObj = static_cast<TimerObj*>(pJSObj->GetEmbedObject());
  if (!pTimerObj)
    return;

  GlobalTimer::Cancel(pTimerObj->GetTimerID());
}

bool app::execMenuItem(CJS_Runtime* pRuntime,
                       const std::vector<CJS_Value>& params,
                       CJS_Value& vRet,
                       CFX_WideString& sError) {
  return false;
}

void app::TimerProc(GlobalTimer* pTimer) {
  CJS_Runtime* pRuntime = pTimer->GetRuntime();
  if (pRuntime && (!pTimer->IsOneShot() || pTimer->GetTimeOut() > 0))
    RunJsScript(pRuntime, pTimer->GetJScript());
}

void app::CancelProc(GlobalTimer* pTimer) {
  m_Timers.erase(pdfium::FakeUniquePtr<GlobalTimer>(pTimer));
}

void app::RunJsScript(CJS_Runtime* pRuntime, const CFX_WideString& wsScript) {
  if (!pRuntime->IsBlocking()) {
    IJS_EventContext* pContext = pRuntime->NewEventContext();
    pContext->OnExternal_Exec();
    CFX_WideString wtInfo;
    pContext->RunScript(wsScript, &wtInfo);
    pRuntime->ReleaseEventContext(pContext);
  }
}

bool app::goBack(CJS_Runtime* pRuntime,
                 const std::vector<CJS_Value>& params,
                 CJS_Value& vRet,
                 CFX_WideString& sError) {
  // Not supported.
  return true;
}

bool app::goForward(CJS_Runtime* pRuntime,
                    const std::vector<CJS_Value>& params,
                    CJS_Value& vRet,
                    CFX_WideString& sError) {
  // Not supported.
  return true;
}

bool app::mailMsg(CJS_Runtime* pRuntime,
                  const std::vector<CJS_Value>& params,
                  CJS_Value& vRet,
                  CFX_WideString& sError) {
  std::vector<CJS_Value> newParams =
      JS_ExpandKeywordParams(pRuntime, params, 6, L"bUI", L"cTo", L"cCc",
                             L"cBcc", L"cSubject", L"cMsg");

  if (newParams[0].GetType() == CJS_Value::VT_unknown) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }
  bool bUI = newParams[0].ToBool(pRuntime);

  CFX_WideString cTo;
  if (newParams[1].GetType() != CJS_Value::VT_unknown) {
    cTo = newParams[1].ToCFXWideString(pRuntime);
  } else {
    if (!bUI) {
      // cTo parameter required when UI not invoked.
      sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
      return false;
    }
  }

  CFX_WideString cCc;
  if (newParams[2].GetType() != CJS_Value::VT_unknown)
    cCc = newParams[2].ToCFXWideString(pRuntime);

  CFX_WideString cBcc;
  if (newParams[3].GetType() != CJS_Value::VT_unknown)
    cBcc = newParams[3].ToCFXWideString(pRuntime);

  CFX_WideString cSubject;
  if (newParams[4].GetType() != CJS_Value::VT_unknown)
    cSubject = newParams[4].ToCFXWideString(pRuntime);

  CFX_WideString cMsg;
  if (newParams[5].GetType() != CJS_Value::VT_unknown)
    cMsg = newParams[5].ToCFXWideString(pRuntime);

  pRuntime->BeginBlock();
  pRuntime->GetFormFillEnv()->JS_docmailForm(nullptr, 0, bUI, cTo.c_str(),
                                             cSubject.c_str(), cCc.c_str(),
                                             cBcc.c_str(), cMsg.c_str());
  pRuntime->EndBlock();
  return true;
}

bool app::launchURL(CJS_Runtime* pRuntime,
                    const std::vector<CJS_Value>& params,
                    CJS_Value& vRet,
                    CFX_WideString& sError) {
  // Unsafe, not supported.
  return true;
}

bool app::runtimeHighlight(CJS_Runtime* pRuntime,
                           CJS_PropValue& vp,
                           CFX_WideString& sError) {
  if (vp.IsSetting()) {
    vp >> m_bRuntimeHighLight;
  } else {
    vp << m_bRuntimeHighLight;
  }
  return true;
}

bool app::fullscreen(CJS_Runtime* pRuntime,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  return false;
}

bool app::popUpMenu(CJS_Runtime* pRuntime,
                    const std::vector<CJS_Value>& params,
                    CJS_Value& vRet,
                    CFX_WideString& sError) {
  return false;
}

bool app::browseForDoc(CJS_Runtime* pRuntime,
                       const std::vector<CJS_Value>& params,
                       CJS_Value& vRet,
                       CFX_WideString& sError) {
  // Unsafe, not supported.
  return true;
}

CFX_WideString app::SysPathToPDFPath(const CFX_WideString& sOldPath) {
  CFX_WideString sRet = L"/";

  for (int i = 0, sz = sOldPath.GetLength(); i < sz; i++) {
    wchar_t c = sOldPath.GetAt(i);
    if (c == L':') {
    } else {
      if (c == L'\\') {
        sRet += L"/";
      } else {
        sRet += c;
      }
    }
  }

  return sRet;
}

bool app::newDoc(CJS_Runtime* pRuntime,
                 const std::vector<CJS_Value>& params,
                 CJS_Value& vRet,
                 CFX_WideString& sError) {
  return false;
}

bool app::openDoc(CJS_Runtime* pRuntime,
                  const std::vector<CJS_Value>& params,
                  CJS_Value& vRet,
                  CFX_WideString& sError) {
  return false;
}

bool app::response(CJS_Runtime* pRuntime,
                   const std::vector<CJS_Value>& params,
                   CJS_Value& vRet,
                   CFX_WideString& sError) {
  std::vector<CJS_Value> newParams =
      JS_ExpandKeywordParams(pRuntime, params, 5, L"cQuestion", L"cTitle",
                             L"cDefault", L"bPassword", L"cLabel");

  if (newParams[0].GetType() == CJS_Value::VT_unknown) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }
  CFX_WideString swQuestion = newParams[0].ToCFXWideString(pRuntime);

  CFX_WideString swTitle = L"PDF";
  if (newParams[1].GetType() != CJS_Value::VT_unknown)
    swTitle = newParams[1].ToCFXWideString(pRuntime);

  CFX_WideString swDefault;
  if (newParams[2].GetType() != CJS_Value::VT_unknown)
    swDefault = newParams[2].ToCFXWideString(pRuntime);

  bool bPassword = false;
  if (newParams[3].GetType() != CJS_Value::VT_unknown)
    bPassword = newParams[3].ToBool(pRuntime);

  CFX_WideString swLabel;
  if (newParams[4].GetType() != CJS_Value::VT_unknown)
    swLabel = newParams[4].ToCFXWideString(pRuntime);

  const int MAX_INPUT_BYTES = 2048;
  std::unique_ptr<char[]> pBuff(new char[MAX_INPUT_BYTES + 2]);
  memset(pBuff.get(), 0, MAX_INPUT_BYTES + 2);

  int nLengthBytes = pRuntime->GetFormFillEnv()->JS_appResponse(
      swQuestion.c_str(), swTitle.c_str(), swDefault.c_str(), swLabel.c_str(),
      bPassword, pBuff.get(), MAX_INPUT_BYTES);

  if (nLengthBytes < 0 || nLengthBytes > MAX_INPUT_BYTES) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAM_TOOLONG);
    return false;
  }

  vRet = CJS_Value(pRuntime, CFX_WideString::FromUTF16LE(
                                 reinterpret_cast<uint16_t*>(pBuff.get()),
                                 nLengthBytes / sizeof(uint16_t))
                                 .c_str());

  return true;
}

bool app::media(CJS_Runtime* pRuntime,
                CJS_PropValue& vp,
                CFX_WideString& sError) {
  return false;
}

bool app::execDialog(CJS_Runtime* pRuntime,
                     const std::vector<CJS_Value>& params,
                     CJS_Value& vRet,
                     CFX_WideString& sError) {
  return true;
}
