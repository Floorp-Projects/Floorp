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
#include "jni.h"

extern "C" NS_EXPORT
void GeckoStart(JNIEnv* aEnv, char** argv, int argc, const mozilla::StaticXREAppData& aAppData);
#endif

namespace mozilla {

/**
 * This class is virtual abstract so that using it does not require linking
 * any symbols. The singleton instance of this class is obtained from the
 * exported method XRE_GetBootstrap.
 */
class Bootstrap
{
protected:
  Bootstrap() { }

  // Because of allocator mismatches, code outside libxul shouldn't delete a
  // Bootstrap instance. Use Dispose().
  virtual ~Bootstrap() { }

  /**
   * Destroy and deallocate this Bootstrap instance.
   */
  virtual void Dispose() = 0;

  /**
   * Helper class to use with UniquePtr.
   */
  class BootstrapDelete
  {
  public:
    constexpr BootstrapDelete() { }
    void operator()(Bootstrap* aPtr) const
    {
      aPtr->Dispose();
    }
  };

public:
  typedef mozilla::UniquePtr<Bootstrap, BootstrapDelete> UniquePtr;

  virtual void NS_LogInit() = 0;

  virtual void NS_LogTerm() = 0;

  virtual nsresult XRE_GetFileFromPath(const char* aPath, nsIFile** aResult) = 0;

  virtual nsresult XRE_ParseAppData(nsIFile* aINIFile, mozilla::XREAppData& aAppData) = 0;

  virtual void XRE_TelemetryAccumulate(int aID, uint32_t aSample) = 0;

  virtual void XRE_StartupTimelineRecord(int aEvent, mozilla::TimeStamp aWhen) = 0;

  virtual int XRE_main(int argc, char* argv[], const mozilla::XREAppData& aAppData) = 0;

  virtual void XRE_StopLateWriteChecks() = 0;

  virtual int XRE_XPCShellMain(int argc, char** argv, char** envp, const XREShellData* aShellData) = 0;

  virtual GeckoProcessType XRE_GetProcessType() = 0;

  virtual void XRE_SetProcessType(const char* aProcessTypeString) = 0;

  virtual nsresult XRE_InitChildProcess(int argc, char* argv[], const XREChildData* aChildData) = 0;

  virtual void XRE_EnableSameExecutableForContentProc() = 0;

#ifdef MOZ_WIDGET_ANDROID
  virtual void GeckoStart(JNIEnv* aEnv, char** argv, int argc, const StaticXREAppData& aAppData) = 0;

  virtual void XRE_SetAndroidChildFds(int aCrashFd, int aIPCFd) = 0;
#endif

#ifdef LIBFUZZER
  virtual void XRE_LibFuzzerSetMain(int argc, char** argv, LibFuzzerMain aMain) = 0;

  virtual void XRE_LibFuzzerGetFuncs(const char* aModuleName, LibFuzzerInitFunc* aInitFunc, LibFuzzerTestingFunc* aTestingFunc) = 0;
#endif

#ifdef MOZ_IPDL_TESTS
  virtual int XRE_RunIPDLTest(int argc, char **argv) = 0;
#endif
};

/**
 * Creates and returns the singleton instnace of the bootstrap object.
 * @param `b` is an outparam. We use a parameter and not a return value
 *        because MSVC doesn't let us return a c++ class from a function with
 *        "C" linkage. On failure this will be null.
 * @note This function may only be called once and will crash if called again.
 */
extern "C" NS_EXPORT void NS_FROZENCALL
XRE_GetBootstrap(Bootstrap::UniquePtr& b);
typedef void (*GetBootstrapType)(Bootstrap::UniquePtr&);

} // namespace mozilla

#endif // mozilla_Bootstrap_h
