/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintingProxy.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/layout/RemotePrintJobChild.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/Unused.h"
#include "nsIDocShell.h"
#include "nsIPrintingPromptService.h"
#include "nsIPrintSession.h"
#include "nsPIDOMWindow.h"
#include "nsPrintSettingsService.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::embedding;
using namespace mozilla::layout;

static StaticRefPtr<nsPrintingProxy> sPrintingProxyInstance;

NS_IMPL_ISUPPORTS0(nsPrintingProxy)

nsPrintingProxy::nsPrintingProxy() = default;

nsPrintingProxy::~nsPrintingProxy() = default;

/* static */
already_AddRefed<nsPrintingProxy> nsPrintingProxy::GetInstance() {
  if (!sPrintingProxyInstance) {
    sPrintingProxyInstance = new nsPrintingProxy();
    if (!sPrintingProxyInstance) {
      return nullptr;
    }
    nsresult rv = sPrintingProxyInstance->Init();
    if (NS_FAILED(rv)) {
      sPrintingProxyInstance = nullptr;
      return nullptr;
    }
    ClearOnShutdown(&sPrintingProxyInstance);
  }

  RefPtr<nsPrintingProxy> inst = sPrintingProxyInstance.get();
  return inst.forget();
}

nsresult nsPrintingProxy::Init() {
  mozilla::Unused << ContentChild::GetSingleton()->SendPPrintingConstructor(
      this);
  return NS_OK;
}

already_AddRefed<PRemotePrintJobChild>
nsPrintingProxy::AllocPRemotePrintJobChild() {
  RefPtr<RemotePrintJobChild> remotePrintJob = new RemotePrintJobChild();
  return remotePrintJob.forget();
}
