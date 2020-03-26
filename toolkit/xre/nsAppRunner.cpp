/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"

#include "mozilla/AppShutdown.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Components.h"
#include "mozilla/FilePreferences.h"
#include "mozilla/ChaosMode.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/Poison.h"
#include "mozilla/Preferences.h"
#include "mozilla/Printf.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Utf8.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/JSONWriter.h"
#include "BaseProfiler.h"

#include "nsAppRunner.h"
#include "mozilla/XREAppData.h"
#include "mozilla/Bootstrap.h"
#if defined(MOZ_UPDATER) && !defined(MOZ_WIDGET_ANDROID)
#  include "nsUpdateDriver.h"
#endif
#include "ProfileReset.h"

#ifdef MOZ_INSTRUMENT_EVENT_LOOP
#  include "EventTracer.h"
#endif

#ifdef XP_MACOSX
#  include "nsVersionComparator.h"
#  include "MacLaunchHelper.h"
#  include "MacApplicationDelegate.h"
#  include "MacAutoreleasePool.h"
// these are needed for sysctl
#  include <sys/types.h>
#  include <sys/sysctl.h>
#endif

#include "prnetdb.h"
#include "prprf.h"
#include "prproces.h"
#include "prenv.h"
#include "prtime.h"

#include "nsIAppStartup.h"
#include "nsAppStartupNotifier.h"
#include "nsIMutableArray.h"
#include "nsCommandLine.h"
#include "nsIComponentRegistrar.h"
#include "nsIDialogParamBlock.h"
#include "mozilla/ModuleUtils.h"
#include "nsIIOService.h"
#include "nsIObserverService.h"
#include "nsINativeAppSupport.h"
#include "nsIPlatformInfo.h"
#include "nsIProcess.h"
#include "nsIProfileUnlocker.h"
#include "nsIPromptService.h"
#include "nsIPropertyBag2.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsISupportsPrimitives.h"
#include "nsIToolkitProfile.h"
#include "nsIUUIDGenerator.h"
#include "nsToolkitProfileService.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIWindowCreator.h"
#include "nsIWindowWatcher.h"
#include "nsIXULAppInfo.h"
#include "nsIXULRuntime.h"
#include "nsPIDOMWindow.h"
#include "nsIWidget.h"
#include "nsAppShellCID.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/scache/StartupCache.h"
#include "gfxPlatform.h"

#include "mozilla/Unused.h"

#ifdef XP_WIN
#  include "nsIWinAppHelper.h"
#  include <windows.h>
#  include <intrin.h>
#  include <math.h>
#  include "cairo/cairo-features.h"
#  include "mozilla/WindowsDllBlocklist.h"
#  include "mozilla/WinHeaderOnlyUtils.h"
#  include "mozilla/mscom/ProcessRuntime.h"
#  include "mozilla/widget/AudioSession.h"
#  include "WinTokenUtils.h"

#  if defined(MOZ_LAUNCHER_PROCESS)
#    include "mozilla/LauncherRegistryInfo.h"
#  endif

#  if defined(MOZ_DEFAULT_BROWSER_AGENT)
#    include "nsIWindowsRegKey.h"
#  endif

#  ifndef PROCESS_DEP_ENABLE
#    define PROCESS_DEP_ENABLE 0x1
#  endif
#endif

#if defined(MOZ_SANDBOX)
#  include "mozilla/SandboxSettings.h"
#  if (defined(XP_WIN) || defined(XP_MACOSX))
#    include "nsIUUIDGenerator.h"
#  endif
#endif

#ifdef ACCESSIBILITY
#  include "nsAccessibilityService.h"
#  if defined(XP_WIN)
#    include "mozilla/a11y/Compatibility.h"
#    include "mozilla/a11y/Platform.h"
#  endif
#endif

#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsEmbedCID.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMCIDInternal.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "nsVersionComparator.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsXULAppAPI.h"
#include "nsXREDirProvider.h"

#include "nsINIParser.h"
#include "mozilla/Omnijar.h"
#include "mozilla/StartupTimeline.h"
#include "mozilla/LateWriteChecks.h"

#include <stdlib.h>
#include <locale.h>

#ifdef XP_UNIX
#  include <errno.h>
#  include <pwd.h>
#  include <string.h>
#  include <sys/resource.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

#ifdef XP_WIN
#  include <process.h>
#  include <shlobj.h>
#  include "mozilla/WinDllServices.h"
#  include "nsThreadUtils.h"
#  include <comdef.h>
#  include <wbemidl.h>
#  include "WinUtils.h"
#endif

#ifdef XP_MACOSX
#  include "nsILocalFileMac.h"
#  include "nsCommandLineServiceMac.h"
#endif

// for X remote support
#if defined(MOZ_HAS_REMOTE)
#  include "nsRemoteService.h"
#endif

#if defined(DEBUG) && defined(XP_WIN)
#  include <malloc.h>
#endif

#if defined(XP_MACOSX)
#  include <Carbon/Carbon.h>
#endif

#ifdef DEBUG
#  include "mozilla/Logging.h"
#endif

#ifdef MOZ_JPROF
#  include "jprof.h"
#endif

#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#define NS_CRASHREPORTER_CONTRACTID "@mozilla.org/toolkit/crash-reporter;1"
#include "nsIPrefService.h"
#include "nsIMemoryInfoDumper.h"
#if defined(XP_LINUX) && !defined(ANDROID)
#  include "mozilla/widget/LSBUtils.h"
#endif

#include "base/command_line.h"
#include "GTestRunner.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "GeneratedJNIWrappers.h"
#endif

#if defined(MOZ_SANDBOX)
#  if defined(XP_LINUX) && !defined(ANDROID)
#    include "mozilla/SandboxInfo.h"
#  elif defined(XP_WIN)
#    include "sandboxBroker.h"
#    include "sandboxPermissions.h"
#  endif
#endif

#ifdef MOZ_CODE_COVERAGE
#  include "mozilla/CodeCoverageHandler.h"
#endif

#include "mozilla/mozalloc_oom.h"
#include "SafeMode.h"

#ifdef MOZ_THUNDERBIRD
#  include "nsIPK11TokenDB.h"
#  include "nsIPK11Token.h"
#endif

extern uint32_t gRestartMode;
extern void InstallSignalHandlers(const char* ProgramName);

#define FILE_COMPATIBILITY_INFO NS_LITERAL_CSTRING("compatibility.ini")
#define FILE_INVALIDATE_CACHES NS_LITERAL_CSTRING(".purgecaches")
#define FILE_STARTUP_INCOMPLETE NS_LITERAL_STRING(".startup-incomplete")

#if defined(MOZ_BLOCK_PROFILE_DOWNGRADE) || defined(MOZ_LAUNCHER_PROCESS) || \
    defined(MOZ_DEFAULT_BROWSER_AGENT)
static const char kPrefHealthReportUploadEnabled[] =
    "datareporting.healthreport.uploadEnabled";
#endif  // defined(MOZ_BLOCK_PROFILE_DOWNGRADE) || defined(MOZ_LAUNCHER_PROCESS)
        // || defined(MOZ_DEFAULT_BROWSER_AGENT)
#if defined(MOZ_DEFAULT_BROWSER_AGENT)
static const char kPrefDefaultAgentEnabled[] = "default-browser-agent.enabled";
#endif  // defined(MOZ_DEFAULT_BROWSER_AGENT)

int gArgc;
char** gArgv;

static const char gToolkitVersion[] = MOZ_STRINGIFY(GRE_MILESTONE);
// The gToolkitBuildID global is defined to MOZ_BUILDID via gen_buildid.py
// in toolkit/library. See related comment in toolkit/library/moz.build.
extern const char gToolkitBuildID[];

static nsIProfileLock* gProfileLock;

int gRestartArgc;
char** gRestartArgv;

// If gRestartedByOS is set, we were automatically restarted by the OS.
bool gRestartedByOS = false;

bool gIsGtest = false;

nsString gAbsoluteArgv0Path;

#if defined(MOZ_WIDGET_GTK)
#  include <glib.h>
#  if defined(DEBUG) || defined(NS_BUILD_REFCNT_LOGGING)
#    define CLEANUP_MEMORY 1
#    define PANGO_ENABLE_BACKEND
#    include <pango/pangofc-fontmap.h>
#  endif
#  include <gtk/gtk.h>
#  ifdef MOZ_WAYLAND
#    include <gdk/gdkwayland.h>
#  endif
#  ifdef MOZ_X11
#    include <gdk/gdkx.h>
#  endif /* MOZ_X11 */
#  include <fontconfig/fontconfig.h>
#endif
#include "BinaryPath.h"

#ifdef MOZ_LINKER
extern "C" MFBT_API bool IsSignalHandlingBroken();
#endif

#ifdef FUZZING
#  include "FuzzerRunner.h"

namespace mozilla {
FuzzerRunner* fuzzerRunner = 0;
}  // namespace mozilla

#  ifdef LIBFUZZER
void XRE_LibFuzzerSetDriver(LibFuzzerDriver aDriver) {
  mozilla::fuzzerRunner->setParams(aDriver);
}
#  endif
#endif  // FUZZING

// Undo X11/X.h's definition of None
#undef None

namespace mozilla {
int (*RunGTest)(int*, char**) = 0;
}  // namespace mozilla

using namespace mozilla;
using namespace mozilla::startup;
using mozilla::Unused;
using mozilla::dom::ContentChild;
using mozilla::dom::ContentParent;
using mozilla::dom::quota::QuotaManager;
using mozilla::intl::LocaleService;
using mozilla::scache::StartupCache;

// Save the given word to the specified environment variable.
static void MOZ_NEVER_INLINE SaveWordToEnv(const char* name,
                                           const nsACString& word) {
  char* expr =
      Smprintf("%s=%s", name, PromiseFlatCString(word).get()).release();
  if (expr) PR_SetEnv(expr);
  // We intentionally leak |expr| here since it is required by PR_SetEnv.
}

// Save the path of the given file to the specified environment variable.
static void SaveFileToEnv(const char* name, nsIFile* file) {
#ifdef XP_WIN
  nsAutoString path;
  file->GetPath(path);
  SetEnvironmentVariableW(NS_ConvertASCIItoUTF16(name).get(), path.get());
#else
  nsAutoCString path;
  file->GetNativePath(path);
  SaveWordToEnv(name, path);
#endif
}

static bool gIsExpectedExit = false;

void MozExpectedExit() { gIsExpectedExit = true; }

/**
 * Runs atexit() to catch unexpected exit from 3rd party libraries like the
 * Intel graphics driver calling exit in an error condition. When they
 * call exit() to report an error we won't shutdown correctly and wont catch
 * the issue with our crash reporter.
 */
static void UnexpectedExit() {
  if (!gIsExpectedExit) {
    gIsExpectedExit = true;  // Don't risk re-entrency issues when crashing.
    MOZ_CRASH("Exit called by third party code.");
  }
}

/**
 * Output a string to the user.  This method is really only meant to be used to
 * output last-ditch error messages designed for developers NOT END USERS.
 *
 * @param isError
 *        Pass true to indicate severe errors.
 * @param fmt
 *        printf-style format string followed by arguments.
 */
static MOZ_FORMAT_PRINTF(2, 3) void Output(bool isError, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

#if defined(XP_WIN) && !MOZ_WINCONSOLE
  SmprintfPointer msg = mozilla::Vsmprintf(fmt, ap);
  if (msg) {
    UINT flags = MB_OK;
    if (isError)
      flags |= MB_ICONERROR;
    else
      flags |= MB_ICONINFORMATION;

    wchar_t wide_msg[1024];
    MultiByteToWideChar(CP_ACP, 0, msg.get(), -1, wide_msg,
                        sizeof(wide_msg) / sizeof(wchar_t));

    MessageBoxW(nullptr, wide_msg, L"XULRunner", flags);
  }
#else
  vfprintf(stderr, fmt, ap);
#endif

  va_end(ap);
}

/**
 * Check for a commandline flag. If the flag takes a parameter, the
 * parameter is returned in aParam. Flags may be in the form -arg or
 * --arg (or /arg on win32).
 *
 * @param aArg the parameter to check. Must be lowercase.
 * @param aParam if non-null, the -arg <data> will be stored in this pointer.
 *        This is *not* allocated, but rather a pointer to the argv data.
 * @param aFlags flags @see CheckArgFlag
 */
static ArgResult CheckArg(const char* aArg, const char** aParam = nullptr,
                          CheckArgFlag aFlags = CheckArgFlag::RemoveArg) {
  MOZ_ASSERT(gArgv, "gArgv must be initialized before CheckArg()");
  return CheckArg(gArgc, gArgv, aArg, aParam, aFlags);
}

/**
 * Check for a commandline flag. Ignore data that's passed in with the flag.
 * Flags may be in the form -arg or --arg (or /arg on win32).
 * Will not remove flag if found.
 *
 * @param aArg the parameter to check. Must be lowercase.
 */
static ArgResult CheckArgExists(const char* aArg) {
  return CheckArg(aArg, nullptr, CheckArgFlag::None);
}

bool gSafeMode = false;

/**
 * The nsXULAppInfo object implements nsIFactory so that it can be its own
 * singleton.
 */
class nsXULAppInfo : public nsIXULAppInfo,
                     public nsIObserver,
#ifdef XP_WIN
                     public nsIWinAppHelper,
#endif
                     public nsICrashReporter,
                     public nsIFinishDumpingCallback,
                     public nsIXULRuntime

{
 public:
  constexpr nsXULAppInfo() = default;
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPLATFORMINFO
  NS_DECL_NSIXULAPPINFO
  NS_DECL_NSIXULRUNTIME
  NS_DECL_NSIOBSERVER
  NS_DECL_NSICRASHREPORTER
  NS_DECL_NSIFINISHDUMPINGCALLBACK
#ifdef XP_WIN
  NS_DECL_NSIWINAPPHELPER
#endif
};

NS_INTERFACE_MAP_BEGIN(nsXULAppInfo)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXULRuntime)
  NS_INTERFACE_MAP_ENTRY(nsIXULRuntime)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
#ifdef XP_WIN
  NS_INTERFACE_MAP_ENTRY(nsIWinAppHelper)
#endif
  NS_INTERFACE_MAP_ENTRY(nsICrashReporter)
  NS_INTERFACE_MAP_ENTRY(nsIFinishDumpingCallback)
  NS_INTERFACE_MAP_ENTRY(nsIPlatformInfo)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIXULAppInfo,
                                     gAppData || XRE_IsContentProcess())
NS_INTERFACE_MAP_END

NS_IMETHODIMP_(MozExternalRefCountType)
nsXULAppInfo::AddRef() { return 1; }

NS_IMETHODIMP_(MozExternalRefCountType)
nsXULAppInfo::Release() { return 1; }

NS_IMETHODIMP
nsXULAppInfo::GetVendor(nsACString& aResult) {
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    aResult = cc->GetAppInfo().vendor;
    return NS_OK;
  }
  aResult.Assign(gAppData->vendor);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetName(nsACString& aResult) {
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    aResult = cc->GetAppInfo().name;
    return NS_OK;
  }

#ifdef MOZ_WIDGET_ANDROID
  nsCString name = java::GeckoAppShell::GetAppName()->ToCString();
  aResult.Assign(std::move(name));
