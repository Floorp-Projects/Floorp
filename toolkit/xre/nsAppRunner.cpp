/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
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
#include "mozilla/intl/LocaleService.h"

#include "nsAppRunner.h"
#include "mozilla/XREAppData.h"
#include "mozilla/Bootstrap.h"
#if defined(MOZ_UPDATER) && !defined(MOZ_WIDGET_ANDROID)
#include "nsUpdateDriver.h"
#endif
#include "ProfileReset.h"

#ifdef MOZ_INSTRUMENT_EVENT_LOOP
#include "EventTracer.h"
#endif

#ifdef XP_MACOSX
#include "nsVersionComparator.h"
#include "MacLaunchHelper.h"
#include "MacApplicationDelegate.h"
#include "MacAutoreleasePool.h"
// these are needed for sysctl
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include "prnetdb.h"
#include "prprf.h"
#include "prproces.h"
#include "prenv.h"
#include "prtime.h"

#include "nsIAppShellService.h"
#include "nsIAppStartup.h"
#include "nsIAppStartupNotifier.h"
#include "nsIMutableArray.h"
#include "nsICategoryManager.h"
#include "nsIChromeRegistry.h"
#include "nsICommandLineRunner.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIConsoleService.h"
#include "nsIContentHandler.h"
#include "nsIDialogParamBlock.h"
#include "nsIDOMWindow.h"
#include "mozilla/ModuleUtils.h"
#include "nsIIOService.h"
#include "nsIObserverService.h"
#include "nsINativeAppSupport.h"
#include "nsIPlatformInfo.h"
#include "nsIProcess.h"
#include "nsIProfileUnlocker.h"
#include "nsIPromptService.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsISupportsPrimitives.h"
#include "nsIToolkitChromeRegistry.h"
#include "nsIToolkitProfile.h"
#include "nsIToolkitProfileService.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIWindowCreator.h"
#include "nsIWindowMediator.h"
#include "nsIWindowWatcher.h"
#include "nsIXULAppInfo.h"
#include "nsIXULRuntime.h"
#include "nsPIDOMWindow.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"
#include "nsIDocShell.h"
#include "nsAppShellCID.h"
#include "mozilla/scache/StartupCache.h"
#include "gfxPlatform.h"
#include "gfxPrefs.h"

#include "mozilla/Unused.h"

#ifdef XP_WIN
#include "nsIWinAppHelper.h"
#include <windows.h>
#include <intrin.h>
#include <math.h>
#include "cairo/cairo-features.h"
#include "mozilla/WindowsDllBlocklist.h"
#include "mozilla/mscom/MainThreadRuntime.h"
#include "mozilla/widget/AudioSession.h"

#ifndef PROCESS_DEP_ENABLE
#define PROCESS_DEP_ENABLE 0x1
#endif
#endif

#if defined(MOZ_CONTENT_SANDBOX)
#include "mozilla/SandboxSettings.h"
#if (defined(XP_WIN) || defined(XP_MACOSX))
#include "nsIUUIDGenerator.h"
#endif
#endif

#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#if defined(XP_WIN)
#include "mozilla/a11y/Compatibility.h"
#include "mozilla/a11y/Platform.h"
#endif
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
#include "nsToolkitCompsCID.h"

#include "nsINIParser.h"
#include "mozilla/Omnijar.h"
#include "mozilla/StartupTimeline.h"
#include "mozilla/LateWriteChecks.h"

#include <stdlib.h>
#include <locale.h>

#ifdef XP_UNIX
#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef XP_WIN
#include <process.h>
#include <shlobj.h>
#include "mozilla/WinDllServices.h"
#include "nsThreadUtils.h"
#include <comdef.h>
#include <wbemidl.h>
#include "WinUtils.h"
#endif

#ifdef XP_MACOSX
#include "nsILocalFileMac.h"
#include "nsCommandLineServiceMac.h"
#endif

// for X remote support
#ifdef MOZ_ENABLE_XREMOTE
#include "XRemoteClient.h"
#include "nsIRemoteService.h"
#include "nsProfileLock.h"
#include "SpecialSystemDirectory.h"
#include <sched.h>
#ifdef MOZ_ENABLE_DBUS
#include "DBusRemoteClient.h"
#endif
// Time to wait for the remoting service to start
#define MOZ_XREMOTE_START_TIMEOUT_SEC 5
#endif

#if defined(DEBUG) && defined(XP_WIN32)
#include <malloc.h>
#endif

#if defined (XP_MACOSX)
#include <Carbon/Carbon.h>
#endif

#ifdef DEBUG
#include "mozilla/Logging.h"
#endif

#ifdef MOZ_JPROF
#include "jprof.h"
#endif

#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#define NS_CRASHREPORTER_CONTRACTID "@mozilla.org/toolkit/crash-reporter;1"
#include "nsIPrefService.h"
#include "nsIMemoryInfoDumper.h"
#if defined(XP_LINUX) && !defined(ANDROID)
#include "mozilla/widget/LSBUtils.h"
#endif

#include "base/command_line.h"
#include "GTestRunner.h"

#ifdef MOZ_WIDGET_ANDROID
#include "GeneratedJNIWrappers.h"
#endif

#if defined(MOZ_SANDBOX)
#if defined(XP_LINUX) && !defined(ANDROID)
#include "mozilla/SandboxInfo.h"
#elif defined(XP_WIN)
#include "sandboxBroker.h"
#include "sandboxPermissions.h"
#endif
#endif

#ifdef MOZ_CODE_COVERAGE
#include "mozilla/CodeCoverageHandler.h"
#endif

#include "mozilla/mozalloc_oom.h"
#include "SafeMode.h"

extern uint32_t gRestartMode;
extern void InstallSignalHandlers(const char *ProgramName);

#define FILE_COMPATIBILITY_INFO NS_LITERAL_CSTRING("compatibility.ini")
#define FILE_INVALIDATE_CACHES NS_LITERAL_CSTRING(".purgecaches")
#define FILE_STARTUP_INCOMPLETE NS_LITERAL_STRING(".startup-incomplete")

int    gArgc;
char **gArgv;

#include "buildid.h"

static const char gToolkitVersion[] = NS_STRINGIFY(GRE_MILESTONE);
static const char gToolkitBuildID[] = NS_STRINGIFY(MOZ_BUILDID);

static nsIProfileLock* gProfileLock;

int    gRestartArgc;
char **gRestartArgv;

// If gRestartedByOS is set, we were automatically restarted by the OS.
bool gRestartedByOS = false;

bool gIsGtest = false;

nsString gAbsoluteArgv0Path;

#if defined(MOZ_WIDGET_GTK)
#include <glib.h>
#if defined(DEBUG) || defined(NS_BUILD_REFCNT_LOGGING)
#define CLEANUP_MEMORY 1
#define PANGO_ENABLE_BACKEND
#include <pango/pangofc-fontmap.h>
#endif
#include <gtk/gtk.h>
#ifdef MOZ_WAYLAND
#include <gdk/gdkwayland.h>
#endif
#ifdef MOZ_X11
#include <gdk/gdkx.h>
#endif /* MOZ_X11 */
#include "nsGTKToolkit.h"
#include <fontconfig/fontconfig.h>
#endif
#include "BinaryPath.h"

#ifdef MOZ_LINKER
extern "C" MFBT_API bool IsSignalHandlingBroken();
#endif

#ifdef FUZZING
#include "FuzzerRunner.h"

namespace mozilla {
FuzzerRunner* fuzzerRunner = 0;
} // namespace mozilla

#ifdef LIBFUZZER
void XRE_LibFuzzerSetDriver(LibFuzzerDriver aDriver) {
  mozilla::fuzzerRunner->setParams(aDriver);
}
#endif
#endif // FUZZING

