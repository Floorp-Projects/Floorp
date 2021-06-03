/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "SandboxTest.h"

#include "mozilla/Components.h"
#include "mozilla/Preferences.h"
#include "SandboxTestingParent.h"
#include "SandboxTestingChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/gfx/GPUChild.h"
#include "mozilla/net/SocketProcessParent.h"
#include "mozilla/ipc/Endpoint.h"
#include "nsIOService.h"

using namespace mozilla;
using namespace mozilla::ipc;
using namespace mozilla::dom;

namespace mozilla {

NS_IMPL_ISUPPORTS(SandboxTest, mozISandboxTest)

GeckoProcessType GeckoProcessStringToType(const nsCString& aString) {
  for (GeckoProcessType type = GeckoProcessType(0);
       type < GeckoProcessType::GeckoProcessType_End;
       type = GeckoProcessType(type + 1)) {
    if (aString == XRE_GeckoProcessTypeToString(type)) {
      return type;
    }
  }
  return GeckoProcessType::GeckoProcessType_Invalid;
}

// Set up tests on remote process connected to the given actor.
// The actor must handle the InitSandboxTesting message.
template <typename Actor>
SandboxTestingParent* InitializeSandboxTestingActors(Actor* aActor) {
  Endpoint<PSandboxTestingParent> sandboxTestingParentEnd;
  Endpoint<PSandboxTestingChild> sandboxTestingChildEnd;
  nsresult rv = PSandboxTesting::CreateEndpoints(
      base::GetCurrentProcId(), aActor->OtherPid(), &sandboxTestingParentEnd,
      &sandboxTestingChildEnd);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  Unused << aActor->SendInitSandboxTesting(std::move(sandboxTestingChildEnd));
  return SandboxTestingParent::Create(std::move(sandboxTestingParentEnd));
}

NS_IMETHODIMP
SandboxTest::StartTests(const nsTArray<nsCString>& aProcessesList) {
  for (auto processTypeName : aProcessesList) {
    GeckoProcessType type = GeckoProcessStringToType(processTypeName);
    if (type == GeckoProcessType::GeckoProcessType_Invalid) {
      return NS_ERROR_ILLEGAL_VALUE;
    }

    switch (type) {
      case GeckoProcessType_Content: {
        nsTArray<ContentParent*> parents;
        ContentParent::GetAll(parents);
        MOZ_ASSERT(parents.Length() > 0);
        mSandboxTestingParents[type] =
            InitializeSandboxTestingActors(parents[0]);
        break;
      }

      case GeckoProcessType_GPU: {
        gfx::GPUProcessManager* gpuProc = gfx::GPUProcessManager::Get();
        gfx::GPUChild* gpuChild = gpuProc ? gpuProc->GetGPUChild() : nullptr;
        if (!gpuChild) {
          // There is no GPU process for this OS.  Report test done.
          nsCOMPtr<nsIObserverService> observerService =
              mozilla::services::GetObserverService();
          MOZ_RELEASE_ASSERT(observerService);
          observerService->NotifyObservers(nullptr, "sandbox-test-done", 0);
          return NS_OK;
        }

        mSandboxTestingParents[type] = InitializeSandboxTestingActors(gpuChild);
        break;
      }

      case GeckoProcessType_Socket: {
        // mochitest harness force this variable, but we actually do not want
        // that
        int rv_unset =
#ifdef XP_UNIX
            unsetenv("MOZ_DISABLE_SOCKET_PROCESS");
#endif  // XP_UNIX
#ifdef XP_WIN
        _putenv("MOZ_DISABLE_SOCKET_PROCESS=");
#endif  // XP_WIN
        MOZ_ASSERT(rv_unset == 0, "Error unsetting env var");

        nsresult rv_pref =
            Preferences::SetBool("network.process.enabled", true);
        MOZ_ASSERT(rv_pref == NS_OK, "Error enforcing pref");

        MOZ_ASSERT(net::gIOService, "No gIOService?");
        RefPtr<SandboxTest> self = this;
        net::gIOService->CallOrWaitForSocketProcess([self, type]() {
          // If socket process was previously disabled by env,
          // nsIOService code will take some time before it creates the new
          // process and it triggers this callback
          //
          // If that happens, then by the time we reach the end of StartTests()
          // the mSandboxTestingParents[type] might not yet have been updated
          // this is why below we allow for a delayed dispatch to give a chance
          net::SocketProcessParent* parent =
              net::SocketProcessParent::GetSingleton();
          self->mSandboxTestingParents[type] =
              InitializeSandboxTestingActors(parent);
        });
        break;
      }

      default:
        MOZ_ASSERT_UNREACHABLE(
            "SandboxTest does not yet support this process type");
        return NS_ERROR_ILLEGAL_VALUE;
    }

    if (!mSandboxTestingParents[type]) {
      if (type == GeckoProcessType_Socket) {
        // Give a chance to the socket process to be present and to the callback
        // above to be ran. We delay by 5 seconds, but hopefully since it is all
        // on the main thread, the value itself should not be important.
        RefPtr<SandboxTest> self = this;
        NS_DelayedDispatchToCurrentThread(
            NS_NewRunnableFunction(
                "SandboxTest::StartTests",
                [self, type]() {
                  if (!self->mSandboxTestingParents[type]) {
                    MOZ_ASSERT_UNREACHABLE(
                        "SandboxTest failed to get a Parent");
                  }
                }),
            5e3 /* delay by five seconds */);
      } else {
        return NS_ERROR_FAILURE;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
SandboxTest::FinishTests() {
  for (SandboxTestingParent* stp : mSandboxTestingParents) {
    SandboxTestingParent::Destroy(stp);
  }
  return NS_OK;
}

}  // namespace mozilla

NS_IMPL_COMPONENT_FACTORY(mozISandboxTest) {
  return MakeAndAddRef<SandboxTest>().downcast<nsISupports>();
}