#else
  aResult.Assign(gAppData->name);
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetID(nsACString& aResult) {
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    aResult = cc->GetAppInfo().ID;
    return NS_OK;
  }
  aResult.Assign(gAppData->ID);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetVersion(nsACString& aResult) {
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    aResult = cc->GetAppInfo().version;
    return NS_OK;
  }
  aResult.Assign(gAppData->version);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetPlatformVersion(nsACString& aResult) {
  aResult.Assign(gToolkitVersion);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetAppBuildID(nsACString& aResult) {
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    aResult = cc->GetAppInfo().buildID;
    return NS_OK;
  }
  aResult.Assign(gAppData->buildID);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetPlatformBuildID(nsACString& aResult) {
  aResult.Assign(gToolkitBuildID);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetUAName(nsACString& aResult) {
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    aResult = cc->GetAppInfo().UAName;
    return NS_OK;
  }
  aResult.Assign(gAppData->UAName);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetSourceURL(nsACString& aResult) {
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    aResult = cc->GetAppInfo().sourceURL;
    return NS_OK;
  }
  aResult.Assign(gAppData->sourceURL);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetLogConsoleErrors(bool* aResult) {
  *aResult = gLogConsoleErrors;
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::SetLogConsoleErrors(bool aValue) {
  gLogConsoleErrors = aValue;
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetInSafeMode(bool* aResult) {
  *aResult = gSafeMode;
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetOS(nsACString& aResult) {
  aResult.AssignLiteral(OS_TARGET);
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetXPCOMABI(nsACString& aResult) {
#ifdef TARGET_XPCOM_ABI
  aResult.AssignLiteral(TARGET_XPCOM_ABI);
  return NS_OK;
#else
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

NS_IMETHODIMP
nsXULAppInfo::GetWidgetToolkit(nsACString& aResult) {
  aResult.AssignLiteral(MOZ_WIDGET_TOOLKIT);
  return NS_OK;
}

// Ensure that the GeckoProcessType enum, defined in xpcom/build/nsXULAppAPI.h,
// is synchronized with the const unsigned longs defined in
// xpcom/system/nsIXULRuntime.idl.
#define SYNC_ENUMS(a, b)                                                   \
  static_assert(nsIXULRuntime::PROCESS_TYPE_##a ==                         \
                    static_cast<int>(GeckoProcessType_##b),                \
                "GeckoProcessType in nsXULAppAPI.h not synchronized with " \
                "nsIXULRuntime.idl");

SYNC_ENUMS(DEFAULT, Default)
SYNC_ENUMS(PLUGIN, Plugin)
SYNC_ENUMS(CONTENT, Content)
SYNC_ENUMS(IPDLUNITTEST, IPDLUnitTest)
SYNC_ENUMS(GMPLUGIN, GMPlugin)
SYNC_ENUMS(GPU, GPU)
SYNC_ENUMS(VR, VR)
SYNC_ENUMS(RDD, RDD)
SYNC_ENUMS(SOCKET, Socket)
SYNC_ENUMS(SANDBOX_BROKER, RemoteSandboxBroker)
SYNC_ENUMS(FORKSERVER, ForkServer)

// .. and ensure that that is all of them:
static_assert(GeckoProcessType_ForkServer + 1 == GeckoProcessType_End,
              "Did not find the final GeckoProcessType");

NS_IMETHODIMP
nsXULAppInfo::GetProcessType(uint32_t* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = XRE_GetProcessType();
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetProcessID(uint32_t* aResult) {
#ifdef XP_WIN
  *aResult = GetCurrentProcessId();
#else
  *aResult = getpid();
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetUniqueProcessID(uint64_t* aResult) {
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    *aResult = cc->GetID();
  } else {
    *aResult = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetRemoteType(nsAString& aRemoteType) {
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    aRemoteType.Assign(cc->GetRemoteType());
  } else {
    SetDOMStringToNull(aRemoteType);
  }

  return NS_OK;
}

static nsCString gLastAppVersion;
static nsCString gLastAppBuildID;

NS_IMETHODIMP
nsXULAppInfo::GetLastAppVersion(nsACString& aResult) {
  if (XRE_IsContentProcess()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!gLastAppVersion.IsVoid() && gLastAppVersion.IsEmpty()) {
    NS_WARNING("Attempt to retrieve lastAppVersion before it has been set.");
    return NS_ERROR_NOT_AVAILABLE;
  }

  aResult.Assign(gLastAppVersion);
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetLastAppBuildID(nsACString& aResult) {
  if (XRE_IsContentProcess()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!gLastAppBuildID.IsVoid() && gLastAppBuildID.IsEmpty()) {
    NS_WARNING("Attempt to retrieve lastAppBuildID before it has been set.");
    return NS_ERROR_NOT_AVAILABLE;
  }

  aResult.Assign(gLastAppBuildID);
  return NS_OK;
}

static bool gBrowserTabsRemoteAutostart = false;
static uint64_t gBrowserTabsRemoteStatus = 0;
static bool gBrowserTabsRemoteAutostartInitialized = false;

NS_IMETHODIMP
nsXULAppInfo::Observe(nsISupports* aSubject, const char* aTopic,
                      const char16_t* aData) {
  if (!nsCRT::strcmp(aTopic, "getE10SBlocked")) {
    nsCOMPtr<nsISupportsPRUint64> ret = do_QueryInterface(aSubject);
    if (!ret) return NS_ERROR_FAILURE;

    ret->SetData(gBrowserTabsRemoteStatus);

    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULAppInfo::GetBrowserTabsRemoteAutostart(bool* aResult) {
  *aResult = BrowserTabsRemoteAutostart();
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetMaxWebProcessCount(uint32_t* aResult) {
  *aResult = mozilla::GetMaxWebProcessCount();
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetAccessibilityEnabled(bool* aResult) {
#ifdef ACCESSIBILITY
  *aResult = GetAccService() != nullptr;
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetAccessibleHandlerUsed(bool* aResult) {
#if defined(ACCESSIBILITY) && defined(XP_WIN)
  *aResult = Preferences::GetBool("accessibility.handler.enabled", false) &&
             a11y::IsHandlerRegistered();
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetAccessibilityInstantiator(nsAString& aInstantiator) {
#if defined(ACCESSIBILITY) && defined(XP_WIN)
  if (!GetAccService()) {
    aInstantiator = NS_LITERAL_STRING("");
    return NS_OK;
  }
  nsAutoString ipClientInfo;
  a11y::Compatibility::GetHumanReadableConsumersStr(ipClientInfo);
  aInstantiator.Append(ipClientInfo);
  aInstantiator.AppendLiteral("|");

  nsCOMPtr<nsIFile> oopClientExe;
  if (a11y::GetInstantiator(getter_AddRefs(oopClientExe))) {
    nsAutoString oopClientInfo;
    if (NS_SUCCEEDED(oopClientExe->GetPath(oopClientInfo))) {
      aInstantiator.Append(oopClientInfo);
    }
  }
#else
  aInstantiator = NS_LITERAL_STRING("");
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetShouldBlockIncompatJaws(bool* aResult) {
  *aResult = false;
#if defined(ACCESSIBILITY) && defined(XP_WIN)
  *aResult = mozilla::a11y::Compatibility::IsOldJAWS();
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetIs64Bit(bool* aResult) {
#ifdef HAVE_64BIT_BUILD
  *aResult = true;
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::EnsureContentProcess() {
  if (!XRE_IsParentProcess()) return NS_ERROR_NOT_AVAILABLE;

  RefPtr<ContentParent> unused = ContentParent::GetNewOrUsedBrowserProcess(
      nullptr, NS_LITERAL_STRING(DEFAULT_REMOTE_TYPE));
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::InvalidateCachesOnRestart() {
  nsCOMPtr<nsIFile> file;
  nsresult rv =
      NS_GetSpecialDirectory(NS_APP_PROFILE_DIR_STARTUP, getter_AddRefs(file));
  if (NS_FAILED(rv)) return rv;
  if (!file) return NS_ERROR_NOT_AVAILABLE;

  file->AppendNative(FILE_COMPATIBILITY_INFO);

  nsINIParser parser;
  rv = parser.Init(file);
  if (NS_FAILED(rv)) {
    // This fails if compatibility.ini is not there, so we'll
    // flush the caches on the next restart anyways.
    return NS_OK;
  }

  nsAutoCString buf;
  rv = parser.GetString("Compatibility", "InvalidateCaches", buf);

  if (NS_FAILED(rv)) {
    PRFileDesc* fd;
    rv = file->OpenNSPRFileDesc(PR_RDWR | PR_APPEND, 0600, &fd);
    if (NS_FAILED(rv)) {
      NS_ERROR("could not create output stream");
      return NS_ERROR_NOT_AVAILABLE;
    }
    static const char kInvalidationHeader[] =
        NS_LINEBREAK "InvalidateCaches=1" NS_LINEBREAK;
    PR_Write(fd, kInvalidationHeader, sizeof(kInvalidationHeader) - 1);
    PR_Close(fd);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetReplacedLockTime(PRTime* aReplacedLockTime) {
  if (!gProfileLock) return NS_ERROR_NOT_AVAILABLE;
  gProfileLock->GetReplacedLockTime(aReplacedLockTime);
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetIsReleaseOrBeta(bool* aResult) {
#ifdef RELEASE_OR_BETA
  *aResult = true;
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetIsOfficialBranding(bool* aResult) {
#ifdef MOZ_OFFICIAL_BRANDING
  *aResult = true;
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetDefaultUpdateChannel(nsACString& aResult) {
  aResult.AssignLiteral(MOZ_STRINGIFY(MOZ_UPDATE_CHANNEL));
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetDistributionID(nsACString& aResult) {
  aResult.AssignLiteral(MOZ_DISTRIBUTION_ID);
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetWindowsDLLBlocklistStatus(bool* aResult) {
#if defined(HAS_DLL_BLOCKLIST)
  *aResult = DllBlocklist_CheckStatus();
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetRestartedByOS(bool* aResult) {
  *aResult = gRestartedByOS;
  return NS_OK;
}

#if defined(XP_WIN) && defined(MOZ_LAUNCHER_PROCESS)
// Forward declaration
void SetupLauncherProcessPref();

static Maybe<LauncherRegistryInfo::EnabledState> gLauncherProcessState;
#endif  // defined(XP_WIN) && defined(MOZ_LAUNCHER_PROCESS)

NS_IMETHODIMP
nsXULAppInfo::GetLauncherProcessState(uint32_t* aResult) {
#if defined(XP_WIN) && defined(MOZ_LAUNCHER_PROCESS)
  SetupLauncherProcessPref();

  if (!gLauncherProcessState) {
    return NS_ERROR_UNEXPECTED;
  }

  *aResult = static_cast<uint32_t>(gLauncherProcessState.value());
  return NS_OK;
#else
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

#ifdef XP_WIN
NS_IMETHODIMP
nsXULAppInfo::GetUserCanElevate(bool* aUserCanElevate) {
  HANDLE rawToken;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &rawToken)) {
    *aUserCanElevate = false;
    return NS_OK;
  }

  nsAutoHandle token(rawToken);
  LauncherResult<TOKEN_ELEVATION_TYPE> elevationType = GetElevationType(token);
  if (elevationType.isErr()) {
    *aUserCanElevate = false;
    return NS_OK;
  }

  // The possible values returned for elevationType and their meanings are:
  //   TokenElevationTypeDefault: The token does not have a linked token
  //     (e.g. UAC disabled or a standard user, so they can't be elevated)
  //   TokenElevationTypeFull: The token is linked to an elevated token
  //     (e.g. UAC is enabled and the user is already elevated so they can't
  //      be elevated again)
  //   TokenElevationTypeLimited: The token is linked to a limited token
  //     (e.g. UAC is enabled and the user is not elevated, so they can be
  //      elevated)
  *aUserCanElevate = (elevationType.inspect() == TokenElevationTypeLimited);
  return NS_OK;
}
#endif

NS_IMETHODIMP
nsXULAppInfo::GetEnabled(bool* aEnabled) {
  *aEnabled = CrashReporter::GetEnabled();
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::SetEnabled(bool aEnabled) {
  if (aEnabled) {
    if (CrashReporter::GetEnabled()) {
      // no point in erroring for double-enabling
      return NS_OK;
    }

    nsCOMPtr<nsIFile> greBinDir;
    NS_GetSpecialDirectory(NS_GRE_BIN_DIR, getter_AddRefs(greBinDir));
    if (!greBinDir) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIFile> xreBinDirectory = greBinDir;
    if (!xreBinDirectory) {
      return NS_ERROR_FAILURE;
    }

    return CrashReporter::SetExceptionHandler(xreBinDirectory, true);
  }

  if (!CrashReporter::GetEnabled()) {
    // no point in erroring for double-disabling
    return NS_OK;
  }

  return CrashReporter::UnsetExceptionHandler();
}

NS_IMETHODIMP
nsXULAppInfo::GetServerURL(nsIURL** aServerURL) {
  NS_ENSURE_ARG_POINTER(aServerURL);
  if (!CrashReporter::GetEnabled()) return NS_ERROR_NOT_INITIALIZED;

  nsAutoCString data;
  if (!CrashReporter::GetServerURL(data)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), data);
  if (!uri) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIURL> url;
  url = do_QueryInterface(uri);
  NS_ADDREF(*aServerURL = url);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::SetServerURL(nsIURL* aServerURL) {
  // Only allow https or http URLs
  if (!aServerURL->SchemeIs("http") && !aServerURL->SchemeIs("https")) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoCString spec;
  nsresult rv = aServerURL->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  return CrashReporter::SetServerURL(spec);
}

NS_IMETHODIMP
nsXULAppInfo::GetMinidumpPath(nsIFile** aMinidumpPath) {
  if (!CrashReporter::GetEnabled()) return NS_ERROR_NOT_INITIALIZED;

  nsAutoString path;
  if (!CrashReporter::GetMinidumpPath(path)) return NS_ERROR_FAILURE;

  nsresult rv = NS_NewLocalFile(path, false, aMinidumpPath);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::SetMinidumpPath(nsIFile* aMinidumpPath) {
  nsAutoString path;
  nsresult rv = aMinidumpPath->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);
  return CrashReporter::SetMinidumpPath(path);
}

NS_IMETHODIMP
nsXULAppInfo::GetMinidumpForID(const nsAString& aId, nsIFile** aMinidump) {
  if (!CrashReporter::GetMinidumpForID(aId, aMinidump)) {
    return NS_ERROR_FILE_NOT_FOUND;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetExtraFileForID(const nsAString& aId, nsIFile** aExtraFile) {
  if (!CrashReporter::GetExtraFileForID(aId, aExtraFile)) {
    return NS_ERROR_FILE_NOT_FOUND;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::AnnotateCrashReport(const nsACString& key,
                                  const nsACString& data) {
  CrashReporter::Annotation annotation;

  if (!AnnotationFromString(annotation, PromiseFlatCString(key).get())) {
    return NS_ERROR_INVALID_ARG;
  }

  return CrashReporter::AnnotateCrashReport(annotation, data);
}

NS_IMETHODIMP
nsXULAppInfo::RemoveCrashReportAnnotation(const nsACString& key) {
  CrashReporter::Annotation annotation;

  if (!AnnotationFromString(annotation, PromiseFlatCString(key).get())) {
    return NS_ERROR_INVALID_ARG;
  }

  return CrashReporter::RemoveCrashReportAnnotation(annotation);
}

NS_IMETHODIMP
nsXULAppInfo::IsAnnotationWhitelistedForPing(const nsACString& aValue,
                                             bool* aIsWhitelisted) {
  CrashReporter::Annotation annotation;

  if (!AnnotationFromString(annotation, PromiseFlatCString(aValue).get())) {
    return NS_ERROR_INVALID_ARG;
  }

  *aIsWhitelisted = CrashReporter::IsAnnotationWhitelistedForPing(annotation);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::AppendAppNotesToCrashReport(const nsACString& data) {
  return CrashReporter::AppendAppNotesToCrashReport(data);
}

NS_IMETHODIMP
nsXULAppInfo::RegisterAppMemory(uint64_t pointer, uint64_t len) {
  return CrashReporter::RegisterAppMemory((void*)pointer, len);
}

NS_IMETHODIMP
nsXULAppInfo::WriteMinidumpForException(void* aExceptionInfo) {
#ifdef XP_WIN
  return CrashReporter::WriteMinidumpForException(
      static_cast<EXCEPTION_POINTERS*>(aExceptionInfo));
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsXULAppInfo::AppendObjCExceptionInfoToAppNotes(void* aException) {
#ifdef XP_MACOSX
  return CrashReporter::AppendObjCExceptionInfoToAppNotes(aException);
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsXULAppInfo::GetSubmitReports(bool* aEnabled) {
  return CrashReporter::GetSubmitReports(aEnabled);
}

NS_IMETHODIMP
nsXULAppInfo::SetSubmitReports(bool aEnabled) {
  return CrashReporter::SetSubmitReports(aEnabled);
}

NS_IMETHODIMP
nsXULAppInfo::UpdateCrashEventsDir() {
  CrashReporter::UpdateCrashEventsDir();
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::SaveMemoryReport() {
  if (!CrashReporter::GetEnabled()) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsCOMPtr<nsIFile> file;
  nsresult rv = CrashReporter::GetDefaultMemoryReportFile(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsString path;
  file->GetPath(path);

  nsCOMPtr<nsIMemoryInfoDumper> dumper =
      do_GetService("@mozilla.org/memory-info-dumper;1");
  if (NS_WARN_IF(!dumper)) {
    return NS_ERROR_UNEXPECTED;
  }

  rv = dumper->DumpMemoryReportsToNamedFile(path, this, file,
                                            true /* anonymize */);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

// This method is from nsIFInishDumpingCallback.
NS_IMETHODIMP
nsXULAppInfo::Callback(nsISupports* aData) {
  nsCOMPtr<nsIFile> file = do_QueryInterface(aData);
  MOZ_ASSERT(file);

  CrashReporter::SetMemoryReportFile(file);
  return NS_OK;
}

static const nsXULAppInfo kAppInfo;
namespace mozilla {
nsresult AppInfoConstructor(nsISupports* aOuter, REFNSIID aIID,
                            void** aResult) {
  NS_ENSURE_NO_AGGREGATION(aOuter);

  return const_cast<nsXULAppInfo*>(&kAppInfo)->QueryInterface(aIID, aResult);
}
}  // namespace mozilla

bool gLogConsoleErrors = false;

#define NS_ENSURE_TRUE_LOG(x, ret)               \
  PR_BEGIN_MACRO                                 \
  if (MOZ_UNLIKELY(!(x))) {                      \
    NS_WARNING("NS_ENSURE_TRUE(" #x ") failed"); \
    gLogConsoleErrors = true;                    \
    return ret;                                  \
  }                                              \
  PR_END_MACRO

#define NS_ENSURE_SUCCESS_LOG(res, ret) \
  NS_ENSURE_TRUE_LOG(NS_SUCCEEDED(res), ret)

/**
 * Because we're starting/stopping XPCOM several times in different scenarios,
 * this class is a stack-based critter that makes sure that XPCOM is shut down
 * during early returns.
 */

class ScopedXPCOMStartup {
 public:
  ScopedXPCOMStartup() : mServiceManager(nullptr) {}
  ~ScopedXPCOMStartup();

  nsresult Initialize(bool aInitJSContext = true);
  nsresult SetWindowCreator(nsINativeAppSupport* native);

 private:
  nsIServiceManager* mServiceManager;
  static nsINativeAppSupport* gNativeAppSupport;

  friend already_AddRefed<nsINativeAppSupport> NS_GetNativeAppSupport();
};

ScopedXPCOMStartup::~ScopedXPCOMStartup() {
  NS_IF_RELEASE(gNativeAppSupport);

  if (mServiceManager) {
#ifdef XP_MACOSX
    // On OS X, we need a pool to catch cocoa objects that are autoreleased
    // during teardown.
    mozilla::MacAutoreleasePool pool;
#endif

    nsCOMPtr<nsIAppStartup> appStartup(components::AppStartup::Service());
    if (appStartup) appStartup->DestroyHiddenWindow();

    gDirServiceProvider->DoShutdown();
    PROFILER_ADD_MARKER("Shutdown early", OTHER);

    WriteConsoleLog();

    NS_ShutdownXPCOM(mServiceManager);
    mServiceManager = nullptr;
  }
}

// {5F5E59CE-27BC-47eb-9D1F-B09CA9049836}
static const nsCID kProfileServiceCID = {
    0x5f5e59ce,
    0x27bc,
    0x47eb,
    {0x9d, 0x1f, 0xb0, 0x9c, 0xa9, 0x4, 0x98, 0x36}};

static already_AddRefed<nsIFactory> ProfileServiceFactoryConstructor(
    const mozilla::Module& module, const mozilla::Module::CIDEntry& entry) {
  nsCOMPtr<nsIFactory> factory;
  NS_NewToolkitProfileFactory(getter_AddRefs(factory));
  return factory.forget();
}

static const mozilla::Module::CIDEntry kXRECIDs[] = {
    {&kProfileServiceCID, false, ProfileServiceFactoryConstructor, nullptr},
    {nullptr}};

static const mozilla::Module::ContractIDEntry kXREContracts[] = {
    {NS_PROFILESERVICE_CONTRACTID, &kProfileServiceCID}, {nullptr}};

extern const mozilla::Module kXREModule = {mozilla::Module::kVersion, kXRECIDs,
                                           kXREContracts};

nsresult ScopedXPCOMStartup::Initialize(bool aInitJSContext) {
  NS_ASSERTION(gDirServiceProvider, "Should not get here!");

  nsresult rv;

  rv = NS_InitXPCOM(&mServiceManager, gDirServiceProvider->GetAppDir(),
                    gDirServiceProvider, aInitJSContext);
  if (NS_FAILED(rv)) {
    NS_ERROR("Couldn't start xpcom!");
    mServiceManager = nullptr;
  } else {
#ifdef DEBUG
    nsCOMPtr<nsIComponentRegistrar> reg = do_QueryInterface(mServiceManager);
    NS_ASSERTION(reg, "Service Manager doesn't QI to Registrar.");
#endif
  }

  return rv;
}

/**
 * This is a little factory class that serves as a singleton-service-factory
 * for the nativeappsupport object.
 */
class nsSingletonFactory final : public nsIFactory {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFACTORY

  explicit nsSingletonFactory(nsISupports* aSingleton);

 private:
  ~nsSingletonFactory() = default;
  nsCOMPtr<nsISupports> mSingleton;
};

nsSingletonFactory::nsSingletonFactory(nsISupports* aSingleton)
    : mSingleton(aSingleton) {
  NS_ASSERTION(mSingleton, "Singleton was null!");
}

NS_IMPL_ISUPPORTS(nsSingletonFactory, nsIFactory)

NS_IMETHODIMP
nsSingletonFactory::CreateInstance(nsISupports* aOuter, const nsIID& aIID,
                                   void** aResult) {
  NS_ENSURE_NO_AGGREGATION(aOuter);

  return mSingleton->QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
nsSingletonFactory::LockFactory(bool) { return NS_OK; }

/**
 * Set our windowcreator on the WindowWatcher service.
 */
nsresult ScopedXPCOMStartup::SetWindowCreator(nsINativeAppSupport* native) {
  nsresult rv;

  NS_IF_ADDREF(gNativeAppSupport = native);

  nsCOMPtr<nsIWindowCreator> creator(components::AppStartup::Service());
  if (!creator) return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIWindowWatcher> wwatch(
      do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  return wwatch->SetWindowCreator(creator);
}

/* static */ already_AddRefed<nsINativeAppSupport> NS_GetNativeAppSupport() {
  if (!ScopedXPCOMStartup::gNativeAppSupport) {
    return nullptr;
  }

  return do_AddRef(ScopedXPCOMStartup::gNativeAppSupport);
}

nsINativeAppSupport* ScopedXPCOMStartup::gNativeAppSupport;

static void DumpArbitraryHelp() {
  nsresult rv;

  ScopedLogging log;

  {
    ScopedXPCOMStartup xpcom;
    xpcom.Initialize();

    nsCOMPtr<nsICommandLineRunner> cmdline(new nsCommandLine());

    nsCString text;
    rv = cmdline->GetHelpText(text);
    if (NS_SUCCEEDED(rv)) printf("%s", text.get());
  }
}

// English text needs to go into a dtd file.
// But when this is called we have no components etc. These strings must either
// be here, or in a native resource file.
static void DumpHelp() {
  printf(
      "Usage: %s [ options ... ] [URL]\n"
      "       where options include:\n\n",
      gArgv[0]);

#ifdef MOZ_X11
  printf(
      "X11 options\n"
      "  --display=DISPLAY  X display to use\n"
      "  --sync             Make X calls synchronous\n");
#endif
#ifdef XP_UNIX
  printf(
      "  --g-fatal-warnings Make all warnings fatal\n"
      "\n%s options\n",
      (const char*)gAppData->name);
#endif

  printf(
      "  -h or --help       Print this message.\n"
      "  -v or --version    Print %s version.\n"
      "  -P <profile>       Start with <profile>.\n"
      "  --profile <path>   Start with profile at <path>.\n"
      "  --migration        Start with migration wizard.\n"
      "  --ProfileManager   Start with ProfileManager.\n"
#ifdef MOZ_HAS_REMOTE
      "  --no-remote        Do not accept or send remote commands; implies\n"
      "                     --new-instance.\n"
      "  --new-instance     Open new instance, not a new window in running "
      "instance.\n"
#endif
      "  --UILocale <locale> Start with <locale> resources as UI Locale.\n"
      "  --safe-mode        Disables extensions and themes for this session.\n"
#ifdef MOZ_BLOCK_PROFILE_DOWNGRADE
      "  --allow-downgrade  Allows downgrading a profile.\n"
#endif
      "  --MOZ_LOG=<modules> Treated as MOZ_LOG=<modules> environment "
      "variable,\n"
      "                     overrides it.\n"
      "  --MOZ_LOG_FILE=<file> Treated as MOZ_LOG_FILE=<file> environment "
      "variable,\n"
      "                     overrides it. If MOZ_LOG_FILE is not specified as "
      "an\n"
      "                     argument or as an environment variable, logging "
      "will be\n"
      "                     written to stdout.\n",
      (const char*)gAppData->name);

#if defined(XP_WIN)
  printf("  --console          Start %s with a debugging console.\n",
         (const char*)gAppData->name);
#endif

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) || defined(XP_MACOSX)
  printf("  --headless         Run without a GUI.\n");
#endif

  // this works, but only after the components have registered.  so if you drop
  // in a new command line handler, --help won't not until the second run. out
  // of the bug, because we ship a component.reg file, it works correctly.
  DumpArbitraryHelp();
}

static inline void DumpVersion() {
  if (gAppData->vendor) {
    printf("%s ", (const char*)gAppData->vendor);
  }
  printf("%s ", (const char*)gAppData->name);

  // Use the displayed version
  // For example, for beta, we would display 42.0b2 instead of 42.0
  printf("%s", MOZ_STRINGIFY(MOZ_APP_VERSION_DISPLAY));

  if (gAppData->copyright) {
    printf(", %s", (const char*)gAppData->copyright);
  }
  printf("\n");
}

void XRE_InitOmnijar(nsIFile* greOmni, nsIFile* appOmni) {
  mozilla::Omnijar::Init(greOmni, appOmni);
}

nsresult XRE_GetBinaryPath(nsIFile** aResult) {
  return mozilla::BinaryPath::GetFile(aResult);
}

#ifdef XP_WIN
#  include "nsWindowsRestart.cpp"
#  include <shellapi.h>

typedef BOOL(WINAPI* SetProcessDEPPolicyFunc)(DWORD dwFlags);

static void RegisterApplicationRestartChanged(const char* aPref, void* aData) {
  DWORD cchCmdLine = 0;
  HRESULT rc = ::GetApplicationRestartSettings(::GetCurrentProcess(), nullptr,
                                               &cchCmdLine, nullptr);
  bool wasRegistered = false;
  if (rc == S_OK) {
    wasRegistered = true;
  }

  if (Preferences::GetBool(PREF_WIN_REGISTER_APPLICATION_RESTART, false) &&
      !wasRegistered) {
    // Make the command line to use when restarting.
    // Excludes argv[0] because RegisterApplicationRestart adds the
    // executable name, replace that temporarily with -os-restarted
    char* exeName = gRestartArgv[0];
    gRestartArgv[0] = const_cast<char*>("-os-restarted");
    wchar_t** restartArgvConverted =
        AllocConvertUTF8toUTF16Strings(gRestartArgc, gRestartArgv);
    gRestartArgv[0] = exeName;

    mozilla::UniquePtr<wchar_t[]> restartCommandLine;
    if (restartArgvConverted) {
      restartCommandLine =
          mozilla::MakeCommandLine(gRestartArgc, restartArgvConverted);
      FreeAllocStrings(gRestartArgc, restartArgvConverted);
    }

    if (restartCommandLine) {
      // Flags RESTART_NO_PATCH and RESTART_NO_REBOOT are not set, so we
      // should be restarted if terminated by an update or restart.
      ::RegisterApplicationRestart(restartCommandLine.get(),
                                   RESTART_NO_CRASH | RESTART_NO_HANG);
    }
  } else if (wasRegistered) {
    ::UnregisterApplicationRestart();
  }
}

#  if defined(MOZ_LAUNCHER_PROCESS)

static void OnLauncherPrefChanged(const char* aPref, void* aData) {
  bool prefVal = Preferences::GetBool(PREF_WIN_LAUNCHER_PROCESS_ENABLED, true);

  mozilla::LauncherRegistryInfo launcherRegInfo;
  mozilla::LauncherVoidResult reflectResult =
      launcherRegInfo.ReflectPrefToRegistry(prefVal);
  MOZ_ASSERT(reflectResult.isOk());
}

static void OnLauncherTelemetryPrefChanged(const char* aPref, void* aData) {
  bool prefVal = Preferences::GetBool(kPrefHealthReportUploadEnabled, true);

  mozilla::LauncherRegistryInfo launcherRegInfo;
  mozilla::LauncherVoidResult reflectResult =
      launcherRegInfo.ReflectTelemetryPrefToRegistry(prefVal);
  MOZ_ASSERT(reflectResult.isOk());
}

static void SetupLauncherProcessPref() {
  if (gLauncherProcessState) {
    // We've already successfully run
    return;
  }

  mozilla::LauncherRegistryInfo launcherRegInfo;

  mozilla::LauncherResult<mozilla::LauncherRegistryInfo::EnabledState>
      enabledState = launcherRegInfo.IsEnabled();

  if (enabledState.isOk()) {
    gLauncherProcessState = Some(enabledState.unwrap());

    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::LauncherProcessState,
        static_cast<uint32_t>(enabledState.unwrap()));

    // Reflect the launcher process registry state into user prefs
    Preferences::SetBool(
        PREF_WIN_LAUNCHER_PROCESS_ENABLED,
        enabledState.unwrap() !=
            mozilla::LauncherRegistryInfo::EnabledState::ForceDisabled);
  }

  mozilla::LauncherVoidResult reflectResult =
      launcherRegInfo.ReflectTelemetryPrefToRegistry(
          Preferences::GetBool(kPrefHealthReportUploadEnabled, true));
  MOZ_ASSERT(reflectResult.isOk());

  Preferences::RegisterCallback(&OnLauncherPrefChanged,
                                PREF_WIN_LAUNCHER_PROCESS_ENABLED);
  Preferences::RegisterCallback(&OnLauncherTelemetryPrefChanged,
                                kPrefHealthReportUploadEnabled);
}

#  endif  // defined(MOZ_LAUNCHER_PROCESS)

#  if defined(MOZ_DEFAULT_BROWSER_AGENT)
static void OnDefaultAgentTelemetryPrefChanged(const char* aPref, void* aData) {
  bool prefVal = Preferences::GetBool(aPref, true);

  nsresult rv;
  nsCOMPtr<nsIWindowsRegKey> regKey =
      do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsAutoString keyName;
  keyName.AppendLiteral("SOFTWARE\\" MOZ_APP_VENDOR "\\" MOZ_APP_NAME
                        "\\Default Browser Agent");

  nsCOMPtr<nsIFile> binaryPath;
  rv = XRE_GetBinaryPath(getter_AddRefs(binaryPath));
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<nsIFile> binaryDir;
  rv = binaryPath->GetParent(getter_AddRefs(binaryDir));
  NS_ENSURE_SUCCESS_VOID(rv);

  nsAutoString valueName;
  rv = binaryDir->GetPath(valueName);
  NS_ENSURE_SUCCESS_VOID(rv);

  if (strcmp(aPref, kPrefHealthReportUploadEnabled) == 0) {
    valueName.AppendLiteral("|DisableTelemetry");
  } else if (strcmp(aPref, kPrefDefaultAgentEnabled) == 0) {
    valueName.AppendLiteral("|DisableDefaultBrowserAgent");
  } else {
    return;
  }

  rv = regKey->Create(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER, keyName,
                      nsIWindowsRegKey::ACCESS_WRITE);
  NS_ENSURE_SUCCESS_VOID(rv);

  // We're recording whether the pref is *disabled*, so invert the value.
  rv = regKey->WriteIntValue(valueName, prefVal ? 0 : 1);
  NS_ENSURE_SUCCESS_VOID(rv);
}

#  endif  // defined(MOZ_DEFAULT_BROWSER_AGENT)

#endif  // XP_WIN

void UnlockProfile() {
  if (gProfileLock) {
    gProfileLock->Unlock();
  }
}

// If aBlankCommandLine is true, then the application will be launched with a
// blank command line instead of being launched with the same command line that
// it was initially started with.
nsresult LaunchChild(bool aBlankCommandLine) {
  // Restart this process by exec'ing it into the current process
  // if supported by the platform.  Otherwise, use NSPR.

#ifdef MOZ_JPROF
  // make sure JPROF doesn't think we're E10s
  unsetenv("JPROF_SLAVE");
#endif

  if (aBlankCommandLine) {
    gRestartArgc = 1;
    gRestartArgv[gRestartArgc] = nullptr;
  }

  SaveToEnv("MOZ_LAUNCHED_CHILD=1");
#if defined(MOZ_LAUNCHER_PROCESS)
  SaveToEnv("MOZ_LAUNCHER_PROCESS=1");
#endif  // defined(MOZ_LAUNCHER_PROCESS)

#if !defined(MOZ_WIDGET_ANDROID)  // Android has separate restart code.
#  if defined(XP_MACOSX)
  CommandLineServiceMac::SetupMacCommandLine(gRestartArgc, gRestartArgv, true);
  LaunchChildMac(gRestartArgc, gRestartArgv);
#  else
  nsCOMPtr<nsIFile> lf;
  nsresult rv = XRE_GetBinaryPath(getter_AddRefs(lf));
  if (NS_FAILED(rv)) return rv;

#    if defined(XP_WIN)
  nsAutoString exePath;
  rv = lf->GetPath(exePath);
  if (NS_FAILED(rv)) return rv;

  HANDLE hProcess;
  if (!WinLaunchChild(exePath.get(), gRestartArgc, gRestartArgv, nullptr,
                      &hProcess))
    return NS_ERROR_FAILURE;
  // Keep the current process around until the restarted process has created
  // its message queue, to avoid the launched process's windows being forced
  // into the background.
  mozilla::WaitForInputIdle(hProcess);
  ::CloseHandle(hProcess);

#    else
  nsAutoCString exePath;
  rv = lf->GetNativePath(exePath);
  if (NS_FAILED(rv)) return rv;

#      if defined(XP_UNIX)
  if (execv(exePath.get(), gRestartArgv) == -1) return NS_ERROR_FAILURE;
#      else
  PRProcess* process =
      PR_CreateProcess(exePath.get(), gRestartArgv, nullptr, nullptr);
  if (!process) return NS_ERROR_FAILURE;

  int32_t exitCode;
  PRStatus failed = PR_WaitProcess(process, &exitCode);
  if (failed || exitCode) return NS_ERROR_FAILURE;
#      endif  // XP_UNIX
#    endif    // WP_WIN
#  endif      // WP_MACOSX
#endif        // MOZ_WIDGET_ANDROID

  return NS_ERROR_LAUNCHED_CHILD_PROCESS;
}

static const char kProfileProperties[] =
    "chrome://mozapps/locale/profile/profileSelection.properties";

namespace {

/**
 * This class, instead of a raw nsresult, should be the return type of any
 * function called by SelectProfile that initializes XPCOM.
 */
class ReturnAbortOnError {
 public:
  MOZ_IMPLICIT ReturnAbortOnError(nsresult aRv) { mRv = ConvertRv(aRv); }

  operator nsresult() { return mRv; }

 private:
  inline nsresult ConvertRv(nsresult aRv) {
    if (NS_SUCCEEDED(aRv) || aRv == NS_ERROR_LAUNCHED_CHILD_PROCESS) {
      return aRv;
    }
    return NS_ERROR_ABORT;
  }

  nsresult mRv;
};

}  // namespace

static nsresult ProfileMissingDialog(nsINativeAppSupport* aNative) {
  nsresult rv;

  ScopedXPCOMStartup xpcom;
  rv = xpcom.Initialize();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = xpcom.SetWindowCreator(aNative);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  {  // extra scoping is needed so we release these components before xpcom
     // shutdown
    nsCOMPtr<nsIStringBundleService> sbs =
        mozilla::services::GetStringBundleService();
    NS_ENSURE_TRUE(sbs, NS_ERROR_FAILURE);

    nsCOMPtr<nsIStringBundle> sb;
    sbs->CreateBundle(kProfileProperties, getter_AddRefs(sb));
    NS_ENSURE_TRUE_LOG(sbs, NS_ERROR_FAILURE);

    NS_ConvertUTF8toUTF16 appName(gAppData->name);
    AutoTArray<nsString, 2> params = {appName, appName};

    // profileMissing
    nsAutoString missingMessage;
    rv = sb->FormatStringFromName("profileMissing", params, missingMessage);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_ABORT);

    nsAutoString missingTitle;
    params.SetLength(1);
    rv = sb->FormatStringFromName("profileMissingTitle", params, missingTitle);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_ABORT);

    nsCOMPtr<nsIPromptService> ps(do_GetService(NS_PROMPTSERVICE_CONTRACTID));
    NS_ENSURE_TRUE(ps, NS_ERROR_FAILURE);

    ps->Alert(nullptr, missingTitle.get(), missingMessage.get());

    return NS_ERROR_ABORT;
  }
}

static ReturnAbortOnError ProfileLockedDialog(nsIFile* aProfileDir,
                                              nsIFile* aProfileLocalDir,
                                              nsIProfileUnlocker* aUnlocker,
                                              nsINativeAppSupport* aNative,
                                              nsIProfileLock** aResult) {
  nsresult rv;

  bool exists;
  aProfileDir->Exists(&exists);
  if (!exists) {
    return ProfileMissingDialog(aNative);
  }

  ScopedXPCOMStartup xpcom;
  rv = xpcom.Initialize();
  NS_ENSURE_SUCCESS(rv, rv);

  mozilla::Telemetry::WriteFailedProfileLock(aProfileDir);

  rv = xpcom.SetWindowCreator(aNative);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  {  // extra scoping is needed so we release these components before xpcom
     // shutdown
    nsCOMPtr<nsIStringBundleService> sbs =
        mozilla::services::GetStringBundleService();
    NS_ENSURE_TRUE(sbs, NS_ERROR_FAILURE);

    nsCOMPtr<nsIStringBundle> sb;
    sbs->CreateBundle(kProfileProperties, getter_AddRefs(sb));
    NS_ENSURE_TRUE_LOG(sbs, NS_ERROR_FAILURE);

    NS_ConvertUTF8toUTF16 appName(gAppData->name);
    AutoTArray<nsString, 2> params = {appName, appName};

    nsAutoString killMessage;
#ifndef XP_MACOSX
    rv = sb->FormatStringFromName(
        aUnlocker ? "restartMessageUnlocker" : "restartMessageNoUnlocker",
        params, killMessage);
#else
    rv = sb->FormatStringFromName(
        aUnlocker ? "restartMessageUnlockerMac" : "restartMessageNoUnlockerMac",
        params, killMessage);
#endif
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    params.SetLength(1);
    nsAutoString killTitle;
    rv = sb->FormatStringFromName("restartTitle", params, killTitle);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    if (gfxPlatform::IsHeadless()) {
      // TODO: make a way to turn off all dialogs when headless.
      Output(true, "%s\n", NS_LossyConvertUTF16toASCII(killMessage).get());
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIPromptService> ps(do_GetService(NS_PROMPTSERVICE_CONTRACTID));
    NS_ENSURE_TRUE(ps, NS_ERROR_FAILURE);

    if (aUnlocker) {
      int32_t button;
#ifdef MOZ_WIDGET_ANDROID
      java::GeckoAppShell::KillAnyZombies();
      button = 0;
#else
      const uint32_t flags = (nsIPromptService::BUTTON_TITLE_IS_STRING *
                              nsIPromptService::BUTTON_POS_0) +
                             (nsIPromptService::BUTTON_TITLE_CANCEL *
                              nsIPromptService::BUTTON_POS_1);

      bool checkState = false;
      rv = ps->ConfirmEx(nullptr, killTitle.get(), killMessage.get(), flags,
                         killTitle.get(), nullptr, nullptr, nullptr,
                         &checkState, &button);
      NS_ENSURE_SUCCESS_LOG(rv, rv);
#endif

      if (button == 0) {
        rv = aUnlocker->Unlock(nsIProfileUnlocker::FORCE_QUIT);
        if (NS_FAILED(rv)) {
          return rv;
        }

        SaveFileToEnv("XRE_PROFILE_PATH", aProfileDir);
        SaveFileToEnv("XRE_PROFILE_LOCAL_PATH", aProfileLocalDir);

        return LaunchChild(false);
      }
    } else {
#ifdef MOZ_WIDGET_ANDROID
      if (java::GeckoAppShell::UnlockProfile()) {
        return NS_LockProfilePath(aProfileDir, aProfileLocalDir, nullptr,
                                  aResult);
      }
#else
      rv = ps->Alert(nullptr, killTitle.get(), killMessage.get());
      NS_ENSURE_SUCCESS_LOG(rv, rv);
#endif
    }

    return NS_ERROR_ABORT;
  }
}

static const char kProfileManagerURL[] =
    "chrome://mozapps/content/profile/profileSelection.xhtml";

static ReturnAbortOnError ShowProfileManager(
    nsIToolkitProfileService* aProfileSvc, nsINativeAppSupport* aNative) {
  nsresult rv;

  nsCOMPtr<nsIFile> profD, profLD;
  bool offline = false;
  int32_t dialogReturn;

  {
    ScopedXPCOMStartup xpcom;
    rv = xpcom.Initialize();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = xpcom.SetWindowCreator(aNative);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

#ifdef XP_MACOSX
    CommandLineServiceMac::SetupMacCommandLine(gRestartArgc, gRestartArgv,
                                               true);
#endif

    {  // extra scoping is needed so we release these components before xpcom
       // shutdown
      nsCOMPtr<nsIWindowWatcher> windowWatcher(
          do_GetService(NS_WINDOWWATCHER_CONTRACTID));
      nsCOMPtr<nsIDialogParamBlock> ioParamBlock(
          do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID));
      nsCOMPtr<nsIMutableArray> dlgArray(
          do_CreateInstance(NS_ARRAY_CONTRACTID));
      NS_ENSURE_TRUE(windowWatcher && ioParamBlock && dlgArray,
                     NS_ERROR_FAILURE);

      ioParamBlock->SetObjects(dlgArray);

      nsCOMPtr<nsIAppStartup> appStartup(components::AppStartup::Service());
      NS_ENSURE_TRUE(appStartup, NS_ERROR_FAILURE);

      nsCOMPtr<mozIDOMWindowProxy> newWindow;
      rv = windowWatcher->OpenWindow(nullptr, kProfileManagerURL, "_blank",
                                     "centerscreen,chrome,modal,titlebar",
                                     ioParamBlock, getter_AddRefs(newWindow));

      NS_ENSURE_SUCCESS_LOG(rv, rv);

      rv = ioParamBlock->GetInt(0, &dialogReturn);
      if (NS_FAILED(rv) || dialogReturn == nsIToolkitProfileService::exit) {
        return NS_ERROR_ABORT;
      }

      int32_t startOffline;
      rv = ioParamBlock->GetInt(1, &startOffline);
      offline = NS_SUCCEEDED(rv) && startOffline == 1;

      rv = dlgArray->QueryElementAt(0, NS_GET_IID(nsIFile),
                                    getter_AddRefs(profD));
      NS_ENSURE_SUCCESS_LOG(rv, rv);

      rv = dlgArray->QueryElementAt(1, NS_GET_IID(nsIFile),
                                    getter_AddRefs(profLD));
      NS_ENSURE_SUCCESS_LOG(rv, rv);
    }
  }

  if (offline) {
    SaveToEnv("XRE_START_OFFLINE=1");
  }

  // User requested that we restart back into the profile manager.
  if (dialogReturn == nsIToolkitProfileService::restart) {
    SaveToEnv("XRE_RESTART_TO_PROFILE_MANAGER=1");
    SaveToEnv("XRE_RESTARTED_BY_PROFILE_MANAGER=1");
  } else {
    MOZ_ASSERT(dialogReturn == nsIToolkitProfileService::launchWithProfile);
    SaveFileToEnv("XRE_PROFILE_PATH", profD);
    SaveFileToEnv("XRE_PROFILE_LOCAL_PATH", profLD);
    SaveToEnv("XRE_RESTARTED_BY_PROFILE_MANAGER=1");
  }

  if (gRestartedByOS) {
    // Re-add this argument when actually starting the application.
    char** newArgv =
        (char**)realloc(gRestartArgv, sizeof(char*) * (gRestartArgc + 2));
    NS_ENSURE_TRUE(newArgv, NS_ERROR_OUT_OF_MEMORY);
    gRestartArgv = newArgv;
    gRestartArgv[gRestartArgc++] = const_cast<char*>("-os-restarted");
    gRestartArgv[gRestartArgc] = nullptr;
  }

  return LaunchChild(false);
}

static bool gDoMigration = false;
static bool gDoProfileReset = false;
static nsCOMPtr<nsIToolkitProfile> gResetOldProfile;

static nsresult LockProfile(nsINativeAppSupport* aNative, nsIFile* aRootDir,
                            nsIFile* aLocalDir, nsIToolkitProfile* aProfile,
                            nsIProfileLock** aResult) {
  // If you close Firefox and very quickly reopen it, the old Firefox may
  // still be closing down. Rather than immediately showing the
  // "Firefox is running but is not responding" message, we spend a few
  // seconds retrying first.

  static const int kLockRetrySeconds = 5;
  static const int kLockRetrySleepMS = 100;

  nsresult rv;
  nsCOMPtr<nsIProfileUnlocker> unlocker;
  const TimeStamp start = TimeStamp::Now();
  do {
    if (aProfile) {
      rv = aProfile->Lock(getter_AddRefs(unlocker), aResult);
    } else {
      rv = NS_LockProfilePath(aRootDir, aLocalDir, getter_AddRefs(unlocker),
                              aResult);
    }
    if (NS_SUCCEEDED(rv)) {
      StartupTimeline::Record(StartupTimeline::AFTER_PROFILE_LOCKED);
      return NS_OK;
    }
    PR_Sleep(kLockRetrySleepMS);
  } while (TimeStamp::Now() - start <
           TimeDuration::FromSeconds(kLockRetrySeconds));

  return ProfileLockedDialog(aRootDir, aLocalDir, unlocker, aNative, aResult);
}

// Pick a profile. We need to end up with a profile root dir, local dir and
// potentially an nsIToolkitProfile instance.
//
// 1) check for --profile <path>
// 2) check for -P <name>
// 3) check for --ProfileManager
// 4) use the default profile, if there is one
// 5) if there are *no* profiles, set up profile-migration
// 6) display the profile-manager UI
static nsresult SelectProfile(nsToolkitProfileService* aProfileSvc,
                              nsINativeAppSupport* aNative, nsIFile** aRootDir,
                              nsIFile** aLocalDir, nsIToolkitProfile** aProfile,
                              bool* aWasDefaultSelection) {
  StartupTimeline::Record(StartupTimeline::SELECT_PROFILE);

  nsresult rv;

  if (EnvHasValue("MOZ_RESET_PROFILE_RESTART")) {
    gDoProfileReset = true;
    gDoMigration = true;
    SaveToEnv("MOZ_RESET_PROFILE_RESTART=");
    // We only want to restore the previous session if the profile refresh was
    // triggered by user. And if it was a user-triggered profile refresh
    // through, say, the safeMode dialog or the troubleshooting page, the
    // MOZ_RESET_PROFILE_RESTART env variable would be set. Hence we set
    // MOZ_RESET_PROFILE_MIGRATE_SESSION here so that Firefox profile migrator
    // would migrate old session data later.
    SaveToEnv("MOZ_RESET_PROFILE_MIGRATE_SESSION=1");
  }

  // reset-profile and migration args need to be checked before any profiles are
  // chosen below.
  ArgResult ar = CheckArg("reset-profile");
  if (ar == ARG_FOUND) {
    gDoProfileReset = true;
  }

  ar = CheckArg("migration");
  if (ar == ARG_FOUND) {
    gDoMigration = true;
  }

  if (EnvHasValue("XRE_RESTART_TO_PROFILE_MANAGER")) {
    return ShowProfileManager(aProfileSvc, aNative);
  }

  // Ask the profile manager to select the profile directories to use.
  bool didCreate = false;
  rv = aProfileSvc->SelectStartupProfile(&gArgc, gArgv, gDoProfileReset,
                                         aRootDir, aLocalDir, aProfile,
                                         &didCreate, aWasDefaultSelection);

  if (rv == NS_ERROR_SHOW_PROFILE_MANAGER) {
    return ShowProfileManager(aProfileSvc, aNative);
  }

  NS_ENSURE_SUCCESS(rv, rv);

  if (didCreate) {
    // For a fresh install, we would like to let users decide
    // to do profile migration on their own later after using.
    gDoProfileReset = false;
    gDoMigration = false;
  }

  if (gDoProfileReset && !*aProfile) {
    NS_WARNING("Profile reset is only supported for named profiles.");
    return NS_ERROR_ABORT;
  }

  // No profile could be found. This generally shouldn't happen, a new profile
  // should be created in all cases except for profile reset which is covered
  // above, but just in case...
  if (!*aRootDir) {
    NS_WARNING("Failed to select or create profile.");
    return NS_ERROR_ABORT;
  }

  return NS_OK;
}

#ifdef MOZ_BLOCK_PROFILE_DOWNGRADE
struct FileWriteFunc : public JSONWriteFunc {
  FILE* mFile;
  explicit FileWriteFunc(FILE* aFile) : mFile(aFile) {}

  void Write(const char* aStr) override { fprintf(mFile, "%s", aStr); }
};

static void SubmitDowngradeTelemetry(const nsCString& aLastVersion,
                                     bool aHasSync, int32_t aButton) {
  nsCOMPtr<nsIPrefService> prefSvc =
      do_GetService("@mozilla.org/preferences-service;1");
  NS_ENSURE_TRUE_VOID(prefSvc);

  nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(prefSvc);
  NS_ENSURE_TRUE_VOID(prefBranch);

  bool enabled;
  nsresult rv =
      prefBranch->GetBoolPref(kPrefHealthReportUploadEnabled, &enabled);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (!enabled) {
    return;
  }

  nsCString server;
  rv = prefBranch->GetCharPref("toolkit.telemetry.server", server);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCString clientId;
  rv = prefBranch->GetCharPref("toolkit.telemetry.cachedClientID", clientId);
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = prefSvc->GetDefaultBranch(nullptr, getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCString channel("default");
  rv = prefBranch->GetCharPref("app.update.channel", channel);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsID uuid;
  nsCOMPtr<nsIUUIDGenerator> uuidGen =
      do_GetService("@mozilla.org/uuid-generator;1");
  NS_ENSURE_TRUE_VOID(uuidGen);
  rv = uuidGen->GenerateUUIDInPlace(&uuid);
  NS_ENSURE_SUCCESS_VOID(rv);

  char strid[NSID_LENGTH];
  uuid.ToProvidedString(strid);

  nsCString arch("null");
  nsCOMPtr<nsIPropertyBag2> sysInfo =
      do_GetService("@mozilla.org/system-info;1");
  NS_ENSURE_TRUE_VOID(sysInfo);
  sysInfo->GetPropertyAsACString(NS_LITERAL_STRING("arch"), arch);

  time_t now;
  time(&now);
  char date[sizeof "YYYY-MM-DDThh:mm:ss.000Z"];
  strftime(date, sizeof date, "%FT%T.000Z", gmtime(&now));

  // NSID_LENGTH includes the trailing \0 and we also want to strip off the
  // surrounding braces so the length becomes NSID_LENGTH - 3.
  nsDependentCSubstring pingId(strid + 1, NSID_LENGTH - 3);
  NS_NAMED_LITERAL_CSTRING(pingType, "downgrade");

  int32_t pos = aLastVersion.Find("_");
  if (pos == kNotFound) {
    return;
  }

  const nsDependentCSubstring lastVersion = Substring(aLastVersion, 0, pos);
  const nsDependentCSubstring lastBuildId =
      Substring(aLastVersion, pos + 1, 14);

  nsPrintfCString url("%s/submit/telemetry/%s/%s/%s/%s/%s/%s?v=%d",
                      server.get(), PromiseFlatCString(pingId).get(),
                      pingType.get(), (const char*)gAppData->name,
                      (const char*)gAppData->version, channel.get(),
                      (const char*)gAppData->buildID,
                      TELEMETRY_PING_FORMAT_VERSION);

  nsCOMPtr<nsIFile> pingFile;
  rv = NS_GetSpecialDirectory(XRE_USER_APP_DATA_DIR, getter_AddRefs(pingFile));
  NS_ENSURE_SUCCESS_VOID(rv);
  rv = pingFile->Append(NS_LITERAL_STRING("Pending Pings"));
  NS_ENSURE_SUCCESS_VOID(rv);
  rv = pingFile->Create(nsIFile::DIRECTORY_TYPE, 0755);
  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS) {
    return;
  }
  rv = pingFile->Append(NS_ConvertUTF8toUTF16(pingId));
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<nsIFile> pingSender;
  rv = NS_GetSpecialDirectory(NS_GRE_BIN_DIR, getter_AddRefs(pingSender));
  NS_ENSURE_SUCCESS_VOID(rv);
#  ifdef XP_WIN
  pingSender->Append(NS_LITERAL_STRING("pingsender.exe"));
#  else
  pingSender->Append(NS_LITERAL_STRING("pingsender"));
#  endif

  bool exists;
  rv = pingSender->Exists(&exists);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (!exists) {
    return;
  }

  FILE* file;
  rv = pingFile->OpenANSIFileDesc("w", &file);
  NS_ENSURE_SUCCESS_VOID(rv);

  JSONWriter w(MakeUnique<FileWriteFunc>(file));
  w.Start();
  {
    w.StringProperty("type", pingType.get());
    w.StringProperty("id", PromiseFlatCString(pingId).get());
    w.StringProperty("creationDate", date);
    w.IntProperty("version", TELEMETRY_PING_FORMAT_VERSION);
    w.StringProperty("clientId", clientId.get());
    w.StartObjectProperty("application");
    {
      w.StringProperty("architecture", arch.get());
      w.StringProperty("buildId", gAppData->buildID);
      w.StringProperty("name", gAppData->name);
      w.StringProperty("version", gAppData->version);
      w.StringProperty("displayVersion",
                       MOZ_STRINGIFY(MOZ_APP_VERSION_DISPLAY));
      w.StringProperty("vendor", gAppData->vendor);
      w.StringProperty("platformVersion", gToolkitVersion);
#  ifdef TARGET_XPCOM_ABI
      w.StringProperty("xpcomAbi", TARGET_XPCOM_ABI);
#  else
      w.StringProperty("xpcomAbi", "unknown");
#  endif
      w.StringProperty("channel", channel.get());
    }
    w.EndObject();
    w.StartObjectProperty("payload");
    {
      w.StringProperty("lastVersion", PromiseFlatCString(lastVersion).get());
      w.StringProperty("lastBuildId", PromiseFlatCString(lastBuildId).get());
      w.BoolProperty("hasSync", aHasSync);
      w.IntProperty("button", aButton);
    }
    w.EndObject();
  }
  w.End();

  fclose(file);

  PathString filePath = pingFile->NativePath();
  const filesystem::Path::value_type* args[2];
#  ifdef XP_WIN
  nsString urlw = NS_ConvertUTF8toUTF16(url);
  args[0] = urlw.get();
#  else
  args[0] = url.get();
#  endif
  args[1] = filePath.get();

  nsCOMPtr<nsIProcess> process =
      do_CreateInstance("@mozilla.org/process/util;1");
  NS_ENSURE_TRUE_VOID(process);
  process->Init(pingSender);
  process->SetStartHidden(true);
  process->SetNoShell(true);

#  ifdef XP_WIN
  process->Runw(false, args, 2);
#  else
  process->Run(false, args, 2);
#  endif
}

static const char kProfileDowngradeURL[] =
    "chrome://mozapps/content/profile/profileDowngrade.xhtml";

static ReturnAbortOnError CheckDowngrade(nsIFile* aProfileDir,
                                         nsINativeAppSupport* aNative,
                                         nsIToolkitProfileService* aProfileSvc,
                                         const nsCString& aLastVersion) {
  int32_t result = 0;
  nsresult rv;

  {
    if (gfxPlatform::IsHeadless()) {
      // TODO: make a way to turn off all dialogs when headless.
      Output(true,
             "This profile was last used with a newer version of this "
             "application. Please create a new profile.\n");
      return NS_ERROR_ABORT;
    }

    ScopedXPCOMStartup xpcom;
    rv = xpcom.Initialize();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = xpcom.SetWindowCreator(aNative);
    NS_ENSURE_SUCCESS(rv, rv);

    {  // extra scoping is needed so we release these components before xpcom
       // shutdown
      bool hasSync = false;
      nsCOMPtr<nsIPrefService> prefSvc =
          do_GetService("@mozilla.org/preferences-service;1");
      NS_ENSURE_TRUE(prefSvc, rv);

      nsCOMPtr<nsIFile> prefsFile;
      rv = aProfileDir->Clone(getter_AddRefs(prefsFile));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = prefsFile->Append(NS_LITERAL_STRING("prefs.js"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = prefSvc->ReadUserPrefsFromFile(prefsFile);
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(prefSvc);

        rv = prefBranch->PrefHasUserValue("services.sync.username", &hasSync);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      nsCOMPtr<nsIWindowWatcher> windowWatcher =
          do_GetService(NS_WINDOWWATCHER_CONTRACTID);
      NS_ENSURE_TRUE(windowWatcher, NS_ERROR_ABORT);

      nsCOMPtr<nsIAppStartup> appStartup(components::AppStartup::Service());
      NS_ENSURE_TRUE(appStartup, NS_ERROR_FAILURE);

      nsCOMPtr<nsIDialogParamBlock> paramBlock =
          do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID);
      NS_ENSURE_TRUE(paramBlock, NS_ERROR_ABORT);

      uint8_t flags = 0;
      if (hasSync) {
        flags |= nsIToolkitProfileService::hasSync;
      }

      paramBlock->SetInt(0, flags);

      nsCOMPtr<mozIDOMWindowProxy> newWindow;
      rv = windowWatcher->OpenWindow(nullptr, kProfileDowngradeURL, "_blank",
                                     "centerscreen,chrome,modal,titlebar",
                                     paramBlock, getter_AddRefs(newWindow));
      NS_ENSURE_SUCCESS(rv, rv);

      paramBlock->GetInt(1, &result);

      SubmitDowngradeTelemetry(aLastVersion, hasSync, result);
    }
  }

  if (result == nsIToolkitProfileService::createNewProfile) {
    // Create a new profile and start it.
    nsCString profileName;
    profileName.AssignLiteral("default");
#  ifdef MOZ_DEDICATED_PROFILES
    profileName.Append("-" MOZ_STRINGIFY(MOZ_UPDATE_CHANNEL));
#  endif
    nsCOMPtr<nsIToolkitProfile> newProfile;
    rv = aProfileSvc->CreateUniqueProfile(nullptr, profileName,
                                          getter_AddRefs(newProfile));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aProfileSvc->SetDefaultProfile(newProfile);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aProfileSvc->Flush();
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> profD, profLD;
    rv = newProfile->GetRootDir(getter_AddRefs(profD));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = newProfile->GetLocalDir(getter_AddRefs(profLD));
    NS_ENSURE_SUCCESS(rv, rv);

    SaveFileToEnv("XRE_PROFILE_PATH", profD);
    SaveFileToEnv("XRE_PROFILE_LOCAL_PATH", profLD);

    return LaunchChild(false);
  }

  // Cancel
  return NS_ERROR_ABORT;
}
#endif

/**
 * Extracts the various parts of a compatibility version string.
 *
 * Compatibility versions are of the form
 * "<appversion>_<appbuildid>/<platformbuildid>". The toolkit version comparator
 * can only handle 32-bit numbers and in the normal case build IDs are larger
 * than this. So if the build ID is numeric we split it into two version parts.
 */
static void ExtractCompatVersionInfo(const nsACString& aCompatVersion,
                                     nsACString& aAppVersion,
                                     nsACString& aAppBuildID,
                                     nsACString& aPlatformBuildID) {
  int32_t underscorePos = aCompatVersion.FindChar('_');
  int32_t slashPos = aCompatVersion.FindChar('/');

  if (underscorePos == kNotFound || slashPos == kNotFound ||
      slashPos < underscorePos) {
    NS_WARNING(
        "compatibility.ini Version string does not match the expected format.");

    // Fall back to just using the entire string as the version.
    aAppVersion = aCompatVersion;
    aAppBuildID.Truncate(0);
    aPlatformBuildID.Truncate(0);
    return;
  }

  aAppVersion = Substring(aCompatVersion, 0, underscorePos);
  aAppBuildID = Substring(aCompatVersion, underscorePos + 1,
                          slashPos - (underscorePos + 1));
  aPlatformBuildID = Substring(aCompatVersion, slashPos + 1);
}

/**
 * Compares two build IDs. Returns 0 if they match, < 0 if newID is considered
 * newer than oldID and > 0 if the oldID is considered newer than newID.
 */
static int32_t CompareBuildIDs(nsACString& oldID, nsACString& newID) {
  // For Mozilla builds the build ID is a numeric date string. But it is too
  // large a number for the version comparator to handle so try to just compare
  // them as integer values first.

  // ToInteger64 succeeds if the strings contain trailing non-digits so first
  // check that all the characters are digits.
  bool isNumeric = true;
  const char* pos = oldID.BeginReading();
  const char* end = oldID.EndReading();
  while (pos != end) {
    if (!IsAsciiDigit(*pos)) {
      isNumeric = false;
      break;
    }
    pos++;
  }

  if (isNumeric) {
    pos = newID.BeginReading();
    end = newID.EndReading();
    while (pos != end) {
      if (!IsAsciiDigit(*pos)) {
        isNumeric = false;
        break;
      }
      pos++;
    }
  }

  if (isNumeric) {
    nsresult rv;
    CheckedInt<uint64_t> oldVal = oldID.ToInteger64(&rv);

    if (NS_SUCCEEDED(rv) && oldVal.isValid()) {
      CheckedInt<uint64_t> newVal = newID.ToInteger64(&rv);

      if (NS_SUCCEEDED(rv) && newVal.isValid()) {
        // We have simple numbers for both IDs.
        if (oldVal.value() == newVal.value()) {
          return 0;
        }

        if (oldVal.value() > newVal.value()) {
          return 1;
        }

        return -1;
      }
    }
  }

  // If either could not be parsed as a number then something (likely a Linux
  // distribution could have modified the build ID in some way. We don't know
  // what format this may be so let's just fall back to assuming that it's a
  // valid toolkit version.
  return CompareVersions(PromiseFlatCString(oldID).get(),
                         PromiseFlatCString(newID).get());
}

/**
 * Compares the provided compatibility versions. Returns 0 if they match,
 * < 0 if the new version is considered an upgrade from the old version and
 * > 0 if the new version is considered a downgrade from the old version.
 */
int32_t CompareCompatVersions(const nsACString& aOldCompatVersion,
                              const nsACString& aNewCompatVersion) {
  // Quick path for the common case.
  if (aOldCompatVersion.Equals(aNewCompatVersion)) {
    gLastAppVersion.Assign(gAppData->version);
    gLastAppBuildID.Assign(gAppData->buildID);

    return 0;
  }

  // The versions differ for some reason so we will only ever return false from
  // here onwards. We just have to figure out if this is a downgrade or not.

  // Hardcode the case where the last run was in safe mode (Bug 1556612). We
  // cannot tell if this is a downgrade or not so just assume it isn't and let
  // the user proceed.
  if (aOldCompatVersion.EqualsLiteral("Safe Mode")) {
    gLastAppVersion.SetIsVoid(true);
    gLastAppBuildID.SetIsVoid(true);

    return -1;
  }

  nsCString oldPlatformBuildID;
  ExtractCompatVersionInfo(aOldCompatVersion, gLastAppVersion, gLastAppBuildID,
                           oldPlatformBuildID);

  nsCString newVersion;
  nsCString newAppBuildID;
  nsCString newPlatformBuildID;
  ExtractCompatVersionInfo(aNewCompatVersion, newVersion, newAppBuildID,
                           newPlatformBuildID);

  // In most cases the app version will differ and this is an easy check.
  int32_t result = CompareVersions(gLastAppVersion.get(), newVersion.get());
  if (result != 0) {
    return result;
  }

  // Fall back to build ID comparison.
  result = CompareBuildIDs(gLastAppBuildID, newAppBuildID);
  if (result != 0) {
    return result;
  }

  return CompareBuildIDs(oldPlatformBuildID, newPlatformBuildID);
}

/**
 * Checks the compatibility.ini file to see if we have updated our application
 * or otherwise invalidated our caches. If the application has been updated,
 * we return false; otherwise, we return true. We also write the status
 * of the caches (valid/invalid) into the return param aCachesOK. The aCachesOK
 * is always invalid if the application has been updated. aIsDowngrade is set to
 * true if the current application is older than that previously used by the
 * profile.
 */
static bool CheckCompatibility(nsIFile* aProfileDir, const nsCString& aVersion,
                               const nsCString& aOSABI, nsIFile* aXULRunnerDir,
                               nsIFile* aAppDir, nsIFile* aFlagFile,
                               bool* aCachesOK, bool* aIsDowngrade,
                               nsCString& aLastVersion) {
  *aCachesOK = false;
  *aIsDowngrade = false;
  gLastAppVersion.SetIsVoid(true);
  gLastAppBuildID.SetIsVoid(true);

  nsCOMPtr<nsIFile> file;
  aProfileDir->Clone(getter_AddRefs(file));
  if (!file) return false;
  file->AppendNative(FILE_COMPATIBILITY_INFO);

  nsINIParser parser;
  nsresult rv = parser.Init(file);
  if (NS_FAILED(rv)) return false;

  rv = parser.GetString("Compatibility", "LastVersion", aLastVersion);
  if (NS_FAILED(rv)) {
    return false;
  }

  int32_t result = CompareCompatVersions(aLastVersion, aVersion);
  if (result != 0) {
    *aIsDowngrade = result > 0;
    return false;
  }

  nsAutoCString buf;
  rv = parser.GetString("Compatibility", "LastOSABI", buf);
  if (NS_FAILED(rv) || !aOSABI.Equals(buf)) return false;

  rv = parser.GetString("Compatibility", "LastPlatformDir", buf);
  if (NS_FAILED(rv)) return false;

  nsCOMPtr<nsIFile> lf;
  rv = NS_NewNativeLocalFile(EmptyCString(), false, getter_AddRefs(lf));
  if (NS_FAILED(rv)) return false;

  rv = lf->SetPersistentDescriptor(buf);
  if (NS_FAILED(rv)) return false;

  bool eq;
  rv = lf->Equals(aXULRunnerDir, &eq);
  if (NS_FAILED(rv) || !eq) return false;

  if (aAppDir) {
    rv = parser.GetString("Compatibility", "LastAppDir", buf);
    if (NS_FAILED(rv)) return false;

    rv = NS_NewNativeLocalFile(EmptyCString(), false, getter_AddRefs(lf));
    if (NS_FAILED(rv)) return false;

    rv = lf->SetPersistentDescriptor(buf);
    if (NS_FAILED(rv)) return false;

    rv = lf->Equals(aAppDir, &eq);
    if (NS_FAILED(rv) || !eq) return false;
  }

  // If we see this flag, caches are invalid.
  rv = parser.GetString("Compatibility", "InvalidateCaches", buf);
  *aCachesOK = (NS_FAILED(rv) || !buf.EqualsLiteral("1"));

  bool purgeCaches = false;
  if (aFlagFile && NS_SUCCEEDED(aFlagFile->Exists(&purgeCaches)) &&
      purgeCaches) {
    *aCachesOK = false;
  }

  return true;
}

void BuildCompatVersion(const char* aAppVersion, const char* aAppBuildID,
                        const char* aToolkitBuildID, nsACString& aBuf) {
  aBuf.Assign(aAppVersion);
  aBuf.Append('_');
  aBuf.Append(aAppBuildID);
  aBuf.Append('/');
  aBuf.Append(aToolkitBuildID);
}

static void BuildVersion(nsCString& aBuf) {
  BuildCompatVersion(gAppData->version, gAppData->buildID, gToolkitBuildID,
                     aBuf);
}

static void WriteVersion(nsIFile* aProfileDir, const nsCString& aVersion,
                         const nsCString& aOSABI, nsIFile* aXULRunnerDir,
                         nsIFile* aAppDir, bool invalidateCache) {
  nsCOMPtr<nsIFile> file;
  aProfileDir->Clone(getter_AddRefs(file));
  if (!file) return;
  file->AppendNative(FILE_COMPATIBILITY_INFO);

  nsAutoCString platformDir;
  Unused << aXULRunnerDir->GetPersistentDescriptor(platformDir);

  nsAutoCString appDir;
  if (aAppDir) Unused << aAppDir->GetPersistentDescriptor(appDir);

  PRFileDesc* fd;
  nsresult rv = file->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                                       0600, &fd);
  if (NS_FAILED(rv)) {
    NS_ERROR("could not create output stream");
    return;
  }

  static const char kHeader[] = "[Compatibility]" NS_LINEBREAK "LastVersion=";

  PR_Write(fd, kHeader, sizeof(kHeader) - 1);
  PR_Write(fd, aVersion.get(), aVersion.Length());

  static const char kOSABIHeader[] = NS_LINEBREAK "LastOSABI=";
  PR_Write(fd, kOSABIHeader, sizeof(kOSABIHeader) - 1);
  PR_Write(fd, aOSABI.get(), aOSABI.Length());

  static const char kPlatformDirHeader[] = NS_LINEBREAK "LastPlatformDir=";

  PR_Write(fd, kPlatformDirHeader, sizeof(kPlatformDirHeader) - 1);
  PR_Write(fd, platformDir.get(), platformDir.Length());

  static const char kAppDirHeader[] = NS_LINEBREAK "LastAppDir=";
  if (aAppDir) {
    PR_Write(fd, kAppDirHeader, sizeof(kAppDirHeader) - 1);
    PR_Write(fd, appDir.get(), appDir.Length());
  }

  static const char kInvalidationHeader[] = NS_LINEBREAK "InvalidateCaches=1";
  if (invalidateCache)
    PR_Write(fd, kInvalidationHeader, sizeof(kInvalidationHeader) - 1);

  static const char kNL[] = NS_LINEBREAK;
  PR_Write(fd, kNL, sizeof(kNL) - 1);

  PR_Close(fd);
}

/**
 * Returns true if the startup cache file was successfully removed.
 * Returns false if file->Clone fails at any point (OOM) or if unable
 * to remove the startup cache file. Note in particular the return value
 * is unaffected by a failure to remove extensions.ini
 */
static bool RemoveComponentRegistries(nsIFile* aProfileDir,
                                      nsIFile* aLocalProfileDir,
                                      bool aRemoveEMFiles) {
  nsCOMPtr<nsIFile> file;
  aProfileDir->Clone(getter_AddRefs(file));
  if (!file) return false;

  if (aRemoveEMFiles) {
    file->SetNativeLeafName(NS_LITERAL_CSTRING("extensions.ini"));
    file->Remove(false);
  }

  aLocalProfileDir->Clone(getter_AddRefs(file));
  if (!file) return false;

#if defined(XP_UNIX) || defined(XP_BEOS)
#  define PLATFORM_FASL_SUFFIX ".mfasl"
#elif defined(XP_WIN)
#  define PLATFORM_FASL_SUFFIX ".mfl"
#endif

  file->AppendNative(NS_LITERAL_CSTRING("XUL" PLATFORM_FASL_SUFFIX));
  file->Remove(false);

  file->SetNativeLeafName(NS_LITERAL_CSTRING("XPC" PLATFORM_FASL_SUFFIX));
  file->Remove(false);

  file->SetNativeLeafName(NS_LITERAL_CSTRING("startupCache"));
  nsresult rv = file->Remove(true);
  return NS_SUCCEEDED(rv) || rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST ||
         rv == NS_ERROR_FILE_NOT_FOUND;
}

// When we first initialize the crash reporter we don't have a profile,
// so we set the minidump path to $TEMP.  Once we have a profile,
// we set it to $PROFILE/minidumps, creating the directory
// if needed.
static void MakeOrSetMinidumpPath(nsIFile* profD) {
  nsCOMPtr<nsIFile> dumpD;
  profD->Clone(getter_AddRefs(dumpD));

  if (dumpD) {
    bool fileExists;
    // XXX: do some more error checking here
    dumpD->Append(NS_LITERAL_STRING("minidumps"));
    dumpD->Exists(&fileExists);
    if (!fileExists) {
      nsresult rv = dumpD->Create(nsIFile::DIRECTORY_TYPE, 0700);
      NS_ENSURE_SUCCESS_VOID(rv);
    }

    nsAutoString pathStr;
    if (NS_SUCCEEDED(dumpD->GetPath(pathStr)))
      CrashReporter::SetMinidumpPath(pathStr);
  }
}

const XREAppData* gAppData = nullptr;

#ifdef MOZ_WIDGET_GTK
static void MOZ_gdk_display_close(GdkDisplay* display) {
#  if CLEANUP_MEMORY
  // XXX wallpaper for bug 417163: don't close the Display if we're using the
  // Qt theme because we crash (in Qt code) when using jemalloc.
  bool skip_display_close = false;
  GtkSettings* settings =
      gtk_settings_get_for_screen(gdk_display_get_default_screen(display));
  gchar* theme_name;
  g_object_get(settings, "gtk-theme-name", &theme_name, nullptr);
  if (theme_name) {
    skip_display_close = strcmp(theme_name, "Qt") == 0;
    if (skip_display_close) NS_WARNING("wallpaper bug 417163 for Qt theme");
    g_free(theme_name);
  }

  // A workaround for https://bugzilla.gnome.org/show_bug.cgi?id=703257
  if (gtk_check_version(3, 9, 8) != NULL) skip_display_close = true;

  bool buggyCairoShutdown = cairo_version() < CAIRO_VERSION_ENCODE(1, 4, 0);

  if (!buggyCairoShutdown) {
    // We should shut down GDK before we shut down libraries it depends on
    // like Pango and cairo. But if cairo shutdown is buggy, we should
    // shut down cairo first otherwise it may crash because of dangling
    // references to Display objects (see bug 469831).
    if (!skip_display_close) gdk_display_close(display);
  }

  // Tell PangoCairo to release its default fontmap.
  pango_cairo_font_map_set_default(nullptr);

  // cairo_debug_reset_static_data() is prototyped through cairo.h included
  // by gtk.h.
#    ifdef cairo_debug_reset_static_data
#      error \
          "Looks like we're including Mozilla's cairo instead of system cairo"
#    endif
  cairo_debug_reset_static_data();
  // FIXME: Do we need to call this in non-GTK2 cases as well?
  FcFini();

  if (buggyCairoShutdown) {
    if (!skip_display_close) gdk_display_close(display);
  }
#  else  // not CLEANUP_MEMORY
  // Don't do anything to avoid running into driver bugs under XCloseDisplay().
  // See bug 973192.
  (void)display;
#  endif
}
#endif

/**
 * NSPR will search for the "nspr_use_zone_allocator" symbol throughout
 * the process and use it to determine whether the application defines its own
 * memory allocator or not.
 *
 * Since most applications (e.g. Firefox and Thunderbird) don't use any special
 * allocators and therefore don't define this symbol, NSPR must search the
 * entire process, which reduces startup performance.
 *
 * By defining the symbol here, we can avoid the wasted lookup and hopefully
 * improve startup performance.
 */
NS_VISIBILITY_DEFAULT PRBool nspr_use_zone_allocator = PR_FALSE;

#ifdef CAIRO_HAS_DWRITE_FONT

#  include <dwrite.h>

#  ifdef DEBUG_DWRITE_STARTUP

#    define LOGREGISTRY(msg) LogRegistryEvent(msg)

// for use when monitoring process
static void LogRegistryEvent(const wchar_t* msg) {
  HKEY dummyKey;
  HRESULT hr;
  wchar_t buf[512];

  wsprintf(buf, L" log %s", msg);
  hr = RegOpenKeyEx(HKEY_LOCAL_MACHINE, buf, 0, KEY_READ, &dummyKey);
  if (SUCCEEDED(hr)) {
    RegCloseKey(dummyKey);
  }
}
#  else

#    define LOGREGISTRY(msg)

#  endif

static DWORD WINAPI InitDwriteBG(LPVOID lpdwThreadParam) {
  SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
  LOGREGISTRY(L"loading dwrite.dll");
  HMODULE dwdll = LoadLibraryW(L"dwrite.dll");
  if (dwdll) {
    decltype(DWriteCreateFactory)* createDWriteFactory =
        (decltype(DWriteCreateFactory)*)GetProcAddress(dwdll,
                                                       "DWriteCreateFactory");
    if (createDWriteFactory) {
      LOGREGISTRY(L"creating dwrite factory");
      IDWriteFactory* factory;
      HRESULT hr = createDWriteFactory(DWRITE_FACTORY_TYPE_SHARED,
                                       __uuidof(IDWriteFactory),
                                       reinterpret_cast<IUnknown**>(&factory));
      if (SUCCEEDED(hr)) {
        LOGREGISTRY(L"dwrite factory done");
        factory->Release();
        LOGREGISTRY(L"freed factory");
      } else {
        LOGREGISTRY(L"failed to create factory");
      }
    }
  }
  SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
  return 0;
}
#endif

#ifdef USE_GLX_TEST
bool fire_glxtest_process();
#endif

#include "GeckoProfiler.h"

// Encapsulates startup and shutdown state for XRE_main
class XREMain {
 public:
  XREMain()
      : mStartOffline(false),
        mShuttingDown(false)
#ifdef MOZ_HAS_REMOTE
        ,
        mDisableRemoteClient(false),
        mDisableRemoteServer(false)
#endif
#if defined(MOZ_WIDGET_GTK)
        ,
        mGdkDisplay(nullptr)
#endif
            {};

  ~XREMain() {
    mScopedXPCOM = nullptr;
    mAppData = nullptr;
  }

  int XRE_main(int argc, char* argv[], const BootstrapConfig& aConfig);
  int XRE_mainInit(bool* aExitFlag);
  int XRE_mainStartup(bool* aExitFlag);
  nsresult XRE_mainRun();

  Result<bool, nsresult> CheckLastStartupWasCrash();

  nsCOMPtr<nsINativeAppSupport> mNativeApp;
  RefPtr<nsToolkitProfileService> mProfileSvc;
  nsCOMPtr<nsIFile> mProfD;
  nsCOMPtr<nsIFile> mProfLD;
  nsCOMPtr<nsIProfileLock> mProfileLock;
#if defined(MOZ_HAS_REMOTE)
  RefPtr<nsRemoteService> mRemoteService;
#endif

  UniquePtr<ScopedXPCOMStartup> mScopedXPCOM;
  UniquePtr<XREAppData> mAppData;

  nsXREDirProvider mDirProvider;
  nsAutoCString mDesktopStartupID;

  bool mStartOffline;
  bool mShuttingDown;
#if defined(MOZ_HAS_REMOTE)
  bool mDisableRemoteClient;
  bool mDisableRemoteServer;
#endif

#if defined(MOZ_WIDGET_GTK)
  GdkDisplay* mGdkDisplay;
#endif
};

#if defined(XP_UNIX) && !defined(ANDROID)
static SmprintfPointer FormatUid(uid_t aId) {
  if (const auto pw = getpwuid(aId)) {
    return mozilla::Smprintf("%s", pw->pw_name);
  }
  return mozilla::Smprintf("uid %d", static_cast<int>(aId));
}

// Bug 1323302: refuse to run under sudo or similar.
static bool CheckForUserMismatch() {
  static char const* const kVars[] = {
      "HOME",
#  ifdef MOZ_WIDGET_GTK
      "XDG_RUNTIME_DIR",
#  endif
#  ifdef MOZ_X11
      "XAUTHORITY",
#  endif
  };

  const uid_t euid = geteuid();
  if (euid != 0) {
    // On Linux it's possible to have superuser capabilities with a
    // nonzero uid, but anyone who knows enough to make that happen
    // probably knows enough to debug the resulting problems.
    // Otherwise, a non-root user can't cause the problems we're
    // concerned about.
    return false;
  }

  for (const auto var : kVars) {
    if (const auto path = PR_GetEnv(var)) {
      struct stat st;
      if (stat(path, &st) == 0) {
        if (st.st_uid != euid) {
          const auto owner = FormatUid(st.st_uid);
          Output(true,
                 "Running " MOZ_APP_DISPLAYNAME
                 " as root in a regular"
                 " user's session is not supported.  ($%s is %s which is"
                 " owned by %s.)\n",
                 var, path, owner.get());
          return true;
        }
      }
    }
  }
  return false;
}
#else  // !XP_UNIX || ANDROID
static bool CheckForUserMismatch() { return false; }
#endif

static void IncreaseDescriptorLimits() {
#ifdef XP_UNIX
  // Increase the fd limit to accomodate IPC resources like shared memory.
  // See also the Darwin case in config/external/nspr/pr/moz.build
  static const rlim_t kFDs = 4096;
  struct rlimit rlim;

  if (getrlimit(RLIMIT_NOFILE, &rlim) != 0) {
    Output(false, "getrlimit: %s\n", strerror(errno));
    return;
  }
  // Don't decrease the limit if it's already high enough, but don't
  // try to go over the hard limit.  (RLIM_INFINITY isn't required to
  // be the numerically largest rlim_t, so don't assume that.)
  if (rlim.rlim_cur != RLIM_INFINITY && rlim.rlim_cur < kFDs &&
      rlim.rlim_cur < rlim.rlim_max) {
    if (rlim.rlim_max != RLIM_INFINITY && rlim.rlim_max < kFDs) {
      rlim.rlim_cur = rlim.rlim_max;
    } else {
      rlim.rlim_cur = kFDs;
    }
    if (setrlimit(RLIMIT_NOFILE, &rlim) != 0) {
      Output(false, "setrlimit: %s\n", strerror(errno));
    }
  }
#endif
}

/*
 * XRE_mainInit - Initial setup and command line parameter processing.
 * Main() will exit early if either return value != 0 or if aExitFlag is
 * true.
 */
int XREMain::XRE_mainInit(bool* aExitFlag) {
  if (!aExitFlag) return 1;
  *aExitFlag = false;

  atexit(UnexpectedExit);
  auto expectedShutdown = mozilla::MakeScopeExit([&] { MozExpectedExit(); });

  StartupTimeline::Record(StartupTimeline::MAIN);

  if (CheckForUserMismatch()) {
    return 1;
  }

#ifdef XP_MACOSX
  DisableAppNap();
#endif

#ifndef ANDROID
  if (PR_GetEnv("MOZ_RUN_GTEST")
#  ifdef FUZZING
      || PR_GetEnv("FUZZER")
#  endif
  ) {
    // Enable headless mode and assert that it worked, since gfxPlatform
    // uses a static bool set after the first call to `IsHeadless`.
    // Note: Android gtests seem to require an Activity and fail to start
    // with headless mode enabled.
    PR_SetEnv("MOZ_HEADLESS=1");
    MOZ_ASSERT(gfxPlatform::IsHeadless());
  }
#endif  // ANDROID

  if (PR_GetEnv("MOZ_CHAOSMODE")) {
    ChaosFeature feature = ChaosFeature::Any;
    long featureInt = strtol(PR_GetEnv("MOZ_CHAOSMODE"), nullptr, 16);
    if (featureInt) {
      // NOTE: MOZ_CHAOSMODE=0 or a non-hex value maps to Any feature.
      feature = static_cast<ChaosFeature>(featureInt);
    }
    ChaosMode::SetChaosFeature(feature);
  }

#ifdef MOZ_ASAN_REPORTER
  // In ASan Reporter builds, we enable certain chaos features by default unless
  // the user explicitly requests a particular set of features.
  if (!PR_GetEnv("MOZ_CHAOSMODE")) {
    ChaosMode::SetChaosFeature(static_cast<ChaosFeature>(
        ChaosFeature::ThreadScheduling | ChaosFeature::NetworkScheduling |
        ChaosFeature::TimerScheduling));
  }
#endif

  if (ChaosMode::isActive(ChaosFeature::Any)) {
    printf_stderr(
        "*** You are running in chaos test mode. See ChaosMode.h. ***\n");
  }

  if (CheckArg("headless") || CheckArgExists("screenshot")) {
    PR_SetEnv("MOZ_HEADLESS=1");
  }

  if (gfxPlatform::IsHeadless()) {
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) || defined(XP_MACOSX)
    printf_stderr("*** You are running in headless mode.\n");
#else
    Output(
        true,
        "Error: headless mode is not currently supported on this platform.\n");
    return 1;
#endif

#ifdef XP_MACOSX
    // To avoid taking focus when running in headless mode immediately
    // transition Firefox to a background application.
    ProcessSerialNumber psn = {0, kCurrentProcess};
    OSStatus transformStatus =
        TransformProcessType(&psn, kProcessTransformToBackgroundApplication);
    if (transformStatus != noErr) {
      NS_ERROR("Failed to make process a background application.");
      return 1;
    }
#endif
  }

  nsresult rv;
  ArgResult ar;

#ifdef DEBUG
  if (PR_GetEnv("XRE_MAIN_BREAK")) NS_BREAK();
#endif

  IncreaseDescriptorLimits();

#ifdef USE_GLX_TEST
  // bug 639842 - it's very important to fire this process BEFORE we set up
  // error handling. indeed, this process is expected to be crashy, and we
  // don't want the user to see its crashes. That's the whole reason for
  // doing this in a separate process.
  //
  // This call will cause a fork and the fork will terminate itself separately
  // from the usual shutdown sequence
  fire_glxtest_process();
#endif

  SetupErrorHandling(gArgv[0]);

#ifdef CAIRO_HAS_DWRITE_FONT
  {
    // Bug 602792 - when DWriteCreateFactory is called the dwrite client dll
    // starts the FntCache service if it isn't already running (it's set
    // to manual startup by default in Windows 7 RTM).  Subsequent DirectWrite
    // calls cause the IDWriteFactory object to communicate with the FntCache
    // service with a timeout; if there's no response after the timeout, the
    // DirectWrite client library will assume the service isn't around and do
    // manual font file I/O on _all_ system fonts.  To avoid this, load the
    // dwrite library and create a factory as early as possible so that the
    // FntCache service is ready by the time it's needed.

    CreateThread(nullptr, 0, &InitDwriteBG, nullptr, 0, nullptr);
  }
#endif

#ifdef XP_UNIX
  const char* home = PR_GetEnv("HOME");
  if (!home || !*home) {
    struct passwd* pw = getpwuid(geteuid());
    if (!pw || !pw->pw_dir) {
      Output(true, "Could not determine HOME directory");
      return 1;
    }
    SaveWordToEnv("HOME", nsDependentCString(pw->pw_dir));
  }
#endif

#ifdef MOZ_ACCESSIBILITY_ATK
  // Suppress atk-bridge init at startup, until mozilla accessibility is
  // initialized.  This works after gnome 2.24.2.
  SaveToEnv("NO_AT_BRIDGE=1");
#endif

  // Check for application.ini overrides
  const char* override = nullptr;
  ar = CheckArg("override", &override);
  if (ar == ARG_BAD) {
    Output(true, "Incorrect number of arguments passed to --override");
    return 1;
  }
  if (ar == ARG_FOUND) {
    nsCOMPtr<nsIFile> overrideLF;
    rv = XRE_GetFileFromPath(override, getter_AddRefs(overrideLF));
    if (NS_FAILED(rv)) {
      Output(true, "Error: unrecognized override.ini path.\n");
      return 1;
    }

    rv = XRE_ParseAppData(overrideLF, *mAppData);
    if (NS_FAILED(rv)) {
      Output(true, "Couldn't read override.ini");
      return 1;
    }
  }

  // Check sanity and correctness of app data.

  if (!mAppData->name) {
    Output(true, "Error: App:Name not specified in application.ini\n");
    return 1;
  }
  if (!mAppData->buildID) {
    Output(true, "Error: App:BuildID not specified in application.ini\n");
    return 1;
  }

  // XXX Originally ScopedLogging was here? Now it's in XRE_main above
  // XRE_mainInit.

  if (!mAppData->minVersion) {
    Output(true, "Error: Gecko:MinVersion not specified in application.ini\n");
    return 1;
  }

  if (!mAppData->maxVersion) {
    // If no maxVersion is specified, we assume the app is only compatible
    // with the initial preview release. Do not increment this number ever!
    mAppData->maxVersion = "1.*";
  }

  if (mozilla::Version(mAppData->minVersion) > gToolkitVersion ||
      mozilla::Version(mAppData->maxVersion) < gToolkitVersion) {
    Output(true,
           "Error: Platform version '%s' is not compatible with\n"
           "minVersion >= %s\nmaxVersion <= %s\n",
           (const char*)gToolkitVersion, (const char*)mAppData->minVersion,
           (const char*)mAppData->maxVersion);
    return 1;
  }

  rv = mDirProvider.Initialize(mAppData->directory, mAppData->xreDirectory);
  if (NS_FAILED(rv)) return 1;

  if (EnvHasValue("MOZ_CRASHREPORTER")) {
    mAppData->flags |= NS_XRE_ENABLE_CRASH_REPORTER;
  }

  nsCOMPtr<nsIFile> xreBinDirectory;
  xreBinDirectory = mDirProvider.GetGREBinDir();

  if ((mAppData->flags & NS_XRE_ENABLE_CRASH_REPORTER) &&
      NS_SUCCEEDED(CrashReporter::SetExceptionHandler(xreBinDirectory))) {
    nsCOMPtr<nsIFile> file;
    rv = nsXREDirProvider::GetUserAppDataDirectory(getter_AddRefs(file));
    if (NS_SUCCEEDED(rv)) {
      CrashReporter::SetUserAppDataDirectory(file);
    }
    if (mAppData->crashReporterURL)
      CrashReporter::SetServerURL(
          nsDependentCString(mAppData->crashReporterURL));

    // We overwrite this once we finish starting up.
    CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::StartupCrash,
                                       true);

    // pass some basic info from the app data
    if (mAppData->vendor)
      CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::Vendor,
                                         nsDependentCString(mAppData->vendor));
    if (mAppData->name)
      CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::ProductName,
                                         nsDependentCString(mAppData->name));
    if (mAppData->ID)
      CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::ProductID,
                                         nsDependentCString(mAppData->ID));
    if (mAppData->version)
      CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::Version,
                                         nsDependentCString(mAppData->version));
    if (mAppData->buildID)
      CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::BuildID,
                                         nsDependentCString(mAppData->buildID));

    nsDependentCString releaseChannel(MOZ_STRINGIFY(MOZ_UPDATE_CHANNEL));
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::ReleaseChannel, releaseChannel);
#ifdef MOZ_LINKER
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::CrashAddressLikelyWrong,
        IsSignalHandlingBroken());
#endif

#ifdef XP_WIN
    nsAutoString appInitDLLs;
    if (widget::WinUtils::GetAppInitDLLs(appInitDLLs)) {
      CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::AppInitDLLs,
                                         NS_ConvertUTF16toUTF8(appInitDLLs));
    }
#endif

    CrashReporter::SetRestartArgs(gArgc, gArgv);

    // annotate other data (user id etc)
    nsCOMPtr<nsIFile> userAppDataDir;
    if (NS_SUCCEEDED(mDirProvider.GetUserAppDataDirectory(
            getter_AddRefs(userAppDataDir)))) {
      CrashReporter::SetupExtraData(userAppDataDir,
                                    nsDependentCString(mAppData->buildID));

      // see if we have a crashreporter-override.ini in the application
      // directory
      nsCOMPtr<nsIFile> overrideini;
      if (NS_SUCCEEDED(
              mDirProvider.GetAppDir()->Clone(getter_AddRefs(overrideini))) &&
          NS_SUCCEEDED(overrideini->AppendNative(
              NS_LITERAL_CSTRING("crashreporter-override.ini")))) {
#ifdef XP_WIN
        nsAutoString overridePathW;
        overrideini->GetPath(overridePathW);
        NS_ConvertUTF16toUTF8 overridePath(overridePathW);
#else
        nsAutoCString overridePath;
        overrideini->GetNativePath(overridePath);
#endif

        SaveWordToEnv("MOZ_CRASHREPORTER_STRINGS_OVERRIDE", overridePath);
      }
    }
  }

#if defined(MOZ_SANDBOX) && defined(XP_WIN)
  if (mAppData->sandboxBrokerServices) {
    SandboxBroker::Initialize(mAppData->sandboxBrokerServices);
  } else {
#  if defined(MOZ_SANDBOX)
    // If we're sandboxing content and we fail to initialize, then crashing here
    // seems like the sensible option.
    if (BrowserTabsRemoteAutostart()) {
      MOZ_CRASH("Failed to initialize broker services, can't continue.");
    }
#  endif
    // Otherwise just warn for the moment, as most things will work.
    NS_WARNING(
        "Failed to initialize broker services, sandboxed processes will "
        "fail to start.");
  }
  if (mAppData->sandboxPermissionsService) {
    SandboxPermissions::Initialize(mAppData->sandboxPermissionsService,
                                   nullptr);
  }
#endif

#ifdef XP_MACOSX
  // Set up ability to respond to system (Apple) events. This must occur before
  // ProcessUpdates to ensure that links clicked in external applications aren't
  // lost when updates are pending.
  SetupMacApplicationDelegate();

  if (EnvHasValue("MOZ_LAUNCHED_CHILD")) {
    // This is needed, on relaunch, to force the OS to use the "Cocoa Dock
    // API".  Otherwise the call to ReceiveNextEvent() below will make it
    // use the "Carbon Dock API".  For more info see bmo bug 377166.
    EnsureUseCocoaDockAPI();

    // When the app relaunches, the original process exits.  This causes
    // the dock tile to stop bouncing, lose the "running" triangle, and
    // if the tile does not permanently reside in the Dock, even disappear.
    // This can be confusing to the user, who is expecting the app to launch.
    // Calling ReceiveNextEvent without requesting any event is enough to
    // cause a dock tile for the child process to appear.
    const EventTypeSpec kFakeEventList[] = {{INT_MAX, INT_MAX}};
    EventRef event;
    ::ReceiveNextEvent(GetEventTypeCount(kFakeEventList), kFakeEventList,
                       kEventDurationNoWait, false, &event);
  }

  if (CheckArg("foreground")) {
    // The original process communicates that it was in the foreground by
    // adding this argument.  This new process, which is taking over for
    // the old one, should make itself the active application.
    ProcessSerialNumber psn;
    if (::GetCurrentProcess(&psn) == noErr) ::SetFrontProcess(&psn);
  }
#endif

  SaveToEnv("MOZ_LAUNCHED_CHILD=");

  // On Windows, the -os-restarted command line switch lets us know when we are
  // restarted via RegisterApplicationRestart. May be used for other OSes later.
  if (CheckArg("os-restarted", nullptr, CheckArgFlag::RemoveArg) == ARG_FOUND) {
    gRestartedByOS = true;
  }

  gRestartArgc = gArgc;
  gRestartArgv =
      (char**)malloc(sizeof(char*) * (gArgc + 1 + (override ? 2 : 0)));
  if (!gRestartArgv) {
    return 1;
  }

  int i;
  for (i = 0; i < gArgc; ++i) {
    gRestartArgv[i] = gArgv[i];
  }

  // Add the -override argument back (it is removed automatically be CheckArg)
  // if there is one
  if (override) {
    gRestartArgv[gRestartArgc++] = const_cast<char*>("-override");
    gRestartArgv[gRestartArgc++] = const_cast<char*>(override);
  }

  gRestartArgv[gRestartArgc] = nullptr;

  Maybe<bool> safeModeRequested = IsSafeModeRequested(gArgc, gArgv);
  if (!safeModeRequested) {
    return 1;
  }

  gSafeMode = safeModeRequested.value();

#ifdef XP_WIN
  {
    // Add CPU microcode version to the crash report as "CPUMicrocodeVersion".
    // It feels like this code may belong in nsSystemInfo instead.
    int cpuUpdateRevision = -1;
    HKEY key;
    static const WCHAR keyName[] =
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyName, 0, KEY_QUERY_VALUE, &key) ==
        ERROR_SUCCESS) {
      DWORD updateRevision[2];
      DWORD len = sizeof(updateRevision);
      DWORD vtype;

      // Windows 7 uses "Update Signature", 8 uses "Update Revision".
      // For AMD CPUs, "CurrentPatchLevel" is sometimes used.
      // Take the first one we find.
      LPCWSTR choices[] = {L"Update Signature", L"Update Revision",
                           L"CurrentPatchLevel"};
      for (size_t oneChoice = 0; oneChoice < ArrayLength(choices);
           oneChoice++) {
        if (RegQueryValueExW(key, choices[oneChoice], 0, &vtype,
                             reinterpret_cast<LPBYTE>(updateRevision),
                             &len) == ERROR_SUCCESS) {
          if (vtype == REG_BINARY && len == sizeof(updateRevision)) {
            // The first word is unused
            cpuUpdateRevision = static_cast<int>(updateRevision[1]);
            break;
          } else if (vtype == REG_DWORD && len == sizeof(updateRevision[0])) {
            cpuUpdateRevision = static_cast<int>(updateRevision[0]);
            break;
          }
        }
      }
    }

    if (cpuUpdateRevision > 0) {
      CrashReporter::AnnotateCrashReport(
          CrashReporter::Annotation::CPUMicrocodeVersion,
          nsPrintfCString("0x%x", cpuUpdateRevision));
    }
  }
#endif

  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::SafeMode,
                                     gSafeMode);

#if defined(MOZ_HAS_REMOTE)
  // Handle --no-remote and --new-instance command line arguments. Setup
  // the environment to better accommodate other components and various
  // restart scenarios.
  ar = CheckArg("no-remote");
  if (ar == ARG_FOUND || EnvHasValue("MOZ_NO_REMOTE")) {
    mDisableRemoteClient = true;
    mDisableRemoteServer = true;
    if (!EnvHasValue("MOZ_NO_REMOTE")) {
      SaveToEnv("MOZ_NO_REMOTE=1");
    }
  }

  ar = CheckArg("new-instance");
  if (ar == ARG_FOUND || EnvHasValue("MOZ_NEW_INSTANCE")) {
    mDisableRemoteClient = true;
  }
#else
  // These arguments do nothing in platforms with no remoting support but we
  // should remove them from the command line anyway.
  CheckArg("no-remote");
  CheckArg("new-instance");
#endif

  ar = CheckArg("offline");
  if (ar || EnvHasValue("XRE_START_OFFLINE")) {
    mStartOffline = true;
  }

  // Handle --help and --version command line arguments.
  // They should return quickly, so we deal with them here.
  if (CheckArg("h") || CheckArg("help") || CheckArg("?")) {
    DumpHelp();
    *aExitFlag = true;
    return 0;
  }

  if (CheckArg("v") || CheckArg("version")) {
    DumpVersion();
    *aExitFlag = true;
    return 0;
  }

  rv = XRE_InitCommandLine(gArgc, gArgv);
  NS_ENSURE_SUCCESS(rv, 1);

  return 0;
}

#ifdef XP_WIN
static bool QueryOneWMIProperty(IWbemServices* aServices,
                                const wchar_t* aWMIClass,
                                const wchar_t* aProperty, VARIANT* aResult) {
  RefPtr<IEnumWbemClassObject> enumerator;

  _bstr_t query(L"SELECT * FROM ");
  query += _bstr_t(aWMIClass);

  HRESULT hr = aServices->ExecQuery(
      _bstr_t(L"WQL"), query,
      WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
      getter_AddRefs(enumerator));

  if (FAILED(hr) || !enumerator) {
    return false;
  }

  RefPtr<IWbemClassObject> classObject;
  ULONG results;

  hr =
      enumerator->Next(WBEM_INFINITE, 1, getter_AddRefs(classObject), &results);

  if (FAILED(hr) || results == 0) {
    return false;
  }

  hr = classObject->Get(aProperty, 0, aResult, 0, 0);

  return SUCCEEDED(hr);
}

/**
 * Uses WMI to read some information that may be useful for diagnosing
 * crashes. This function is best-effort; failures shouldn't burden the
 * caller. COM must be initialized before calling.
 */

static const char kMemoryErrorCorrectionValues[][15] = {
    "Reserved",        // 0
    "Other",           // 1
    "Unknown",         // 2
    "None",            // 3
    "Parity",          // 4
    "Single-bit ECC",  // 5
    "Multi-bit ECC",   // 6
    "CRC"              // 7
};

static void AnnotateWMIData() {
  RefPtr<IWbemLocator> locator;

  HRESULT hr =
      CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                       IID_IWbemLocator, getter_AddRefs(locator));

  if (FAILED(hr)) {
    return;
  }

  RefPtr<IWbemServices> services;

  hr =
      locator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, getter_AddRefs(services));

  if (FAILED(hr)) {
    return;
  }

  hr = CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                         RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                         nullptr, EOAC_NONE);

  if (FAILED(hr)) {
    return;
  }

  VARIANT value;
  VariantInit(&value);

  // Annotate information about the system manufacturer.
  if (QueryOneWMIProperty(services, L"Win32_BIOS", L"Manufacturer", &value) &&
      V_VT(&value) == VT_BSTR) {
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::BIOS_Manufacturer,
        NS_ConvertUTF16toUTF8(V_BSTR(&value)));
  }

  VariantClear(&value);

  // Annotate information about type of memory error correction.
  if (QueryOneWMIProperty(services, L"Win32_PhysicalMemoryArray",
                          L"MemoryErrorCorrection", &value) &&
      V_VT(&value) == VT_I4) {
    long valueInt = V_I4(&value);
    nsCString valueString;
    if (valueInt < 0 ||
        valueInt >= long(ArrayLength(kMemoryErrorCorrectionValues))) {
      valueString.AssignLiteral("Unexpected value");
    } else {
      valueString.AssignASCII(kMemoryErrorCorrectionValues[valueInt]);
    }
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::MemoryErrorCorrection, valueString);
  }

  VariantClear(&value);
}

static void PR_CALLBACK AnnotateWMIData_ThreadStart(void*) {
  mscom::MTARegion mta;
  if (!mta.IsValid()) {
    return;
  }

  AnnotateWMIData();
}
#endif  // XP_WIN

#if defined(XP_LINUX) && !defined(ANDROID)

static void AnnotateLSBRelease(void*) {
  nsCString dist, desc, release, codename;
  if (widget::lsb::GetLSBRelease(dist, desc, release, codename)) {
    CrashReporter::AppendAppNotesToCrashReport(desc);
  }
}

#endif  // defined(XP_LINUX) && !defined(ANDROID)

#ifdef XP_WIN
static void ReadAheadDll(const wchar_t* dllName) {
  wchar_t dllPath[MAX_PATH];
  if (ConstructSystem32Path(dllName, dllPath, MAX_PATH)) {
    ReadAheadLib(dllPath);
  }
}

static void PR_CALLBACK ReadAheadDlls_ThreadStart(void*) {
  // Load DataExchange.dll and twinapi.appcore.dll for nsWindow::EnableDragDrop
  ReadAheadDll(L"DataExchange.dll");
  ReadAheadDll(L"twinapi.appcore.dll");

  // Load twinapi.dll for WindowsUIUtils::UpdateTabletModeState
  ReadAheadDll(L"twinapi.dll");

  // Load explorerframe.dll for WinTaskbar::Initialize
  ReadAheadDll(L"ExplorerFrame.dll");

  // Load WinTypes.dll for nsOSHelperAppService::GetApplicationDescription
  ReadAheadDll(L"WinTypes.dll");
}
#endif

#if defined(MOZ_WAYLAND)
bool IsWaylandDisabled() {
  // Enable Wayland on Gtk+ >= 3.22 where we can expect recent enough
  // compositor & libwayland interface.
  bool disableWayland = (gtk_check_version(3, 22, 0) != nullptr);
  if (!disableWayland) {
    // Make X11 backend the default one unless MOZ_ENABLE_WAYLAND or
    // GDK_BACKEND are specified.
    disableWayland = (PR_GetEnv("GDK_BACKEND") == nullptr) &&
                     (PR_GetEnv("MOZ_ENABLE_WAYLAND") == nullptr);
  }
  return disableWayland;
}
#endif

namespace mozilla::startup {
Result<nsCOMPtr<nsIFile>, nsresult> GetIncompleteStartupFile(nsIFile* aProfLD) {
  nsCOMPtr<nsIFile> crashFile;
  MOZ_TRY(aProfLD->Clone(getter_AddRefs(crashFile)));
  MOZ_TRY(crashFile->Append(FILE_STARTUP_INCOMPLETE));
  return std::move(crashFile);
}
}  // namespace mozilla::startup

// Check whether the last startup attempt resulted in a crash within the
// last 6 hours.
// Note that this duplicates the logic in nsAppStartup::TrackStartupCrashBegin,
// which runs too late for our purposes.
Result<bool, nsresult> XREMain::CheckLastStartupWasCrash() {
  constexpr int32_t MAX_TIME_SINCE_STARTUP = 6 * 60 * 60 * 1000;

  nsCOMPtr<nsIFile> crashFile;
  MOZ_TRY_VAR(crashFile, GetIncompleteStartupFile(mProfLD));

  // Attempt to create the incomplete startup canary file. If the file already
  // exists, this fails, and we know the last startup was a success. If it
  // doesn't already exist, it is created, and will be removed at the end of
  // the startup crash detection window.
  AutoFDClose fd;
  Unused << crashFile->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | PR_EXCL,
                                        0666, &fd.rwget());
  if (fd) {
    return false;
  }

  PRTime lastModifiedTime;
  MOZ_TRY(crashFile->GetLastModifiedTime(&lastModifiedTime));

  // If the file exists, and was created within the appropriate time window,
  // the last startup was recent and resulted in a crash.
  PRTime now = PR_Now() / PR_USEC_PER_MSEC;
  return now - lastModifiedTime <= MAX_TIME_SINCE_STARTUP;
}

/*
 * XRE_mainStartup - Initializes the profile and various other services.
 * Main() will exit early if either return value != 0 or if aExitFlag is
 * true.
 */
int XREMain::XRE_mainStartup(bool* aExitFlag) {
  nsresult rv;

  if (!aExitFlag) return 1;
  *aExitFlag = false;

  // Enable Telemetry IO Reporting on DEBUG, nightly and local builds,
  // but disable it on FUZZING builds.
#ifndef FUZZING
#  ifdef DEBUG
  mozilla::Telemetry::InitIOReporting(gAppData->xreDirectory);
#  else
  {
    const char* releaseChannel = MOZ_STRINGIFY(MOZ_UPDATE_CHANNEL);
    if (strcmp(releaseChannel, "nightly") == 0 ||
        strcmp(releaseChannel, "default") == 0) {
      mozilla::Telemetry::InitIOReporting(gAppData->xreDirectory);
    }
  }
#  endif /* DEBUG */
#endif   /* FUZZING */

#if defined(XP_WIN)
  // Enable the HeapEnableTerminationOnCorruption exploit mitigation. We ignore
  // the return code because it always returns success, although it has no
  // effect on Windows older than XP SP3.
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
#endif /* XP_WIN */

#if defined(MOZ_WIDGET_GTK)
  // Stash DESKTOP_STARTUP_ID in malloc'ed memory because gtk_init will clear
  // it.
#  define HAVE_DESKTOP_STARTUP_ID
  const char* desktopStartupIDEnv = PR_GetEnv("DESKTOP_STARTUP_ID");
  if (desktopStartupIDEnv) {
    mDesktopStartupID.Assign(desktopStartupIDEnv);
  }
#endif

#if defined(MOZ_WIDGET_GTK)
  // setup for private colormap.  Ideally we'd like to do this
  // in nsAppShell::Create, but we need to get in before gtk
  // has been initialized to make sure everything is running
  // consistently.

  // Set program name to the one defined in application.ini.
  {
    nsAutoCString program(gAppData->name);
    ToLowerCase(program);
    g_set_prgname(program.get());
  }

  // Initialize GTK here for splash.

#  if defined(MOZ_WIDGET_GTK) && defined(MOZ_X11)
  // Disable XInput2 multidevice support due to focus bugginess.
  // See bugs 1182700, 1170342.
  // gdk_disable_multidevice() affects Gdk X11 backend only,
  // the multidevice support is always enabled on Wayland backend.
  const char* useXI2 = PR_GetEnv("MOZ_USE_XINPUT2");
  if (!useXI2 || (*useXI2 == '0')) gdk_disable_multidevice();
#  endif

  // Open the display ourselves instead of using gtk_init, so that we can
  // close it without fear that one day gtk might clean up the display it
  // opens.
  if (!gtk_parse_args(&gArgc, &gArgv)) return 1;
#endif /* MOZ_WIDGET_GTK */

#ifdef FUZZING
  if (PR_GetEnv("FUZZER")) {
    *aExitFlag = true;
    return mozilla::fuzzerRunner->Run(&gArgc, &gArgv);
  }
#endif

  if (PR_GetEnv("MOZ_RUN_GTEST")) {
    int result;
#ifdef XP_WIN
    UseParentConsole();
#endif
    // RunGTest will only be set if we're in xul-unit
    if (mozilla::RunGTest) {
      gIsGtest = true;
      result = mozilla::RunGTest(&gArgc, gArgv);
      gIsGtest = false;
    } else {
      result = 1;
      printf("TEST-UNEXPECTED-FAIL | gtest | Not compiled with enable-tests\n");
    }
    *aExitFlag = true;
    return result;
  }

#ifdef MOZ_HAS_REMOTE
  if (gfxPlatform::IsHeadless()) {
    mDisableRemoteClient = true;
    mDisableRemoteServer = true;
  }
#endif

#ifdef MOZ_X11
  // Init X11 in thread-safe mode. Must be called prior to the first call to
  // XOpenDisplay (called inside gdk_display_open). This is a requirement for
  // off main tread compositing.
  if (!gfxPlatform::IsHeadless()) {
    XInitThreads();
  }
#endif
#if defined(MOZ_WIDGET_GTK)
  if (!gfxPlatform::IsHeadless()) {
    const char* display_name = nullptr;
    bool saveDisplayArg = false;

    // display_name is owned by gdk.
    display_name = gdk_get_display_arg_name();
    // if --display argument is given make sure it's
    // also passed to ContentChild::Init() by MOZ_GDK_DISPLAY.
    if (display_name) {
      SaveWordToEnv("MOZ_GDK_DISPLAY", nsDependentCString(display_name));
      saveDisplayArg = true;
    }

    bool disableWayland = true;
#  if defined(MOZ_WAYLAND)
    disableWayland = IsWaylandDisabled();
#  endif
    // On Wayland disabled builds read X11 DISPLAY env exclusively
    // and don't care about different displays.
    if (disableWayland && !display_name) {
      display_name = PR_GetEnv("DISPLAY");
      if (!display_name) {
        PR_fprintf(PR_STDERR,
                   "Error: no DISPLAY environment variable specified\n");
        return 1;
      }
    }

    if (display_name) {
      mGdkDisplay = gdk_display_open(display_name);
      if (!mGdkDisplay) {
        PR_fprintf(PR_STDERR, "Error: cannot open display: %s\n", display_name);
        return 1;
      }
      gdk_display_manager_set_default_display(gdk_display_manager_get(),
                                              mGdkDisplay);
      if (saveDisplayArg) {
        if (GDK_IS_X11_DISPLAY(mGdkDisplay)) {
          SaveWordToEnv("DISPLAY", nsDependentCString(display_name));
        }
#  ifdef MOZ_WAYLAND
        else if (!GDK_IS_X11_DISPLAY(mGdkDisplay)) {
          SaveWordToEnv("WAYLAND_DISPLAY", nsDependentCString(display_name));
        }
#  endif
      }
    }
#  ifdef MOZ_WIDGET_GTK
    else {
      mGdkDisplay =
          gdk_display_manager_open_display(gdk_display_manager_get(), nullptr);
    }
#  endif
  }
#endif
#if defined(MOZ_HAS_REMOTE)
  // handle --remote now that xpcom is fired up
  mRemoteService = new nsRemoteService(gAppData->remotingName);
  if (mRemoteService && !mDisableRemoteServer) {
    mRemoteService->LockStartup();
  }
#endif
#if defined(MOZ_WIDGET_GTK)
  g_set_application_name(mAppData->name);

#endif /* defined(MOZ_WIDGET_GTK) */
#ifdef MOZ_X11
  // Do this after initializing GDK, or GDK will install its own handler.
  XRE_InstallX11ErrorHandler();
#endif

  // Call the code to install our handler
#ifdef MOZ_JPROF
  setupProfilingStuff();
#endif

  rv = NS_CreateNativeAppSupport(getter_AddRefs(mNativeApp));
  if (NS_FAILED(rv)) return 1;

  bool canRun = false;
  rv = mNativeApp->Start(&canRun);
  if (NS_FAILED(rv) || !canRun) {
    return 1;
  }

#if defined(HAVE_DESKTOP_STARTUP_ID) && defined(MOZ_WIDGET_GTK)
  // DESKTOP_STARTUP_ID is cleared now,
  // we recover it in case we need a restart.
  if (!mDesktopStartupID.IsEmpty()) {
    nsAutoCString desktopStartupEnv;
    desktopStartupEnv.AssignLiteral("DESKTOP_STARTUP_ID=");
    desktopStartupEnv.Append(mDesktopStartupID);
    // Leak it with extreme prejudice!
    PR_SetEnv(ToNewCString(desktopStartupEnv));
  }
#endif

  // Support exiting early for testing startup sequence. Bug 1360493
  if (CheckArg("test-launch-without-hang")) {
    *aExitFlag = true;
    return 0;
  }

  rv = NS_NewToolkitProfileService(getter_AddRefs(mProfileSvc));
  if (rv == NS_ERROR_FILE_ACCESS_DENIED) {
    PR_fprintf(PR_STDERR,
               "Error: Access was denied while trying to open files in "
               "your profile directory.\n");
  }
  if (NS_FAILED(rv)) {
    // We failed to choose or create profile - notify user and quit
    ProfileMissingDialog(mNativeApp);
    return 1;
  }

  bool wasDefaultSelection;
  nsCOMPtr<nsIToolkitProfile> profile;
  rv = SelectProfile(mProfileSvc, mNativeApp, getter_AddRefs(mProfD),
                     getter_AddRefs(mProfLD), getter_AddRefs(profile),
                     &wasDefaultSelection);
  if (rv == NS_ERROR_LAUNCHED_CHILD_PROCESS || rv == NS_ERROR_ABORT) {
    *aExitFlag = true;
    return 0;
  }

  if (NS_FAILED(rv)) {
    // We failed to choose or create profile - notify user and quit
    ProfileMissingDialog(mNativeApp);
    return 1;
  }

#if defined(MOZ_HAS_REMOTE)
  if (mRemoteService) {
    // We want a unique profile name to identify the remote instance.
    nsCString profileName;
    if (profile) {
      rv = profile->GetName(profileName);
    }
    if (!profile || NS_FAILED(rv) || profileName.IsEmpty()) {
      // Couldn't get a name from the profile. Use the directory name?
      nsString leafName;
      rv = mProfD->GetLeafName(leafName);
      if (NS_SUCCEEDED(rv)) {
        profileName = NS_ConvertUTF16toUTF8(leafName);
      }
    }

    mRemoteService->SetProfile(profileName);

    if (!mDisableRemoteClient) {
      // Try to remote the entire command line. If this fails, start up
      // normally.
      const char* desktopStartupIDPtr =
          mDesktopStartupID.IsEmpty() ? nullptr : mDesktopStartupID.get();

      RemoteResult rr = mRemoteService->StartClient(desktopStartupIDPtr);
      if (rr == REMOTE_FOUND) {
        *aExitFlag = true;
        mRemoteService->UnlockStartup();
        return 0;
      }
      if (rr == REMOTE_ARG_BAD) {
        mRemoteService->UnlockStartup();
        return 1;
      }
    }
  }
#endif

#if defined(MOZ_UPDATER) && !defined(MOZ_WIDGET_ANDROID)
  // Check for and process any available updates
  nsCOMPtr<nsIFile> updRoot;
  bool persistent;
  rv = mDirProvider.GetFile(XRE_UPDATE_ROOT_DIR, &persistent,
                            getter_AddRefs(updRoot));
  // XRE_UPDATE_ROOT_DIR may fail. Fallback to appDir if failed
  if (NS_FAILED(rv)) {
    updRoot = mDirProvider.GetAppDir();
  }

  // If the MOZ_TEST_PROCESS_UPDATES environment variable already exists, then
  // we are being called from the callback application.
  if (EnvHasValue("MOZ_TEST_PROCESS_UPDATES")) {
    // If the caller has asked us to log our arguments, do so.  This is used
    // to make sure that the maintenance service successfully launches the
    // callback application.
    const char* logFile = nullptr;
    if (ARG_FOUND == CheckArg("dump-args", &logFile)) {
      FILE* logFP = fopen(logFile, "wb");
      if (logFP) {
        for (int i = 1; i < gRestartArgc; ++i) {
          fprintf(logFP, "%s\n", gRestartArgv[i]);
        }
        fclose(logFP);
      }
    }
    *aExitFlag = true;
    return 0;
  }

  // Support for processing an update and exiting. The MOZ_TEST_PROCESS_UPDATES
  // environment variable will be part of the updater's environment and the
  // application that is relaunched by the updater. When the application is
  // relaunched by the updater it will be removed below and the application
  // will exit.
  if (CheckArg("test-process-updates")) {
    SaveToEnv("MOZ_TEST_PROCESS_UPDATES=1");
  }
  nsCOMPtr<nsIFile> exeFile, exeDir;
  rv = mDirProvider.GetFile(XRE_EXECUTABLE_FILE, &persistent,
                            getter_AddRefs(exeFile));
  NS_ENSURE_SUCCESS(rv, 1);
  rv = exeFile->GetParent(getter_AddRefs(exeDir));
  NS_ENSURE_SUCCESS(rv, 1);
  ProcessUpdates(mDirProvider.GetGREDir(), exeDir, updRoot, gRestartArgc,
                 gRestartArgv, mAppData->version);
  if (EnvHasValue("MOZ_TEST_PROCESS_UPDATES")) {
    SaveToEnv("MOZ_TEST_PROCESS_UPDATES=");
    *aExitFlag = true;
    return 0;
  }
#endif

  // We now know there is no existing instance using the selected profile. If
  // the profile wasn't selected by specific command line arguments and the
  // user has chosen to show the profile manager on startup then do that.
  if (wasDefaultSelection) {
    bool useSelectedProfile;
    rv = mProfileSvc->GetStartWithLastProfile(&useSelectedProfile);
    NS_ENSURE_SUCCESS(rv, 1);

    if (!useSelectedProfile) {
      rv = ShowProfileManager(mProfileSvc, mNativeApp);
      if (rv == NS_ERROR_LAUNCHED_CHILD_PROCESS || rv == NS_ERROR_ABORT) {
        *aExitFlag = true;
        return 0;
      }
      if (NS_FAILED(rv)) {
        return 1;
      }
    }
  }

  // We always want to lock the profile even if we're actually going to reset
  // it later.
  rv = LockProfile(mNativeApp, mProfD, mProfLD, profile,
                   getter_AddRefs(mProfileLock));
  if (rv == NS_ERROR_LAUNCHED_CHILD_PROCESS || rv == NS_ERROR_ABORT) {
    *aExitFlag = true;
    return 0;
  } else if (NS_FAILED(rv)) {
    return 1;
  }

  if (gDoProfileReset) {
    // Unlock the source profile.
    mProfileLock->Unlock();

    // If we're resetting a profile, create a new one and use it to startup.
    gResetOldProfile = profile;
    rv = mProfileSvc->CreateResetProfile(getter_AddRefs(profile));
    if (NS_SUCCEEDED(rv)) {
      rv = profile->GetRootDir(getter_AddRefs(mProfD));
      NS_ENSURE_SUCCESS(rv, 1);
      SaveFileToEnv("XRE_PROFILE_PATH", mProfD);

      rv = profile->GetLocalDir(getter_AddRefs(mProfLD));
      NS_ENSURE_SUCCESS(rv, 1);
      SaveFileToEnv("XRE_PROFILE_LOCAL_PATH", mProfLD);

      // Lock the new profile
      rv = LockProfile(mNativeApp, mProfD, mProfLD, profile,
                       getter_AddRefs(mProfileLock));
      if (rv == NS_ERROR_LAUNCHED_CHILD_PROCESS || rv == NS_ERROR_ABORT) {
        *aExitFlag = true;
        return 0;
      } else if (NS_FAILED(rv)) {
        return 1;
      }
    } else {
      NS_WARNING("Profile reset failed.");
      return 1;
    }
  }

  gProfileLock = mProfileLock;

  nsAutoCString version;
  BuildVersion(version);

#ifdef TARGET_OS_ABI
  NS_NAMED_LITERAL_CSTRING(osABI, TARGET_OS_ABI);
#else
  // No TARGET_XPCOM_ABI, but at least the OS is known
  NS_NAMED_LITERAL_CSTRING(osABI, OS_TARGET "_UNKNOWN");
#endif

  // Check for version compatibility with the last version of the app this
  // profile was started with.  The format of the version stamp is defined
  // by the BuildVersion function.
  // Also check to see if something has happened to invalidate our
  // fastload caches, like an extension upgrade or installation.

  // If we see .purgecaches, that means someone did a make.
  // Re-register components to catch potential changes.
  nsCOMPtr<nsIFile> flagFile;
  if (mAppData->directory) {
    Unused << mAppData->directory->Clone(getter_AddRefs(flagFile));
  }
  if (flagFile) {
    flagFile->AppendNative(FILE_INVALIDATE_CACHES);
  }

  bool cachesOK;
  bool isDowngrade;
  nsCString lastVersion;
  bool versionOK = CheckCompatibility(
      mProfD, version, osABI, mDirProvider.GetGREDir(), mAppData->directory,
      flagFile, &cachesOK, &isDowngrade, lastVersion);

  MOZ_RELEASE_ASSERT(!cachesOK || versionOK,
                     "Caches cannot be good if the version has changed.");

#ifdef MOZ_BLOCK_PROFILE_DOWNGRADE
  // The argument check must come first so the argument is always removed from
  // the command line regardless of whether this is a downgrade or not.
  if (!CheckArg("allow-downgrade") && isDowngrade &&
      !EnvHasValue("MOZ_ALLOW_DOWNGRADE")) {
    rv = CheckDowngrade(mProfD, mNativeApp, mProfileSvc, lastVersion);
    if (rv == NS_ERROR_LAUNCHED_CHILD_PROCESS || rv == NS_ERROR_ABORT) {
      *aExitFlag = true;
      return 0;
    }
  }
#endif

  rv = mDirProvider.SetProfile(mProfD, mProfLD);
  NS_ENSURE_SUCCESS(rv, 1);

  //////////////////////// NOW WE HAVE A PROFILE ////////////////////////

  mozilla::Telemetry::SetProfileDir(mProfD);

  if (mAppData->flags & NS_XRE_ENABLE_CRASH_REPORTER) {
    MakeOrSetMinidumpPath(mProfD);
  }

  CrashReporter::SetProfileDirectory(mProfD);

#ifdef MOZ_ASAN_REPORTER
  // In ASan reporter builds, we need to set ASan's log_path as early as
  // possible, so it dumps its errors into files there instead of using
  // the default stderr location. Since this is crucial for ASan reporter
  // to work at all (and we don't want people to use a non-functional
  // ASan reporter build), all failures while setting log_path are fatal.
  setASanReporterPath(mProfD);

  // Export to env for child processes
  SaveFileToEnv("ASAN_REPORTER_PATH", mProfD);
#endif

  bool lastStartupWasCrash = CheckLastStartupWasCrash().unwrapOr(false);

  if (CheckArg("purgecaches") || PR_GetEnv("MOZ_PURGE_CACHES") ||
      lastStartupWasCrash || gSafeMode) {
    cachesOK = false;
  }

  // Every time a profile is loaded by a build with a different version,
  // it updates the compatibility.ini file saying what version last wrote
  // the fastload caches.  On subsequent launches if the version matches,
  // there is no need for re-registration.  If the user loads the same
  // profile in different builds the component registry must be
  // re-generated to prevent mysterious component loading failures.
  //
  bool startupCacheValid = true;

  if (!cachesOK || !versionOK) {
    QuotaManager::InvalidateQuotaCache();

    startupCacheValid = RemoveComponentRegistries(mProfD, mProfLD, false);

    // Rewrite compatibility.ini to match the current build. The next run
    // should attempt to invalidate the caches if either this run is safe mode
    // or the attempt to invalidate the caches this time failed.
    WriteVersion(mProfD, version, osABI, mDirProvider.GetGREDir(),
                 mAppData->directory, gSafeMode || !startupCacheValid);
  }

  if (!startupCacheValid) StartupCache::IgnoreDiskCache();

  if (flagFile) {
    flagFile->Remove(true);
  }

  return 0;
}

#if defined(MOZ_SANDBOX)
void AddSandboxAnnotations() {
  // Include the sandbox content level, regardless of platform
  int level = GetEffectiveContentSandboxLevel();

  nsAutoCString levelString;
  levelString.AppendInt(level);

  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::ContentSandboxLevel, levelString);

  // Include whether or not this instance is capable of content sandboxing
  bool sandboxCapable = false;

#  if defined(XP_WIN)
  // All supported Windows versions support some level of content sandboxing
  sandboxCapable = true;
#  elif defined(XP_MACOSX)
  // All supported OS X versions are capable
  sandboxCapable = true;
#  elif defined(XP_LINUX)
  sandboxCapable = SandboxInfo::Get().CanSandboxContent();
#  elif defined(__OpenBSD__)
  sandboxCapable = true;
  StartOpenBSDSandbox(GeckoProcessType_Default);
#  endif

  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::ContentSandboxCapable, sandboxCapable);
}
#endif /* MOZ_SANDBOX */

/*
 * XRE_mainRun - Command line startup, profile migration, and
 * the calling of appStartup->Run().
 */
nsresult XREMain::XRE_mainRun() {
  nsresult rv = NS_OK;
  NS_ASSERTION(mScopedXPCOM, "Scoped xpcom not initialized.");

#if defined(XP_WIN)
  RefPtr<mozilla::DllServices> dllServices(mozilla::DllServices::Get());
  dllServices->StartUntrustedModulesProcessor();
  auto dllServicesDisable =
      MakeScopeExit([&dllServices]() { dllServices->DisableFull(); });
#endif  // defined(XP_WIN)

#ifdef NS_FUNCTION_TIMER
  // initialize some common services, so we don't pay the cost for these at odd
  // times later on; SetWindowCreator -> ChromeRegistry -> IOService ->
  // SocketTransportService -> (nspr wspm init), Prefs
  {
    nsCOMPtr<nsISupports> comp;

    comp = do_GetService("@mozilla.org/preferences-service;1");

    comp = do_GetService("@mozilla.org/network/socket-transport-service;1");

    comp = do_GetService("@mozilla.org/network/dns-service;1");

    comp = do_GetService("@mozilla.org/network/io-service;1");

    comp = do_GetService("@mozilla.org/chrome/chrome-registry;1");

    comp = do_GetService("@mozilla.org/focus-event-suppressor-service;1");
  }
#endif

  rv = mScopedXPCOM->SetWindowCreator(mNativeApp);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  // tell the crash reporter to also send the release channel
  nsCOMPtr<nsIPrefService> prefs =
      do_GetService("@mozilla.org/preferences-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIPrefBranch> defaultPrefBranch;
    rv = prefs->GetDefaultBranch(nullptr, getter_AddRefs(defaultPrefBranch));

    if (NS_SUCCEEDED(rv)) {
      nsAutoCString sval;
      rv = defaultPrefBranch->GetCharPref("app.update.channel", sval);
      if (NS_SUCCEEDED(rv)) {
        CrashReporter::AnnotateCrashReport(
            CrashReporter::Annotation::ReleaseChannel, sval);
      }
    }
  }
  // Needs to be set after xpcom initialization.
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::FramePoisonBase,
      nsPrintfCString("%.16" PRIu64, uint64_t(gMozillaPoisonBase)));
  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::FramePoisonSize,
                                     uint32_t(gMozillaPoisonSize));

  bool includeContextHeap =
      Preferences::GetBool("toolkit.crashreporter.include_context_heap", false);
  CrashReporter::SetIncludeContextHeap(includeContextHeap);

#ifdef XP_WIN
  PR_CreateThread(PR_USER_THREAD, AnnotateWMIData_ThreadStart, 0,
                  PR_PRIORITY_LOW, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD, 0);
#endif

#if defined(XP_LINUX) && !defined(ANDROID)
  PR_CreateThread(PR_USER_THREAD, AnnotateLSBRelease, 0, PR_PRIORITY_LOW,
                  PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD, 0);
#endif

  if (mStartOffline) {
    nsCOMPtr<nsIIOService> io(
        do_GetService("@mozilla.org/network/io-service;1"));
    NS_ENSURE_TRUE(io, NS_ERROR_FAILURE);
    io->SetManageOfflineStatus(false);
    io->SetOffline(true);
  }

#ifdef XP_WIN
  if (!PR_GetEnv("XRE_NO_DLL_READAHEAD")) {
    PR_CreateThread(PR_USER_THREAD, ReadAheadDlls_ThreadStart, 0,
                    PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD,
                    0);
  }
#endif

  if (gDoMigration) {
    nsCOMPtr<nsIFile> file;
    mDirProvider.GetAppDir()->Clone(getter_AddRefs(file));
    file->AppendNative(NS_LITERAL_CSTRING("override.ini"));
    nsINIParser parser;
    nsresult rv = parser.Init(file);
    // if override.ini doesn't exist, also check for distribution.ini
    if (NS_FAILED(rv)) {
      bool persistent;
      mDirProvider.GetFile(XRE_APP_DISTRIBUTION_DIR, &persistent,
                           getter_AddRefs(file));
      file->AppendNative(NS_LITERAL_CSTRING("distribution.ini"));
      rv = parser.Init(file);
    }
    if (NS_SUCCEEDED(rv)) {
      nsAutoCString buf;
      rv = parser.GetString("XRE", "EnableProfileMigrator", buf);
      if (NS_SUCCEEDED(rv)) {
        if (buf[0] == '0' || buf[0] == 'f' || buf[0] == 'F') {
          gDoMigration = false;
        }
      }
    }
  }

  // We'd like to initialize the JSContext *after* reading the user prefs.
  // Unfortunately that's not possible if we have to do profile migration
  // because that requires us to execute JS before reading user prefs.
  // Restarting the browser after profile migration would fix this. See
  // bug 1592523.
  bool initializedJSContext = false;

  {
    // Profile Migration
    if (mAppData->flags & NS_XRE_ENABLE_PROFILE_MIGRATOR && gDoMigration) {
      gDoMigration = false;

      xpc::InitializeJSContext();
      initializedJSContext = true;

      nsCOMPtr<nsIProfileMigrator> pm(
          do_CreateInstance(NS_PROFILEMIGRATOR_CONTRACTID));
      if (pm) {
        nsAutoCString aKey;
        nsAutoCString aName;
        if (gDoProfileReset) {
          // Automatically migrate from the current application if we just
          // reset the profile.
          aKey = MOZ_APP_NAME;
          gResetOldProfile->GetName(aName);
        }
        pm->Migrate(&mDirProvider, aKey, aName);
      }
    }

    if (gDoProfileReset) {
      if (!initializedJSContext) {
        xpc::InitializeJSContext();
        initializedJSContext = true;
      }

      nsresult backupCreated =
          ProfileResetCleanup(mProfileSvc, gResetOldProfile);
      if (NS_FAILED(backupCreated)) {
        NS_WARNING("Could not cleanup the profile that was reset");
      }
    }
  }

#ifndef XP_WIN
  nsCOMPtr<nsIFile> profileDir;
  nsAutoCString path;
  rv = mDirProvider.GetProfileStartupDir(getter_AddRefs(profileDir));
  if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(profileDir->GetNativePath(path)) &&
      !IsUtf8(path)) {
    PR_fprintf(
        PR_STDERR,
        "Error: The profile path is not valid UTF-8. Unable to continue.\n");
    return NS_ERROR_FAILURE;
  }
#endif

  // Initialize user preferences before notifying startup observers so they're
  // ready in time for early consumers, such as the component loader.
  mDirProvider.InitializeUserPrefs();

  // Now that all (user) prefs have been loaded we can initialize the main
  // thread's JSContext.
  if (!initializedJSContext) {
    xpc::InitializeJSContext();
  }

  // Finally, now that JS has been initialized, we can finish pref loading. This
  // needs to happen after JS and XPConnect initialization because AutoConfig
  // files require JS execution. Note that this means AutoConfig files can't
  // override JS engine start-up prefs.
  mDirProvider.FinishInitializingUserPrefs();

  nsAppStartupNotifier::NotifyObservers(APPSTARTUP_CATEGORY);

  nsCOMPtr<nsIAppStartup> appStartup(components::AppStartup::Service());
  NS_ENSURE_TRUE(appStartup, NS_ERROR_FAILURE);

  mDirProvider.DoStartup();

#ifdef MOZ_THUNDERBIRD
  if (Preferences::GetBool("security.prompt_for_master_password_on_startup",
                           false)) {
    // Prompt for the master password prior to opening application windows,
    // to avoid the race that triggers multiple prompts (see bug 177175).
    // We use this code until we have a better solution, possibly as
    // described in bug 177175 comment 384.
    nsCOMPtr<nsIPK11TokenDB> db =
        do_GetService("@mozilla.org/security/pk11tokendb;1");
    nsCOMPtr<nsIPK11Token> token;
    if (NS_SUCCEEDED(db->GetInternalKeyToken(getter_AddRefs(token)))) {
      Unused << token->Login(false);
    }
  }
#endif

  // As FilePreferences need the profile directory, we must initialize right
  // here.
  mozilla::FilePreferences::InitDirectoriesWhitelist();
  mozilla::FilePreferences::InitPrefs();

  OverrideDefaultLocaleIfNeeded();

  nsCString userAgentLocale;
  LocaleService::GetInstance()->GetAppLocaleAsBCP47(userAgentLocale);
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::useragent_locale, userAgentLocale);

  appStartup->GetShuttingDown(&mShuttingDown);

  nsCOMPtr<nsICommandLineRunner> cmdLine;

  nsCOMPtr<nsIFile> workingDir;
  rv = NS_GetSpecialDirectory(NS_OS_CURRENT_WORKING_DIR,
                              getter_AddRefs(workingDir));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  if (!mShuttingDown) {
    cmdLine = new nsCommandLine();

    rv = cmdLine->Init(gArgc, gArgv, workingDir,
                       nsICommandLine::STATE_INITIAL_LAUNCH);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    /* Special-case services that need early access to the command
        line. */
    nsCOMPtr<nsIObserverService> obsService =
        mozilla::services::GetObserverService();
    if (obsService) {
      obsService->NotifyObservers(cmdLine, "command-line-startup", nullptr);
    }
  }

#ifdef XP_WIN
  // Hack to sync up the various environment storages. XUL_APP_FILE is special
  // in that it comes from a different CRT (firefox.exe's static-linked copy).
  // Ugly details in http://bugzil.la/1175039#c27
  char appFile[MAX_PATH];
  if (GetEnvironmentVariableA("XUL_APP_FILE", appFile, sizeof(appFile))) {
    SmprintfPointer saved = mozilla::Smprintf("XUL_APP_FILE=%s", appFile);
    // We intentionally leak the string here since it is required by PR_SetEnv.
    PR_SetEnv(saved.release());
  }
#endif

  mozilla::AppShutdown::SaveEnvVarsForPotentialRestart();

  // clear out any environment variables which may have been set
  // during the relaunch process now that we know we won't be relaunching.
  SaveToEnv("XRE_PROFILE_PATH=");
  SaveToEnv("XRE_PROFILE_LOCAL_PATH=");
  SaveToEnv("XRE_START_OFFLINE=");
  SaveToEnv("XUL_APP_FILE=");
  SaveToEnv("XRE_BINARY_PATH=");
  SaveToEnv("XRE_RESTARTED_BY_PROFILE_MANAGER=");

  if (!mShuttingDown) {
#ifdef XP_MACOSX
    bool lazyHiddenWindow = false;
#else
    bool lazyHiddenWindow =
        Preferences::GetBool("toolkit.lazyHiddenWindow", false);
#endif

    if (!lazyHiddenWindow) {
      rv = appStartup->CreateHiddenWindow();
      NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    }

#ifdef XP_WIN
    Preferences::RegisterCallbackAndCall(RegisterApplicationRestartChanged,
                                         PREF_WIN_REGISTER_APPLICATION_RESTART);
#  if defined(MOZ_LAUNCHER_PROCESS)
    SetupLauncherProcessPref();
#  endif  // defined(MOZ_LAUNCHER_PROCESS)
#  if defined(MOZ_DEFAULT_BROWSER_AGENT)
    Preferences::RegisterCallbackAndCall(&OnDefaultAgentTelemetryPrefChanged,
                                         kPrefHealthReportUploadEnabled);
    Preferences::RegisterCallbackAndCall(&OnDefaultAgentTelemetryPrefChanged,
                                         kPrefDefaultAgentEnabled);
#  endif  // defined(MOZ_DEFAULT_BROWSER_AGENT)
#endif

#if defined(HAVE_DESKTOP_STARTUP_ID) && defined(MOZ_WIDGET_GTK)
    // Clear the environment variable so it won't be inherited by
    // child processes and confuse things.
    g_unsetenv("DESKTOP_STARTUP_ID");
#endif

#ifdef XP_MACOSX
    // we re-initialize the command-line service and do appleevents munging
    // after we are sure that we're not restarting
    cmdLine = new nsCommandLine();

    char** tempArgv = static_cast<char**>(malloc(gArgc * sizeof(char*)));
    for (int i = 0; i < gArgc; i++) {
      tempArgv[i] = strdup(gArgv[i]);
    }
    CommandLineServiceMac::SetupMacCommandLine(gArgc, tempArgv, false);
    rv = cmdLine->Init(gArgc, tempArgv, workingDir,
                       nsICommandLine::STATE_INITIAL_LAUNCH);
    free(tempArgv);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
#endif

    nsCOMPtr<nsIObserverService> obsService =
        mozilla::services::GetObserverService();
    if (obsService)
      obsService->NotifyObservers(nullptr, "final-ui-startup", nullptr);

    (void)appStartup->DoneStartingUp();

    CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::StartupCrash,
                                       false);

    appStartup->GetShuttingDown(&mShuttingDown);
  }

  if (!mShuttingDown) {
    rv = cmdLine->Run();
    NS_ENSURE_SUCCESS_LOG(rv, NS_ERROR_FAILURE);

    appStartup->GetShuttingDown(&mShuttingDown);
  }

  if (!mShuttingDown) {
#if defined(MOZ_HAS_REMOTE)
    // if we have X remote support, start listening for requests on the
    // proxy window.
    if (mRemoteService && !mDisableRemoteServer) {
      mRemoteService->StartupServer();
      mRemoteService->UnlockStartup();
    }
#endif /* MOZ_WIDGET_GTK */

    mNativeApp->Enable();
  }

#ifdef MOZ_INSTRUMENT_EVENT_LOOP
  if (PR_GetEnv("MOZ_INSTRUMENT_EVENT_LOOP")) {
    bool logToConsole = true;
    mozilla::InitEventTracing(logToConsole);
  }
#endif /* MOZ_INSTRUMENT_EVENT_LOOP */

  // Send Telemetry about Gecko version and buildid
  Telemetry::ScalarSet(Telemetry::ScalarID::GECKO_VERSION,
                       NS_ConvertASCIItoUTF16(gAppData->version));
  Telemetry::ScalarSet(Telemetry::ScalarID::GECKO_BUILD_ID,
                       NS_ConvertASCIItoUTF16(gAppData->buildID));

#if defined(MOZ_SANDBOX) && defined(XP_LINUX)
  // If we're on Linux, we now have information about the OS capabilities
  // available to us.
  SandboxInfo sandboxInfo = SandboxInfo::Get();
  Telemetry::Accumulate(Telemetry::SANDBOX_HAS_SECCOMP_BPF,
                        sandboxInfo.Test(SandboxInfo::kHasSeccompBPF));
  Telemetry::Accumulate(Telemetry::SANDBOX_HAS_SECCOMP_TSYNC,
                        sandboxInfo.Test(SandboxInfo::kHasSeccompTSync));
  Telemetry::Accumulate(
      Telemetry::SANDBOX_HAS_USER_NAMESPACES_PRIVILEGED,
      sandboxInfo.Test(SandboxInfo::kHasPrivilegedUserNamespaces));
  Telemetry::Accumulate(Telemetry::SANDBOX_HAS_USER_NAMESPACES,
                        sandboxInfo.Test(SandboxInfo::kHasUserNamespaces));
  Telemetry::Accumulate(Telemetry::SANDBOX_CONTENT_ENABLED,
                        sandboxInfo.Test(SandboxInfo::kEnabledForContent));
  Telemetry::Accumulate(Telemetry::SANDBOX_MEDIA_ENABLED,
                        sandboxInfo.Test(SandboxInfo::kEnabledForMedia));
  nsAutoCString flagsString;
  flagsString.AppendInt(sandboxInfo.AsInteger());

  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::ContentSandboxCapabilities, flagsString);
#endif /* MOZ_SANDBOX && XP_LINUX */

#if defined(XP_WIN)
  LauncherResult<bool> isAdminWithoutUac = IsAdminWithoutUac();
  if (isAdminWithoutUac.isOk()) {
    Telemetry::ScalarSet(
        Telemetry::ScalarID::OS_ENVIRONMENT_IS_ADMIN_WITHOUT_UAC,
        isAdminWithoutUac.unwrap());
  }
#endif /* XP_WIN */

#if defined(MOZ_SANDBOX)
  AddSandboxAnnotations();
#endif /* MOZ_SANDBOX */

  mProfileSvc->CompleteStartup();

  {
    rv = appStartup->Run();
    if (NS_FAILED(rv)) {
      NS_ERROR("failed to run appstartup");
      gLogConsoleErrors = true;
    }
  }

  return rv;
}

/*
 * XRE_main - A class based main entry point used by most platforms.
 *            Note that on OSX, aAppData->xreDirectory will point to
 *            .app/Contents/Resources.
 */
int XREMain::XRE_main(int argc, char* argv[], const BootstrapConfig& aConfig) {
  gArgc = argc;
  gArgv = argv;

  ScopedLogging log;

  mozilla::LogModule::Init(gArgc, gArgv);

#ifdef MOZ_CODE_COVERAGE
  CodeCoverageHandler::Init();
#endif

  AUTO_BASE_PROFILER_LABEL("XREMain::XRE_main (around Gecko Profiler)", OTHER);
  AUTO_PROFILER_INIT;
  AUTO_PROFILER_LABEL("XREMain::XRE_main", OTHER);

  nsresult rv = NS_OK;

  if (aConfig.appData) {
    mAppData = MakeUnique<XREAppData>(*aConfig.appData);
  } else {
    MOZ_RELEASE_ASSERT(aConfig.appDataPath);
    nsCOMPtr<nsIFile> appini;
    rv = XRE_GetFileFromPath(aConfig.appDataPath, getter_AddRefs(appini));
    if (NS_FAILED(rv)) {
      Output(true, "Error: unrecognized path: %s\n", aConfig.appDataPath);
      return 1;
    }

    mAppData = MakeUnique<XREAppData>();
    rv = XRE_ParseAppData(appini, *mAppData);
    if (NS_FAILED(rv)) {
      Output(true, "Couldn't read application.ini");
      return 1;
    }

    appini->GetParent(getter_AddRefs(mAppData->directory));
  }

  if (!mAppData->remotingName) {
    mAppData->remotingName = mAppData->name;
  }
  // used throughout this file
  gAppData = mAppData.get();

  nsCOMPtr<nsIFile> binFile;
  rv = XRE_GetBinaryPath(getter_AddRefs(binFile));
  NS_ENSURE_SUCCESS(rv, 1);

  rv = binFile->GetPath(gAbsoluteArgv0Path);
  NS_ENSURE_SUCCESS(rv, 1);

  if (!mAppData->xreDirectory) {
    nsCOMPtr<nsIFile> greDir;
    rv = binFile->GetParent(getter_AddRefs(greDir));
    if (NS_FAILED(rv)) return 2;

#ifdef XP_MACOSX
    nsCOMPtr<nsIFile> parent;
    greDir->GetParent(getter_AddRefs(parent));
    greDir = parent.forget();
    greDir->AppendNative(NS_LITERAL_CSTRING("Resources"));
#endif

    mAppData->xreDirectory = greDir;
  }

  if (aConfig.appData && aConfig.appDataPath) {
    mAppData->xreDirectory->Clone(getter_AddRefs(mAppData->directory));
    mAppData->directory->AppendNative(nsDependentCString(aConfig.appDataPath));
  }

  if (!mAppData->directory) {
    mAppData->directory = mAppData->xreDirectory;
  }

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  mAppData->sandboxBrokerServices = aConfig.sandboxBrokerServices;
  mAppData->sandboxPermissionsService = aConfig.sandboxPermissionsService;
#endif

  mozilla::IOInterposerInit ioInterposerGuard;

#if defined(XP_WIN)
  // Some COM settings are global to the process and must be set before any non-
  // trivial COM is run in the application. Since these settings may affect
  // stability, we should instantiate COM ASAP so that we can ensure that these
  // global settings are configured before anything can interfere.
  mozilla::mscom::ProcessRuntime msCOMRuntime;
#endif

  // init
  bool exit = false;
  int result = XRE_mainInit(&exit);
  if (result != 0 || exit) return result;

  // If we exit gracefully, remove the startup crash canary file.
  auto cleanup = MakeScopeExit([&]() -> nsresult {
    if (mProfLD) {
      nsCOMPtr<nsIFile> crashFile;
      MOZ_TRY_VAR(crashFile, GetIncompleteStartupFile(mProfLD));
      crashFile->Remove(false);
    }
    return NS_OK;
  });

  // startup
  result = XRE_mainStartup(&exit);
  if (result != 0 || exit) return result;

  // Start the real application. We use |aInitJSContext = false| because
  // XRE_mainRun wants to initialize the JSContext after reading user prefs.

  mScopedXPCOM = MakeUnique<ScopedXPCOMStartup>();
  if (!mScopedXPCOM) return 1;

  rv = mScopedXPCOM->Initialize(/* aInitJSContext = */ false);
  NS_ENSURE_SUCCESS(rv, 1);

  // run!
  rv = XRE_mainRun();

#ifdef MOZ_INSTRUMENT_EVENT_LOOP
  mozilla::ShutdownEventTracing();
#endif

  gAbsoluteArgv0Path.Truncate();

#if defined(MOZ_HAS_REMOTE)
  // Shut down the remote service. We must do this before calling LaunchChild
  // if we're restarting because otherwise the new instance will attempt to
  // remote to this instance.
  if (mRemoteService && !mDisableRemoteServer) {
    mRemoteService->ShutdownServer();
  }
#endif /* MOZ_WIDGET_GTK */

  mScopedXPCOM = nullptr;

#if defined(XP_WIN)
  mozilla::widget::StopAudioSession();
#endif

  // unlock the profile after ScopedXPCOMStartup object (xpcom)
  // has gone out of scope.  see bug #386739 for more details
  mProfileLock->Unlock();
  gProfileLock = nullptr;

  mozilla::AppShutdown::MaybeDoRestart();

#ifdef MOZ_WIDGET_GTK
  // gdk_display_close also calls gdk_display_manager_set_default_display
  // appropriately when necessary.
  if (!gfxPlatform::IsHeadless()) {
    MOZ_gdk_display_close(mGdkDisplay);
  }
#endif

  if (mAppData->flags & NS_XRE_ENABLE_CRASH_REPORTER)
    CrashReporter::UnsetExceptionHandler();

  XRE_DeinitCommandLine();

  return NS_FAILED(rv) ? 1 : 0;
}

void XRE_StopLateWriteChecks(void) { mozilla::StopLateWriteChecks(); }

int XRE_main(int argc, char* argv[], const BootstrapConfig& aConfig) {
  XREMain main;

  int result = main.XRE_main(argc, argv, aConfig);
  mozilla::RecordShutdownEndTimeStamp();
  return result;
}

nsresult XRE_InitCommandLine(int aArgc, char* aArgv[]) {
  nsresult rv = NS_OK;

#if defined(OS_WIN)
  CommandLine::Init(aArgc, aArgv);
#else

  // these leak on error, but that's OK: we'll just exit()
  char** canonArgs = new char*[aArgc];

  // get the canonical version of the binary's path
  nsCOMPtr<nsIFile> binFile;
  rv = XRE_GetBinaryPath(getter_AddRefs(binFile));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  nsAutoCString canonBinPath;
  rv = binFile->GetNativePath(canonBinPath);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  canonArgs[0] = strdup(canonBinPath.get());

  for (int i = 1; i < aArgc; ++i) {
    if (aArgv[i]) {
      canonArgs[i] = strdup(aArgv[i]);
    }
  }

  NS_ASSERTION(!CommandLine::IsInitialized(), "Bad news!");
  CommandLine::Init(aArgc, canonArgs);

  for (int i = 0; i < aArgc; ++i) free(canonArgs[i]);
  delete[] canonArgs;
#endif

#if defined(MOZ_WIDGET_ANDROID)
  const char* path = nullptr;
  ArgResult ar = CheckArg("greomni", &path);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR,
               "Error: argument --greomni requires a path argument\n");
    return NS_ERROR_FAILURE;
  }

  if (!path) return rv;

  nsCOMPtr<nsIFile> greOmni;
  rv = XRE_GetFileFromPath(path, getter_AddRefs(greOmni));
  if (NS_FAILED(rv)) {
    PR_fprintf(PR_STDERR, "Error: argument --greomni requires a valid path\n");
    return rv;
  }

  ar = CheckArg("appomni", &path);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR,
               "Error: argument --appomni requires a path argument\n");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIFile> appOmni;
  if (path) {
    rv = XRE_GetFileFromPath(path, getter_AddRefs(appOmni));
    if (NS_FAILED(rv)) {
      PR_fprintf(PR_STDERR,
                 "Error: argument --appomni requires a valid path\n");
      return rv;
    }
  }

  mozilla::Omnijar::Init(greOmni, appOmni);
#endif

  return rv;
}

nsresult XRE_DeinitCommandLine() {
  nsresult rv = NS_OK;

  CommandLine::Terminate();

  return rv;
}

GeckoProcessType XRE_GetProcessType() {
  return mozilla::startup::sChildProcessType;
}

const char* XRE_GetProcessTypeString() {
  return XRE_GeckoProcessTypeToString(XRE_GetProcessType());
}

bool XRE_IsE10sParentProcess() {
#ifdef MOZ_WIDGET_ANDROID
  return XRE_IsParentProcess() && BrowserTabsRemoteAutostart() &&
         mozilla::jni::IsAvailable();
#else
  return XRE_IsParentProcess() && BrowserTabsRemoteAutostart();
#endif
}

#define GECKO_PROCESS_TYPE(enum_name, string_name, xre_name, bin_type) \
  bool XRE_Is##xre_name##Process() {                                   \
    return XRE_GetProcessType() == GeckoProcessType_##enum_name;       \
  }
#include "mozilla/GeckoProcessTypes.h"
#undef GECKO_PROCESS_TYPE

bool XRE_UseNativeEventProcessing() {
#if defined(XP_MACOSX) || defined(XP_WIN)
  if (XRE_IsRDDProcess() || XRE_IsSocketProcess()) {
    return false;
  }
#endif
  if (XRE_IsContentProcess()) {
    return StaticPrefs::dom_ipc_useNativeEventProcessing_content();
  }

  return true;
}

#if defined(XP_WIN)
bool XRE_Win32kCallsAllowed() {
  switch (XRE_GetProcessType()) {
    case GeckoProcessType_GMPlugin:
    case GeckoProcessType_RDD:
      return false;
    default:
      return true;
  }
}
#endif

// If you add anything to this enum, please update about:support to reflect it
enum {
  // kE10sEnabledByUser = 0, removed when ending non-e10s support
  kE10sEnabledByDefault = 1,
  kE10sDisabledByUser = 2,
  // kE10sDisabledInSafeMode = 3, was removed in bug 1172491.
  // kE10sDisabledForAccessibility = 4,
  // kE10sDisabledForMacGfx = 5, was removed in bug 1068674.
  // kE10sDisabledForBidi = 6, removed in bug 1309599
  // kE10sDisabledForAddons = 7, removed in bug 1406212
  kE10sForceDisabled = 8,
  // kE10sDisabledForXPAcceleration = 9, removed in bug 1296353
  // kE10sDisabledForOperatingSystem = 10, removed due to xp-eol
};

namespace mozilla {

bool BrowserTabsRemoteAutostart() {
  if (gBrowserTabsRemoteAutostartInitialized) {
    return gBrowserTabsRemoteAutostart;
  }
  gBrowserTabsRemoteAutostartInitialized = true;

  // If we're in the content process, we are running E10S.
  if (XRE_IsContentProcess()) {
    gBrowserTabsRemoteAutostart = true;
    return gBrowserTabsRemoteAutostart;
  }

#if defined(MOZILLA_OFFICIAL) && MOZ_BUILD_APP_IS_BROWSER
  bool allowSingleProcessOutsideAutomation = false;
#else
  bool allowSingleProcessOutsideAutomation = true;
#endif

  int status = kE10sEnabledByDefault;
  // We use "are non-local connections disabled" as a proxy for
  // "are we running some kind of automated test". It would be nicer to use
  // xpc::IsInAutomation(), but that depends on some prefs being set, which
  // they are not in (at least) gtests (where we can't) and xpcshell.
  // Long-term, hopefully we can make tests switch to environment variables
  // to disable e10s and then we can get rid of this.
  if (allowSingleProcessOutsideAutomation ||
      xpc::AreNonLocalConnectionsDisabled()) {
    bool optInPref =
        Preferences::GetBool("browser.tabs.remote.autostart", true);

    if (optInPref) {
      gBrowserTabsRemoteAutostart = true;
    } else {
      status = kE10sDisabledByUser;
    }
  } else {
    gBrowserTabsRemoteAutostart = true;
  }

  // Uber override pref for emergency blocking
  if (gBrowserTabsRemoteAutostart && EnvHasValue("MOZ_FORCE_DISABLE_E10S")) {
    gBrowserTabsRemoteAutostart = false;
    status = kE10sForceDisabled;
  }

  gBrowserTabsRemoteStatus = status;

  return gBrowserTabsRemoteAutostart;
}

uint32_t GetMaxWebProcessCount() {
  // multiOptOut is in int to allow us to run multiple experiments without
  // introducing multiple prefs a la the autostart.N prefs.
  if (Preferences::GetInt("dom.ipc.multiOptOut", 0) >=
      nsIXULRuntime::E10S_MULTI_EXPERIMENT) {
    return 1;
  }

  const char* optInPref = "dom.ipc.processCount";
  uint32_t optInPrefValue = Preferences::GetInt(optInPref, 1);
  return std::max(1u, optInPrefValue);
}

const char* PlatformBuildID() { return gToolkitBuildID; }

}  // namespace mozilla

void SetupErrorHandling(const char* progname) {
#ifdef XP_WIN
  /* On Windows XPSP3 and Windows Vista if DEP is configured off-by-default
     we still want DEP protection: enable it explicitly and programmatically.

     This function is not available on WinXPSP2 so we dynamically load it.
  */

  HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
  SetProcessDEPPolicyFunc _SetProcessDEPPolicy =
      (SetProcessDEPPolicyFunc)GetProcAddress(kernel32, "SetProcessDEPPolicy");
  if (_SetProcessDEPPolicy) _SetProcessDEPPolicy(PROCESS_DEP_ENABLE);
#endif

#ifdef XP_WIN
  // Suppress the "DLL Foo could not be found" dialog, such that if dependent
  // libraries (such as GDI+) are not preset, we gracefully fail to load those
  // XPCOM components, instead of being ungraceful.
  UINT realMode = SetErrorMode(0);
  realMode |= SEM_FAILCRITICALERRORS;
  // If XRE_NO_WINDOWS_CRASH_DIALOG is set, suppress displaying the "This
  // application has crashed" dialog box.  This is mainly useful for
  // automated testing environments, e.g. tinderbox, where there's no need
  // for a dozen of the dialog boxes to litter the console
  if (getenv("XRE_NO_WINDOWS_CRASH_DIALOG"))
    realMode |= SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX;

  SetErrorMode(realMode);

#endif

  InstallSignalHandlers(progname);

  // Unbuffer stdout, needed for tinderbox tests.
  setbuf(stdout, 0);
}

// Note: This function should not be needed anymore. See Bug 818634 for details.
void OverrideDefaultLocaleIfNeeded() {
  // Read pref to decide whether to override default locale with US English.
  if (mozilla::Preferences::GetBool("javascript.use_us_english_locale",
                                    false)) {
    // Set the application-wide C-locale. Needed to resist fingerprinting
    // of Date.toLocaleFormat(). We use the locale to "C.UTF-8" if possible,
    // to avoid interfering with non-ASCII keyboard input on some Linux
    // desktops. Otherwise fall back to the "C" locale, which is available on
    // all platforms.
    setlocale(LC_ALL, "C.UTF-8") || setlocale(LC_ALL, "C");
  }
}

static bool gRunSelfAsContentProc = false;

void XRE_EnableSameExecutableForContentProc() {
  if (!PR_GetEnv("MOZ_SEPARATE_CHILD_PROCESS")) {
    gRunSelfAsContentProc = true;
  }
}

mozilla::BinPathType XRE_GetChildProcBinPathType(
    GeckoProcessType aProcessType) {
  MOZ_ASSERT(aProcessType != GeckoProcessType_Default);

  if (!gRunSelfAsContentProc) {
    return BinPathType::PluginContainer;
  }

  switch (aProcessType) {
#define GECKO_PROCESS_TYPE(enum_name, string_name, xre_name, bin_type) \
  case GeckoProcessType_##enum_name:                                   \
    return BinPathType::bin_type;
#include "mozilla/GeckoProcessTypes.h"
#undef GECKO_PROCESS_TYPE
    default:
      return BinPathType::PluginContainer;
  }
}

// Because rust doesn't handle weak symbols, this function wraps the weak
// malloc_handle_oom for it.
extern "C" void GeckoHandleOOM(size_t size) { mozalloc_handle_oom(size); }

// From toolkit/library/rust/shared/lib.rs
extern "C" void install_rust_panic_hook();
extern "C" void install_rust_oom_hook();

struct InstallRustHooks {
  InstallRustHooks() {
    install_rust_panic_hook();
    install_rust_oom_hook();
  }
};

InstallRustHooks sInstallRustHooks;

#ifdef MOZ_ASAN_REPORTER
void setASanReporterPath(nsIFile* aDir) {
  nsCOMPtr<nsIFile> dir;
  aDir->Clone(getter_AddRefs(dir));

  dir->Append(NS_LITERAL_STRING("asan"));
  nsresult rv = dir->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (NS_WARN_IF(NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS)) {
    MOZ_CRASH("[ASan Reporter] Unable to create crash directory.");
  }

  dir->Append(NS_LITERAL_STRING("ff_asan_log"));

#  ifdef XP_WIN
  nsAutoString nspathW;
  rv = dir->GetPath(nspathW);
  NS_ConvertUTF16toUTF8 nspath(nspathW);
#  else
  nsAutoCString nspath;
  rv = dir->GetNativePath(nspath);
#  endif
  if (NS_FAILED(rv)) {
    MOZ_CRASH("[ASan Reporter] Unable to get native path for crash directory.");
  }

  __sanitizer_set_report_path(nspath.get());
}
#endif
