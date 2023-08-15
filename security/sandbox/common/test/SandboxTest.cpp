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
#include "mozilla/ipc/UtilityProcessManager.h"
#include "mozilla/ipc/UtilityProcessParent.h"
#include "mozilla/ipc/UtilityProcessSandboxing.h"
#include "GMPService.h"
#include "mozilla/gmp/GMPTypes.h"
#include "mozilla/ipc/Endpoint.h"
#include "nsIOService.h"

#ifdef XP_WIN
#  include "nsAppDirectoryServiceDefs.h"
#endif

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
void InitializeSandboxTestingActors(
    Actor* aActor,
    const RefPtr<SandboxTest::ProcessPromise::Private>& aProcessPromise) {
  MOZ_ASSERT(aActor, "Should have provided an IPC actor");
  Endpoint<PSandboxTestingParent> sandboxTestingParentEnd;
  Endpoint<PSandboxTestingChild> sandboxTestingChildEnd;
  nsresult rv = PSandboxTesting::CreateEndpoints(&sandboxTestingParentEnd,
                                                 &sandboxTestingChildEnd);
  if (NS_FAILED(rv)) {
    aProcessPromise->Reject(NS_ERROR_FAILURE, __func__);
    return;
  }

  // GMPlugin binds us to the GMP Thread, so we need IPC's Send to be done on
  // the same thread
  Unused << aActor->SendInitSandboxTesting(std::move(sandboxTestingChildEnd));
  // But then the SandboxTestingParent::Create() call needs to be on the main
  // thread
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "SandboxTestingParent::Create",
      [stpE = std::move(sandboxTestingParentEnd), aProcessPromise]() mutable {
        return aProcessPromise->Resolve(
            SandboxTestingParent::Create(std::move(stpE)), __func__);
      }));
}

NS_IMETHODIMP
SandboxTest::StartTests(const nsTArray<nsCString>& aProcessesList) {
  MOZ_ASSERT(NS_IsMainThread());

#if defined(XP_WIN)
  nsCOMPtr<nsIFile> testFile;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(testFile));
  MOZ_ASSERT(testFile);
  nsCOMPtr<nsIFile> testChromeFile;
  testFile->Clone(getter_AddRefs(testChromeFile));
  testChromeFile->Append(u"chrome"_ns);
  testChromeFile->Exists(&mChromeDirExisted);
  testFile->Append(u"sandboxTest.txt"_ns);
  testChromeFile->Append(u"sandboxTest.txt"_ns);
  MOZ_ALWAYS_SUCCEEDS(testFile->Create(nsIFile::NORMAL_FILE_TYPE, 0666));
  MOZ_ALWAYS_SUCCEEDS(testChromeFile->Create(nsIFile::NORMAL_FILE_TYPE, 0666));
