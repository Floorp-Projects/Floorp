// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/fpdfxfa/cxfa_fwladaptertimermgr.h"

#include <utility>
#include <vector>

#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/fsdk_define.h"

namespace {

class CFWL_FWLAdapterTimerInfo : public CFWL_TimerInfo {
 public:
  CFWL_FWLAdapterTimerInfo(IFWL_AdapterTimerMgr* mgr,
                           int32_t event,
                           CFWL_Timer* timer)
      : CFWL_TimerInfo(mgr), idEvent(event), pTimer(timer) {}

  int32_t idEvent;
  CFWL_Timer* pTimer;
};

}  // namespace

std::vector<CFWL_TimerInfo*>* CXFA_FWLAdapterTimerMgr::s_TimerArray = nullptr;

void CXFA_FWLAdapterTimerMgr::Start(CFWL_Timer* pTimer,
                                    uint32_t dwElapse,
                                    bool bImmediately,
                                    CFWL_TimerInfo** pTimerInfo) {
  if (!m_pFormFillEnv)
    return;

  int32_t id_event = m_pFormFillEnv->SetTimer(dwElapse, TimerProc);
  if (!s_TimerArray)
    s_TimerArray = new std::vector<CFWL_TimerInfo*>;

  *pTimerInfo = new CFWL_FWLAdapterTimerInfo(this, id_event, pTimer);
  s_TimerArray->push_back(*pTimerInfo);
}

void CXFA_FWLAdapterTimerMgr::Stop(CFWL_TimerInfo* pTimerInfo) {
  if (!pTimerInfo || !m_pFormFillEnv)
    return;

  CFWL_FWLAdapterTimerInfo* pInfo =
      static_cast<CFWL_FWLAdapterTimerInfo*>(pTimerInfo);
  m_pFormFillEnv->KillTimer(pInfo->idEvent);
  if (!s_TimerArray)
    return;

  auto it = std::find(s_TimerArray->begin(), s_TimerArray->end(), pInfo);
  if (it != s_TimerArray->end()) {
    s_TimerArray->erase(it);
    delete pInfo;
  }
}

// static
void CXFA_FWLAdapterTimerMgr::TimerProc(int32_t idEvent) {
  if (!s_TimerArray)
    return;

  for (const auto info : *s_TimerArray) {
    CFWL_FWLAdapterTimerInfo* pInfo =
        static_cast<CFWL_FWLAdapterTimerInfo*>(info);
    if (pInfo->idEvent == idEvent) {
      pInfo->pTimer->Run(pInfo);
      break;
    }
  }
}
