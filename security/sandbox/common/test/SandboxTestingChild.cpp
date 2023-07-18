/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "SandboxTestingChild.h"
#include "SandboxTestingChildTests.h"
#include "SandboxTestingThread.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/UtilityProcessSandboxing.h"
#include "mozilla/ipc/UtilityProcessChild.h"

#ifdef XP_LINUX
#  include "mozilla/Sandbox.h"
#endif

#include "nsXULAppAPI.h"

namespace mozilla {

StaticRefPtr<SandboxTestingChild> SandboxTestingChild::sInstance;

bool SandboxTestingChild::IsTestThread() { return mThread->IsOnThread(); }

void SandboxTestingChild::PostToTestThread(
    already_AddRefed<nsIRunnable>&& runnable) {
  mThread->Dispatch(std::move(runnable));
}

/* static */
bool SandboxTestingChild::Initialize(
    Endpoint<PSandboxTestingChild>&& aSandboxTestingEndpoint) {
  MOZ_ASSERT(!sInstance);
  SandboxTestingThread* thread = SandboxTestingThread::Create();
  if (!thread) {
    return false;
  }
  sInstance =
      new SandboxTestingChild(thread, std::move(aSandboxTestingEndpoint));
  thread->Dispatch(NewRunnableMethod<Endpoint<PSandboxTestingChild>&&>(
      "SandboxTestingChild::Bind", sInstance.get(), &SandboxTestingChild::Bind,
      std::move(aSandboxTestingEndpoint)));
  return true;
}

/* static */
SandboxTestingChild* SandboxTestingChild::GetInstance() {
  MOZ_ASSERT(sInstance, "Must initialize SandboxTestingChild before using it");
  return sInstance;
}

SandboxTestingChild::SandboxTestingChild(
    SandboxTestingThread* aThread, Endpoint<PSandboxTestingChild>&& aEndpoint)
    : mThread(aThread) {}

SandboxTestingChild::~SandboxTestingChild() = default;

void SandboxTestingChild::Bind(Endpoint<PSandboxTestingChild>&& aEndpoint) {
  MOZ_RELEASE_ASSERT(mThread->IsOnThread());
  DebugOnly<bool> ok = aEndpoint.Bind(this);
  MOZ_ASSERT(ok);

#ifdef XP_LINUX
  bool sandboxCrashOnError = SetSandboxCrashOnError(false);
#endif

  if (XRE_IsContentProcess()) {
    RunTestsContent(this);
  }

  if (XRE_IsRDDProcess()) {
    RunTestsRDD(this);
  }

  if (XRE_IsGMPluginProcess()) {
    RunTestsGMPlugin(this);
  }

  if (XRE_IsSocketProcess()) {
    RunTestsSocket(this);
  }

  if (XRE_IsGPUProcess()) {
    RunTestsGPU(this);
  }

  if (XRE_IsUtilityProcess()) {
    RefPtr<ipc::UtilityProcessChild> s = ipc::UtilityProcessChild::Get();
    MOZ_ASSERT(s, "Unable to grab a UtilityProcessChild");
    switch (s->mSandbox) {
      case ipc::SandboxingKind::GENERIC_UTILITY:
        RunTestsGenericUtility(this);
        RunTestsUtilityAudioDecoder(this, s->mSandbox);
        break;
#ifdef MOZ_APPLEMEDIA
      case ipc::SandboxingKind::UTILITY_AUDIO_DECODING_APPLE_MEDIA:
        RunTestsUtilityAudioDecoder(this, s->mSandbox);
        break;
#endif
#ifdef XP_WIN
      case ipc::SandboxingKind::UTILITY_AUDIO_DECODING_WMF:
        RunTestsUtilityAudioDecoder(this, s->mSandbox);
        break;
#endif

      default:
        MOZ_ASSERT(false, "Invalid SandboxingKind");
        break;
    }
  }

#ifdef XP_LINUX
  SetSandboxCrashOnError(sandboxCrashOnError);
#endif

  // Tell SandboxTest that this process is done with all tests.
  SendTestCompleted();
}

void SandboxTestingChild::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ASSERT(mThread->IsOnThread());
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "SandboxChildDestroyer", []() { SandboxTestingChild::Destroy(); }));
}

void SandboxTestingChild::Destroy() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sInstance);
  sInstance = nullptr;
}

ipc::IPCResult SandboxTestingChild::RecvShutDown() {
  Close();
  return IPC_OK();
}

void SandboxTestingChild::ReportNoTests() {
  SendReportTestResults("dummy_test"_ns, /* passed */ true,
                        "The test framework fails if there are no cases."_ns);
}

template <typename F>
void SandboxTestingChild::ErrnoTest(const nsCString& aName, bool aExpectSuccess,
                                    F&& aFunction) {
  int status = aFunction() >= 0 ? 0 : errno;
  PosixTest(aName, aExpectSuccess, status);
}

template <typename F>
void SandboxTestingChild::ErrnoValueTest(const nsCString& aName,
                                         int aExpectedErrno, F&& aFunction) {
  int status = aFunction() >= 0 ? 0 : errno;
  PosixTest(aName, aExpectedErrno == 0, status, Some(aExpectedErrno));
}

void SandboxTestingChild::PosixTest(const nsCString& aName, bool aExpectSuccess,
                                    int aStatus, Maybe<int> aExpectedError) {
  nsAutoCString message;
  bool passed;

  // The "expected" arguments are a little redundant.
  MOZ_ASSERT(!aExpectedError || aExpectSuccess == (*aExpectedError == 0));

  // Decide whether the test passed, and stringify the actual result.
  if (aStatus == 0) {
    message = "Succeeded"_ns;
    passed = aExpectSuccess;
  } else {
    message = "Error: "_ns;
    message += strerror(aStatus);
    if (aExpectedError) {
      passed = aStatus == *aExpectedError;
    } else {
      passed = !aExpectSuccess;
    }
  }

  // If something unexpected happened, mention the expected result.
  if (!passed) {
    message += "; expected ";
    if (aExpectSuccess) {
      message += "success";
    } else {
      message += "error";
      if (aExpectedError) {
        message += ": ";
        message += strerror(*aExpectedError);
      }
    }
  }

  SendReportTestResults(aName, passed, message);
}

}  // namespace mozilla
