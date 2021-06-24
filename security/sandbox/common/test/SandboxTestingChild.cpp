/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "SandboxTestingChild.h"
#include "SandboxTestingChildTests.h"
#include "SandboxTestingThread.h"

#ifdef XP_LINUX
#  include "mozilla/Sandbox.h"
#endif

#include "nsXULAppAPI.h"

namespace mozilla {

SandboxTestingChild* SandboxTestingChild::sInstance = nullptr;

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
  return true;
}

/* static */
SandboxTestingChild* SandboxTestingChild::GetInstance() {
  MOZ_ASSERT(sInstance, "Must initialize SandboxTestingChild before using it");
  return sInstance;
}

SandboxTestingChild::SandboxTestingChild(
    SandboxTestingThread* aThread, Endpoint<PSandboxTestingChild>&& aEndpoint)
    : mThread(aThread) {
  MOZ_ASSERT(aThread);
  PostToTestThread(NewNonOwningRunnableMethod<Endpoint<PSandboxTestingChild>&&>(
      "SandboxTestingChild::Bind", this, &SandboxTestingChild::Bind,
      std::move(aEndpoint)));
}

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
  delete sInstance;
  sInstance = nullptr;
}

bool SandboxTestingChild::RecvShutDown() {
  Close();
  return true;
}

void SandboxTestingChild::ReportNoTests() {
  SendReportTestResults("dummy_test"_ns, /* shouldSucceed */ true,
                        /* didSucceed */ true,
                        "The test framework fails if there are no cases."_ns);
}

#ifdef XP_UNIX
template <typename F>
void SandboxTestingChild::ErrnoTest(const nsCString& aName, bool aExpectSuccess,
                                    F&& aFunction) {
  int status = aFunction() >= 0 ? 0 : errno;
  PosixTest(aName, aExpectSuccess, status);
}

template <typename F>
void SandboxTestingChild::ErrnoValueTest(const nsCString& aName,
                                         bool aExpectEquals, int aExpectedErrno,
                                         F&& aFunction) {
  int status = aFunction() >= 0 ? 0 : errno;
  PosixTest(aName, aExpectEquals, status == aExpectedErrno);
}

void SandboxTestingChild::PosixTest(const nsCString& aName, bool aExpectSuccess,
                                    int aStatus) {
  bool succeeded = aStatus == 0;
  nsAutoCString message;
  if (succeeded) {
    message = "Succeeded"_ns;
  } else {
    message.AppendPrintf("Error: %s", strerror(aStatus));
  }

  SendReportTestResults(aName, aExpectSuccess, succeeded, message);
}
#endif  // XP_UNIX

}  // namespace mozilla
