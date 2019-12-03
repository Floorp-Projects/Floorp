/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file represents the only external interface exposed from libxul. It
 * is used by the various stub binaries (nsBrowserApp, xpcshell,
 * plugin-container) to initialize XPCOM and start their main loop.
 */

#ifndef mozilla_Bootstrap_h
#define mozilla_Bootstrap_h

#include "mozilla/UniquePtr.h"
#include "nscore.h"
#include "nsXULAppAPI.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "jni.h"

extern "C" NS_EXPORT void GeckoStart(JNIEnv* aEnv, char** argv, int argc,
                                     const mozilla::StaticXREAppData& aAppData);
#endif

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
namespace sandbox {
class BrokerServices;
}
#endif

namespace mozilla {

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
namespace sandboxing {
class PermissionsService;
}
#endif

struct BootstrapConfig {
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  /* Chromium sandbox BrokerServices. */
  sandbox::BrokerServices* sandboxBrokerServices;
  sandboxing::PermissionsService* sandboxPermissionsService;
#endif
  /* Pointer to static XRE AppData from application.ini.h */
  const StaticXREAppData* appData;
  /* When the pointer above is null, points to the (string) path of an
   * application.ini file to open and parse.
   * When the pointer above is non-null, may indicate the directory where
   * application files are, relative to the XRE. */
  const char* appDataPath;
};

/**
 * This class is virtual abstract so that using it does not require linking
 * any symbols. The singleton instance of this class is obtained from the
 * exported method XRE_GetBootstrap.
 */
class Bootstrap {
 protected:
  Bootstrap() {}

  // Because of allocator mismatches, code outside libxul shouldn't delete a
  // Bootstrap instance. Use Dispose().
  virtual ~Bootstrap() {}

  /**
   * Destroy and deallocate this Bootstrap instance.
   */
  virtual void Dispose() = 0;

  /**
   * Helper class to use with UniquePtr.
   */
  class BootstrapDelete {
   public:
    constexpr BootstrapDelete() {}
    void operator()(Bootstrap* aPtr) const { aPtr->Dispose(); }
  };

 public:
  typedef mozilla::UniquePtr<Bootstrap, BootstrapDelete> UniquePtr;

  virtual void NS_LogInit() = 0;

  virtual void NS_LogTerm() = 0;

  virtual void XRE_TelemetryAccumulate(int aID, uint32_t aSample) = 0;

  virtual void XRE_StartupTimelineRecord(int aEvent,
                                         mozilla::TimeStamp aWhen) = 0;

  virtual int XRE_main(int argc, char* argv[],
                       const BootstrapConfig& aConfig) = 0;

  virtual void XRE_StopLateWriteChecks() = 0;

  virtual int XRE_XPCShellMain(int argc, char** argv, char** envp,
                               const XREShellData* aShellData) = 0;

  virtual GeckoProcessType XRE_GetProcessType() = 0;

  virtual void XRE_SetProcessType(const char* aProcessTypeString) = 0;

  virtual nsresult XRE_InitChildProcess(int argc, char* argv[],
                                        const XREChildData* aChildData) = 0;

  virtual void XRE_EnableSameExecutableForContentProc() = 0;

#ifdef MOZ_WIDGET_ANDROID
  virtual void GeckoStart(JNIEnv* aEnv, char** argv, int argc,
                          const StaticXREAppData& aAppData) = 0;

  virtual void XRE_SetAndroidChildFds(JNIEnv* aEnv,
                                      const XRE_AndroidChildFds& fds) = 0;
#  ifdef MOZ_PROFILE_GENERATE
  virtual void XRE_WriteLLVMProfData() = 0;
#  endif
#endif

#ifdef LIBFUZZER
  virtual void XRE_LibFuzzerSetDriver(LibFuzzerDriver aDriver) = 0;
#endif

#ifdef MOZ_IPDL_TESTS
  virtual int XRE_RunIPDLTest(int argc, char** argv) = 0;
#endif

#ifdef MOZ_ENABLE_FORKSERVER
  virtual int XRE_ForkServer(int* argc, char*** argv) = 0;
#endif
};

enum class LibLoadingStrategy {
  NoReadAhead,
  ReadAhead,
};

/**
 * Creates and returns the singleton instnace of the bootstrap object.
 * @param `b` is an outparam. We use a parameter and not a return value
 *        because MSVC doesn't let us return a c++ class from a function with
 *        "C" linkage. On failure this will be null.
 * @note This function may only be called once and will crash if called again.
 */
#ifdef XPCOM_GLUE
typedef void (*GetBootstrapType)(Bootstrap::UniquePtr&);
Bootstrap::UniquePtr GetBootstrap(
    const char* aXPCOMFile = nullptr,
    LibLoadingStrategy aLibLoadingStrategy = LibLoadingStrategy::NoReadAhead);
#else
extern "C" NS_EXPORT void NS_FROZENCALL
XRE_GetBootstrap(Bootstrap::UniquePtr& b);

inline Bootstrap::UniquePtr GetBootstrap(const char* aXPCOMFile = nullptr) {
  Bootstrap::UniquePtr bootstrap;
  XRE_GetBootstrap(bootstrap);
  return bootstrap;
}
#endif

}  // namespace mozilla

#endif  // mozilla_Bootstrap_h
