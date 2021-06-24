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
#include "mozilla/RDDProcessManager.h"
#include "mozilla/RDDChild.h"
#include "mozilla/ipc/Endpoint.h"
#include "nsIOService.h"

using namespace mozilla;
using namespace mozilla::ipc;
using namespace mozilla::dom;

namespace mozilla {

NS_IMPL_ISUPPORTS(SandboxTest, mozISandboxTest)

inline void UnsetEnvVariable(const nsCString& aEnvVarName) {
  nsCString aEnvVarNameFull = aEnvVarName + "="_ns;
  int rv_unset =
#ifdef XP_UNIX
      unsetenv(aEnvVarName.get());
#endif  // XP_UNIX
#ifdef XP_WIN
  _putenv(aEnvVarNameFull.get());
#endif  // XP_WIN
  MOZ_ASSERT(rv_unset == 0, "Error unsetting env var");
}

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

    RefPtr<ProcessPromise::Private> mProcessPromise =
        MakeRefPtr<ProcessPromise::Private>(__func__);

    switch (type) {
      case GeckoProcessType_Content: {
        nsTArray<ContentParent*> parents;
        ContentParent::GetAll(parents);
        if (parents[0]) {
          mProcessPromise->Resolve(
              std::move(InitializeSandboxTestingActors(parents[0])), __func__);
        } else {
          mProcessPromise->Reject(NS_ERROR_FAILURE, __func__);
          MOZ_ASSERT_UNREACHABLE("SandboxTest; failure to get Content process");
        }
        break;
      }

      case GeckoProcessType_GPU: {
        gfx::GPUProcessManager* gpuProc = gfx::GPUProcessManager::Get();
        gfx::GPUChild* gpuChild = gpuProc ? gpuProc->GetGPUChild() : nullptr;
        if (gpuChild) {
          mProcessPromise->Resolve(
              std::move(InitializeSandboxTestingActors(gpuChild)), __func__);
        } else {
          mProcessPromise->Reject(NS_OK, __func__);
        }
        break;
      }

      case GeckoProcessType_RDD: {
        RDDProcessManager* rddProc = RDDProcessManager::Get();
        rddProc->LaunchRDDProcess()->Then(
            GetMainThreadSerialEventTarget(), __func__,
            [mProcessPromise, rddProc]() {
              RDDChild* rddChild = rddProc ? rddProc->GetRDDChild() : nullptr;
              if (rddChild) {
                return mProcessPromise->Resolve(
                    std::move(InitializeSandboxTestingActors(rddChild)),
                    __func__);
              }
              return mProcessPromise->Reject(NS_ERROR_FAILURE, __func__);
            },
            [mProcessPromise](nsresult aError) {
              MOZ_ASSERT_UNREACHABLE("SandboxTest; failure to get RDD process");
              return mProcessPromise->Reject(aError, __func__);
            });
        break;
      }

      case GeckoProcessType_Socket: {
        // mochitest harness force this variable, but we actually do not want
        // that
        UnsetEnvVariable("MOZ_DISABLE_SOCKET_PROCESS"_ns);

        nsresult rv_pref =
            Preferences::SetBool("network.process.enabled", true);
        MOZ_ASSERT(rv_pref == NS_OK, "Error enforcing pref");

        MOZ_ASSERT(net::gIOService, "No gIOService?");

        net::gIOService->CallOrWaitForSocketProcess([mProcessPromise]() {
          // If socket process was previously disabled by env,
          // nsIOService code will take some time before it creates the new
          // process and it triggers this callback
          net::SocketProcessParent* parent =
              net::SocketProcessParent::GetSingleton();
          if (parent) {
            return mProcessPromise->Resolve(
                std::move(InitializeSandboxTestingActors(parent)), __func__);
          }
          return mProcessPromise->Reject(NS_ERROR_FAILURE, __func__);
        });
        break;
      }

      default:
        MOZ_ASSERT_UNREACHABLE(
            "SandboxTest does not yet support this process type");
        return NS_ERROR_ILLEGAL_VALUE;
    }

    RefPtr<SandboxTest> self = this;
    RefPtr<ProcessPromise> aPromise(mProcessPromise);
    aPromise->Then(
        GetMainThreadSerialEventTarget(), __func__,
        [self, type](SandboxTestingParent* aValue) {
          self->mSandboxTestingParents[type] = std::move(aValue);
          return NS_OK;
        },
        [](nsresult aError) {
          if (aError == NS_OK) {
            // There is no such process for this OS.  Report test done.
            nsCOMPtr<nsIObserverService> observerService =
                mozilla::services::GetObserverService();
            MOZ_RELEASE_ASSERT(observerService);
            observerService->NotifyObservers(nullptr, "sandbox-test-done",
                                             nullptr);
            return NS_OK;
          }
          MOZ_ASSERT_UNREACHABLE("SandboxTest; failure to get a process");
          return NS_ERROR_FAILURE;
        });
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