#endif

  for (const auto& processTypeName : aProcessesList) {
    SandboxingKind sandboxingKind = SandboxingKind::COUNT;
    GeckoProcessType type = GeckoProcessType::GeckoProcessType_Invalid;
    if (processTypeName.Find(":") != kNotFound) {
      int32_t pos = processTypeName.Find(":");
      nsCString processType = nsCString(Substring(processTypeName, 0, pos));
      nsCString sandboxKindStr = nsCString(
          Substring(processTypeName, pos + 1, processTypeName.Length()));

      nsresult err;
      uint64_t sbVal = (uint64_t)(sandboxKindStr.ToDouble(&err));
      if (NS_FAILED(err)) {
        NS_WARNING("Unable to get SandboxingKind");
        return NS_ERROR_ILLEGAL_VALUE;
      }

      if (sbVal >= SandboxingKind::COUNT) {
        NS_WARNING("Invalid sandboxing kind");
        return NS_ERROR_ILLEGAL_VALUE;
      }

      if (!processType.Equals(
              XRE_GeckoProcessTypeToString(GeckoProcessType_Utility))) {
        NS_WARNING("Expected utility process type");
        return NS_ERROR_ILLEGAL_VALUE;
      }

      sandboxingKind = (SandboxingKind)sbVal;
      type = GeckoProcessType_Utility;
    } else {
      type = GeckoProcessStringToType(processTypeName);

      if (type == GeckoProcessType::GeckoProcessType_Invalid) {
        NS_WARNING("Invalid process type");
        return NS_ERROR_ILLEGAL_VALUE;
      }
    }

    RefPtr<ProcessPromise::Private> processPromise =
        MakeRefPtr<ProcessPromise::Private>(__func__);

    switch (type) {
      case GeckoProcessType_Content: {
        nsTArray<ContentParent*> parents;
        ContentParent::GetAll(parents);
        if (parents[0]) {
          InitializeSandboxTestingActors(parents[0], processPromise);
        } else {
          processPromise->Reject(NS_ERROR_FAILURE, __func__);
          MOZ_ASSERT_UNREACHABLE("SandboxTest; failure to get Content process");
        }
        break;
      }

      case GeckoProcessType_GPU: {
        gfx::GPUProcessManager* gpuProc = gfx::GPUProcessManager::Get();
        gfx::GPUChild* gpuChild = gpuProc ? gpuProc->GetGPUChild() : nullptr;
        if (gpuChild) {
          InitializeSandboxTestingActors(gpuChild, processPromise);
        } else {
          processPromise->Reject(NS_OK, __func__);
        }
        break;
      }

      case GeckoProcessType_RDD: {
        RDDProcessManager* rddProc = RDDProcessManager::Get();
        rddProc->LaunchRDDProcess()->Then(
            GetMainThreadSerialEventTarget(), __func__,
            [processPromise, rddProc]() {
              RDDChild* rddChild = rddProc ? rddProc->GetRDDChild() : nullptr;
              if (rddChild) {
                return InitializeSandboxTestingActors(rddChild, processPromise);
              }
              return processPromise->Reject(NS_ERROR_FAILURE, __func__);
            },
            [processPromise](nsresult aError) {
              MOZ_ASSERT_UNREACHABLE("SandboxTest; failure to get RDD process");
              return processPromise->Reject(aError, __func__);
            });
        break;
      }

      case GeckoProcessType_GMPlugin: {
        UnsetEnvVariable("MOZ_DISABLE_GMP_SANDBOX"_ns);
        RefPtr<gmp::GeckoMediaPluginService> service =
            gmp::GeckoMediaPluginService::GetGeckoMediaPluginService();
        MOZ_ASSERT(service, "We have a GeckoMediaPluginService");

        RefPtr<SandboxTest> self = this;
        nsCOMPtr<nsISerialEventTarget> thread = service->GetGMPThread();
        nsresult rv = thread->Dispatch(NS_NewRunnableFunction(
            "SandboxTest::GMPlugin", [self, processPromise, service, thread]() {
              service->GetContentParentForTest()->Then(
                  thread, __func__,
                  [self, processPromise](
                      const RefPtr<gmp::GMPContentParentCloseBlocker>&
                          wrapper) {
                    RefPtr<gmp::GMPContentParent> parent = wrapper->mParent;
                    MOZ_ASSERT(parent,
                               "Wrapper should wrap a valid parent if we're in "
                               "this path.");
                    if (!parent) {
                      return processPromise->Reject(NS_ERROR_ILLEGAL_VALUE,
                                                    __func__);
                    }
                    NS_DispatchToMainThread(NS_NewRunnableFunction(
                        "SandboxTesting::Wrapper", [self, wrapper]() {
                          self->mGMPContentParentWrapper = wrapper;
                        }));
                    return InitializeSandboxTestingActors(parent.get(),
                                                          processPromise);
                  },
                  [processPromise](const MediaResult& rv) {
                    return processPromise->Reject(NS_ERROR_FAILURE, __func__);
                  });
            }));
        NS_ENSURE_SUCCESS(rv, rv);
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

        net::gIOService->CallOrWaitForSocketProcess([processPromise]() {
          // If socket process was previously disabled by env,
          // nsIOService code will take some time before it creates the new
          // process and it triggers this callback
          net::SocketProcessParent* parent =
              net::SocketProcessParent::GetSingleton();
          if (parent) {
            return InitializeSandboxTestingActors(parent, processPromise);
          }
          return processPromise->Reject(NS_ERROR_FAILURE, __func__);
        });
        break;
      }

      case GeckoProcessType_Utility: {
        RefPtr<UtilityProcessManager> utilityProc =
            UtilityProcessManager::GetSingleton();
        utilityProc->LaunchProcess(sandboxingKind)
            ->Then(
                GetMainThreadSerialEventTarget(), __func__,
                [processPromise, utilityProc, sandboxingKind]() {
                  RefPtr<UtilityProcessParent> utilityParent =
                      utilityProc
                          ? utilityProc->GetProcessParent(sandboxingKind)
                          : nullptr;
                  if (utilityParent) {
                    return InitializeSandboxTestingActors(utilityParent.get(),
                                                          processPromise);
                  }
                  return processPromise->Reject(NS_ERROR_FAILURE, __func__);
                },
                [processPromise](nsresult aError) {
                  MOZ_ASSERT_UNREACHABLE(
                      "SandboxTest; failure to get Utility process");
                  return processPromise->Reject(aError, __func__);
                });
        break;
      }

      default:
        MOZ_ASSERT_UNREACHABLE(
            "SandboxTest does not yet support this process type");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    RefPtr<SandboxTest> self = this;
    RefPtr<ProcessPromise> aPromise(processPromise);
    aPromise->Then(
        GetMainThreadSerialEventTarget(), __func__,
        [self](RefPtr<SandboxTestingParent> aValue) {
          self->mSandboxTestingParents.AppendElement(std::move(aValue));
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
  if (mGMPContentParentWrapper) {
    RefPtr<gmp::GeckoMediaPluginService> service =
        gmp::GeckoMediaPluginService::GetGeckoMediaPluginService();
    MOZ_ASSERT(service, "We have a GeckoMediaPluginService");

    nsCOMPtr<nsISerialEventTarget> thread = service->GetGMPThread();
    nsresult rv = thread->Dispatch(NS_NewRunnableFunction(
        "SandboxTest::FinishTests",
        [wrapper = std::move(mGMPContentParentWrapper)]() {
          // Release mGMPContentWrapper's reference. We hold this to keep an
          // active reference on the CloseBlocker produced by GMPService,
          // otherwise it would automatically shutdown the GMPlugin thread we
          // started.
          // If somehow it does not work as expected, then tests will fail
          // because of leaks happening on GMPService and others.
        }));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  for (RefPtr<SandboxTestingParent>& stp : mSandboxTestingParents) {
    SandboxTestingParent::Destroy(stp.forget());
  }

  // Make sure there is no leftover for test --verify to run without failure
  mSandboxTestingParents.Clear();

#if defined(XP_WIN)
  nsCOMPtr<nsIFile> testFile;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(testFile));
  MOZ_ASSERT(testFile);
  nsCOMPtr<nsIFile> testChromeFile;
  testFile->Clone(getter_AddRefs(testChromeFile));
  testChromeFile->Append(u"chrome"_ns);
  testFile->Append(u"sandboxTest.txt"_ns);
  if (mChromeDirExisted) {
    // Chrome dir existed, just delete test file.
    testChromeFile->Append(u"sandboxTest.txt"_ns);
  }
  testFile->Remove(false);
  testChromeFile->Remove(true);
#endif

  return NS_OK;
}

}  // namespace mozilla

NS_IMPL_COMPONENT_FACTORY(mozISandboxTest) {
  return MakeAndAddRef<SandboxTest>().downcast<nsISupports>();
}