namespace mozilla {
int (*RunGTest)(int*, char**) = 0;
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::startup;
using mozilla::Unused;
using mozilla::scache::StartupCache;
using mozilla::dom::ContentParent;
using mozilla::dom::ContentChild;
using mozilla::intl::LocaleService;

// Save the given word to the specified environment variable.
static void
SaveWordToEnv(const char *name, const nsACString & word)
{
  char *expr = Smprintf("%s=%s", name, PromiseFlatCString(word).get()).release();
  if (expr)
    PR_SetEnv(expr);
  // We intentionally leak |expr| here since it is required by PR_SetEnv.
}

// Save the path of the given file to the specified environment variable.
static void
SaveFileToEnv(const char *name, nsIFile *file)
{
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

// Load the path of a file saved with SaveFileToEnv
static already_AddRefed<nsIFile>
GetFileFromEnv(const char *name)
{
  nsresult rv;
  nsCOMPtr<nsIFile> file;

#ifdef XP_WIN
  WCHAR path[_MAX_PATH];
  if (!GetEnvironmentVariableW(NS_ConvertASCIItoUTF16(name).get(),
                               path, _MAX_PATH))
    return nullptr;

  rv = NS_NewLocalFile(nsDependentString(path), true, getter_AddRefs(file));
  if (NS_FAILED(rv))
    return nullptr;

  return file.forget();
#else
  const char *arg = PR_GetEnv(name);
  if (!arg || !*arg)
    return nullptr;

  rv = NS_NewNativeLocalFile(nsDependentCString(arg), true,
                             getter_AddRefs(file));
  if (NS_FAILED(rv))
    return nullptr;

  return file.forget();
#endif
}

// Save the path of the given word to the specified environment variable
// provided the environment variable does not have a value.
static void
SaveWordToEnvIfUnset(const char *name, const nsACString & word)
{
  if (!EnvHasValue(name))
    SaveWordToEnv(name, word);
}

// Save the path of the given file to the specified environment variable
// provided the environment variable does not have a value.
static void
SaveFileToEnvIfUnset(const char *name, nsIFile *file)
{
  if (!EnvHasValue(name))
    SaveFileToEnv(name, file);
}

static bool gIsExpectedExit = false;

void MozExpectedExit() {
  gIsExpectedExit = true;
}

/**
 * Runs atexit() to catch unexpected exit from 3rd party libraries like the
 * Intel graphics driver calling exit in an error condition. When they
 * call exit() to report an error we won't shutdown correctly and wont catch
 * the issue with our crash reporter.
 */
static void UnexpectedExit() {
  if (!gIsExpectedExit) {
    gIsExpectedExit = true; // Don't risk re-entrency issues when crashing.
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
static MOZ_FORMAT_PRINTF(2, 3) void Output(bool isError, const char *fmt, ... )
{
  va_list ap;
  va_start(ap, fmt);

#if defined(XP_WIN) && !MOZ_WINCONSOLE
  SmprintfPointer msg = mozilla::Vsmprintf(fmt, ap);
  if (msg)
  {
    UINT flags = MB_OK;
    if (isError)
      flags |= MB_ICONERROR;
    else
      flags |= MB_ICONINFORMATION;

    wchar_t wide_msg[1024];
    MultiByteToWideChar(CP_ACP,
                        0,
                        msg.get(),
                        -1,
                        wide_msg,
                        sizeof(wide_msg) / sizeof(wchar_t));

    MessageBoxW(nullptr, wide_msg, L"XULRunner", flags);
  }
#else
  vfprintf(stderr, fmt, ap);
#endif

  va_end(ap);
}

enum RemoteResult {
  REMOTE_NOT_FOUND  = 0,
  REMOTE_FOUND      = 1,
  REMOTE_ARG_BAD    = 2
};

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
static ArgResult
CheckArg(const char* aArg, const char** aParam = nullptr,
         CheckArgFlag aFlags = CheckArgFlag::RemoveArg)
{
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
static ArgResult
CheckArgExists(const char* aArg)
{
  return CheckArg(aArg, nullptr, CheckArgFlag::None);
}

#if defined(XP_WIN)
/**
 * Check for a commandline flag from the windows shell and remove it from the
 * argv used when restarting. Flags MUST be in the form -arg.
 *
 * @param aArg the parameter to check. Must be lowercase.
 */
static ArgResult
CheckArgShell(const char* aArg)
{
  char **curarg = gRestartArgv + 1; // skip argv[0]

  while (*curarg) {
    char *arg = curarg[0];

    if (arg[0] == '-') {
      ++arg;

      if (strimatch(aArg, arg)) {
        do {
          *curarg = *(curarg + 1);
          ++curarg;
        } while (*curarg);

        --gRestartArgc;

        return ARG_FOUND;
      }
    }

    ++curarg;
  }

  return ARG_NONE;
}

/**
 * Enabled Native App Support to process DDE messages when the app needs to
 * restart and the app has been launched by the Windows shell to open an url.
 * When aWait is false this will process the DDE events manually. This prevents
 * Windows from displaying an error message due to the DDE message not being
 * acknowledged.
 */
static void
ProcessDDE(nsINativeAppSupport* aNative, bool aWait)
{
  // When the app is launched by the windows shell the windows shell
  // expects the app to be available for DDE messages and if it isn't
  // windows displays an error dialog. To prevent the error the DDE server
  // is enabled and pending events are processed when the app needs to
  // restart after it was launched by the shell with the requestpending
  // argument. The requestpending pending argument is removed to
  // differentiate it from being launched when an app restart is not
  // required.
  ArgResult ar;
  ar = CheckArgShell("requestpending");
  if (ar == ARG_FOUND) {
    aNative->Enable(); // enable win32 DDE responses
    if (aWait) {
      // This is just a guesstimate based on testing different values.
      // If count is 8 or less windows will display an error dialog.
      int32_t count = 20;
      SpinEventLoopUntil([&]() { return --count < 0; });
    }
  }
}
#endif

/**
 * Determines if there is support for showing the profile manager
 *
 * @return true in all environments
*/
static bool
CanShowProfileManager()
{
  return true;
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
  constexpr nsXULAppInfo() {}
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
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIXULAppInfo, gAppData ||
                                     XRE_IsContentProcess())
NS_INTERFACE_MAP_END

NS_IMETHODIMP_(MozExternalRefCountType)
nsXULAppInfo::AddRef()
{
  return 1;
}

NS_IMETHODIMP_(MozExternalRefCountType)
nsXULAppInfo::Release()
{
  return 1;
}

NS_IMETHODIMP
nsXULAppInfo::GetVendor(nsACString& aResult)
{
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    aResult = cc->GetAppInfo().vendor;
    return NS_OK;
  }
  aResult.Assign(gAppData->vendor);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetName(nsACString& aResult)
{
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    aResult = cc->GetAppInfo().name;
    return NS_OK;
  }
  aResult.Assign(gAppData->name);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetID(nsACString& aResult)
{
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    aResult = cc->GetAppInfo().ID;
    return NS_OK;
  }
  aResult.Assign(gAppData->ID);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetVersion(nsACString& aResult)
{
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    aResult = cc->GetAppInfo().version;
    return NS_OK;
  }
  aResult.Assign(gAppData->version);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetPlatformVersion(nsACString& aResult)
{
  aResult.Assign(gToolkitVersion);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetAppBuildID(nsACString& aResult)
{
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    aResult = cc->GetAppInfo().buildID;
    return NS_OK;
  }
  aResult.Assign(gAppData->buildID);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetPlatformBuildID(nsACString& aResult)
{
  aResult.Assign(gToolkitBuildID);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetUAName(nsACString& aResult)
{
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    aResult = cc->GetAppInfo().UAName;
    return NS_OK;
  }
  aResult.Assign(gAppData->UAName);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetSourceURL(nsACString& aResult)
{
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    aResult = cc->GetAppInfo().sourceURL;
    return NS_OK;
  }
  aResult.Assign(gAppData->sourceURL);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetLogConsoleErrors(bool *aResult)
{
  *aResult = gLogConsoleErrors;
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::SetLogConsoleErrors(bool aValue)
{
  gLogConsoleErrors = aValue;
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetInSafeMode(bool *aResult)
{
  *aResult = gSafeMode;
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetOS(nsACString& aResult)
{
  aResult.AssignLiteral(OS_TARGET);
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetXPCOMABI(nsACString& aResult)
{
#ifdef TARGET_XPCOM_ABI
  aResult.AssignLiteral(TARGET_XPCOM_ABI);
  return NS_OK;
#else
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

NS_IMETHODIMP
nsXULAppInfo::GetWidgetToolkit(nsACString& aResult)
{
  aResult.AssignLiteral(MOZ_WIDGET_TOOLKIT);
  return NS_OK;
}

// Ensure that the GeckoProcessType enum, defined in xpcom/build/nsXULAppAPI.h,
// is synchronized with the const unsigned longs defined in
// xpcom/system/nsIXULRuntime.idl.
#define SYNC_ENUMS(a,b) \
  static_assert(nsIXULRuntime::PROCESS_TYPE_ ## a == \
                static_cast<int>(GeckoProcessType_ ## b), \
                "GeckoProcessType in nsXULAppAPI.h not synchronized with nsIXULRuntime.idl");

SYNC_ENUMS(DEFAULT, Default)
SYNC_ENUMS(PLUGIN, Plugin)
SYNC_ENUMS(CONTENT, Content)
SYNC_ENUMS(IPDLUNITTEST, IPDLUnitTest)
SYNC_ENUMS(GMPLUGIN, GMPlugin)
SYNC_ENUMS(GPU, GPU)
SYNC_ENUMS(PDFIUM, PDFium)

// .. and ensure that that is all of them:
static_assert(GeckoProcessType_PDFium + 1 == GeckoProcessType_End,
              "Did not find the final GeckoProcessType");

NS_IMETHODIMP
nsXULAppInfo::GetProcessType(uint32_t* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = XRE_GetProcessType();
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetProcessID(uint32_t* aResult)
{
#ifdef XP_WIN
  *aResult = GetCurrentProcessId();
#else
  *aResult = getpid();
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetUniqueProcessID(uint64_t* aResult)
{
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    *aResult = cc->GetID();
  } else {
    *aResult = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetRemoteType(nsAString& aRemoteType)
{
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    aRemoteType.Assign(cc->GetRemoteType());
  } else {
    SetDOMStringToNull(aRemoteType);
  }

  return NS_OK;
}

static bool gBrowserTabsRemoteAutostart = false;
static uint64_t gBrowserTabsRemoteStatus = 0;
static bool gBrowserTabsRemoteAutostartInitialized = false;

NS_IMETHODIMP
nsXULAppInfo::Observe(nsISupports *aSubject, const char *aTopic, const char16_t *aData) {
  if (!nsCRT::strcmp(aTopic, "getE10SBlocked")) {
    nsCOMPtr<nsISupportsPRUint64> ret = do_QueryInterface(aSubject);
    if (!ret)
      return NS_ERROR_FAILURE;

    ret->SetData(gBrowserTabsRemoteStatus);

    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULAppInfo::GetBrowserTabsRemoteAutostart(bool* aResult)
{
  *aResult = BrowserTabsRemoteAutostart();
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetMaxWebProcessCount(uint32_t* aResult)
{
  *aResult = mozilla::GetMaxWebProcessCount();
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetAccessibilityEnabled(bool* aResult)
{
#ifdef ACCESSIBILITY
  *aResult = GetAccService() != nullptr;
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetAccessibleHandlerUsed(bool* aResult)
{
#if defined(ACCESSIBILITY) && defined(XP_WIN)
  *aResult = Preferences::GetBool("accessibility.handler.enabled", false) &&
    a11y::IsHandlerRegistered();
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetAccessibilityInstantiator(nsAString &aInstantiator)
{
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
nsXULAppInfo::GetShouldBlockIncompatJaws(bool* aResult)
{
  *aResult = false;
#if defined(ACCESSIBILITY) && defined(XP_WIN)
  *aResult = mozilla::a11y::Compatibility::IsOldJAWS();
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetIs64Bit(bool* aResult)
{
#ifdef HAVE_64BIT_BUILD
  *aResult = true;
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::EnsureContentProcess()
{
  if (!XRE_IsParentProcess())
    return NS_ERROR_NOT_AVAILABLE;

  RefPtr<ContentParent> unused = ContentParent::GetNewOrUsedBrowserProcess(
    NS_LITERAL_STRING(DEFAULT_REMOTE_TYPE));
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::InvalidateCachesOnRestart()
{
  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DIR_STARTUP,
                                       getter_AddRefs(file));
  if (NS_FAILED(rv))
    return rv;
  if (!file)
    return NS_ERROR_NOT_AVAILABLE;

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
    PRFileDesc *fd;
    rv = file->OpenNSPRFileDesc(PR_RDWR | PR_APPEND, 0600, &fd);
    if (NS_FAILED(rv)) {
      NS_ERROR("could not create output stream");
      return NS_ERROR_NOT_AVAILABLE;
    }
    static const char kInvalidationHeader[] = NS_LINEBREAK "InvalidateCaches=1" NS_LINEBREAK;
    PR_Write(fd, kInvalidationHeader, sizeof(kInvalidationHeader) - 1);
    PR_Close(fd);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetReplacedLockTime(PRTime *aReplacedLockTime)
{
  if (!gProfileLock)
    return NS_ERROR_NOT_AVAILABLE;
  gProfileLock->GetReplacedLockTime(aReplacedLockTime);
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetIsReleaseOrBeta(bool* aResult)
{
#ifdef RELEASE_OR_BETA
  *aResult = true;
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetIsOfficialBranding(bool* aResult)
{
#ifdef MOZ_OFFICIAL_BRANDING
  *aResult = true;
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetDefaultUpdateChannel(nsACString& aResult)
{
  aResult.AssignLiteral(NS_STRINGIFY(MOZ_UPDATE_CHANNEL));
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetDistributionID(nsACString& aResult)
{
  aResult.AssignLiteral(MOZ_DISTRIBUTION_ID);
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetWindowsDLLBlocklistStatus(bool* aResult)
{
#if defined(HAS_DLL_BLOCKLIST)
  *aResult = DllBlocklist_CheckStatus();
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetRestartedByOS(bool* aResult)
{
  *aResult = gRestartedByOS;
  return NS_OK;
}

#ifdef XP_WIN
// Matches the enum in WinNT.h for the Vista SDK but renamed so that we can
// safely build with the Vista SDK and without it.
typedef enum
{
  VistaTokenElevationTypeDefault = 1,
  VistaTokenElevationTypeFull,
  VistaTokenElevationTypeLimited
} VISTA_TOKEN_ELEVATION_TYPE;

// avoid collision with TokeElevationType enum in WinNT.h
// of the Vista SDK
#define VistaTokenElevationType static_cast< TOKEN_INFORMATION_CLASS >( 18 )

NS_IMETHODIMP
nsXULAppInfo::GetUserCanElevate(bool *aUserCanElevate)
{
  HANDLE hToken;

  VISTA_TOKEN_ELEVATION_TYPE elevationType;
  DWORD dwSize;

  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken) ||
      !GetTokenInformation(hToken, VistaTokenElevationType, &elevationType,
                           sizeof(elevationType), &dwSize)) {
    *aUserCanElevate = false;
  }
  else {
    // The possible values returned for elevationType and their meanings are:
    //   TokenElevationTypeDefault: The token does not have a linked token
    //     (e.g. UAC disabled or a standard user, so they can't be elevated)
    //   TokenElevationTypeFull: The token is linked to an elevated token
    //     (e.g. UAC is enabled and the user is already elevated so they can't
    //      be elevated again)
    //   TokenElevationTypeLimited: The token is linked to a limited token
    //     (e.g. UAC is enabled and the user is not elevated, so they can be
    //      elevated)
    *aUserCanElevate = (elevationType == VistaTokenElevationTypeLimited);
  }

  if (hToken)
    CloseHandle(hToken);

  return NS_OK;
}
#endif

NS_IMETHODIMP
nsXULAppInfo::GetEnabled(bool *aEnabled)
{
  *aEnabled = CrashReporter::GetEnabled();
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::SetEnabled(bool aEnabled)
{
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

    nsCOMPtr<nsIFile> xreBinDirectory = do_QueryInterface(greBinDir);
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
nsXULAppInfo::GetServerURL(nsIURL** aServerURL)
{
  NS_ENSURE_ARG_POINTER(aServerURL);
  if (!CrashReporter::GetEnabled())
    return NS_ERROR_NOT_INITIALIZED;

  nsAutoCString data;
  if (!CrashReporter::GetServerURL(data)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), data);
  if (!uri)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIURL> url;
  url = do_QueryInterface(uri);
  NS_ADDREF(*aServerURL = url);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::SetServerURL(nsIURL* aServerURL)
{
  bool schemeOk;
  // only allow https or http URLs
  nsresult rv = aServerURL->SchemeIs("https", &schemeOk);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!schemeOk) {
    rv = aServerURL->SchemeIs("http", &schemeOk);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!schemeOk)
      return NS_ERROR_INVALID_ARG;
  }
  nsAutoCString spec;
  rv = aServerURL->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  return CrashReporter::SetServerURL(spec);
}

NS_IMETHODIMP
nsXULAppInfo::GetMinidumpPath(nsIFile** aMinidumpPath)
{
  if (!CrashReporter::GetEnabled())
    return NS_ERROR_NOT_INITIALIZED;

  nsAutoString path;
  if (!CrashReporter::GetMinidumpPath(path))
    return NS_ERROR_FAILURE;

  nsresult rv = NS_NewLocalFile(path, false, aMinidumpPath);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::SetMinidumpPath(nsIFile* aMinidumpPath)
{
  nsAutoString path;
  nsresult rv = aMinidumpPath->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);
  return CrashReporter::SetMinidumpPath(path);
}

NS_IMETHODIMP
nsXULAppInfo::GetMinidumpForID(const nsAString& aId, nsIFile** aMinidump)
{
  if (!CrashReporter::GetMinidumpForID(aId, aMinidump)) {
    return NS_ERROR_FILE_NOT_FOUND;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetExtraFileForID(const nsAString& aId, nsIFile** aExtraFile)
{
  if (!CrashReporter::GetExtraFileForID(aId, aExtraFile)) {
    return NS_ERROR_FILE_NOT_FOUND;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::AnnotateCrashReport(const nsACString& key,
                                  const nsACString& data)
{
  return CrashReporter::AnnotateCrashReport(key, data);
}

NS_IMETHODIMP
nsXULAppInfo::AppendAppNotesToCrashReport(const nsACString& data)
{
  return CrashReporter::AppendAppNotesToCrashReport(data);
}

NS_IMETHODIMP
nsXULAppInfo::RegisterAppMemory(uint64_t pointer,
                                uint64_t len)
{
  return CrashReporter::RegisterAppMemory((void *)pointer, len);
}

NS_IMETHODIMP
nsXULAppInfo::WriteMinidumpForException(void* aExceptionInfo)
{
#ifdef XP_WIN32
  return CrashReporter::WriteMinidumpForException(static_cast<EXCEPTION_POINTERS*>(aExceptionInfo));
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsXULAppInfo::AppendObjCExceptionInfoToAppNotes(void* aException)
{
#ifdef XP_MACOSX
  return CrashReporter::AppendObjCExceptionInfoToAppNotes(aException);
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsXULAppInfo::GetSubmitReports(bool* aEnabled)
{
  return CrashReporter::GetSubmitReports(aEnabled);
}

NS_IMETHODIMP
nsXULAppInfo::SetSubmitReports(bool aEnabled)
{
  return CrashReporter::SetSubmitReports(aEnabled);
}

NS_IMETHODIMP
nsXULAppInfo::UpdateCrashEventsDir()
{
  CrashReporter::UpdateCrashEventsDir();
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::SaveMemoryReport()
{
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

  rv = dumper->DumpMemoryReportsToNamedFile(path, this, file, true /* anonymize */);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::SetTelemetrySessionId(const nsACString& id)
{
  CrashReporter::SetTelemetrySessionId(id);
  return NS_OK;
}

// This method is from nsIFInishDumpingCallback.
NS_IMETHODIMP
nsXULAppInfo::Callback(nsISupports* aData)
{
  nsCOMPtr<nsIFile> file = do_QueryInterface(aData);
  MOZ_ASSERT(file);

  CrashReporter::SetMemoryReportFile(file);
  return NS_OK;
}

static const nsXULAppInfo kAppInfo;
static nsresult AppInfoConstructor(nsISupports* aOuter,
                                   REFNSIID aIID, void **aResult)
{
  NS_ENSURE_NO_AGGREGATION(aOuter);

  return const_cast<nsXULAppInfo*>(&kAppInfo)->
    QueryInterface(aIID, aResult);
}

bool gLogConsoleErrors = false;

#define NS_ENSURE_TRUE_LOG(x, ret)               \
  PR_BEGIN_MACRO                                 \
  if (MOZ_UNLIKELY(!(x))) {                      \
    NS_WARNING("NS_ENSURE_TRUE(" #x ") failed"); \
    gLogConsoleErrors = true;                    \
    return ret;                                  \
  }                                              \
  PR_END_MACRO

#define NS_ENSURE_SUCCESS_LOG(res, ret)          \
  NS_ENSURE_TRUE_LOG(NS_SUCCEEDED(res), ret)

/**
 * Because we're starting/stopping XPCOM several times in different scenarios,
 * this class is a stack-based critter that makes sure that XPCOM is shut down
 * during early returns.
 */

class ScopedXPCOMStartup
{
public:
  ScopedXPCOMStartup() :
    mServiceManager(nullptr) { }
  ~ScopedXPCOMStartup();

  nsresult Initialize();
  nsresult SetWindowCreator(nsINativeAppSupport* native);

  static nsresult CreateAppSupport(nsISupports* aOuter, REFNSIID aIID, void** aResult);

private:
  nsIServiceManager* mServiceManager;
  static nsINativeAppSupport* gNativeAppSupport;
};

ScopedXPCOMStartup::~ScopedXPCOMStartup()
{
  NS_IF_RELEASE(gNativeAppSupport);

  if (mServiceManager) {
#ifdef XP_MACOSX
    // On OS X, we need a pool to catch cocoa objects that are autoreleased
    // during teardown.
    mozilla::MacAutoreleasePool pool;
#endif

    nsCOMPtr<nsIAppStartup> appStartup (do_GetService(NS_APPSTARTUP_CONTRACTID));
    if (appStartup)
      appStartup->DestroyHiddenWindow();

    gDirServiceProvider->DoShutdown();
    PROFILER_ADD_MARKER("Shutdown early");

    WriteConsoleLog();

    NS_ShutdownXPCOM(mServiceManager);
    mServiceManager = nullptr;
  }
}

// {95d89e3e-a169-41a3-8e56-719978e15b12}
#define APPINFO_CID \
  { 0x95d89e3e, 0xa169, 0x41a3, { 0x8e, 0x56, 0x71, 0x99, 0x78, 0xe1, 0x5b, 0x12 } }

// {0C4A446C-EE82-41f2-8D04-D366D2C7A7D4}
static const nsCID kNativeAppSupportCID =
  { 0xc4a446c, 0xee82, 0x41f2, { 0x8d, 0x4, 0xd3, 0x66, 0xd2, 0xc7, 0xa7, 0xd4 } };

// {5F5E59CE-27BC-47eb-9D1F-B09CA9049836}
static const nsCID kProfileServiceCID =
  { 0x5f5e59ce, 0x27bc, 0x47eb, { 0x9d, 0x1f, 0xb0, 0x9c, 0xa9, 0x4, 0x98, 0x36 } };

static already_AddRefed<nsIFactory>
ProfileServiceFactoryConstructor(const mozilla::Module& module, const mozilla::Module::CIDEntry& entry)
{
  nsCOMPtr<nsIFactory> factory;
  NS_NewToolkitProfileFactory(getter_AddRefs(factory));
  return factory.forget();
}

NS_DEFINE_NAMED_CID(APPINFO_CID);

static const mozilla::Module::CIDEntry kXRECIDs[] = {
  { &kAPPINFO_CID, false, nullptr, AppInfoConstructor },
  { &kProfileServiceCID, false, ProfileServiceFactoryConstructor, nullptr },
  { &kNativeAppSupportCID, false, nullptr, ScopedXPCOMStartup::CreateAppSupport },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kXREContracts[] = {
  { XULAPPINFO_SERVICE_CONTRACTID, &kAPPINFO_CID },
  { XULRUNTIME_SERVICE_CONTRACTID, &kAPPINFO_CID },
#ifdef MOZ_CRASHREPORTER
  { NS_CRASHREPORTER_CONTRACTID, &kAPPINFO_CID },
#endif // MOZ_CRASHREPORTER
  { NS_PROFILESERVICE_CONTRACTID, &kProfileServiceCID },
  { NS_NATIVEAPPSUPPORT_CONTRACTID, &kNativeAppSupportCID },
  { nullptr }
};

static const mozilla::Module kXREModule = {
  mozilla::Module::kVersion,
  kXRECIDs,
  kXREContracts
};

NSMODULE_DEFN(Apprunner) = &kXREModule;

nsresult
ScopedXPCOMStartup::Initialize()
{
  NS_ASSERTION(gDirServiceProvider, "Should not get here!");

  nsresult rv;

  rv = NS_InitXPCOM2(&mServiceManager, gDirServiceProvider->GetAppDir(),
                     gDirServiceProvider);
  if (NS_FAILED(rv)) {
    NS_ERROR("Couldn't start xpcom!");
    mServiceManager = nullptr;
  }
  else {
#ifdef DEBUG
    nsCOMPtr<nsIComponentRegistrar> reg =
      do_QueryInterface(mServiceManager);
    NS_ASSERTION(reg, "Service Manager doesn't QI to Registrar.");
#endif
  }

  return rv;
}

/**
 * This is a little factory class that serves as a singleton-service-factory
 * for the nativeappsupport object.
 */
class nsSingletonFactory final : public nsIFactory
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFACTORY

  explicit nsSingletonFactory(nsISupports* aSingleton);

private:
  ~nsSingletonFactory() { }
  nsCOMPtr<nsISupports> mSingleton;
};

nsSingletonFactory::nsSingletonFactory(nsISupports* aSingleton)
  : mSingleton(aSingleton)
{
  NS_ASSERTION(mSingleton, "Singleton was null!");
}

NS_IMPL_ISUPPORTS(nsSingletonFactory, nsIFactory)

NS_IMETHODIMP
nsSingletonFactory::CreateInstance(nsISupports* aOuter,
                                   const nsIID& aIID,
                                   void* *aResult)
{
  NS_ENSURE_NO_AGGREGATION(aOuter);

  return mSingleton->QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
nsSingletonFactory::LockFactory(bool)
{
  return NS_OK;
}

/**
 * Set our windowcreator on the WindowWatcher service.
 */
nsresult
ScopedXPCOMStartup::SetWindowCreator(nsINativeAppSupport* native)
{
  nsresult rv;

  NS_IF_ADDREF(gNativeAppSupport = native);

  // Inform the chrome registry about OS accessibility
  nsCOMPtr<nsIToolkitChromeRegistry> cr =
    mozilla::services::GetToolkitChromeRegistryService();

  if (cr)
    cr->CheckForOSAccessibility();

  nsCOMPtr<nsIWindowCreator> creator (do_GetService(NS_APPSTARTUP_CONTRACTID));
  if (!creator) return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIWindowWatcher> wwatch
    (do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  return wwatch->SetWindowCreator(creator);
}

/* static */ nsresult
ScopedXPCOMStartup::CreateAppSupport(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  if (!gNativeAppSupport)
    return NS_ERROR_NOT_INITIALIZED;

  return gNativeAppSupport->QueryInterface(aIID, aResult);
}

nsINativeAppSupport* ScopedXPCOMStartup::gNativeAppSupport;

static void DumpArbitraryHelp()
{
  nsresult rv;

  ScopedLogging log;

  {
    ScopedXPCOMStartup xpcom;
    xpcom.Initialize();

    nsCOMPtr<nsICommandLineRunner> cmdline
      (do_CreateInstance("@mozilla.org/toolkit/command-line;1"));
    if (!cmdline)
      return;

    nsCString text;
    rv = cmdline->GetHelpText(text);
    if (NS_SUCCEEDED(rv))
      printf("%s", text.get());
  }
}

// English text needs to go into a dtd file.
// But when this is called we have no components etc. These strings must either be
// here, or in a native resource file.
static void
DumpHelp()
{
  printf("Usage: %s [ options ... ] [URL]\n"
         "       where options include:\n\n", gArgv[0]);

#ifdef MOZ_X11
  printf("X11 options\n"
         "  --display=DISPLAY  X display to use\n"
         "  --sync             Make X calls synchronous\n");
#endif
#ifdef XP_UNIX
  printf("  --g-fatal-warnings Make all warnings fatal\n"
         "\n%s options\n", (const char*) gAppData->name);
#endif

  printf("  -h or --help       Print this message.\n"
         "  -v or --version    Print %s version.\n"
         "  -P <profile>       Start with <profile>.\n"
         "  --profile <path>   Start with profile at <path>.\n"
         "  --migration        Start with migration wizard.\n"
         "  --ProfileManager   Start with ProfileManager.\n"
         "  --no-remote        Do not accept or send remote commands; implies\n"
         "                     --new-instance.\n"
         "  --new-instance     Open new instance, not a new window in running instance.\n"
         "  --UILocale <locale> Start with <locale> resources as UI Locale.\n"
         "  --safe-mode        Disables extensions and themes for this session.\n"
         "  -MOZ_LOG=<modules> Treated as MOZ_LOG=<modules> environment variable, overrides it.\n"
         "  -MOZ_LOG_FILE=<file> Treated as MOZ_LOG_FILE=<file> environment variable, overrides it.\n"
         "                     If MOZ_LOG_FILE is not specified as an argument or as an environment variable,\n"
         "                     logging will be written to stdout.\n"
         , (const char*)gAppData->name);

#if defined(XP_WIN)
  printf("  --console          Start %s with a debugging console.\n", (const char*) gAppData->name);
#endif

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) || defined(XP_MACOSX)
  printf("  --headless         Run without a GUI.\n");
#endif

  // this works, but only after the components have registered.  so if you drop in a new command line handler, --help
  // won't not until the second run.
  // out of the bug, because we ship a component.reg file, it works correctly.
  DumpArbitraryHelp();
}

#if defined(DEBUG) && defined(XP_WIN)
#ifdef DEBUG_warren
#define _CRTDBG_MAP_ALLOC
#endif
// Set a CRT ReportHook function to capture and format MSCRT
// warnings, errors and assertions.
// See http://msdn.microsoft.com/en-US/library/74kabxyx(v=VS.80).aspx
#include <stdio.h>
#include <crtdbg.h>
#include "mozilla/mozalloc_abort.h"
static int MSCRTReportHook( int aReportType, char *aMessage, int *oReturnValue)
{
  *oReturnValue = 0; // continue execution

  // Do not use fprintf or other functions which may allocate
  // memory from the heap which may be corrupted. Instead,
  // use fputs to output the leading portion of the message
  // and use mozalloc_abort to emit the remainder of the
  // message.

  switch(aReportType) {
  case 0:
    fputs("\nWARNING: CRT WARNING", stderr);
    fputs(aMessage, stderr);
    fputs("\n", stderr);
    break;
  case 1:
    fputs("\n###!!! ABORT: CRT ERROR ", stderr);
    mozalloc_abort(aMessage);
    break;
  case 2:
    fputs("\n###!!! ABORT: CRT ASSERT ", stderr);
    mozalloc_abort(aMessage);
    break;
  }

  // do not invoke the debugger
  return 1;
}

#endif

static inline void
DumpVersion()
{
  if (gAppData->vendor)
    printf("%s ", (const char*) gAppData->vendor);
  printf("%s %s", (const char*) gAppData->name, (const char*) gAppData->version);
  if (gAppData->copyright)
      printf(", %s", (const char*) gAppData->copyright);
  printf("\n");
}

#ifdef MOZ_ENABLE_XREMOTE
static RemoteResult
ParseRemoteCommandLine(nsCString& program,
                       const char** profile,
                       const char** username)
{
  ArgResult ar;

  ar = CheckArg("p", profile, CheckArgFlag::None);
  if (ar == ARG_BAD) {
    // Leave it to the normal command line handling to handle this situation.
    return REMOTE_NOT_FOUND;
  }

  const char *temp = nullptr;
  ar = CheckArg("a", &temp, CheckArgFlag::CheckOSInt | CheckArgFlag::RemoveArg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -a requires an application name\n");
    return REMOTE_ARG_BAD;
  }
  if (ar == ARG_FOUND) {
    program.Assign(temp);
  }

  ar = CheckArg("u", username, CheckArgFlag::CheckOSInt | CheckArgFlag::RemoveArg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -u requires a username\n");
    return REMOTE_ARG_BAD;
  }

  return REMOTE_FOUND;
}

static RemoteResult
StartRemoteClient(const char* aDesktopStartupID,
                  nsCString& program,
                  const char* profile,
                  const char* username)
{
  nsAutoPtr<nsRemoteClient> client;

#if defined(MOZ_ENABLE_DBUS) && defined(MOZ_WAYLAND)
  client = new DBusRemoteClient();
#else
  client = new XRemoteClient();
#endif

  nsresult rv = client->Init();
  if (NS_FAILED(rv))
    return REMOTE_NOT_FOUND;

  nsCString response;
  bool success = false;
  rv = client->SendCommandLine(program.get(), username, profile,
                               gArgc, gArgv, aDesktopStartupID,
                               getter_Copies(response), &success);
  // did the command fail?
  if (!success)
    return REMOTE_NOT_FOUND;

  // The "command not parseable" error is returned when the
  // nsICommandLineHandler throws a NS_ERROR_ABORT.
  if (response.EqualsLiteral("500 command not parseable"))
    return REMOTE_ARG_BAD;

  if (NS_FAILED(rv))
    return REMOTE_NOT_FOUND;

  return REMOTE_FOUND;
}
#endif // MOZ_ENABLE_XREMOTE

void
XRE_InitOmnijar(nsIFile* greOmni, nsIFile* appOmni)
{
  mozilla::Omnijar::Init(greOmni, appOmni);
}

nsresult
XRE_GetBinaryPath(nsIFile* *aResult)
{
  return mozilla::BinaryPath::GetFile(aResult);
}

#ifdef XP_WIN
#include "nsWindowsRestart.cpp"
#include <shellapi.h>

typedef BOOL (WINAPI* SetProcessDEPPolicyFunc)(DWORD dwFlags);

static void
RegisterApplicationRestartChanged(const char* aPref, void* aData) {
  DWORD cchCmdLine = 0;
  HRESULT rc =
    ::GetApplicationRestartSettings(::GetCurrentProcess(), nullptr, &cchCmdLine, nullptr);
  bool wasRegistered = false;
  if (rc == S_OK) {
    wasRegistered = true;
  }

  if (Preferences::GetBool(PREF_WIN_REGISTER_APPLICATION_RESTART, false) && !wasRegistered) {
    // Make the command line to use when restarting.
    // Excludes argv[0] because RegisterApplicationRestart adds the
    // executable name, replace that temporarily with -os-restarted
    char* exeName = gRestartArgv[0];
    gRestartArgv[0] = "-os-restarted";
    wchar_t** restartArgvConverted =
      AllocConvertUTF8toUTF16Strings(gRestartArgc, gRestartArgv);
    gRestartArgv[0] = exeName;

    mozilla::UniquePtr<wchar_t[]> restartCommandLine;
    if (restartArgvConverted) {
      restartCommandLine = mozilla::MakeCommandLine(gRestartArgc, restartArgvConverted);
      FreeAllocStrings(gRestartArgc, restartArgvConverted);
    }

    if (restartCommandLine) {
      // Flags RESTART_NO_PATCH and RESTART_NO_REBOOT are not set, so we
      // should be restarted if terminated by an update or restart.
      ::RegisterApplicationRestart(restartCommandLine.get(), RESTART_NO_CRASH |
                                                             RESTART_NO_HANG);
    }
  } else if (wasRegistered) {
    ::UnregisterApplicationRestart();
  }
}
#endif // XP_WIN

// If aBlankCommandLine is true, then the application will be launched with a
// blank command line instead of being launched with the same command line that
// it was initially started with.
static nsresult LaunchChild(nsINativeAppSupport* aNative,
                            bool aBlankCommandLine = false)
{
  aNative->Quit(); // release DDE mutex, if we're holding it

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

#if !defined(MOZ_WIDGET_ANDROID) // Android has separate restart code.
#if defined(XP_MACOSX)
  CommandLineServiceMac::SetupMacCommandLine(gRestartArgc, gRestartArgv, true);
  LaunchChildMac(gRestartArgc, gRestartArgv);
#else
  nsCOMPtr<nsIFile> lf;
  nsresult rv = XRE_GetBinaryPath(getter_AddRefs(lf));
  if (NS_FAILED(rv))
    return rv;

#if defined(XP_WIN)
  nsAutoString exePath;
  rv = lf->GetPath(exePath);
  if (NS_FAILED(rv))
    return rv;

  HANDLE hProcess;
  if (!WinLaunchChild(exePath.get(), gRestartArgc, gRestartArgv, nullptr, &hProcess))
    return NS_ERROR_FAILURE;
  // Keep the current process around until the restarted process has created
  // its message queue, to avoid the launched process's windows being forced
  // into the background.
  ::WaitForInputIdle(hProcess, kWaitForInputIdleTimeoutMS);
  ::CloseHandle(hProcess);

#else
  nsAutoCString exePath;
  rv = lf->GetNativePath(exePath);
  if (NS_FAILED(rv))
    return rv;

#if defined(XP_UNIX)
  if (execv(exePath.get(), gRestartArgv) == -1)
    return NS_ERROR_FAILURE;
#else
  PRProcess* process = PR_CreateProcess(exePath.get(), gRestartArgv,
                                        nullptr, nullptr);
  if (!process) return NS_ERROR_FAILURE;

  int32_t exitCode;
  PRStatus failed = PR_WaitProcess(process, &exitCode);
  if (failed || exitCode)
    return NS_ERROR_FAILURE;
#endif // XP_UNIX
#endif // WP_WIN
#endif // WP_MACOSX
#endif // MOZ_WIDGET_ANDROID

  return NS_ERROR_LAUNCHED_CHILD_PROCESS;
}

static const char kProfileProperties[] =
  "chrome://mozapps/locale/profile/profileSelection.properties";

namespace {

/**
 * This class, instead of a raw nsresult, should be the return type of any
 * function called by SelectProfile that initializes XPCOM.
 */
class ReturnAbortOnError
{
public:
  MOZ_IMPLICIT ReturnAbortOnError(nsresult aRv)
  {
    mRv = ConvertRv(aRv);
  }

  operator nsresult()
  {
    return mRv;
  }

private:
  inline nsresult
  ConvertRv(nsresult aRv)
  {
    if (NS_SUCCEEDED(aRv) || aRv == NS_ERROR_LAUNCHED_CHILD_PROCESS) {
      return aRv;
    }
    return NS_ERROR_ABORT;
  }

  nsresult mRv;
};

} // namespace

static ReturnAbortOnError
ProfileLockedDialog(nsIFile* aProfileDir, nsIFile* aProfileLocalDir,
                    nsIProfileUnlocker* aUnlocker,
                    nsINativeAppSupport* aNative, nsIProfileLock* *aResult)
{
  nsresult rv;

  ScopedXPCOMStartup xpcom;
  rv = xpcom.Initialize();
  NS_ENSURE_SUCCESS(rv, rv);

  mozilla::Telemetry::WriteFailedProfileLock(aProfileDir);

  rv = xpcom.SetWindowCreator(aNative);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  { //extra scoping is needed so we release these components before xpcom shutdown
    nsCOMPtr<nsIStringBundleService> sbs =
      mozilla::services::GetStringBundleService();
    NS_ENSURE_TRUE(sbs, NS_ERROR_FAILURE);

    nsCOMPtr<nsIStringBundle> sb;
    sbs->CreateBundle(kProfileProperties, getter_AddRefs(sb));
    NS_ENSURE_TRUE_LOG(sbs, NS_ERROR_FAILURE);

    NS_ConvertUTF8toUTF16 appName(gAppData->name);
    const char16_t* params[] = {appName.get(), appName.get()};

    nsAutoString killMessage;
#ifndef XP_MACOSX
    rv = sb->FormatStringFromName(aUnlocker ? "restartMessageUnlocker"
                                            : "restartMessageNoUnlocker",
                                  params, 2, killMessage);
#else
    rv = sb->FormatStringFromName(aUnlocker ? "restartMessageUnlockerMac"
                                            : "restartMessageNoUnlockerMac",
                                  params, 2, killMessage);
#endif
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    nsAutoString killTitle;
    rv = sb->FormatStringFromName("restartTitle", params, 1, killTitle);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    if (gfxPlatform::IsHeadless()) {
      // TODO: make a way to turn off all dialogs when headless.
      Output(true, "%s\n", NS_LossyConvertUTF16toASCII(killMessage).get());
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIPromptService> ps
      (do_GetService(NS_PROMPTSERVICE_CONTRACTID));
    NS_ENSURE_TRUE(ps, NS_ERROR_FAILURE);

    if (aUnlocker) {
      int32_t button;
#ifdef MOZ_WIDGET_ANDROID
      java::GeckoAppShell::KillAnyZombies();
      button = 0;
#else
      const uint32_t flags =
        (nsIPromptService::BUTTON_TITLE_IS_STRING *
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

        return LaunchChild(aNative);
      }
    } else {
#ifdef MOZ_WIDGET_ANDROID
      if (java::GeckoAppShell::UnlockProfile()) {
        return NS_LockProfilePath(aProfileDir, aProfileLocalDir,
                                  nullptr, aResult);
      }
#else
      rv = ps->Alert(nullptr, killTitle.get(), killMessage.get());
      NS_ENSURE_SUCCESS_LOG(rv, rv);
#endif
    }

    return NS_ERROR_ABORT;
  }
}

static nsresult
ProfileMissingDialog(nsINativeAppSupport* aNative)
{
  nsresult rv;

  ScopedXPCOMStartup xpcom;
  rv = xpcom.Initialize();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = xpcom.SetWindowCreator(aNative);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  { //extra scoping is needed so we release these components before xpcom shutdown
    nsCOMPtr<nsIStringBundleService> sbs =
      mozilla::services::GetStringBundleService();
    NS_ENSURE_TRUE(sbs, NS_ERROR_FAILURE);

    nsCOMPtr<nsIStringBundle> sb;
    sbs->CreateBundle(kProfileProperties, getter_AddRefs(sb));
    NS_ENSURE_TRUE_LOG(sbs, NS_ERROR_FAILURE);

    NS_ConvertUTF8toUTF16 appName(gAppData->name);
    const char16_t* params[] = {appName.get(), appName.get()};

    // profileMissing
    nsAutoString missingMessage;
    rv = sb->FormatStringFromName("profileMissing", params, 2, missingMessage);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_ABORT);

    nsAutoString missingTitle;
    rv = sb->FormatStringFromName("profileMissingTitle", params, 1, missingTitle);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_ABORT);

    nsCOMPtr<nsIPromptService> ps(do_GetService(NS_PROMPTSERVICE_CONTRACTID));
    NS_ENSURE_TRUE(ps, NS_ERROR_FAILURE);

    ps->Alert(nullptr, missingTitle.get(), missingMessage.get());

    return NS_ERROR_ABORT;
  }
}

static nsresult
ProfileLockedDialog(nsIToolkitProfile* aProfile, nsIProfileUnlocker* aUnlocker,
                    nsINativeAppSupport* aNative, nsIProfileLock* *aResult)
{
  nsCOMPtr<nsIFile> profileDir;
  nsresult rv = aProfile->GetRootDir(getter_AddRefs(profileDir));
  if (NS_FAILED(rv)) return rv;

  bool exists;
  profileDir->Exists(&exists);
  if (!exists) {
    return ProfileMissingDialog(aNative);
  }

  nsCOMPtr<nsIFile> profileLocalDir;
  rv = aProfile->GetLocalDir(getter_AddRefs(profileLocalDir));
  if (NS_FAILED(rv)) return rv;

  return ProfileLockedDialog(profileDir, profileLocalDir, aUnlocker, aNative,
                             aResult);
}

static const char kProfileManagerURL[] =
  "chrome://mozapps/content/profile/profileSelection.xul";

static ReturnAbortOnError
ShowProfileManager(nsIToolkitProfileService* aProfileSvc,
                   nsINativeAppSupport* aNative)
{
  if (!CanShowProfileManager()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsresult rv;

  nsCOMPtr<nsIFile> profD, profLD;
  char16_t* profileNamePtr;
  nsAutoCString profileName;

  {
    ScopedXPCOMStartup xpcom;
    rv = xpcom.Initialize();
    NS_ENSURE_SUCCESS(rv, rv);

    // Initialize the graphics prefs, some of the paths need them before
    // any other graphics is initialized (e.g., showing the profile chooser.)
    gfxPrefs::GetSingleton();

    rv = xpcom.SetWindowCreator(aNative);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

#ifdef XP_MACOSX
    CommandLineServiceMac::SetupMacCommandLine(gRestartArgc, gRestartArgv, true);
#endif

#ifdef XP_WIN
    // we don't have to wait here because profile manager window will pump
    // and DDE message will be handled
    ProcessDDE(aNative, false);
#endif

    { //extra scoping is needed so we release these components before xpcom shutdown
      nsCOMPtr<nsIWindowWatcher> windowWatcher
        (do_GetService(NS_WINDOWWATCHER_CONTRACTID));
      nsCOMPtr<nsIDialogParamBlock> ioParamBlock
        (do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID));
      nsCOMPtr<nsIMutableArray> dlgArray (do_CreateInstance(NS_ARRAY_CONTRACTID));
      NS_ENSURE_TRUE(windowWatcher && ioParamBlock && dlgArray, NS_ERROR_FAILURE);

      ioParamBlock->SetObjects(dlgArray);

      nsCOMPtr<nsIAppStartup> appStartup
        (do_GetService(NS_APPSTARTUP_CONTRACTID));
      NS_ENSURE_TRUE(appStartup, NS_ERROR_FAILURE);

      nsCOMPtr<mozIDOMWindowProxy> newWindow;
      rv = windowWatcher->OpenWindow(nullptr,
                                     kProfileManagerURL,
                                     "_blank",
                                     "centerscreen,chrome,modal,titlebar",
                                     ioParamBlock,
                                     getter_AddRefs(newWindow));

      NS_ENSURE_SUCCESS_LOG(rv, rv);

      aProfileSvc->Flush();

      int32_t dialogConfirmed;
      rv = ioParamBlock->GetInt(0, &dialogConfirmed);
      if (NS_FAILED(rv) || dialogConfirmed == 0) return NS_ERROR_ABORT;

      nsCOMPtr<nsIProfileLock> lock;
      rv = dlgArray->QueryElementAt(0, NS_GET_IID(nsIProfileLock),
                                    getter_AddRefs(lock));
      NS_ENSURE_SUCCESS_LOG(rv, rv);

      rv = lock->GetDirectory(getter_AddRefs(profD));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = lock->GetLocalDirectory(getter_AddRefs(profLD));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = ioParamBlock->GetString(0, &profileNamePtr);
      NS_ENSURE_SUCCESS(rv, rv);

      CopyUTF16toUTF8(profileNamePtr, profileName);
      free(profileNamePtr);

      lock->Unlock();
    }
  }

  SaveFileToEnv("XRE_PROFILE_PATH", profD);
  SaveFileToEnv("XRE_PROFILE_LOCAL_PATH", profLD);
  SaveWordToEnv("XRE_PROFILE_NAME", profileName);

  bool offline = false;
  aProfileSvc->GetStartOffline(&offline);
  if (offline) {
    SaveToEnv("XRE_START_OFFLINE=1");
  }
  if (gRestartedByOS) {
    // Re-add this argument when actually starting the application.
    char** newArgv = (char**) realloc(gRestartArgv, sizeof(char*) * (gRestartArgc + 2));
    NS_ENSURE_TRUE(newArgv, NS_ERROR_OUT_OF_MEMORY);
    gRestartArgv = newArgv;
    gRestartArgv[gRestartArgc++] = const_cast<char*>("-os-restarted");
    gRestartArgv[gRestartArgc] = nullptr;
  }

  return LaunchChild(aNative);
}

/**
 * Get the currently running profile using its root directory.
 *
 * @param aProfileSvc         The profile service
 * @param aCurrentProfileRoot The root directory of the current profile.
 * @param aProfile            Out-param that returns the profile object.
 * @return an error if aCurrentProfileRoot is not found
 */
static nsresult
GetCurrentProfile(nsIToolkitProfileService* aProfileSvc,
                  nsIFile* aCurrentProfileRoot,
                  nsIToolkitProfile** aProfile)
{
  NS_ENSURE_ARG_POINTER(aProfileSvc);
  NS_ENSURE_ARG_POINTER(aProfile);

  nsCOMPtr<nsISimpleEnumerator> profiles;
  nsresult rv = aProfileSvc->GetProfiles(getter_AddRefs(profiles));
  if (NS_FAILED(rv))
    return rv;

  bool foundMatchingProfile = false;
  nsCOMPtr<nsISupports> supports;
  rv = profiles->GetNext(getter_AddRefs(supports));
  while (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIToolkitProfile> profile = do_QueryInterface(supports);
    nsCOMPtr<nsIFile> profileRoot;
    profile->GetRootDir(getter_AddRefs(profileRoot));
    profileRoot->Equals(aCurrentProfileRoot, &foundMatchingProfile);
    if (foundMatchingProfile) {
      profile.forget(aProfile);
      return NS_OK;
    }
    rv = profiles->GetNext(getter_AddRefs(supports));
  }
  return rv;
}

static bool gDoMigration = false;
static bool gDoProfileReset = false;
static nsAutoCString gResetOldProfileName;

// Pick a profile. We need to end up with a profile lock.
//
// 1) check for --profile <path>
// 2) check for -P <name>
// 3) check for --ProfileManager
// 4) use the default profile, if there is one
// 5) if there are *no* profiles, set up profile-migration
// 6) display the profile-manager UI
static nsresult
SelectProfile(nsIProfileLock* *aResult, nsIToolkitProfileService* aProfileSvc, nsINativeAppSupport* aNative,
              bool* aStartOffline, nsACString* aProfileName)
{
  StartupTimeline::Record(StartupTimeline::SELECT_PROFILE);

  nsresult rv;
  ArgResult ar;
  const char* arg;
  *aResult = nullptr;
  *aStartOffline = false;

  ar = CheckArg("offline", nullptr, CheckArgFlag::CheckOSInt | CheckArgFlag::RemoveArg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument --offline is invalid when argument --osint is specified\n");
    return NS_ERROR_FAILURE;
  }

  if (ar || EnvHasValue("XRE_START_OFFLINE"))
    *aStartOffline = true;

  if (EnvHasValue("MOZ_RESET_PROFILE_RESTART")) {
    gDoProfileReset = true;
    gDoMigration = true;
    SaveToEnv("MOZ_RESET_PROFILE_RESTART=");
    // We only want to restore the previous session if the profile refresh was
    // triggered by user. And if it was a user-triggered profile refresh
    // through, say, the safeMode dialog or the troubleshooting page, the MOZ_RESET_PROFILE_RESTART
    // env variable would be set. Hence we set MOZ_RESET_PROFILE_MIGRATE_SESSION here so that
    // Firefox profile migrator would migrate old session data later.
    SaveToEnv("MOZ_RESET_PROFILE_MIGRATE_SESSION=1");
  }

  // reset-profile and migration args need to be checked before any profiles are chosen below.
  ar = CheckArg("reset-profile", nullptr, CheckArgFlag::CheckOSInt | CheckArgFlag::RemoveArg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument --reset-profile is invalid when argument --osint is specified\n");
    return NS_ERROR_FAILURE;
  }
  if (ar == ARG_FOUND) {
    gDoProfileReset = true;
  }

  ar = CheckArg("migration", nullptr, CheckArgFlag::CheckOSInt | CheckArgFlag::RemoveArg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument --migration is invalid when argument --osint is specified\n");
    return NS_ERROR_FAILURE;
  }
  if (ar == ARG_FOUND) {
    gDoMigration = true;
  }

  nsCOMPtr<nsIFile> lf = GetFileFromEnv("XRE_PROFILE_PATH");
  if (lf) {
    nsCOMPtr<nsIFile> localDir =
      GetFileFromEnv("XRE_PROFILE_LOCAL_PATH");
    if (!localDir) {
      localDir = lf;
    }

    arg = PR_GetEnv("XRE_PROFILE_NAME");
    if (arg && *arg && aProfileName) {
      aProfileName->Assign(nsDependentCString(arg));
      if (gDoProfileReset) {
        gResetOldProfileName.Assign(*aProfileName);
      }
    }

    // Clear out flags that we handled (or should have handled!) last startup.
    const char *dummy;
    CheckArg("p", &dummy);
    CheckArg("profile", &dummy);
    CheckArg("profilemanager");

    if (gDoProfileReset) {
      // If we're resetting a profile, create a new one and use it to startup.
      nsCOMPtr<nsIToolkitProfile> newProfile;
      rv = CreateResetProfile(aProfileSvc, gResetOldProfileName, getter_AddRefs(newProfile));
      if (NS_SUCCEEDED(rv)) {
        rv = newProfile->GetRootDir(getter_AddRefs(lf));
        NS_ENSURE_SUCCESS(rv, rv);
        SaveFileToEnv("XRE_PROFILE_PATH", lf);

        rv = newProfile->GetLocalDir(getter_AddRefs(localDir));
        NS_ENSURE_SUCCESS(rv, rv);
        SaveFileToEnv("XRE_PROFILE_LOCAL_PATH", localDir);

        rv = newProfile->GetName(*aProfileName);
        if (NS_FAILED(rv))
          aProfileName->Truncate(0);
        SaveWordToEnv("XRE_PROFILE_NAME", *aProfileName);
      } else {
        NS_WARNING("Profile reset failed.");
        gDoProfileReset = false;
      }
    }

    return NS_LockProfilePath(lf, localDir, nullptr, aResult);
  }

  ar = CheckArg("profile", &arg, CheckArgFlag::CheckOSInt | CheckArgFlag::RemoveArg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument --profile requires a path\n");
    return NS_ERROR_FAILURE;
  }
  if (ar) {
    if (gDoProfileReset) {
      NS_WARNING("Profile reset is not supported in conjunction with --profile.");
      gDoProfileReset = false;
    }

    nsCOMPtr<nsIFile> lf;
    rv = XRE_GetFileFromPath(arg, getter_AddRefs(lf));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIProfileUnlocker> unlocker;

    // Check if the profile path exists and it's a directory.
    bool exists;
    lf->Exists(&exists);
    if (!exists) {
        rv = lf->Create(nsIFile::DIRECTORY_TYPE, 0700);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // If a profile path is specified directory on the command line, then
    // assume that the temp directory is the same as the given directory.
    rv = NS_LockProfilePath(lf, lf, getter_AddRefs(unlocker), aResult);
    if (NS_SUCCEEDED(rv))
      return rv;

    return ProfileLockedDialog(lf, lf, unlocker, aNative, aResult);
  }

  ar = CheckArg("createprofile", &arg, CheckArgFlag::CheckOSInt | CheckArgFlag::RemoveArg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument --createprofile requires a profile name\n");
    return NS_ERROR_FAILURE;
  }
  if (ar) {
    nsCOMPtr<nsIToolkitProfile> profile;

    const char* delim = strchr(arg, ' ');
    if (delim) {
      nsCOMPtr<nsIFile> lf;
      rv = NS_NewNativeLocalFile(nsDependentCString(delim + 1),
                                   true, getter_AddRefs(lf));
      if (NS_FAILED(rv)) {
        PR_fprintf(PR_STDERR, "Error: profile path not valid.\n");
        return rv;
      }

      // As with --profile, assume that the given path will be used for the
      // main profile directory.
      rv = aProfileSvc->CreateProfile(lf, nsDependentCSubstring(arg, delim),
                                     getter_AddRefs(profile));
    } else {
      rv = aProfileSvc->CreateProfile(nullptr, nsDependentCString(arg),
                                     getter_AddRefs(profile));
    }
    // Some pathological arguments can make it this far
    if (NS_FAILED(rv)) {
      PR_fprintf(PR_STDERR, "Error creating profile.\n");
      return rv;
    }
    rv = NS_ERROR_ABORT;
    aProfileSvc->Flush();

    // XXXben need to ensure prefs.js exists here so the tinderboxes will
    //        not go orange.
    nsCOMPtr<nsIFile> prefsJSFile;
    profile->GetRootDir(getter_AddRefs(prefsJSFile));
    prefsJSFile->AppendNative(NS_LITERAL_CSTRING("prefs.js"));
    PR_fprintf(PR_STDERR, "Success: created profile '%s' at '%s'\n", arg,
               prefsJSFile->HumanReadablePath().get());
    bool exists;
    prefsJSFile->Exists(&exists);
    if (!exists) {
      // Ignore any errors; we're about to return NS_ERROR_ABORT anyway.
      Unused << prefsJSFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
    }
    // XXXdarin perhaps 0600 would be better?

    return rv;
  }

  uint32_t count;
  rv = aProfileSvc->GetProfileCount(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  ar = CheckArg("p", &arg);
  if (ar == ARG_BAD) {
    ar = CheckArg("osint");
    if (ar == ARG_FOUND) {
      PR_fprintf(PR_STDERR, "Error: argument -p is invalid when argument --osint is specified\n");
      return NS_ERROR_FAILURE;
    }

    if (CanShowProfileManager()) {
      return ShowProfileManager(aProfileSvc, aNative);
    }
  }
  if (ar) {
    ar = CheckArg("osint");
    if (ar == ARG_FOUND) {
      PR_fprintf(PR_STDERR, "Error: argument -p is invalid when argument --osint is specified\n");
      return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIToolkitProfile> profile;
    rv = aProfileSvc->GetProfileByName(nsDependentCString(arg),
                                      getter_AddRefs(profile));
    if (NS_SUCCEEDED(rv)) {
      if (gDoProfileReset) {
        {
          // Check that the source profile is not in use by temporarily acquiring its lock.
          nsIProfileLock* tempProfileLock;
          nsCOMPtr<nsIProfileUnlocker> unlocker;
          rv = profile->Lock(getter_AddRefs(unlocker), &tempProfileLock);
          if (NS_FAILED(rv))
            return ProfileLockedDialog(profile, unlocker, aNative, &tempProfileLock);
        }

        nsresult gotName = profile->GetName(gResetOldProfileName);
        if (NS_SUCCEEDED(gotName)) {
          nsCOMPtr<nsIToolkitProfile> newProfile;
          rv = CreateResetProfile(aProfileSvc, gResetOldProfileName, getter_AddRefs(newProfile));
          if (NS_FAILED(rv)) {
            NS_WARNING("Failed to create a profile to reset to.");
            gDoProfileReset = false;
          } else {
            profile = newProfile;
          }
        } else {
          NS_WARNING("Failed to get the name of the profile we're resetting, so aborting reset.");
          gResetOldProfileName.Truncate(0);
          gDoProfileReset = false;
        }
      }

      nsCOMPtr<nsIProfileUnlocker> unlocker;
      rv = profile->Lock(getter_AddRefs(unlocker), aResult);
      if (NS_SUCCEEDED(rv)) {
        if (aProfileName)
          aProfileName->Assign(nsDependentCString(arg));
        return NS_OK;
      }

      return ProfileLockedDialog(profile, unlocker, aNative, aResult);
    }

    if (CanShowProfileManager()) {
      return ShowProfileManager(aProfileSvc, aNative);
    }
  }

  ar = CheckArg("profilemanager", nullptr, CheckArgFlag::CheckOSInt | CheckArgFlag::RemoveArg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument --profilemanager is invalid when argument --osint is specified\n");
    return NS_ERROR_FAILURE;
  }
  if (ar == ARG_FOUND && CanShowProfileManager()) {
    return ShowProfileManager(aProfileSvc, aNative);
  }

#ifndef MOZ_DEV_EDITION
  // If the only existing profile is the dev-edition-profile and this is not
  // Developer Edition, then no valid profiles were found.
  if (count == 1) {
    nsCOMPtr<nsIToolkitProfile> deProfile;
    // GetSelectedProfile will auto-select the only profile if there's just one
    aProfileSvc->GetSelectedProfile(getter_AddRefs(deProfile));
    nsAutoCString profileName;
    deProfile->GetName(profileName);
    if (profileName.EqualsLiteral("dev-edition-default")) {
      count = 0;
    }
  }
#endif

  if (!count) {
    // For a fresh install, we would like to let users decide
    // to do profile migration on their own later after using.
    gDoMigration = false;
    gDoProfileReset = false;

    // create a default profile
    nsCOMPtr<nsIToolkitProfile> profile;
    nsresult rv = aProfileSvc->CreateProfile(nullptr, // choose a default dir for us
#ifdef MOZ_DEV_EDITION
                                             NS_LITERAL_CSTRING("dev-edition-default"),
#else
                                             NS_LITERAL_CSTRING("default"),
#endif
                                             getter_AddRefs(profile));
    if (NS_SUCCEEDED(rv)) {
#ifndef MOZ_DEV_EDITION
      aProfileSvc->SetDefaultProfile(profile);
#endif
      aProfileSvc->Flush();
      rv = profile->Lock(nullptr, aResult);
      if (NS_SUCCEEDED(rv)) {
        if (aProfileName)
#ifdef MOZ_DEV_EDITION
          aProfileName->AssignLiteral("dev-edition-default");
#else
          aProfileName->AssignLiteral("default");
#endif
        return NS_OK;
      }
    }
  }

  bool useDefault = true;
  if (count > 1 && CanShowProfileManager()) {
    aProfileSvc->GetStartWithLastProfile(&useDefault);
  }

  if (useDefault) {
    nsCOMPtr<nsIToolkitProfile> profile;
    // GetSelectedProfile will auto-select the only profile if there's just one
    aProfileSvc->GetSelectedProfile(getter_AddRefs(profile));
    if (profile) {
      // If we're resetting a profile, create a new one and use it to startup.
      if (gDoProfileReset) {
        {
          // Check that the source profile is not in use by temporarily acquiring its lock.
          nsIProfileLock* tempProfileLock;
          nsCOMPtr<nsIProfileUnlocker> unlocker;
          rv = profile->Lock(getter_AddRefs(unlocker), &tempProfileLock);
          if (NS_FAILED(rv))
            return ProfileLockedDialog(profile, unlocker, aNative, &tempProfileLock);
        }

        nsresult gotName = profile->GetName(gResetOldProfileName);
        if (NS_SUCCEEDED(gotName)) {
          nsCOMPtr<nsIToolkitProfile> newProfile;
          rv = CreateResetProfile(aProfileSvc, gResetOldProfileName, getter_AddRefs(newProfile));
          if (NS_FAILED(rv)) {
            NS_WARNING("Failed to create a profile to reset to.");
            gDoProfileReset = false;
          }
          else {
            profile = newProfile;
          }
        }
        else {
          NS_WARNING("Failed to get the name of the profile we're resetting, so aborting reset.");
          gResetOldProfileName.Truncate(0);
          gDoProfileReset = false;
        }
      }

      // If you close Firefox and very quickly reopen it, the old Firefox may
      // still be closing down. Rather than immediately showing the
      // "Firefox is running but is not responding" message, we spend a few
      // seconds retrying first.

      static const int kLockRetrySeconds = 5;
      static const int kLockRetrySleepMS = 100;

      nsCOMPtr<nsIProfileUnlocker> unlocker;
      const TimeStamp start = TimeStamp::Now();
      do {
        rv = profile->Lock(getter_AddRefs(unlocker), aResult);
        if (NS_SUCCEEDED(rv)) {
          StartupTimeline::Record(StartupTimeline::AFTER_PROFILE_LOCKED);
          // Try to grab the profile name.
          if (aProfileName) {
            rv = profile->GetName(*aProfileName);
            if (NS_FAILED(rv))
              aProfileName->Truncate(0);
          }
          return NS_OK;
        }
        PR_Sleep(kLockRetrySleepMS);
      } while (TimeStamp::Now() - start < TimeDuration::FromSeconds(kLockRetrySeconds));

      return ProfileLockedDialog(profile, unlocker, aNative, aResult);
    }
  }

  if (!CanShowProfileManager()) {
    return NS_ERROR_FAILURE;
  }

  return ShowProfileManager(aProfileSvc, aNative);
}

/**
 * Checks the compatibility.ini file to see if we have updated our application
 * or otherwise invalidated our caches. If the application has been updated,
 * we return false; otherwise, we return true. We also write the status
 * of the caches (valid/invalid) into the return param aCachesOK. The aCachesOK
 * is always invalid if the application has been updated.
 */
static bool
CheckCompatibility(nsIFile* aProfileDir, const nsCString& aVersion,
                   const nsCString& aOSABI, nsIFile* aXULRunnerDir,
                   nsIFile* aAppDir, nsIFile* aFlagFile,
                   bool* aCachesOK)
{
  *aCachesOK = false;
  nsCOMPtr<nsIFile> file;
  aProfileDir->Clone(getter_AddRefs(file));
  if (!file)
    return false;
  file->AppendNative(FILE_COMPATIBILITY_INFO);

  nsINIParser parser;
  nsresult rv = parser.Init(file);
  if (NS_FAILED(rv))
    return false;

  nsAutoCString buf;
  rv = parser.GetString("Compatibility", "LastVersion", buf);
  if (NS_FAILED(rv) || !aVersion.Equals(buf))
    return false;

  rv = parser.GetString("Compatibility", "LastOSABI", buf);
  if (NS_FAILED(rv) || !aOSABI.Equals(buf))
    return false;

  rv = parser.GetString("Compatibility", "LastPlatformDir", buf);
  if (NS_FAILED(rv))
    return false;

  nsCOMPtr<nsIFile> lf;
  rv = NS_NewNativeLocalFile(EmptyCString(), false,
                             getter_AddRefs(lf));
  if (NS_FAILED(rv))
    return false;

  rv = lf->SetPersistentDescriptor(buf);
  if (NS_FAILED(rv))
    return false;

  bool eq;
  rv = lf->Equals(aXULRunnerDir, &eq);
  if (NS_FAILED(rv) || !eq)
    return false;

  if (aAppDir) {
    rv = parser.GetString("Compatibility", "LastAppDir", buf);
    if (NS_FAILED(rv))
      return false;

    rv = NS_NewNativeLocalFile(EmptyCString(), false,
                               getter_AddRefs(lf));
    if (NS_FAILED(rv))
      return false;

    rv = lf->SetPersistentDescriptor(buf);
    if (NS_FAILED(rv))
      return false;

    rv = lf->Equals(aAppDir, &eq);
    if (NS_FAILED(rv) || !eq)
      return false;
  }

  // If we see this flag, caches are invalid.
  rv = parser.GetString("Compatibility", "InvalidateCaches", buf);
  *aCachesOK = (NS_FAILED(rv) || !buf.EqualsLiteral("1"));

  bool purgeCaches = false;
  if (aFlagFile) {
    aFlagFile->Exists(&purgeCaches);
  }

  *aCachesOK = !purgeCaches && *aCachesOK;
  return true;
}

static void BuildVersion(nsCString &aBuf)
{
  aBuf.Assign(gAppData->version);
  aBuf.Append('_');
  aBuf.Append(gAppData->buildID);
  aBuf.Append('/');
  aBuf.Append(gToolkitBuildID);
}

static void
WriteVersion(nsIFile* aProfileDir, const nsCString& aVersion,
             const nsCString& aOSABI, nsIFile* aXULRunnerDir,
             nsIFile* aAppDir, bool invalidateCache)
{
  nsCOMPtr<nsIFile> file;
  aProfileDir->Clone(getter_AddRefs(file));
  if (!file)
    return;
  file->AppendNative(FILE_COMPATIBILITY_INFO);

  nsAutoCString platformDir;
  Unused << aXULRunnerDir->GetPersistentDescriptor(platformDir);

  nsAutoCString appDir;
  if (aAppDir)
    Unused << aAppDir->GetPersistentDescriptor(appDir);

  PRFileDesc *fd;
  nsresult rv =
    file->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0600, &fd);
  if (NS_FAILED(rv)) {
    NS_ERROR("could not create output stream");
    return;
  }

  static const char kHeader[] = "[Compatibility]" NS_LINEBREAK
                                "LastVersion=";

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
static bool
RemoveComponentRegistries(nsIFile* aProfileDir, nsIFile* aLocalProfileDir,
                                      bool aRemoveEMFiles)
{
  nsCOMPtr<nsIFile> file;
  aProfileDir->Clone(getter_AddRefs(file));
  if (!file)
    return false;

  if (aRemoveEMFiles) {
    file->SetNativeLeafName(NS_LITERAL_CSTRING("extensions.ini"));
    file->Remove(false);
  }

  aLocalProfileDir->Clone(getter_AddRefs(file));
  if (!file)
    return false;

#if defined(XP_UNIX) || defined(XP_BEOS)
#define PLATFORM_FASL_SUFFIX ".mfasl"
#elif defined(XP_WIN)
#define PLATFORM_FASL_SUFFIX ".mfl"
#endif

  file->AppendNative(NS_LITERAL_CSTRING("XUL" PLATFORM_FASL_SUFFIX));
  file->Remove(false);

  file->SetNativeLeafName(NS_LITERAL_CSTRING("XPC" PLATFORM_FASL_SUFFIX));
  file->Remove(false);

  file->SetNativeLeafName(NS_LITERAL_CSTRING("startupCache"));
  nsresult rv = file->Remove(true);
  return NS_SUCCEEDED(rv) || rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;
}

// To support application initiated restart via nsIAppStartup.quit, we
// need to save various environment variables, and then restore them
// before re-launching the application.

static struct SavedVar {
  const char *name;
  char *value;
} gSavedVars[] = {
  {"XUL_APP_FILE", nullptr}
};

static void SaveStateForAppInitiatedRestart()
{
  for (auto & savedVar : gSavedVars) {
    const char *s = PR_GetEnv(savedVar.name);
    if (s)
      savedVar.value = Smprintf("%s=%s", savedVar.name, s).release();
  }
}

static void RestoreStateForAppInitiatedRestart()
{
  for (auto & savedVar : gSavedVars) {
    if (savedVar.value)
      PR_SetEnv(savedVar.value);
  }
}

// When we first initialize the crash reporter we don't have a profile,
// so we set the minidump path to $TEMP.  Once we have a profile,
// we set it to $PROFILE/minidumps, creating the directory
// if needed.
static void MakeOrSetMinidumpPath(nsIFile* profD)
{
  nsCOMPtr<nsIFile> dumpD;
  profD->Clone(getter_AddRefs(dumpD));

  if (dumpD) {
    bool fileExists;
    //XXX: do some more error checking here
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
static void MOZ_gdk_display_close(GdkDisplay *display)
{
#if CLEANUP_MEMORY
  // XXX wallpaper for bug 417163: don't close the Display if we're using the
  // Qt theme because we crash (in Qt code) when using jemalloc.
  bool skip_display_close = false;
  GtkSettings* settings =
    gtk_settings_get_for_screen(gdk_display_get_default_screen(display));
  gchar *theme_name;
  g_object_get(settings, "gtk-theme-name", &theme_name, nullptr);
  if (theme_name) {
    skip_display_close = strcmp(theme_name, "Qt") == 0;
    if (skip_display_close)
      NS_WARNING("wallpaper bug 417163 for Qt theme");
    g_free(theme_name);
  }

#ifdef MOZ_WIDGET_GTK
  // A workaround for https://bugzilla.gnome.org/show_bug.cgi?id=703257
  if (gtk_check_version(3,9,8) != NULL)
    skip_display_close = true;
#endif

  // Get a (new) Pango context that holds a reference to the fontmap that
  // GTK has been using.  gdk_pango_context_get() must be called while GTK
  // has a default display.
  PangoContext *pangoContext = gdk_pango_context_get();

  bool buggyCairoShutdown = cairo_version() < CAIRO_VERSION_ENCODE(1, 4, 0);

  if (!buggyCairoShutdown) {
    // We should shut down GDK before we shut down libraries it depends on
    // like Pango and cairo. But if cairo shutdown is buggy, we should
    // shut down cairo first otherwise it may crash because of dangling
    // references to Display objects (see bug 469831).
    if (!skip_display_close)
      gdk_display_close(display);
  }

  // Clean up PangoCairo's default fontmap.
  // This pango_fc_font_map_shutdown call (and the associated code to
  // get the font map) really shouldn't be needed anymore, except that
  // it's needed to avoid having cairo_debug_reset_static_data fatally
  // assert if we've leaked other things that hold on to the fontmap,
  // which is something that currently happens in mochitest-plugins.
  // Even if it didn't happen in mochitest-plugins, we probably want to
  // avoid the crash-on-leak problem since it makes it harder to use
  // many of our leak tools to debug leaks.

  // This doesn't take a reference.
  PangoFontMap *fontmap = pango_context_get_font_map(pangoContext);
  // Do some shutdown of the fontmap, which releases the fonts, clearing a
  // bunch of circular references from the fontmap through the fonts back to
  // itself.  The shutdown that this does is much less than what's done by
  // the fontmap's finalize, though.
  if (PANGO_IS_FC_FONT_MAP(fontmap))
      pango_fc_font_map_shutdown(PANGO_FC_FONT_MAP(fontmap));
  g_object_unref(pangoContext);

  // Tell PangoCairo to release its default fontmap.
  pango_cairo_font_map_set_default(nullptr);

  // cairo_debug_reset_static_data() is prototyped through cairo.h included
  // by gtk.h.
#ifdef cairo_debug_reset_static_data
#error "Looks like we're including Mozilla's cairo instead of system cairo"
#endif
  cairo_debug_reset_static_data();
  // FIXME: Do we need to call this in non-GTK2 cases as well?
  FcFini();

  if (buggyCairoShutdown) {
    if (!skip_display_close)
      gdk_display_close(display);
  }
#else // not CLEANUP_MEMORY
  // Don't do anything to avoid running into driver bugs under XCloseDisplay().
  // See bug 973192.
  (void) display;
#endif
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

#include <dwrite.h>

#ifdef DEBUG_DWRITE_STARTUP

#define LOGREGISTRY(msg) LogRegistryEvent(msg)

// for use when monitoring process
static void LogRegistryEvent(const wchar_t *msg)
{
  HKEY dummyKey;
  HRESULT hr;
  wchar_t buf[512];

  wsprintf(buf, L" log %s", msg);
  hr = RegOpenKeyEx(HKEY_LOCAL_MACHINE, buf, 0, KEY_READ, &dummyKey);
  if (SUCCEEDED(hr)) {
    RegCloseKey(dummyKey);
  }
}
#else

#define LOGREGISTRY(msg)

#endif

static DWORD WINAPI InitDwriteBG(LPVOID lpdwThreadParam)
{
  SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
  LOGREGISTRY(L"loading dwrite.dll");
  HMODULE dwdll = LoadLibraryW(L"dwrite.dll");
  if (dwdll) {
    decltype(DWriteCreateFactory)* createDWriteFactory = (decltype(DWriteCreateFactory)*)
      GetProcAddress(dwdll, "DWriteCreateFactory");
    if (createDWriteFactory) {
      LOGREGISTRY(L"creating dwrite factory");
      IDWriteFactory *factory;
      HRESULT hr = createDWriteFactory(
        DWRITE_FACTORY_TYPE_SHARED,
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
class XREMain
{
public:
  XREMain() :
    mStartOffline(false)
    , mShuttingDown(false)
#ifdef MOZ_ENABLE_XREMOTE
    , mDisableRemote(false)
#endif
#if defined(MOZ_WIDGET_GTK)
    , mGdkDisplay(nullptr)
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
  nsCOMPtr<nsIToolkitProfileService> mProfileSvc;
  nsCOMPtr<nsIFile> mProfD;
  nsCOMPtr<nsIFile> mProfLD;
  nsCOMPtr<nsIProfileLock> mProfileLock;
#ifdef MOZ_ENABLE_XREMOTE
  nsCOMPtr<nsIRemoteService> mRemoteService;
  nsProfileLock mRemoteLock;
  nsCOMPtr<nsIFile> mRemoteLockDir;
#endif

  UniquePtr<ScopedXPCOMStartup> mScopedXPCOM;
  UniquePtr<XREAppData> mAppData;

  nsXREDirProvider mDirProvider;
  nsAutoCString mProfileName;
  nsAutoCString mDesktopStartupID;

  bool mStartOffline;
  bool mShuttingDown;
#ifdef MOZ_ENABLE_XREMOTE
  bool mDisableRemote;
#endif

#if defined(MOZ_WIDGET_GTK)
  GdkDisplay* mGdkDisplay;
#endif
};

#if defined(XP_UNIX) && !defined(ANDROID)
static SmprintfPointer
FormatUid(uid_t aId)
{
  if (const auto pw = getpwuid(aId)) {
    return mozilla::Smprintf("%s", pw->pw_name);
  }
  return mozilla::Smprintf("uid %d", static_cast<int>(aId));
}

// Bug 1323302: refuse to run under sudo or similar.
static bool
CheckForUserMismatch()
{
  static char const * const kVars[] = {
    "HOME",
#ifdef MOZ_WIDGET_GTK
    "XDG_RUNTIME_DIR",
#endif
#ifdef MOZ_X11
    "XAUTHORITY",
#endif
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
          Output(true, "Running " MOZ_APP_DISPLAYNAME " as root in a regular"
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
#else // !XP_UNIX || ANDROID
static bool
CheckForUserMismatch()
{
  return false;
}
#endif

static void
IncreaseDescriptorLimits()
{
#ifdef XP_UNIX
  // Increase the fd limit to accomodate IPC resources like shared memory.
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
int
XREMain::XRE_mainInit(bool* aExitFlag)
{
  if (!aExitFlag)
    return 1;
  *aExitFlag = false;

  atexit(UnexpectedExit);
  auto expectedShutdown = mozilla::MakeScopeExit([&] {
    MozExpectedExit();
  });

  StartupTimeline::Record(StartupTimeline::MAIN);

  if (CheckForUserMismatch()) {
    return 1;
  }

  if (PR_GetEnv("MOZ_CHAOSMODE")) {
    ChaosFeature feature = ChaosFeature::Any;
    long featureInt = strtol(PR_GetEnv("MOZ_CHAOSMODE"), nullptr, 16);
    if (featureInt) {
      // NOTE: MOZ_CHAOSMODE=0 or a non-hex value maps to Any feature.
      feature = static_cast<ChaosFeature>(featureInt);
    }
    ChaosMode::SetChaosFeature(feature);
  }

  if (ChaosMode::isActive(ChaosFeature::Any)) {
    printf_stderr("*** You are running in chaos test mode. See ChaosMode.h. ***\n");
  }

  if (CheckArg("headless") || CheckArgExists("screenshot")) {
    PR_SetEnv("MOZ_HEADLESS=1");
  }

  if (gfxPlatform::IsHeadless()) {
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) || defined(XP_MACOSX)
    printf_stderr("*** You are running in headless mode.\n");
#else
    Output(true, "Error: headless mode is not currently supported on this platform.\n");
    return 1;
#endif

#ifdef XP_MACOSX
    // To avoid taking focus when running in headless mode immediately
    // transition Firefox to a background application.
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    OSStatus transformStatus = TransformProcessType(&psn, kProcessTransformToBackgroundApplication);
    if (transformStatus != noErr) {
      NS_ERROR("Failed to make process a background application.");
      return 1;
    }
#endif

  }

  nsresult rv;
  ArgResult ar;

#ifdef DEBUG
  if (PR_GetEnv("XRE_MAIN_BREAK"))
    NS_BREAK();
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
  const char *home = PR_GetEnv("HOME");
  if (!home || !*home) {
    struct passwd *pw = getpwuid(geteuid());
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
  ar = CheckArg("override", &override, CheckArgFlag::CheckOSInt | CheckArgFlag::RemoveArg);
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
    Output(true, "Error: Platform version '%s' is not compatible with\n"
           "minVersion >= %s\nmaxVersion <= %s\n",
           (const char*) gToolkitVersion, (const char*) mAppData->minVersion,
           (const char*) mAppData->maxVersion);
    return 1;
  }

  rv = mDirProvider.Initialize(mAppData->directory, mAppData->xreDirectory);
  if (NS_FAILED(rv))
    return 1;

  if (EnvHasValue("MOZ_CRASHREPORTER")) {
    mAppData->flags |= NS_XRE_ENABLE_CRASH_REPORTER;
  }

  nsCOMPtr<nsIFile> xreBinDirectory;
  xreBinDirectory = mDirProvider.GetGREBinDir();

  if ((mAppData->flags & NS_XRE_ENABLE_CRASH_REPORTER) &&
      NS_SUCCEEDED(
        CrashReporter::SetExceptionHandler(xreBinDirectory))) {
    nsCOMPtr<nsIFile> file;
    rv = nsXREDirProvider::GetUserAppDataDirectory(getter_AddRefs(file));
    if (NS_SUCCEEDED(rv)) {
      CrashReporter::SetUserAppDataDirectory(file);
    }
    if (mAppData->crashReporterURL)
      CrashReporter::SetServerURL(nsDependentCString(mAppData->crashReporterURL));

    // We overwrite this once we finish starting up.
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("StartupCrash"),
                                       NS_LITERAL_CSTRING("1"));

    // pass some basic info from the app data
    if (mAppData->vendor)
      CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("Vendor"),
                                         nsDependentCString(mAppData->vendor));
    if (mAppData->name)
      CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("ProductName"),
                                         nsDependentCString(mAppData->name));
    if (mAppData->ID)
      CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("ProductID"),
                                         nsDependentCString(mAppData->ID));
    if (mAppData->version)
      CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("Version"),
                                         nsDependentCString(mAppData->version));
    if (mAppData->buildID)
      CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("BuildID"),
                                         nsDependentCString(mAppData->buildID));

    nsDependentCString releaseChannel(NS_STRINGIFY(MOZ_UPDATE_CHANNEL));
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("ReleaseChannel"),
                                       releaseChannel);
#ifdef MOZ_LINKER
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("CrashAddressLikelyWrong"),
                                       IsSignalHandlingBroken() ? NS_LITERAL_CSTRING("1")
                                                                : NS_LITERAL_CSTRING("0"));
#endif

#ifdef XP_WIN
    nsAutoString appInitDLLs;
    if (widget::WinUtils::GetAppInitDLLs(appInitDLLs)) {
      CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("AppInitDLLs"),
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

      // see if we have a crashreporter-override.ini in the application directory
      nsCOMPtr<nsIFile> overrideini;
      bool exists;
      if (NS_SUCCEEDED(mDirProvider.GetAppDir()->Clone(getter_AddRefs(overrideini))) &&
          NS_SUCCEEDED(overrideini->AppendNative(NS_LITERAL_CSTRING("crashreporter-override.ini"))) &&
          NS_SUCCEEDED(overrideini->Exists(&exists)) &&
          exists) {
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
#if defined(MOZ_CONTENT_SANDBOX)
    // If we're sandboxing content and we fail to initialize, then crashing here
    // seems like the sensible option.
    if (BrowserTabsRemoteAutostart()) {
      MOZ_CRASH("Failed to initialize broker services, can't continue.");
    }
#endif
    // Otherwise just warn for the moment, as most things will work.
    NS_WARNING("Failed to initialize broker services, sandboxed processes will "
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
    const EventTypeSpec kFakeEventList[] = { { INT_MAX, INT_MAX } };
    EventRef event;
    ::ReceiveNextEvent(GetEventTypeCount(kFakeEventList), kFakeEventList,
                       kEventDurationNoWait, false, &event);
  }

  if (CheckArg("foreground")) {
    // The original process communicates that it was in the foreground by
    // adding this argument.  This new process, which is taking over for
    // the old one, should make itself the active application.
    ProcessSerialNumber psn;
    if (::GetCurrentProcess(&psn) == noErr)
      ::SetFrontProcess(&psn);
  }
#endif

  SaveToEnv("MOZ_LAUNCHED_CHILD=");

  // On Windows, the -os-restarted command line switch lets us know when we are
  // restarted via RegisterApplicationRestart. May be used for other OSes later.
  if (CheckArg("os-restarted", nullptr, CheckArgFlag::RemoveArg) == ARG_FOUND) {
    gRestartedByOS = true;
  }

  gRestartArgc = gArgc;
  gRestartArgv = (char**) malloc(sizeof(char*) * (gArgc + 1 + (override ? 2 : 0)));
  if (!gRestartArgv) {
    return 1;
  }

  int i;
  for (i = 0; i < gArgc; ++i) {
    gRestartArgv[i] = gArgv[i];
  }

  // Add the -override argument back (it is removed automatically be CheckArg) if there is one
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

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyName , 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {

      DWORD updateRevision[2];
      DWORD len = sizeof(updateRevision);
      DWORD vtype;

      // Windows 7 uses "Update Signature", 8 uses "Update Revision".
      // For AMD CPUs, "CurrentPatchLevel" is sometimes used.
      // Take the first one we find.
      LPCWSTR choices[] = {L"Update Signature", L"Update Revision", L"CurrentPatchLevel"};
      for (size_t oneChoice=0; oneChoice<ArrayLength(choices); oneChoice++) {
        if (RegQueryValueExW(key, choices[oneChoice],
                             0, &vtype,
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
      CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("CPUMicrocodeVersion"),
                                         nsPrintfCString("0x%x",
                                                         cpuUpdateRevision));
    }
  }
#endif

    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("SafeMode"),
                                       gSafeMode ? NS_LITERAL_CSTRING("1") :
                                                   NS_LITERAL_CSTRING("0"));

  // Handle --no-remote and --new-instance command line arguments. Setup
  // the environment to better accommodate other components and various
  // restart scenarios.
  ar = CheckArg("no-remote", nullptr, CheckArgFlag::CheckOSInt | CheckArgFlag::RemoveArg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument --no-remote is invalid when argument --osint is specified\n");
    return 1;
  }
  if (ar == ARG_FOUND) {
    SaveToEnv("MOZ_NO_REMOTE=1");
  }

  ar = CheckArg("new-instance", nullptr, CheckArgFlag::CheckOSInt | CheckArgFlag::RemoveArg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument --new-instance is invalid when argument --osint is specified\n");
    return 1;
  }
  if (ar == ARG_FOUND) {
    SaveToEnv("MOZ_NEW_INSTANCE=1");
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

  // Check for --register, which registers chrome and then exits immediately.
  ar = CheckArg("register", nullptr, CheckArgFlag::CheckOSInt | CheckArgFlag::RemoveArg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument --register is invalid when argument --osint is specified\n");
    return 1;
  }
  if (ar == ARG_FOUND) {
    ScopedXPCOMStartup xpcom;
    rv = xpcom.Initialize();
    NS_ENSURE_SUCCESS(rv, 1);
    {
      nsCOMPtr<nsIChromeRegistry> chromeReg =
        mozilla::services::GetChromeRegistryService();
      NS_ENSURE_TRUE(chromeReg, 1);

      chromeReg->CheckForNewChrome();
    }
    *aExitFlag = true;
    return 0;
  }

  return 0;
}

#ifdef XP_WIN
/**
 * Uses WMI to read some manufacturer information that may be useful for
 * diagnosing hardware-specific crashes. This function is best-effort; failures
 * shouldn't burden the caller. COM must be initialized before calling.
 */
static void AnnotateSystemManufacturer()
{
  RefPtr<IWbemLocator> locator;

  HRESULT hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IWbemLocator, getter_AddRefs(locator));

  if (FAILED(hr)) {
    return;
  }

  RefPtr<IWbemServices> services;

  hr = locator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr,
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

  RefPtr<IEnumWbemClassObject> enumerator;

  hr = services->ExecQuery(_bstr_t(L"WQL"), _bstr_t(L"SELECT * FROM Win32_BIOS"),
                           WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                           nullptr, getter_AddRefs(enumerator));

  if (FAILED(hr) || !enumerator) {
    return;
  }

  RefPtr<IWbemClassObject> classObject;
  ULONG results;

  hr = enumerator->Next(WBEM_INFINITE, 1, getter_AddRefs(classObject), &results);

  if (FAILED(hr) || results == 0) {
    return;
  }

  VARIANT value;
  VariantInit(&value);

  hr = classObject->Get(L"Manufacturer", 0, &value, 0, 0);

  if (SUCCEEDED(hr) && V_VT(&value) == VT_BSTR) {
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("BIOS_Manufacturer"),
                                       NS_ConvertUTF16toUTF8(V_BSTR(&value)));
  }

  VariantClear(&value);
}

static void PR_CALLBACK AnnotateSystemManufacturer_ThreadStart(void*)
{
  HRESULT hr = CoInitialize(nullptr);

  if (FAILED(hr)) {
    return;
  }

  AnnotateSystemManufacturer();

  CoUninitialize();
}
#endif // XP_WIN

#if defined(XP_LINUX) && !defined(ANDROID)

static void
AnnotateLSBRelease(void*)
{
  nsCString dist, desc, release, codename;
  if (widget::lsb::GetLSBRelease(dist, desc, release, codename)) {
    CrashReporter::AppendAppNotesToCrashReport(desc);
  }
}

#endif // defined(XP_LINUX) && !defined(ANDROID)

#ifdef XP_WIN
static void ReadAheadDll(const wchar_t* dllName) {
  wchar_t dllPath[MAX_PATH];
  if (ConstructSystem32Path(dllName, dllPath, MAX_PATH)) {
    ReadAheadLib(dllPath);
  }
}

static void PR_CALLBACK ReadAheadDlls_ThreadStart(void *) {
  // Load DataExchange.dll and twinapi.appcore.dll for nsWindow::EnableDragDrop
  ReadAheadDll(L"DataExchange.dll");
  ReadAheadDll(L"twinapi.appcore.dll");

  // Load twinapi.dll for WindowsUIUtils::UpdateTabletModeState
  ReadAheadDll(L"twinapi.dll");

  // Load explorerframe.dll for WinTaskbar::Initialize
  ReadAheadDll(L"ExplorerFrame.dll");
}
#endif

namespace mozilla {
  ShutdownChecksMode gShutdownChecks = SCM_NOTHING;
} // namespace mozilla

static void SetShutdownChecks() {
  // Set default first. On debug builds we crash. On nightly and local
  // builds we record. Nightlies will then send the info via telemetry,
  // but it is usefull to have the data in about:telemetry in local builds
  // too.

#ifdef DEBUG
#if defined(MOZ_CODE_COVERAGE)
  gShutdownChecks = SCM_NOTHING;
#else
  gShutdownChecks = SCM_CRASH;
#endif // MOZ_CODE_COVERAGE
#else
  const char* releaseChannel = NS_STRINGIFY(MOZ_UPDATE_CHANNEL);
  if (strcmp(releaseChannel, "nightly") == 0 ||
      strcmp(releaseChannel, "default") == 0) {
    gShutdownChecks = SCM_RECORD;
  } else {
    gShutdownChecks = SCM_NOTHING;
  }
#endif // DEBUG

  // We let an environment variable override the default so that addons
  // authors can use it for debugging shutdown with released firefox versions.
  const char* mozShutdownChecksEnv = PR_GetEnv("MOZ_SHUTDOWN_CHECKS");
  if (mozShutdownChecksEnv) {
    if (strcmp(mozShutdownChecksEnv, "crash") == 0) {
      gShutdownChecks = SCM_CRASH;
    } else if (strcmp(mozShutdownChecksEnv, "record") == 0) {
      gShutdownChecks = SCM_RECORD;
    } else if (strcmp(mozShutdownChecksEnv, "nothing") == 0) {
      gShutdownChecks = SCM_NOTHING;
    }
  }

}

namespace mozilla {
namespace startup {
  Result<nsCOMPtr<nsIFile>, nsresult>
  GetIncompleteStartupFile(nsIFile* aProfLD)
  {
    nsCOMPtr<nsIFile> crashFile;
    MOZ_TRY(aProfLD->Clone(getter_AddRefs(crashFile)));
    MOZ_TRY(crashFile->Append(FILE_STARTUP_INCOMPLETE));
    return std::move(crashFile);
  }
}
}

// Check whether the last startup attempt resulted in a crash within the
// last 6 hours.
// Note that this duplicates the logic in nsAppStartup::TrackStartupCrashBegin,
// which runs too late for our purposes.
Result<bool, nsresult>
XREMain::CheckLastStartupWasCrash()
{
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
int
XREMain::XRE_mainStartup(bool* aExitFlag)
{
  nsresult rv;

  if (!aExitFlag)
    return 1;
  *aExitFlag = false;

  SetShutdownChecks();

  // Enable Telemetry IO Reporting on DEBUG, nightly and local builds
#ifdef DEBUG
  mozilla::Telemetry::InitIOReporting(gAppData->xreDirectory);
#else
  {
    const char* releaseChannel = NS_STRINGIFY(MOZ_UPDATE_CHANNEL);
    if (strcmp(releaseChannel, "nightly") == 0 ||
        strcmp(releaseChannel, "default") == 0) {
      mozilla::Telemetry::InitIOReporting(gAppData->xreDirectory);
    }
  }
#endif /* DEBUG */

#if defined(XP_WIN)
  // Enable the HeapEnableTerminationOnCorruption exploit mitigation. We ignore
  // the return code because it always returns success, although it has no
  // effect on Windows older than XP SP3.
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
#endif /* XP_WIN */

#if defined(MOZ_WIDGET_GTK) || defined(MOZ_ENABLE_XREMOTE)
  // Stash DESKTOP_STARTUP_ID in malloc'ed memory because gtk_init will clear it.
#define HAVE_DESKTOP_STARTUP_ID
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

#if defined(MOZ_WIDGET_GTK) && defined(MOZ_X11)
  // Disable XInput2 multidevice support due to focus bugginess.
  // See bugs 1182700, 1170342.
  // gdk_disable_multidevice() affects Gdk X11 backend only,
  // the multidevice support is always enabled on Wayland backend.
  const char* useXI2 = PR_GetEnv("MOZ_USE_XINPUT2");
  if (!useXI2 || (*useXI2 == '0'))
    gdk_disable_multidevice();
#endif

  // Open the display ourselves instead of using gtk_init, so that we can
  // close it without fear that one day gtk might clean up the display it
  // opens.
  if (!gtk_parse_args(&gArgc, &gArgv))
    return 1;
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

#ifdef MOZ_X11
  // Init X11 in thread-safe mode. Must be called prior to the first call to XOpenDisplay
  // (called inside gdk_display_open). This is a requirement for off main tread compositing.
  if (!gfxPlatform::IsHeadless()) {
    XInitThreads();
  }
#endif
#if defined(MOZ_WIDGET_GTK)
  if (!gfxPlatform::IsHeadless()) {
    const char *display_name = nullptr;
    bool saveDisplayArg = false;

    // display_name is owned by gdk.
    display_name = gdk_get_display_arg_name();
    // if --display argument is given make sure it's
    // also passed to ContentChild::Init() by MOZ_GDK_DISPLAY.
    if (display_name) {
      SaveWordToEnv("MOZ_GDK_DISPLAY", nsDependentCString(display_name));
      saveDisplayArg = true;
    }

    // On Wayland disabled builds read X11 DISPLAY env exclusively
    // and don't care about different displays.
#if !defined(MOZ_WAYLAND)
    if (!display_name) {
      display_name = PR_GetEnv("DISPLAY");
      if (!display_name) {
        PR_fprintf(PR_STDERR,
                   "Error: no DISPLAY environment variable specified\n");
        return 1;
      }
    }
#endif

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
#ifdef MOZ_WAYLAND
        else if (GDK_IS_WAYLAND_DISPLAY(mGdkDisplay)) {
            SaveWordToEnv("WAYLAND_DISPLAY", nsDependentCString(display_name));
        }
#endif
      }
    }
#ifdef MOZ_WIDGET_GTK
    else {
      mGdkDisplay = gdk_display_manager_open_display(gdk_display_manager_get(),
                                                     nullptr);
    }
#endif
  }
  else {
    mDisableRemote = true;
  }
#endif
#ifdef MOZ_ENABLE_XREMOTE
  // handle --remote now that xpcom is fired up
  bool newInstance;
  {
    char *e = PR_GetEnv("MOZ_NO_REMOTE");
    mDisableRemote = (mDisableRemote || (e && *e));
    if (mDisableRemote) {
      newInstance = true;
    } else {
      e = PR_GetEnv("MOZ_NEW_INSTANCE");
      newInstance = (e && *e);
    }
  }

  if (!newInstance) {
    nsAutoCString program(gAppData->remotingName);
    ToLowerCase(program);

    const char* username = getenv("LOGNAME");
    const char* profile  = nullptr;

    RemoteResult rr = ParseRemoteCommandLine(program, &profile, &username);
    if (rr == REMOTE_ARG_BAD) {
      return 1;
    }

    if (!username) {
      struct passwd *pw = getpwuid(geteuid());
      if (pw && pw->pw_name) {
        // Beware that another call to getpwent/getpwname/getpwuid will overwrite
        // pw, but we don't have such another call between here and when username
        // is used last.
        username = pw->pw_name;
      }
    }

    nsCOMPtr<nsIFile> mutexDir;
    rv = GetSpecialSystemDirectory(OS_TemporaryDirectory, getter_AddRefs(mutexDir));
    if (NS_SUCCEEDED(rv)) {
      nsAutoCString mutexPath = program + NS_LITERAL_CSTRING("_");
      // In the unlikely even that LOGNAME is not set and getpwuid failed, just
      // don't put the username in the mutex directory. It will conflict with
      // other users mutex, but the worst that can happen is that they wait for
      // MOZ_XREMOTE_START_TIMEOUT_SEC during startup in that case.
      if (username) {
        mutexPath.Append(username);
      }
      if (profile) {
        mutexPath.Append(NS_LITERAL_CSTRING("_") + nsDependentCString(profile));
      }
      mutexDir->AppendNative(mutexPath);

      rv = mutexDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
      if (NS_SUCCEEDED(rv) || rv == NS_ERROR_FILE_ALREADY_EXISTS) {
        mRemoteLockDir = mutexDir;
      }
    }

    if (mRemoteLockDir) {
      const TimeStamp epoch = mozilla::TimeStamp::Now();
      do {
        rv = mRemoteLock.Lock(mRemoteLockDir, nullptr);
        if (NS_SUCCEEDED(rv))
          break;
        sched_yield();
      } while ((TimeStamp::Now() - epoch)
               < TimeDuration::FromSeconds(MOZ_XREMOTE_START_TIMEOUT_SEC));
      if (NS_FAILED(rv)) {
        NS_WARNING("Cannot lock XRemote start mutex");
      }
    }

    // Try to remote the entire command line. If this fails, start up normally.
    const char* desktopStartupIDPtr =
      mDesktopStartupID.IsEmpty() ? nullptr : mDesktopStartupID.get();

    rr = StartRemoteClient(desktopStartupIDPtr, program, profile, username);
    if (rr == REMOTE_FOUND) {
      *aExitFlag = true;
      return 0;
    }
    if (rr == REMOTE_ARG_BAD) {
      return 1;
    }
  }
#endif
#if defined(MOZ_WIDGET_GTK)
  g_set_application_name(mAppData->name);
  gtk_window_set_auto_startup_notification(false);

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
  if (NS_FAILED(rv))
    return 1;

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

#if defined(MOZ_UPDATER) && !defined(MOZ_WIDGET_ANDROID)
  // Check for and process any available updates
  nsCOMPtr<nsIFile> updRoot;
  bool persistent;
  rv = mDirProvider.GetFile(XRE_UPDATE_ROOT_DIR, &persistent,
                            getter_AddRefs(updRoot));
  // XRE_UPDATE_ROOT_DIR may fail. Fallback to appDir if failed
  if (NS_FAILED(rv))
    updRoot = mDirProvider.GetAppDir();

  // If the MOZ_TEST_PROCESS_UPDATES environment variable already exists, then
  // we are being called from the callback application.
  if (EnvHasValue("MOZ_TEST_PROCESS_UPDATES")) {
    // If the caller has asked us to log our arguments, do so.  This is used
    // to make sure that the maintenance service successfully launches the
    // callback application.
    const char *logFile = nullptr;
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
  ProcessUpdates(mDirProvider.GetGREDir(),
                 exeDir,
                 updRoot,
                 gRestartArgc,
                 gRestartArgv,
                 mAppData->version);
  if (EnvHasValue("MOZ_TEST_PROCESS_UPDATES")) {
    SaveToEnv("MOZ_TEST_PROCESS_UPDATES=");
    *aExitFlag = true;
    return 0;
  }
#endif

  rv = NS_NewToolkitProfileService(getter_AddRefs(mProfileSvc));
  if (rv == NS_ERROR_FILE_ACCESS_DENIED) {
    PR_fprintf(PR_STDERR, "Error: Access was denied while trying to open files in " \
                "your profile directory.\n");
  }
  if (NS_FAILED(rv)) {
    // We failed to choose or create profile - notify user and quit
    ProfileMissingDialog(mNativeApp);
    return 1;
  }

  rv = SelectProfile(getter_AddRefs(mProfileLock), mProfileSvc, mNativeApp, &mStartOffline,
                      &mProfileName);
  if (rv == NS_ERROR_LAUNCHED_CHILD_PROCESS ||
      rv == NS_ERROR_ABORT) {
    *aExitFlag = true;
    return 0;
  }

  if (NS_FAILED(rv)) {
    // We failed to choose or create profile - notify user and quit
    ProfileMissingDialog(mNativeApp);
    return 1;
  }
  gProfileLock = mProfileLock;

  rv = mProfileLock->GetDirectory(getter_AddRefs(mProfD));
  NS_ENSURE_SUCCESS(rv, 1);

  rv = mProfileLock->GetLocalDirectory(getter_AddRefs(mProfLD));
  NS_ENSURE_SUCCESS(rv, 1);

  rv = mDirProvider.SetProfile(mProfD, mProfLD);
  NS_ENSURE_SUCCESS(rv, 1);

  //////////////////////// NOW WE HAVE A PROFILE ////////////////////////

  mozilla::Telemetry::SetProfileDir(mProfD);

  if (mAppData->flags & NS_XRE_ENABLE_CRASH_REPORTER)
      MakeOrSetMinidumpPath(mProfD);

  CrashReporter::SetProfileDirectory(mProfD);

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
  rv = NS_ERROR_FILE_NOT_FOUND;
  if (mAppData->directory) {
    rv = mAppData->directory->Clone(getter_AddRefs(flagFile));
  }
  if (flagFile) {
    flagFile->AppendNative(FILE_INVALIDATE_CACHES);
  }

  bool cachesOK;
  bool versionOK = CheckCompatibility(mProfD, version, osABI,
                                      mDirProvider.GetGREDir(),
                                      mAppData->directory, flagFile,
                                      &cachesOK);

  bool lastStartupWasCrash = CheckLastStartupWasCrash().unwrapOr(false);

  if (CheckArg("purgecaches") || PR_GetEnv("MOZ_PURGE_CACHES") ||
      lastStartupWasCrash) {
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
  if (gSafeMode) {
    startupCacheValid = RemoveComponentRegistries(mProfD, mProfLD, false);
    WriteVersion(mProfD, NS_LITERAL_CSTRING("Safe Mode"), osABI,
                 mDirProvider.GetGREDir(), mAppData->directory, !startupCacheValid);
  }
  else if (versionOK) {
    if (!cachesOK) {
      // Remove caches, forcing component re-registration.
      // The new list of additional components directories is derived from
      // information in "extensions.ini".
      startupCacheValid = RemoveComponentRegistries(mProfD, mProfLD, false);

      // Rewrite compatibility.ini to remove the flag
      WriteVersion(mProfD, version, osABI,
                   mDirProvider.GetGREDir(), mAppData->directory, !startupCacheValid);
    }
    // Nothing need be done for the normal startup case.
  }
  else {
    // Remove caches, forcing component re-registration
    // with the default set of components (this disables any potentially
    // troublesome incompatible XPCOM components).
    startupCacheValid = RemoveComponentRegistries(mProfD, mProfLD, true);

    // Write out version
    WriteVersion(mProfD, version, osABI,
                 mDirProvider.GetGREDir(), mAppData->directory, !startupCacheValid);
  }

  if (!startupCacheValid)
    StartupCache::IgnoreDiskCache();

  if (flagFile) {
    flagFile->Remove(true);
  }

  return 0;
}

#if defined(MOZ_CONTENT_SANDBOX)
void AddSandboxAnnotations()
{
  // Include the sandbox content level, regardless of platform
  int level = GetEffectiveContentSandboxLevel();

  nsAutoCString levelString;
  levelString.AppendInt(level);

  CrashReporter::AnnotateCrashReport(
    NS_LITERAL_CSTRING("ContentSandboxLevel"), levelString);

  // Include whether or not this instance is capable of content sandboxing
  bool sandboxCapable = false;

#if defined(XP_WIN)
  // All supported Windows versions support some level of content sandboxing
  sandboxCapable = true;
#elif defined(XP_MACOSX)
  // All supported OS X versions are capable
  sandboxCapable = true;
#elif defined(XP_LINUX)
  sandboxCapable = SandboxInfo::Get().CanSandboxContent();
#endif

  CrashReporter::AnnotateCrashReport(
    NS_LITERAL_CSTRING("ContentSandboxCapable"),
    sandboxCapable ? NS_LITERAL_CSTRING("1") : NS_LITERAL_CSTRING("0"));
}
#endif /* MOZ_CONTENT_SANDBOX */

/*
 * XRE_mainRun - Command line startup, profile migration, and
 * the calling of appStartup->Run().
 */
nsresult
XREMain::XRE_mainRun()
{
  nsresult rv = NS_OK;
  NS_ASSERTION(mScopedXPCOM, "Scoped xpcom not initialized.");

#if defined(XP_WIN)
  RefPtr<mozilla::DllServices> dllServices(mozilla::DllServices::Get());
  auto dllServicesDisable = MakeScopeExit([&dllServices]() {
    dllServices->Disable();
  });
#endif // defined(XP_WIN)

#ifdef NS_FUNCTION_TIMER
  // initialize some common services, so we don't pay the cost for these at odd times later on;
  // SetWindowCreator -> ChromeRegistry -> IOService -> SocketTransportService -> (nspr wspm init), Prefs
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
  nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIPrefBranch> defaultPrefBranch;
    rv = prefs->GetDefaultBranch(nullptr, getter_AddRefs(defaultPrefBranch));

    if (NS_SUCCEEDED(rv)) {
      nsAutoCString sval;
      rv = defaultPrefBranch->GetCharPref("app.update.channel", sval);
      if (NS_SUCCEEDED(rv)) {
        CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("ReleaseChannel"),
                                            sval);
      }
    }
  }
  // Needs to be set after xpcom initialization.
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("FramePoisonBase"),
                                     nsPrintfCString("%.16" PRIu64, uint64_t(gMozillaPoisonBase)));
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("FramePoisonSize"),
                                     nsPrintfCString("%" PRIu32, uint32_t(gMozillaPoisonSize)));

  bool includeContextHeap =
    Preferences::GetBool("toolkit.crashreporter.include_context_heap", false);
  CrashReporter::SetIncludeContextHeap(includeContextHeap);

#ifdef XP_WIN
  PR_CreateThread(PR_USER_THREAD, AnnotateSystemManufacturer_ThreadStart, 0,
                  PR_PRIORITY_LOW, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD, 0);
#endif

#if defined(XP_LINUX) && !defined(ANDROID)
  PR_CreateThread(PR_USER_THREAD, AnnotateLSBRelease, 0, PR_PRIORITY_LOW,
                  PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD, 0);
#endif

  if (mStartOffline) {
    nsCOMPtr<nsIIOService> io(do_GetService("@mozilla.org/network/io-service;1"));
    NS_ENSURE_TRUE(io, NS_ERROR_FAILURE);
    io->SetManageOfflineStatus(false);
    io->SetOffline(true);
  }


#ifdef XP_WIN
  if (!PR_GetEnv("XRE_NO_DLL_READAHEAD"))
  {
    PR_CreateThread(PR_USER_THREAD, ReadAheadDlls_ThreadStart, 0,
                    PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                    PR_UNJOINABLE_THREAD, 0);
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

  {
    nsCOMPtr<nsIToolkitProfile> profileBeingReset;
    bool profileWasSelected = false;
    if (gDoProfileReset) {
      if (gResetOldProfileName.IsEmpty()) {
        NS_WARNING("Not resetting profile as the profile has no name.");
        gDoProfileReset = false;
      } else {
        rv = mProfileSvc->GetProfileByName(gResetOldProfileName,
                                           getter_AddRefs(profileBeingReset));
        if (NS_FAILED(rv)) {
          gDoProfileReset = false;
          return NS_ERROR_FAILURE;
        }

        nsCOMPtr<nsIToolkitProfile> defaultProfile;
        // This can fail if there is no default profile.
        // That shouldn't stop reset from proceeding.
        nsresult gotSelected = mProfileSvc->GetSelectedProfile(getter_AddRefs(defaultProfile));
        if (NS_SUCCEEDED(gotSelected)) {
          profileWasSelected = defaultProfile == profileBeingReset;
        }
      }
    }

    // Profile Migration
    if (mAppData->flags & NS_XRE_ENABLE_PROFILE_MIGRATOR && gDoMigration) {
      gDoMigration = false;
      nsCOMPtr<nsIProfileMigrator> pm(do_CreateInstance(NS_PROFILEMIGRATOR_CONTRACTID));
      if (pm) {
        nsAutoCString aKey;
        if (gDoProfileReset) {
          // Automatically migrate from the current application if we just
          // reset the profile.
          aKey = MOZ_APP_NAME;
        }
        pm->Migrate(&mDirProvider, aKey, gResetOldProfileName);
      }
    }

    if (gDoProfileReset) {
      nsresult backupCreated = ProfileResetCleanup(profileBeingReset);
      if (NS_FAILED(backupCreated)) NS_WARNING("Could not cleanup the profile that was reset");

      nsCOMPtr<nsIToolkitProfile> newProfile;
      rv = GetCurrentProfile(mProfileSvc, mProfD, getter_AddRefs(newProfile));
      if (NS_SUCCEEDED(rv)) {
        newProfile->SetName(gResetOldProfileName);
        mProfileName.Assign(gResetOldProfileName);
        // Set the new profile as the default after we're done cleaning up the old profile,
        // iff that profile was already the default
        if (profileWasSelected) {
          rv = mProfileSvc->SetDefaultProfile(newProfile);
          if (NS_FAILED(rv)) NS_WARNING("Could not set current profile as the default");
        }
      } else {
        NS_WARNING("Could not find current profile to set as default / change name.");
      }

      // Need to write out the fact that the profile has been removed, the new profile
      // renamed, and potentially that the selected/default profile changed.
      mProfileSvc->Flush();
    }
  }

#ifndef XP_WIN
    nsCOMPtr<nsIFile> profileDir;
    nsAutoCString path;
    rv = mDirProvider.GetProfileStartupDir(getter_AddRefs(profileDir));
    if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(profileDir->GetNativePath(path)) && !IsUTF8(path)) {
      PR_fprintf(PR_STDERR, "Error: The profile path is not valid UTF-8. Unable to continue.\n");
      return NS_ERROR_FAILURE;
    }
#endif

  // Initialize user preferences before notifying startup observers so they're
  // ready in time for early consumers, such as the component loader.
  mDirProvider.InitializeUserPrefs();

  {
    nsCOMPtr<nsIObserver> startupNotifier
      (do_CreateInstance(NS_APPSTARTUPNOTIFIER_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    startupNotifier->Observe(nullptr, APPSTARTUP_TOPIC, nullptr);
  }

  nsCOMPtr<nsIAppStartup> appStartup
    (do_GetService(NS_APPSTARTUP_CONTRACTID));
  NS_ENSURE_TRUE(appStartup, NS_ERROR_FAILURE);

  mDirProvider.DoStartup();

  OverrideDefaultLocaleIfNeeded();

  nsCString userAgentLocale;
  LocaleService::GetInstance()->GetAppLocaleAsLangTag(userAgentLocale);
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("useragent_locale"), userAgentLocale);

  appStartup->GetShuttingDown(&mShuttingDown);

  nsCOMPtr<nsICommandLineRunner> cmdLine;

  nsCOMPtr<nsIFile> workingDir;
  rv = NS_GetSpecialDirectory(NS_OS_CURRENT_WORKING_DIR, getter_AddRefs(workingDir));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  if (!mShuttingDown) {
    cmdLine = do_CreateInstance("@mozilla.org/toolkit/command-line;1");
    NS_ENSURE_TRUE(cmdLine, NS_ERROR_FAILURE);

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

#if defined(MOZ_SANDBOX)
  // Call SandboxBroker to initialize things that depend on Gecko machinery like
  // the directory provider.
  SandboxBroker::GeckoDependentInitialize();
#endif
#endif

  SaveStateForAppInitiatedRestart();

  // clear out any environment variables which may have been set
  // during the relaunch process now that we know we won't be relaunching.
  SaveToEnv("XRE_PROFILE_PATH=");
  SaveToEnv("XRE_PROFILE_LOCAL_PATH=");
  SaveToEnv("XRE_PROFILE_NAME=");
  SaveToEnv("XRE_START_OFFLINE=");
  SaveToEnv("NO_EM_RESTART=");
  SaveToEnv("XUL_APP_FILE=");
  SaveToEnv("XRE_BINARY_PATH=");

  if (!mShuttingDown) {
    rv = appStartup->CreateHiddenWindow();
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

#ifdef XP_WIN
    Preferences::RegisterCallbackAndCall(RegisterApplicationRestartChanged,
                                         PREF_WIN_REGISTER_APPLICATION_RESTART);
#endif

#if defined(HAVE_DESKTOP_STARTUP_ID) && defined(MOZ_WIDGET_GTK)
    nsGTKToolkit* toolkit = nsGTKToolkit::GetToolkit();
    if (toolkit && !mDesktopStartupID.IsEmpty()) {
      toolkit->SetDesktopStartupID(mDesktopStartupID);
    }
    // Clear the environment variable so it won't be inherited by
    // child processes and confuse things.
    g_unsetenv ("DESKTOP_STARTUP_ID");
#endif

#ifdef XP_MACOSX
    // we re-initialize the command-line service and do appleevents munging
    // after we are sure that we're not restarting
    cmdLine = do_CreateInstance("@mozilla.org/toolkit/command-line;1");
    NS_ENSURE_TRUE(cmdLine, NS_ERROR_FAILURE);

    CommandLineServiceMac::SetupMacCommandLine(gArgc, gArgv, false);

    rv = cmdLine->Init(gArgc, gArgv,
                        workingDir, nsICommandLine::STATE_INITIAL_LAUNCH);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
#endif

    nsCOMPtr<nsIObserverService> obsService =
      mozilla::services::GetObserverService();
    if (obsService)
      obsService->NotifyObservers(nullptr, "final-ui-startup", nullptr);

    (void)appStartup->DoneStartingUp();

    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("StartupCrash"),
                                       NS_LITERAL_CSTRING("0"));

    appStartup->GetShuttingDown(&mShuttingDown);
  }

  if (!mShuttingDown) {
    rv = cmdLine->Run();
    NS_ENSURE_SUCCESS_LOG(rv, NS_ERROR_FAILURE);

    appStartup->GetShuttingDown(&mShuttingDown);
  }

  if (!mShuttingDown) {
#ifdef MOZ_ENABLE_XREMOTE
    // if we have X remote support, start listening for requests on the
    // proxy window.
    if (!mDisableRemote)
      mRemoteService = do_GetService("@mozilla.org/toolkit/remote-service;1");
    if (mRemoteService)
      mRemoteService->Startup(mAppData->remotingName, mProfileName.get());
    if (mRemoteLockDir) {
      mRemoteLock.Unlock();
      mRemoteLock.Cleanup();
      mRemoteLockDir->Remove(false);
    }
#endif /* MOZ_ENABLE_XREMOTE */

    mNativeApp->Enable();
  }

#ifdef MOZ_INSTRUMENT_EVENT_LOOP
  if (PR_GetEnv("MOZ_INSTRUMENT_EVENT_LOOP")) {
    bool logToConsole = true;
    mozilla::InitEventTracing(logToConsole);
  }
#endif /* MOZ_INSTRUMENT_EVENT_LOOP */

#if defined(MOZ_SANDBOX) && defined(XP_LINUX)
  // If we're on Linux, we now have information about the OS capabilities
  // available to us.
  SandboxInfo sandboxInfo = SandboxInfo::Get();
  Telemetry::Accumulate(Telemetry::SANDBOX_HAS_SECCOMP_BPF,
                        sandboxInfo.Test(SandboxInfo::kHasSeccompBPF));
  Telemetry::Accumulate(Telemetry::SANDBOX_HAS_SECCOMP_TSYNC,
                        sandboxInfo.Test(SandboxInfo::kHasSeccompTSync));
  Telemetry::Accumulate(Telemetry::SANDBOX_HAS_USER_NAMESPACES_PRIVILEGED,
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
    NS_LITERAL_CSTRING("ContentSandboxCapabilities"), flagsString);
#endif /* MOZ_SANDBOX && XP_LINUX */

#if defined(MOZ_CONTENT_SANDBOX)
  AddSandboxAnnotations();
#endif /* MOZ_CONTENT_SANDBOX */

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
int
XREMain::XRE_main(int argc, char* argv[], const BootstrapConfig& aConfig)
{
  ScopedLogging log;

  mozilla::LogModule::Init(argc, argv);

#ifdef MOZ_CODE_COVERAGE
  CodeCoverageHandler::Init();
#endif

  AUTO_PROFILER_INIT;
  AUTO_PROFILER_LABEL("XREMain::XRE_main", OTHER);

  nsresult rv = NS_OK;

  gArgc = argc;
  gArgv = argv;

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
    nsCOMPtr<nsIFile> lf;
    rv = XRE_GetBinaryPath(getter_AddRefs(lf));
    if (NS_FAILED(rv))
      return 2;

    nsCOMPtr<nsIFile> greDir;
    rv = lf->GetParent(getter_AddRefs(greDir));
    if (NS_FAILED(rv))
      return 2;

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
  mozilla::mscom::MainThreadRuntime msCOMRuntime;
#endif

  // init
  bool exit = false;
  int result = XRE_mainInit(&exit);
  if (result != 0 || exit)
    return result;

  // If we exit gracefully, remove the startup crash canary file.
  auto cleanup = MakeScopeExit([&] () -> nsresult {
    if (mProfLD) {
      nsCOMPtr<nsIFile> crashFile;
      MOZ_TRY_VAR(crashFile, GetIncompleteStartupFile(mProfLD));
      crashFile->Remove(false);
    }
    return NS_OK;
  });

  // startup
  result = XRE_mainStartup(&exit);
  if (result != 0 || exit)
    return result;

  bool appInitiatedRestart = false;

  // Start the real application
  mScopedXPCOM = MakeUnique<ScopedXPCOMStartup>();
  if (!mScopedXPCOM)
    return 1;

  rv = mScopedXPCOM->Initialize();
  NS_ENSURE_SUCCESS(rv, 1);

  // run!
  rv = XRE_mainRun();

#ifdef MOZ_INSTRUMENT_EVENT_LOOP
  mozilla::ShutdownEventTracing();
#endif

  gAbsoluteArgv0Path.Truncate();

  // Check for an application initiated restart.  This is one that
  // corresponds to nsIAppStartup.quit(eRestart)
  if (rv == NS_SUCCESS_RESTART_APP
      || rv == NS_SUCCESS_RESTART_APP_NOT_SAME_PROFILE) {
    appInitiatedRestart = true;

    // We have an application restart don't do any shutdown checks here
    // In particular we don't want to poison IO for checking late-writes.
    gShutdownChecks = SCM_NOTHING;
  }

  if (!mShuttingDown) {
#ifdef MOZ_ENABLE_XREMOTE
    // shut down the x remote proxy window
    if (mRemoteService) {
      mRemoteService->Shutdown();
    }
#endif /* MOZ_ENABLE_XREMOTE */
  }

  mScopedXPCOM = nullptr;

#if defined(XP_WIN)
  mozilla::widget::StopAudioSession();
#endif

  // unlock the profile after ScopedXPCOMStartup object (xpcom)
  // has gone out of scope.  see bug #386739 for more details
  mProfileLock->Unlock();
  gProfileLock = nullptr;

  // Restart the app after XPCOM has been shut down cleanly.
  if (appInitiatedRestart) {
    RestoreStateForAppInitiatedRestart();

    if (rv != NS_SUCCESS_RESTART_APP_NOT_SAME_PROFILE) {
      // Ensure that these environment variables are set:
      SaveFileToEnvIfUnset("XRE_PROFILE_PATH", mProfD);
      SaveFileToEnvIfUnset("XRE_PROFILE_LOCAL_PATH", mProfLD);
      SaveWordToEnvIfUnset("XRE_PROFILE_NAME", mProfileName);
    }

#ifdef MOZ_WIDGET_GTK
    if (!gfxPlatform::IsHeadless()) {
      MOZ_gdk_display_close(mGdkDisplay);
    }
#endif

    {
      rv = LaunchChild(mNativeApp, true);
    }

    if (mAppData->flags & NS_XRE_ENABLE_CRASH_REPORTER)
      CrashReporter::UnsetExceptionHandler();

    return rv == NS_ERROR_LAUNCHED_CHILD_PROCESS ? 0 : 1;
  }

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

void
XRE_StopLateWriteChecks(void) {
  mozilla::StopLateWriteChecks();
}

int
XRE_main(int argc, char* argv[], const BootstrapConfig& aConfig)
{
  XREMain main;

  int result = main.XRE_main(argc, argv, aConfig);
  mozilla::RecordShutdownEndTimeStamp();
  return result;
}

nsresult
XRE_InitCommandLine(int aArgc, char* aArgv[])
{
  nsresult rv = NS_OK;

#if defined(OS_WIN)
  CommandLine::Init(aArgc, aArgv);
#else

  // these leak on error, but that's OK: we'll just exit()
  char** canonArgs = new char*[aArgc];

  // get the canonical version of the binary's path
  nsCOMPtr<nsIFile> binFile;
  rv = XRE_GetBinaryPath(getter_AddRefs(binFile));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsAutoCString canonBinPath;
  rv = binFile->GetNativePath(canonBinPath);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  canonArgs[0] = strdup(canonBinPath.get());

  for (int i = 1; i < aArgc; ++i) {
    if (aArgv[i]) {
      canonArgs[i] = strdup(aArgv[i]);
    }
  }

  NS_ASSERTION(!CommandLine::IsInitialized(), "Bad news!");
  CommandLine::Init(aArgc, canonArgs);

  for (int i = 0; i < aArgc; ++i)
      free(canonArgs[i]);
  delete[] canonArgs;
#endif

  const char *path = nullptr;
  ArgResult ar = CheckArg("greomni", &path);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument --greomni requires a path argument\n");
    return NS_ERROR_FAILURE;
  }

  if (!path)
    return rv;

  nsCOMPtr<nsIFile> greOmni;
  rv = XRE_GetFileFromPath(path, getter_AddRefs(greOmni));
  if (NS_FAILED(rv)) {
    PR_fprintf(PR_STDERR, "Error: argument --greomni requires a valid path\n");
    return rv;
  }

  ar = CheckArg("appomni", &path);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument --appomni requires a path argument\n");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIFile> appOmni;
  if (path) {
      rv = XRE_GetFileFromPath(path, getter_AddRefs(appOmni));
      if (NS_FAILED(rv)) {
        PR_fprintf(PR_STDERR, "Error: argument --appomni requires a valid path\n");
        return rv;
      }
  }

  mozilla::Omnijar::Init(greOmni, appOmni);
  return rv;
}

nsresult
XRE_DeinitCommandLine()
{
  nsresult rv = NS_OK;

  CommandLine::Terminate();

  return rv;
}

GeckoProcessType
XRE_GetProcessType()
{
  return mozilla::startup::sChildProcessType;
}

bool
XRE_IsGPUProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_GPU;
}

/**
 * Returns true in the e10s parent process and in the main process when e10s
 * is disabled.
 */
bool
XRE_IsParentProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_Default;
}

bool
XRE_IsE10sParentProcess()
{
  return XRE_IsParentProcess() && BrowserTabsRemoteAutostart();
}

bool
XRE_IsContentProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_Content;
}

bool
XRE_IsPluginProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_Plugin;
}

bool
XRE_UseNativeEventProcessing()
{
  if (XRE_IsContentProcess()) {
    static bool sInited = false;
    static bool sUseNativeEventProcessing = false;
    if (!sInited) {
      Preferences::AddBoolVarCache(&sUseNativeEventProcessing,
                                   "dom.ipc.useNativeEventProcessing.content");
      sInited = true;
    }

    return sUseNativeEventProcessing;
  }

  return true;
}

// If you add anything to this enum, please update about:support to reflect it
enum {
  kE10sEnabledByUser = 0,
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

const char* kForceEnableE10sPref = "browser.tabs.remote.force-enable";
const char* kForceDisableE10sPref = "browser.tabs.remote.force-disable";

namespace mozilla {

bool
BrowserTabsRemoteAutostart()
{
  if (gBrowserTabsRemoteAutostartInitialized) {
    return gBrowserTabsRemoteAutostart;
  }
  gBrowserTabsRemoteAutostartInitialized = true;

  // If we're in the content process, we are running E10S.
  if (XRE_IsContentProcess()) {
    gBrowserTabsRemoteAutostart = true;
    return gBrowserTabsRemoteAutostart;
  }

  bool optInPref = Preferences::GetBool("browser.tabs.remote.autostart", true);
  int status = kE10sEnabledByDefault;

  if (optInPref) {
    gBrowserTabsRemoteAutostart = true;
  } else {
    status = kE10sDisabledByUser;
  }

  // Uber override pref for manual testing purposes
  if (Preferences::GetBool(kForceEnableE10sPref, false)) {
    gBrowserTabsRemoteAutostart = true;
    status = kE10sEnabledByUser;
  }

  // Uber override pref for emergency blocking
  if (gBrowserTabsRemoteAutostart &&
      (Preferences::GetBool(kForceDisableE10sPref, false) ||
       EnvHasValue("MOZ_FORCE_DISABLE_E10S"))) {
    gBrowserTabsRemoteAutostart = false;
    status = kE10sForceDisabled;
  }

  gBrowserTabsRemoteStatus = status;

  mozilla::Telemetry::Accumulate(mozilla::Telemetry::E10S_STATUS, status);
  return gBrowserTabsRemoteAutostart;
}

uint32_t
GetMaxWebProcessCount()
{
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

const char*
PlatformBuildID()
{
  return gToolkitBuildID;
}

} // namespace mozilla

void
SetupErrorHandling(const char* progname)
{
#ifdef XP_WIN
  /* On Windows XPSP3 and Windows Vista if DEP is configured off-by-default
     we still want DEP protection: enable it explicitly and programmatically.

     This function is not available on WinXPSP2 so we dynamically load it.
  */

  HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
  SetProcessDEPPolicyFunc _SetProcessDEPPolicy =
    (SetProcessDEPPolicyFunc) GetProcAddress(kernel32, "SetProcessDEPPolicy");
  if (_SetProcessDEPPolicy)
    _SetProcessDEPPolicy(PROCESS_DEP_ENABLE);
#endif

#ifdef XP_WIN32
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

#if defined (DEBUG) && defined(XP_WIN)
  // Send MSCRT Warnings, Errors and Assertions to stderr.
  // See http://msdn.microsoft.com/en-us/library/1y71x448(v=VS.80).aspx
  // and http://msdn.microsoft.com/en-us/library/a68f826y(v=VS.80).aspx.

  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);

  _CrtSetReportHook(MSCRTReportHook);
#endif

  InstallSignalHandlers(progname);

  // Unbuffer stdout, needed for tinderbox tests.
  setbuf(stdout, 0);
}

// Note: This function should not be needed anymore. See Bug 818634 for details.
void
OverrideDefaultLocaleIfNeeded() {
  // Read pref to decide whether to override default locale with US English.
  if (mozilla::Preferences::GetBool("javascript.use_us_english_locale", false)) {
    // Set the application-wide C-locale. Needed to resist fingerprinting
    // of Date.toLocaleFormat(). We use the locale to "C.UTF-8" if possible,
    // to avoid interfering with non-ASCII keyboard input on some Linux desktops.
    // Otherwise fall back to the "C" locale, which is available on all platforms.
    setlocale(LC_ALL, "C.UTF-8") || setlocale(LC_ALL, "C");
  }
}

void
XRE_EnableSameExecutableForContentProc() {
  if (!PR_GetEnv("MOZ_SEPARATE_CHILD_PROCESS")) {
    mozilla::ipc::GeckoChildProcessHost::EnableSameExecutableForContentProc();
  }
}

// Because rust doesn't handle weak symbols, this function wraps the weak
// malloc_handle_oom for it.
extern "C" void
GeckoHandleOOM(size_t size) {
  mozalloc_handle_oom(size);
}
