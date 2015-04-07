/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Module.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/NullPtr.h"
#include "mozilla/TimeStamp.h"
#include "nsCategoryManager.h"
#include "nsComponentManager.h"
#include "nsDebugImpl.h"
#include "nsIErrorService.h"
#include "nsMemoryImpl.h"
#include "nsNetCID.h"
#include "nsNetModuleStandalone.h"
#include "nsObserverService.h"
#include "nsThreadManager.h"
#include "nsThreadPool.h"
#include "nsUUIDGenerator.h"
#include "nsXPCOMCIDInternal.h"
#include "nsXPCOMPrivate.h"
#include "TimerThread.h"
#include "XPCOMRTInit.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(nsTimerImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsUUIDGenerator, Init)

static nsresult
nsThreadManagerGetSingleton(nsISupports* aOuter,
                            const nsIID& aIID,
                            void** aInstancePtr)
{
  NS_ASSERTION(aInstancePtr, "null outptr");
  if (NS_WARN_IF(aOuter)) {
    return NS_ERROR_NO_AGGREGATION;
  }

  return nsThreadManager::get()->QueryInterface(aIID, aInstancePtr);
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsThreadPool)

nsComponentManagerImpl* nsComponentManagerImpl::gComponentManager = nullptr;
bool gXPCOMShuttingDown = false;
bool gXPCOMThreadsShutDown = false;

#define COMPONENT(NAME, Ctor) static NS_DEFINE_CID(kNS_##NAME##_CID, NS_##NAME##_CID);
#include "XPCOMRTModule.inc"
#undef COMPONENT

#define COMPONENT(NAME, Ctor) { &kNS_##NAME##_CID, false, nullptr, Ctor },
const mozilla::Module::CIDEntry kXPCOMCIDEntries[] = {
  { &kComponentManagerCID, true, nullptr, nsComponentManagerImpl::Create },
#include "XPCOMRTModule.inc"
  { nullptr }
};
#undef COMPONENT

#define COMPONENT(NAME, Ctor) { NS_##NAME##_CONTRACTID, &kNS_##NAME##_CID },
const mozilla::Module::ContractIDEntry kXPCOMContracts[] = {
#include "XPCOMRTModule.inc"
  { nullptr }
};
#undef COMPONENT

const mozilla::Module kXPCOMRTModule = {
  mozilla::Module::kVersion, kXPCOMCIDEntries, kXPCOMContracts
};

nsresult
NS_InitXPCOMRT()
{
  nsresult rv = NS_OK;

  NS_SetMainThread();

  mozilla::TimeStamp::Startup();

  rv = nsThreadManager::get()->Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Set up the timer globals/timer thread
  rv = nsTimerImpl::Startup();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsComponentManagerImpl::gComponentManager = new nsComponentManagerImpl();
  NS_ADDREF(nsComponentManagerImpl::gComponentManager);

  rv = nsComponentManagerImpl::gComponentManager->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(nsComponentManagerImpl::gComponentManager);
    return rv;
  }

  mozilla::InitNetModuleStandalone();

  return NS_OK;
}

nsresult
NS_ShutdownXPCOMRT()
{
  nsresult rv = NS_OK;

  // Notify observers of xpcom shutting down
  {
    // Block it so that the COMPtr will get deleted before we hit
    // servicemanager shutdown

    nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

    if (NS_WARN_IF(!thread)) {
      return NS_ERROR_UNEXPECTED;
    }

    nsRefPtr<nsObserverService> observerService;
    CallGetService("@mozilla.org/observer-service;1",
                     (nsObserverService**)getter_AddRefs(observerService));

    if (observerService) {
      observerService->NotifyObservers(nullptr,
                                       NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID,
                                       nullptr);

      nsCOMPtr<nsIServiceManager> mgr;
      rv = NS_GetServiceManager(getter_AddRefs(mgr));
      if (NS_SUCCEEDED(rv)) {
        observerService->NotifyObservers(mgr, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                         nullptr);
      }
    }

    // This must happen after the shutdown of media and widgets, which
    // are triggered by the NS_XPCOM_SHUTDOWN_OBSERVER_ID notification.
    NS_ProcessPendingEvents(thread);

    if (observerService)
      observerService->NotifyObservers(nullptr,
                                       NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID,
                                       nullptr);

    gXPCOMThreadsShutDown = true;
    NS_ProcessPendingEvents(thread);

    // Shutdown the timer thread and all timers that might still be alive before
    // shutting down the component manager
    nsTimerImpl::Shutdown();

    NS_ProcessPendingEvents(thread);

    // Net module needs to be shutdown before the thread manager or else
    // the thread manager will hang waiting for the socket transport
    // service to shutdown.
    mozilla::ShutdownNetModuleStandalone();

    // Shutdown all remaining threads.  This method does not return until
    // all threads created using the thread manager (with the exception of
    // the main thread) have exited.
    nsThreadManager::get()->Shutdown();

    NS_ProcessPendingEvents(thread);
  }

  mozilla::services::Shutdown();

  // Shutdown global servicemanager
  if (nsComponentManagerImpl::gComponentManager) {
    nsComponentManagerImpl::gComponentManager->FreeServices();
  }

  // Shutdown xpcom. This will release all loaders and cause others holding
  // a refcount to the component manager to release it.
  if (nsComponentManagerImpl::gComponentManager) {
    rv = (nsComponentManagerImpl::gComponentManager)->Shutdown();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Component Manager shutdown failed.");
  } else {
    NS_WARNING("Component Manager was never created ...");
  }

  // Finally, release the component manager last because it unloads the
  // libraries:
  if (nsComponentManagerImpl::gComponentManager) {
    nsrefcnt cnt;
    NS_RELEASE2(nsComponentManagerImpl::gComponentManager, cnt);
    NS_ASSERTION(cnt == 0, "Component Manager being held past XPCOM shutdown.");
  }
  nsComponentManagerImpl::gComponentManager = nullptr;
  nsCategoryManager::Destroy();

  return NS_OK;
}
