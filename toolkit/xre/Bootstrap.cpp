/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Bootstrap.h"
#include "nsXPCOM.h"

#include "AutoSQLiteLifetime.h"

namespace mozilla {

class BootstrapImpl final : public Bootstrap
{
protected:
  AutoSQLiteLifetime mSQLLT;

  virtual void Dispose() override
  {
    delete this;
  }

public:
  BootstrapImpl()
  {
  }

  ~BootstrapImpl()
  {
  }

  virtual void NS_LogInit() override {
    ::NS_LogInit();
  }

  virtual void NS_LogTerm() override {
    ::NS_LogTerm();
  }

  virtual void XRE_TelemetryAccumulate(int aID, uint32_t aSample) override {
    ::XRE_TelemetryAccumulate(aID, aSample);
  }

  virtual void XRE_StartupTimelineRecord(int aEvent, mozilla::TimeStamp aWhen) override {
    ::XRE_StartupTimelineRecord(aEvent, aWhen);
  }

  virtual int XRE_main(int argc, char* argv[], const BootstrapConfig& aConfig) override {
    return ::XRE_main(argc, argv, aConfig);
  }

  virtual void XRE_StopLateWriteChecks() override {
    ::XRE_StopLateWriteChecks();
  }

  virtual int XRE_XPCShellMain(int argc, char** argv, char** envp, const XREShellData* aShellData) override {
    return ::XRE_XPCShellMain(argc, argv, envp, aShellData);
  }

  virtual GeckoProcessType XRE_GetProcessType() override {
    return ::XRE_GetProcessType();
  }

  virtual void XRE_SetProcessType(const char* aProcessTypeString) override {
    ::XRE_SetProcessType(aProcessTypeString);
  }

  virtual nsresult XRE_InitChildProcess(int argc, char* argv[], const XREChildData* aChildData) override {
    return ::XRE_InitChildProcess(argc, argv, aChildData);
  }

  virtual void XRE_EnableSameExecutableForContentProc() override {
    ::XRE_EnableSameExecutableForContentProc();
  }

#ifdef MOZ_WIDGET_ANDROID
  virtual void GeckoStart(JNIEnv* aEnv, char** argv, int argc, const StaticXREAppData& aAppData) override {
    ::GeckoStart(aEnv, argv, argc, aAppData);
  }

  virtual void XRE_SetAndroidChildFds(JNIEnv* aEnv, const XRE_AndroidChildFds& aFds) override {
    ::XRE_SetAndroidChildFds(aEnv, aFds);
  }
#endif

#ifdef LIBFUZZER
  virtual void XRE_LibFuzzerSetDriver(LibFuzzerDriver aDriver) override {
    ::XRE_LibFuzzerSetDriver(aDriver);
  }
#endif

#ifdef MOZ_IPDL_TESTS
  virtual int XRE_RunIPDLTest(int argc, char **argv) override {
    return ::XRE_RunIPDLTest(argc, argv);
  }
#endif
};

extern "C" NS_EXPORT void NS_FROZENCALL
XRE_GetBootstrap(Bootstrap::UniquePtr& b)
{
  static bool sBootstrapInitialized = false;
  MOZ_RELEASE_ASSERT(!sBootstrapInitialized);

  sBootstrapInitialized = true;
  b.reset(new BootstrapImpl());
}

} // namespace mozilla
