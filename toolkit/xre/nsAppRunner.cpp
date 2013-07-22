/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(XP_OS2) && defined(MOZ_OS2_HIGH_MEMORY)
// os2safe.h has to be included before os2.h, needed for high mem
#include <os2safe.h>
#endif

#if defined(MOZ_WIDGET_QT)
#include <QApplication>
#include <QStringList>
#include "nsQAppInstance.h"
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
#include <QInputContextFactory>
#include <QInputContext>
#endif
#endif // MOZ_WIDGET_QT

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"

#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"
#include "mozilla/Poison.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Util.h"

#include "nsAppRunner.h"
#include "mozilla/AppData.h"
#include "nsUpdateDriver.h"
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

#ifdef XP_OS2
#include "private/pprthred.h"
#endif
#include "prmem.h"
#include "prnetdb.h"
#include "prprf.h"
#include "prproces.h"
#include "prenv.h"

#include "nsIAppShellService.h"
#include "nsIAppStartup.h"
#include "nsIAppStartupNotifier.h"
#include "nsIMutableArray.h"
#include "nsICategoryManager.h"
#include "nsIChromeRegistry.h"
#include "nsICommandLineRunner.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIContentHandler.h"
#include "nsIDialogParamBlock.h"
#include "nsIDOMWindow.h"
#include "mozilla/ModuleUtils.h"
#include "nsIIOService2.h"
#include "nsIObserverService.h"
#include "nsINativeAppSupport.h"
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

#include "mozilla/unused.h"

using namespace mozilla;
using mozilla::unused;
using mozilla::scache::StartupCache;

#ifdef XP_WIN
#include "nsIWinAppHelper.h"
#include <windows.h>
#include "cairo/cairo-features.h"
#ifdef MOZ_METRO
#include <roapi.h>
#endif

#ifndef PROCESS_DEP_ENABLE
#define PROCESS_DEP_ENABLE 0x1
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
#include "nsXPIDLString.h"
#include "nsPrintfCString.h"
#include "nsVersionComparator.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsXULAppAPI.h"
#include "nsXREDirProvider.h"
#include "nsToolkitCompsCID.h"

#include "nsINIParser.h"
#include "mozilla/Omnijar.h"
#include "mozilla/StartupTimeline.h"
#include "mozilla/mozPoisonWrite.h"

#include <stdlib.h>

// for old system jemalloc version check
#if !defined(MOZ_MEMORY) && defined(__NetBSD__)
#include <sys/param.h>
#endif

#ifdef XP_UNIX
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#endif

#ifdef XP_WIN
#include <process.h>
#include <shlobj.h>
#include "nsThreadUtils.h"
#endif

#ifdef XP_MACOSX
#include "nsILocalFileMac.h"
#include "nsCommandLineServiceMac.h"
#endif

// for X remote support
#ifdef MOZ_ENABLE_XREMOTE
#include "XRemoteClient.h"
#include "nsIRemoteService.h"
#endif

#ifdef NS_TRACE_MALLOC
#include "nsTraceMalloc.h"
#endif

#if defined(DEBUG) && defined(XP_WIN32)
#include <malloc.h>
#endif

#if defined (XP_MACOSX)
#include <Carbon/Carbon.h>
#endif

#ifdef DEBUG
#include "prlog.h"
#endif

#ifdef MOZ_JPROF
#include "jprof.h"
#endif

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#define NS_CRASHREPORTER_CONTRACTID "@mozilla.org/toolkit/crash-reporter;1"
#include "nsIPrefService.h"
#endif

#include "base/command_line.h"
#ifdef MOZ_ENABLE_TESTS
#include "GTestRunner.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

extern uint32_t gRestartMode;
extern void InstallSignalHandlers(const char *ProgramName);
#include "nsX11ErrorHandler.h"

#define FILE_COMPATIBILITY_INFO NS_LITERAL_CSTRING("compatibility.ini")
#define FILE_INVALIDATE_CACHES NS_LITERAL_CSTRING(".purgecaches")

int    gArgc;
char **gArgv;

static const char gToolkitVersion[] = NS_STRINGIFY(GRE_MILESTONE);
static const char gToolkitBuildID[] = NS_STRINGIFY(GRE_BUILDID);

static nsIProfileLock* gProfileLock;

int    gRestartArgc;
char **gRestartArgv;

#ifdef MOZ_WIDGET_QT
static int    gQtOnlyArgc;
static char **gQtOnlyArgv;
#endif

#if defined(MOZ_WIDGET_GTK)
#if defined(DEBUG) || defined(NS_BUILD_REFCNT_LOGGING) \
  || defined(NS_TRACE_MALLOC)
#define CLEANUP_MEMORY 1
#define PANGO_ENABLE_BACKEND
#include <pango/pangofc-fontmap.h>
#endif
#include <gtk/gtk.h>
#ifdef MOZ_X11
#include <gdk/gdkx.h>
#endif /* MOZ_X11 */
#include "nsGTKToolkit.h"
#endif
#include "BinaryPath.h"

namespace mozilla {
int (*RunGTest)() = 0;
}

using mozilla::dom::ContentParent;
using mozilla::dom::ContentChild;

// Save literal putenv string to environment variable.
static void
SaveToEnv(const char *putenv)
{
  char *expr = strdup(putenv);
  if (expr)
    PR_SetEnv(expr);
  // We intentionally leak |expr| here since it is required by PR_SetEnv.
}

// Tests that an environment variable exists and has a value
static bool
EnvHasValue(const char *name)
{
  const char *val = PR_GetEnv(name);
  return (val && *val);
}

// Save the given word to the specified environment variable.
static void
SaveWordToEnv(const char *name, const nsACString & word)
{
  char *expr = PR_smprintf("%s=%s", name, PromiseFlatCString(word).get());
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

static bool
strimatch(const char* lowerstr, const char* mixedstr)
{
  while(*lowerstr) {
    if (!*mixedstr) return false; // mixedstr is shorter
    if (tolower(*mixedstr) != *lowerstr) return false; // no match

    ++lowerstr;
    ++mixedstr;
  }

  if (*mixedstr) return false; // lowerstr is shorter

  return true;
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
static void Output(bool isError, const char *fmt, ... )
{
  va_list ap;
  va_start(ap, fmt);

#if defined(XP_WIN) && !MOZ_WINCONSOLE
  char *msg = PR_vsmprintf(fmt, ap);
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
                        msg,
                        -1,
                        wide_msg,
                        sizeof(wide_msg) / sizeof(wchar_t));

    MessageBoxW(NULL, wide_msg, L"XULRunner", flags);
    PR_smprintf_free(msg);
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

enum ArgResult {
  ARG_NONE  = 0,
  ARG_FOUND = 1,
  ARG_BAD   = 2 // you wanted a param, but there isn't one
};

static void RemoveArg(char **argv)
{
  do {
    *argv = *(argv + 1);
    ++argv;
  } while (*argv);

  --gArgc;
}

/**
 * Check for a commandline flag. If the flag takes a parameter, the
 * parameter is returned in aParam. Flags may be in the form -arg or
 * --arg (or /arg on win32/OS2).
 *
 * @param aArg the parameter to check. Must be lowercase.
 * @param aCheckOSInt if true returns ARG_BAD if the osint argument is present
 *        when aArg is also present.
 * @param aParam if non-null, the -arg <data> will be stored in this pointer.
 *        This is *not* allocated, but rather a pointer to the argv data.
 * @param aRemArg if true, the argument is removed from the gArgv array.
 */
static ArgResult
CheckArg(const char* aArg, bool aCheckOSInt = false, const char **aParam = nullptr, bool aRemArg = true)
{
  NS_ABORT_IF_FALSE(gArgv, "gArgv must be initialized before CheckArg()");

  char **curarg = gArgv + 1; // skip argv[0]
  ArgResult ar = ARG_NONE;

  while (*curarg) {
    char *arg = curarg[0];

    if (arg[0] == '-'
#if defined(XP_WIN) || defined(XP_OS2)
        || *arg == '/'
#endif
        ) {
      ++arg;
      if (*arg == '-')
        ++arg;

      if (strimatch(aArg, arg)) {
        if (aRemArg)
          RemoveArg(curarg);
        if (!aParam) {
          ar = ARG_FOUND;
          break;
        }

        if (*curarg) {
          if (**curarg == '-'
#if defined(XP_WIN) || defined(XP_OS2)
              || **curarg == '/'
#endif
              )
            return ARG_BAD;

          *aParam = *curarg;
          if (aRemArg)
            RemoveArg(curarg);
          ar = ARG_FOUND;
          break;
        }
        return ARG_BAD;
      }
    }

    ++curarg;
  }

  if (aCheckOSInt && ar == ARG_FOUND) {
    ArgResult arOSInt = CheckArg("osint");
    if (arOSInt == ARG_FOUND) {
      ar = ARG_BAD;
      PR_fprintf(PR_STDERR, "Error: argument -osint is invalid\n");
    }
  }

  return ar;
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
      nsIThread *thread = NS_GetCurrentThread();
      // This is just a guesstimate based on testing different values.
      // If count is 8 or less windows will display an error dialog.
      int32_t count = 20;
      while(--count >= 0) {
        NS_ProcessNextEvent(thread);
        PR_Sleep(PR_MillisecondsToInterval(1));
      }
    }
  }
}
#endif

bool gSafeMode = false;

/**
 * The nsXULAppInfo object implements nsIFactory so that it can be its own
 * singleton.
 */
class nsXULAppInfo : public nsIXULAppInfo,
#ifdef XP_WIN
                     public nsIWinAppHelper,
#endif
#ifdef MOZ_CRASHREPORTER
                     public nsICrashReporter,
#endif
                     public nsIXULRuntime

{
public:
  nsXULAppInfo() {}
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIXULAPPINFO
  NS_DECL_NSIXULRUNTIME
#ifdef MOZ_CRASHREPORTER
  NS_DECL_NSICRASHREPORTER
#endif
#ifdef XP_WIN
  NS_DECL_NSIWINAPPHELPER
#endif
};

NS_INTERFACE_MAP_BEGIN(nsXULAppInfo)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXULRuntime)
  NS_INTERFACE_MAP_ENTRY(nsIXULRuntime)
#ifdef XP_WIN
  NS_INTERFACE_MAP_ENTRY(nsIWinAppHelper)
#endif
#ifdef MOZ_CRASHREPORTER
  NS_INTERFACE_MAP_ENTRY(nsICrashReporter)
#endif
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIXULAppInfo, gAppData || 
                                     XRE_GetProcessType() == GeckoProcessType_Content)
NS_INTERFACE_MAP_END

NS_IMETHODIMP_(nsrefcnt)
nsXULAppInfo::AddRef()
{
  return 1;
}

NS_IMETHODIMP_(nsrefcnt)
nsXULAppInfo::Release()
{
  return 1;
}

NS_IMETHODIMP
nsXULAppInfo::GetVendor(nsACString& aResult)
{
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aResult.Assign(gAppData->vendor);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetName(nsACString& aResult)
{
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
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
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aResult.Assign(gAppData->ID);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetVersion(nsACString& aResult)
{
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
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
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
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
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    ContentChild* cc = ContentChild::GetSingleton();
    aResult = cc->GetAppInfo().UAName;
    return NS_OK;
  }
  aResult.Assign(gAppData->UAName);

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
  MOZ_STATIC_ASSERT(nsIXULRuntime::PROCESS_TYPE_ ## a == \
                    static_cast<int>(GeckoProcessType_ ## b), \
                    "GeckoProcessType in nsXULAppAPI.h not synchronized with nsIXULRuntime.idl");

SYNC_ENUMS(DEFAULT, Default)
SYNC_ENUMS(PLUGIN, Plugin)
SYNC_ENUMS(CONTENT, Content)
SYNC_ENUMS(IPDLUNITTEST, IPDLUnitTest)

// .. and ensure that that is all of them:
MOZ_STATIC_ASSERT(GeckoProcessType_IPDLUnitTest + 1 == GeckoProcessType_End,
                  "Did not find the final GeckoProcessType");

NS_IMETHODIMP
nsXULAppInfo::GetProcessType(uint32_t* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = XRE_GetProcessType();
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::EnsureContentProcess()
{
  if (XRE_GetProcessType() != GeckoProcessType_Default)
    return NS_ERROR_NOT_AVAILABLE;

  nsRefPtr<ContentParent> unused = ContentParent::GetNewOrUsed();
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
    PRFileDesc *fd = nullptr;
    file->OpenNSPRFileDesc(PR_RDWR | PR_APPEND, 0600, &fd);
    if (!fd) {
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
nsXULAppInfo::GetLastRunCrashID(nsAString &aLastRunCrashID)
{
#ifdef MOZ_CRASHREPORTER
  CrashReporter::GetLastRunCrashID(aLastRunCrashID);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
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

#ifdef MOZ_CRASHREPORTER
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
    if (CrashReporter::GetEnabled())
      // no point in erroring for double-enabling
      return NS_OK;

    nsCOMPtr<nsIFile> xreDirectory;
    if (gAppData) {
      xreDirectory = gAppData->xreDirectory;
    }
    else {
      // We didn't get started through XRE_Main, probably
      nsCOMPtr<nsIFile> greDir;
      NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(greDir));
      if (!greDir)
        return NS_ERROR_FAILURE;

      xreDirectory = do_QueryInterface(greDir);
      if (!xreDirectory)
        return NS_ERROR_FAILURE;
    }
    AnnotateCrashReport(NS_LITERAL_CSTRING("FramePoisonBase"),
                        nsPrintfCString("%.16llx", uint64_t(gMozillaPoisonBase)));
    AnnotateCrashReport(NS_LITERAL_CSTRING("FramePoisonSize"),
                        nsPrintfCString("%lu", uint32_t(gMozillaPoisonSize)));
    return CrashReporter::SetExceptionHandler(xreDirectory, true);
  }
  else {
    if (!CrashReporter::GetEnabled())
      // no point in erroring for double-disabling
      return NS_OK;

    return CrashReporter::UnsetExceptionHandler();
  }
}

NS_IMETHODIMP
nsXULAppInfo::GetServerURL(nsIURL** aServerURL)
{
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

#endif

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
    PROFILER_MARKER("Shutdown early");

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
  { &kAPPINFO_CID, false, NULL, AppInfoConstructor },
  { &kProfileServiceCID, false, ProfileServiceFactoryConstructor, NULL },
  { &kNativeAppSupportCID, false, NULL, ScopedXPCOMStartup::CreateAppSupport },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kXREContracts[] = {
  { XULAPPINFO_SERVICE_CONTRACTID, &kAPPINFO_CID },
  { XULRUNTIME_SERVICE_CONTRACTID, &kAPPINFO_CID },
#ifdef MOZ_CRASHREPORTER
  { NS_CRASHREPORTER_CONTRACTID, &kAPPINFO_CID },
#endif
  { NS_PROFILESERVICE_CONTRACTID, &kProfileServiceCID },
  { NS_NATIVEAPPSUPPORT_CONTRACTID, &kNativeAppSupportCID },
  { NULL }
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
    nsCOMPtr<nsIComponentRegistrar> reg =
      do_QueryInterface(mServiceManager);
    NS_ASSERTION(reg, "Service Manager doesn't QI to Registrar.");
  }

  return rv;
}

/**
 * This is a little factory class that serves as a singleton-service-factory
 * for the nativeappsupport object.
 */
class nsSingletonFactory MOZ_FINAL : public nsIFactory
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFACTORY

  nsSingletonFactory(nsISupports* aSingleton);
  ~nsSingletonFactory() { }

private:
  nsCOMPtr<nsISupports> mSingleton;
};

nsSingletonFactory::nsSingletonFactory(nsISupports* aSingleton)
  : mSingleton(aSingleton)
{
  NS_ASSERTION(mSingleton, "Singleton was null!");
}

NS_IMPL_ISUPPORTS1(nsSingletonFactory, nsIFactory)

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

/**
 * A helper class which calls NS_LogInit/NS_LogTerm in its scope.
 */
class ScopedLogging
{
public:
  ScopedLogging() { NS_LogInit(); }
  ~ScopedLogging() { NS_LogTerm(); }
};

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
         "\n%s options\n", gAppData->name);
#endif

  printf("  -h or -help        Print this message.\n"
         "  -v or -version     Print %s version.\n"
         "  -P <profile>       Start with <profile>.\n"
         "  -migration         Start with migration wizard.\n"
         "  -ProfileManager    Start with ProfileManager.\n"
         "  -no-remote         Do not accept or send remote commands; implies -new-instance.\n"
         "  -new-instance      Open new instance, not a new window in running instance.\n"
         "  -UILocale <locale> Start with <locale> resources as UI Locale.\n"
         "  -safe-mode         Disables extensions and themes for this session.\n", gAppData->name);

#if defined(XP_WIN) || defined(XP_OS2)
  printf("  -console           Start %s with a debugging console.\n", gAppData->name);
#endif

  // this works, but only after the components have registered.  so if you drop in a new command line handler, -help
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
    printf("%s ", gAppData->vendor);
  printf("%s %s", gAppData->name, gAppData->version);
  if (gAppData->copyright)
      printf(", %s", gAppData->copyright);
  printf("\n");
}

#ifdef MOZ_ENABLE_XREMOTE
// use int here instead of a PR type since it will be returned
// from main - just to keep types consistent
static int
HandleRemoteArgument(const char* remote, const char* aDesktopStartupID)
{
  nsresult rv;
  ArgResult ar;

  const char *profile = 0;
  nsAutoCString program(gAppData->name);
  ToLowerCase(program);
  const char *username = getenv("LOGNAME");

  ar = CheckArg("p", false, &profile);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -p requires a profile name\n");
    return 1;
  }

  const char *temp = nullptr;
  ar = CheckArg("a", false, &temp);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -a requires an application name\n");
    return 1;
  } else if (ar == ARG_FOUND) {
    program.Assign(temp);
  }

  ar = CheckArg("u", false, &username);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -u requires a username\n");
    return 1;
  }

  XRemoteClient client;
  rv = client.Init();
  if (NS_FAILED(rv)) {
    PR_fprintf(PR_STDERR, "Error: Failed to connect to X server.\n");
    return 1;
  }

  nsXPIDLCString response;
  bool success = false;
  rv = client.SendCommand(program.get(), username, profile, remote,
                          aDesktopStartupID, getter_Copies(response), &success);
  // did the command fail?
  if (NS_FAILED(rv)) {
    PR_fprintf(PR_STDERR, "Error: Failed to send command: %s\n",
               response ? response.get() : "No response included");
    return 1;
  }

  if (!success) {
    PR_fprintf(PR_STDERR, "Error: No running window found\n");
    return 2;
  }

  return 0;
}

static RemoteResult
RemoteCommandLine(const char* aDesktopStartupID)
{
  nsresult rv;
  ArgResult ar;

  nsAutoCString program(gAppData->name);
  ToLowerCase(program);
  const char *username = getenv("LOGNAME");

  const char *temp = nullptr;
  ar = CheckArg("a", true, &temp);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -a requires an application name\n");
    return REMOTE_ARG_BAD;
  } else if (ar == ARG_FOUND) {
    program.Assign(temp);
  }

  ar = CheckArg("u", true, &username);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -u requires a username\n");
    return REMOTE_ARG_BAD;
  }

  XRemoteClient client;
  rv = client.Init();
  if (NS_FAILED(rv))
    return REMOTE_NOT_FOUND;
 
  nsXPIDLCString response;
  bool success = false;
  rv = client.SendCommandLine(program.get(), username, nullptr,
                              gArgc, gArgv, aDesktopStartupID,
                              getter_Copies(response), &success);
  // did the command fail?
  if (NS_FAILED(rv) || !success)
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
XRE_GetBinaryPath(const char* argv0, nsIFile* *aResult)
{
  return mozilla::BinaryPath::GetFile(argv0, aResult);
}

#ifdef XP_WIN
#include "nsWindowsRestart.cpp"
#include <shellapi.h>

typedef BOOL (WINAPI* SetProcessDEPPolicyFunc)(DWORD dwFlags);
#endif

#if defined(XP_OS2) && (__KLIBC__ == 0 && __KLIBC_MINOR__ >= 6) // broken kLibc
// Copy the environment maintained by the C library into an ASCIIZ array
// that can be used to pass it on to the OS/2 Dos* APIs (which otherwise
// don't know anything about the stuff set by PR_SetEnv() or setenv()).
char *createEnv()
{
  // just allocate the maximum amount (24 kB = 0x60000 bytes), to be able to
  // copy the existing environment
  char *env = (char *)calloc(0x6000, sizeof(char));
  if (!env) {
    return NULL;
  }

  // walk along the environ string array of the C library and copy
  // everything (that fits) into the output environment array, leaving
  // null bytes between the entries
  char *penv = env; // movable pointer to result environment ASCIIZ array
  int i = 0, space = 0x6000;
  while (environ[i] && environ[i][0]) {
    int len = strlen(environ[i]);
    if (space - len <= 0) {
      break;
    }
    strcpy(penv, environ[i]);
    i++; // next environment variable
    penv += len + 1; // jump to after next null byte
    space -= len - 1; // subtract consumed length from usable space
  }

  return env;
}

// OS2LaunchChild() is there to replace _execv() which is broken in the C
// runtime library that comes with GCC 3.3.5 on OS/2. It uses createEnv()
// to copy the process environment and add necessary variables
//
// returns -1 on failure and 0 on success
int OS2LaunchChild(const char *aExePath, int aArgc, char **aArgv)
{
  // find total length of aArgv
  int len = 0;
  for (int i = 0; i < aArgc; i++) {
    len += strlen(aArgv[i]) + 1; // plus space in between
  }
  len++; // leave space for null byte at end
  // allocate enough space for all strings and nulls,
  // calloc helpfully initializes to null
  char *args = (char *)calloc(len, sizeof(char));
  if (!args) {
    return -1;
  }
  char *pargs = args; // extra pointer to after the last argument
  // build argument list in the format the DosStartSession() wants,
  // adding spaces between the arguments
  for (int i = 0; i < aArgc; i++, *pargs++ = ' ') {
    strcpy(pargs, aArgv[i]);
    pargs += strlen(aArgv[i]);
  }
  if (aArgc > 1) {
    *(pargs-1) = '\0'; // replace last space
  }
  *pargs = '\0';
  // make sure that the program is separated by null byte
  pargs = strchr(args, ' ');
  if (pargs) {
    *pargs = '\0';
  }

  char *env = createEnv();

  char error[CCHMAXPATH] = { 0 };
  RESULTCODES crc = { 0 };
  ULONG rc = DosExecPgm(error, sizeof(error), EXEC_ASYNC, args, env,
                        &crc, (PSZ)aExePath);
  free(args); // done with the arguments
  if (env) {
    free(env);
  }
  if (rc != NO_ERROR) {
    return -1;
  }

  return 0;
}
#endif

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
#if defined(MOZ_WIDGET_QT)
    // Remove only arguments not given to Qt
    gRestartArgc = gQtOnlyArgc;
    gRestartArgv = gQtOnlyArgv;
#else
    gRestartArgc = 1;
    gRestartArgv[gRestartArgc] = nullptr;
#endif
  }

  SaveToEnv("MOZ_LAUNCHED_CHILD=1");

#if defined(MOZ_WIDGET_ANDROID)
  mozilla::AndroidBridge::Bridge()->ScheduleRestart();
#else
#if defined(XP_MACOSX)
  CommandLineServiceMac::SetupMacCommandLine(gRestartArgc, gRestartArgv, true);
  uint32_t restartMode = 0;
  restartMode = gRestartMode;
  LaunchChildMac(gRestartArgc, gRestartArgv, restartMode);
#else
  nsCOMPtr<nsIFile> lf;
  nsresult rv = XRE_GetBinaryPath(gArgv[0], getter_AddRefs(lf));
  if (NS_FAILED(rv))
    return rv;

#if defined(XP_WIN)
  nsAutoString exePath;
  rv = lf->GetPath(exePath);
  if (NS_FAILED(rv))
    return rv;

  if (!WinLaunchChild(exePath.get(), gRestartArgc, gRestartArgv))
    return NS_ERROR_FAILURE;

#else
  nsAutoCString exePath;
  rv = lf->GetNativePath(exePath);
  if (NS_FAILED(rv))
    return rv;

#if defined(XP_OS2) && (__KLIBC__ == 0 && __KLIBC_MINOR__ >= 6)
  // implementation of _execv() is broken with kLibc 0.6.x and later
  if (OS2LaunchChild(exePath.get(), gRestartArgc, gRestartArgv) == -1)
    return NS_ERROR_FAILURE;
#elif defined(XP_OS2)
  if (_execv(exePath.get(), gRestartArgv) == -1)
    return NS_ERROR_FAILURE;
#elif defined(XP_UNIX)
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
#endif // XP_OS2 series
#endif // WP_WIN
#endif // WP_MACOSX
#endif // MOZ_WIDGET_ANDROID

  return NS_ERROR_LAUNCHED_CHILD_PROCESS;
}

static const char kProfileProperties[] =
  "chrome://mozapps/locale/profile/profileSelection.properties";

static nsresult
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
    const PRUnichar* params[] = {appName.get(), appName.get()};

    nsXPIDLString killMessage;
#ifndef XP_MACOSX
    static const PRUnichar kRestartNoUnlocker[] = {'r','e','s','t','a','r','t','M','e','s','s','a','g','e','N','o','U','n','l','o','c','k','e','r','\0'}; // "restartMessageNoUnlocker"
    static const PRUnichar kRestartUnlocker[] = {'r','e','s','t','a','r','t','M','e','s','s','a','g','e','U','n','l','o','c','k','e','r','\0'}; // "restartMessageUnlocker"
#else
    static const PRUnichar kRestartNoUnlocker[] = {'r','e','s','t','a','r','t','M','e','s','s','a','g','e','N','o','U','n','l','o','c','k','e','r','M','a','c','\0'}; // "restartMessageNoUnlockerMac"
    static const PRUnichar kRestartUnlocker[] = {'r','e','s','t','a','r','t','M','e','s','s','a','g','e','U','n','l','o','c','k','e','r','M','a','c','\0'}; // "restartMessageUnlockerMac"
#endif

    sb->FormatStringFromName(aUnlocker ? kRestartUnlocker : kRestartNoUnlocker,
                             params, 2, getter_Copies(killMessage));

    nsXPIDLString killTitle;
    sb->FormatStringFromName(NS_LITERAL_STRING("restartTitle").get(),
                             params, 1, getter_Copies(killTitle));

    if (!killMessage || !killTitle)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIPromptService> ps
      (do_GetService(NS_PROMPTSERVICE_CONTRACTID));
    NS_ENSURE_TRUE(ps, NS_ERROR_FAILURE);

    if (aUnlocker) {
      int32_t button;
#ifdef MOZ_WIDGET_ANDROID
      mozilla::AndroidBridge::Bridge()->KillAnyZombies();
      button = 1;
#else
      const uint32_t flags =
        (nsIPromptService::BUTTON_TITLE_CANCEL * 
         nsIPromptService::BUTTON_POS_0) +
        (nsIPromptService::BUTTON_TITLE_IS_STRING * 
         nsIPromptService::BUTTON_POS_1) +
        nsIPromptService::BUTTON_POS_1_DEFAULT;

      // The actual value is irrelevant but we shouldn't be handing out
      // malformed JSBools to XPConnect.
      bool checkState = false;

      rv = ps->ConfirmEx(nullptr, killTitle, killMessage, flags,
                         killTitle, nullptr, nullptr, nullptr, 
                         &checkState, &button);
      NS_ENSURE_SUCCESS_LOG(rv, rv);
#endif

      if (button == 1) {
        rv = aUnlocker->Unlock(nsIProfileUnlocker::FORCE_QUIT);
        if (NS_FAILED(rv)) 
          return rv;

        return NS_LockProfilePath(aProfileDir, aProfileLocalDir, 
                                  nullptr, aResult);
      }
    } else {
#ifdef MOZ_WIDGET_ANDROID
      if (mozilla::AndroidBridge::Bridge()->UnlockProfile()) {
        return NS_LockProfilePath(aProfileDir, aProfileLocalDir, 
                                  nullptr, aResult);
      }
#else
      rv = ps->Alert(nullptr, killTitle, killMessage);
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
    const PRUnichar* params[] = {appName.get(), appName.get()};
  
    nsXPIDLString missingMessage;
  
    // profileMissing  
    static const PRUnichar kMissing[] = {'p','r','o','f','i','l','e','M','i','s','s','i','n','g','\0'};
    sb->FormatStringFromName(kMissing, params, 2, getter_Copies(missingMessage));
  
    nsXPIDLString missingTitle;
    sb->FormatStringFromName(NS_LITERAL_STRING("profileMissingTitle").get(),
                             params, 1, getter_Copies(missingTitle));
  
    if (missingMessage && missingTitle) {
      nsCOMPtr<nsIPromptService> ps
        (do_GetService(NS_PROMPTSERVICE_CONTRACTID));
      NS_ENSURE_TRUE(ps, NS_ERROR_FAILURE);
  
      ps->Alert(nullptr, missingTitle, missingMessage);
    }

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

static nsresult
ShowProfileManager(nsIToolkitProfileService* aProfileSvc,
                   nsINativeAppSupport* aNative)
{
  nsresult rv;

  nsCOMPtr<nsIFile> profD, profLD;
  PRUnichar* profileNamePtr;
  nsAutoCString profileName;

  {
    ScopedXPCOMStartup xpcom;
    rv = xpcom.Initialize();
    NS_ENSURE_SUCCESS(rv, rv);

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

      nsCOMPtr<nsIDOMWindow> newWindow;
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
      NS_Free(profileNamePtr);

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

  return LaunchChild(aNative);
}

static nsresult
GetCurrentProfileIsDefault(nsIToolkitProfileService* aProfileSvc,
                           nsIFile* aCurrentProfileRoot, bool *aResult)
{
  nsresult rv;
  // Check that the profile to reset is the default since reset and the associated migration are
  // only supported in that case.
  nsCOMPtr<nsIToolkitProfile> selectedProfile;
  nsCOMPtr<nsIFile> selectedProfileRoot;
  rv = aProfileSvc->GetSelectedProfile(getter_AddRefs(selectedProfile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = selectedProfile->GetRootDir(getter_AddRefs(selectedProfileRoot));
  NS_ENSURE_SUCCESS(rv, rv);

  bool currentIsSelected;
  rv = aCurrentProfileRoot->Equals(selectedProfileRoot, &currentIsSelected);

  *aResult = currentIsSelected;
  return rv;
}

/**
 * Set the currently running profile as the default/selected one.
 *
 * @param aCurrentProfileRoot The root directory of the current profile.
 * @return an error if aCurrentProfileRoot is not found or the profile could not
 * be set as the default.
 */
static nsresult
SetCurrentProfileAsDefault(nsIToolkitProfileService* aProfileSvc,
                           nsIFile* aCurrentProfileRoot)
{
  NS_ENSURE_ARG_POINTER(aProfileSvc);

  nsCOMPtr<nsISimpleEnumerator> profiles;
  nsresult rv = aProfileSvc->GetProfiles(getter_AddRefs(profiles));
  if (NS_FAILED(rv))
    return rv;

  bool foundMatchingProfile = false;
  nsCOMPtr<nsIToolkitProfile> profile;
  rv = profiles->GetNext(getter_AddRefs(profile));
  while (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIFile> profileRoot;
    profile->GetRootDir(getter_AddRefs(profileRoot));
    profileRoot->Equals(aCurrentProfileRoot, &foundMatchingProfile);
    if (foundMatchingProfile && profile) {
      rv = aProfileSvc->SetSelectedProfile(profile);
      if (NS_SUCCEEDED(rv))
        rv = aProfileSvc->Flush();
      return rv;
    }
    rv = profiles->GetNext(getter_AddRefs(profile));
  }
  return rv;
}

static bool gDoMigration = false;
static bool gDoProfileReset = false;

// Pick a profile. We need to end up with a profile lock.
//
// 1) check for -profile <path>
// 2) check for -P <name>
// 3) check for -ProfileManager
// 4) use the default profile, if there is one
// 5) if there are *no* profiles, set up profile-migration
// 6) display the profile-manager UI
static nsresult
SelectProfile(nsIProfileLock* *aResult, nsIToolkitProfileService* aProfileSvc, nsINativeAppSupport* aNative,
              bool* aStartOffline, nsACString* aProfileName)
{
  nsresult rv;
  ArgResult ar;
  const char* arg;
  *aResult = nullptr;
  *aStartOffline = false;

  ar = CheckArg("offline", true);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -offline is invalid when argument -osint is specified\n");
    return NS_ERROR_FAILURE;
  }

  if (ar || EnvHasValue("XRE_START_OFFLINE"))
    *aStartOffline = true;

  if (EnvHasValue("MOZ_RESET_PROFILE_RESTART")) {
    gDoProfileReset = true;
    gDoMigration = true;
    SaveToEnv("MOZ_RESET_PROFILE_RESTART=");
  }

  // reset-profile and migration args need to be checked before any profiles are chosen below.
  ar = CheckArg("reset-profile", true);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -reset-profile is invalid when argument -osint is specified\n");
    return NS_ERROR_FAILURE;
  } else if (ar == ARG_FOUND) {
    gDoProfileReset = true;
  }

  ar = CheckArg("migration", true);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -migration is invalid when argument -osint is specified\n");
    return NS_ERROR_FAILURE;
  } else if (ar == ARG_FOUND) {
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
    if (arg && *arg && aProfileName)
      aProfileName->Assign(nsDependentCString(arg));

    // Clear out flags that we handled (or should have handled!) last startup.
    const char *dummy;
    CheckArg("p", false, &dummy);
    CheckArg("profile", false, &dummy);
    CheckArg("profilemanager");

    if (gDoProfileReset) {
      // Check that the profile to reset is the default since reset and migration are only
      // supported in that case.
      bool currentIsSelected;
      GetCurrentProfileIsDefault(aProfileSvc, lf, &currentIsSelected);
      if (!currentIsSelected) {
        NS_WARNING("Profile reset is only supported for the default profile.");
        gDoProfileReset = gDoMigration = false;
      }

      // If we're resetting a profile, create a new one and use it to startup.
      nsCOMPtr<nsIToolkitProfile> newProfile;
      rv = CreateResetProfile(aProfileSvc, getter_AddRefs(newProfile));
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

  ar = CheckArg("profile", true, &arg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -profile requires a path\n");
    return NS_ERROR_FAILURE;
  }
  if (ar) {
    if (gDoProfileReset) {
      NS_WARNING("Profile reset is only supported for the default profile.");
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

  ar = CheckArg("createprofile", true, &arg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -createprofile requires a profile name\n");
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
      
      // As with -profile, assume that the given path will be used for both the
      // main profile directory and the temp profile directory.
      rv = aProfileSvc->CreateProfile(lf, lf, nsDependentCSubstring(arg, delim),
                                     getter_AddRefs(profile));
    } else {
      rv = aProfileSvc->CreateProfile(nullptr, nullptr, nsDependentCString(arg),
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
    nsAutoCString pathStr;
    prefsJSFile->GetNativePath(pathStr);
    PR_fprintf(PR_STDERR, "Success: created profile '%s' at '%s'\n", arg, pathStr.get());
    bool exists;
    prefsJSFile->Exists(&exists);
    if (!exists)
      prefsJSFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
    // XXXdarin perhaps 0600 would be better?

    return rv;
  }

  uint32_t count;
  rv = aProfileSvc->GetProfileCount(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  ar = CheckArg("p", false, &arg);
  if (ar == ARG_BAD) {
    ar = CheckArg("osint");
    if (ar == ARG_FOUND) {
      PR_fprintf(PR_STDERR, "Error: argument -p is invalid when argument -osint is specified\n");
      return NS_ERROR_FAILURE;
    }
    return ShowProfileManager(aProfileSvc, aNative);
  }
  if (ar) {
    ar = CheckArg("osint");
    if (ar == ARG_FOUND) {
      PR_fprintf(PR_STDERR, "Error: argument -p is invalid when argument -osint is specified\n");
      return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIToolkitProfile> profile;
    rv = aProfileSvc->GetProfileByName(nsDependentCString(arg),
                                      getter_AddRefs(profile));
    if (NS_SUCCEEDED(rv)) {
      if (gDoProfileReset) {
        NS_WARNING("Profile reset is only supported for the default profile.");
        gDoProfileReset = false;
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

    return ShowProfileManager(aProfileSvc, aNative);
  }

  ar = CheckArg("profilemanager", true);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -profilemanager is invalid when argument -osint is specified\n");
    return NS_ERROR_FAILURE;
  } else if (ar == ARG_FOUND) {
    return ShowProfileManager(aProfileSvc, aNative);
  }

  if (!count) {
    gDoMigration = true;
    gDoProfileReset = false;

    // create a default profile
    nsCOMPtr<nsIToolkitProfile> profile;
    nsresult rv = aProfileSvc->CreateProfile(nullptr, // choose a default dir for us
                                             nullptr, // choose a default dir for us
                                             NS_LITERAL_CSTRING("default"),
                                             getter_AddRefs(profile));
    if (NS_SUCCEEDED(rv)) {
      aProfileSvc->Flush();
      rv = profile->Lock(nullptr, aResult);
      if (NS_SUCCEEDED(rv)) {
        if (aProfileName)
          aProfileName->Assign(NS_LITERAL_CSTRING("default"));
        return NS_OK;
      }
    }
  }

  bool useDefault = true;
  if (count > 1)
    aProfileSvc->GetStartWithLastProfile(&useDefault);

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

        nsCOMPtr<nsIToolkitProfile> newProfile;
        rv = CreateResetProfile(aProfileSvc, getter_AddRefs(newProfile));
        if (NS_SUCCEEDED(rv))
          profile = newProfile;
        else
          gDoProfileReset = false;
      }
      nsCOMPtr<nsIProfileUnlocker> unlocker;
      rv = profile->Lock(getter_AddRefs(unlocker), aResult);
      if (NS_SUCCEEDED(rv)) {
        // Try to grab the profile name.
        if (aProfileName) {
          rv = profile->GetName(*aProfileName);
          if (NS_FAILED(rv))
            aProfileName->Truncate(0);
        }
        return NS_OK;
      }

      return ProfileLockedDialog(profile, unlocker, aNative, aResult);
    }
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
  rv = NS_NewNativeLocalFile(buf, false,
                             getter_AddRefs(lf));
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

    rv = NS_NewNativeLocalFile(buf, false,
                               getter_AddRefs(lf));
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
  aXULRunnerDir->GetNativePath(platformDir);

  nsAutoCString appDir;
  if (aAppDir)
    aAppDir->GetNativePath(appDir);

  PRFileDesc *fd = nullptr;
  file->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0600, &fd);
  if (!fd) {
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

  static const char kInvalidationHeader[] = "InvalidateCaches=1" NS_LINEBREAK;
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
#elif defined(XP_WIN) || defined(XP_OS2)
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
  for (size_t i = 0; i < ArrayLength(gSavedVars); ++i) {
    const char *s = PR_GetEnv(gSavedVars[i].name);
    if (s)
      gSavedVars[i].value = PR_smprintf("%s=%s", gSavedVars[i].name, s);
  }
}

static void RestoreStateForAppInitiatedRestart()
{
  for (size_t i = 0; i < ArrayLength(gSavedVars); ++i) {
    if (gSavedVars[i].value)
      PR_SetEnv(gSavedVars[i].value);
  }
}

#ifdef MOZ_CRASHREPORTER
// When we first initialize the crash reporter we don't have a profile,
// so we set the minidump path to $TEMP.  Once we have a profile,
// we set it to $PROFILE/minidumps, creating the directory
// if needed.
static void MakeOrSetMinidumpPath(nsIFile* profD)
{
  nsCOMPtr<nsIFile> dumpD;
  profD->Clone(getter_AddRefs(dumpD));
  
  if(dumpD) {
    bool fileExists;
    //XXX: do some more error checking here
    dumpD->Append(NS_LITERAL_STRING("minidumps"));
    dumpD->Exists(&fileExists);
    if(!fileExists) {
      dumpD->Create(nsIFile::DIRECTORY_TYPE, 0700);
    }

    nsAutoString pathStr;
    if(NS_SUCCEEDED(dumpD->GetPath(pathStr)))
      CrashReporter::SetMinidumpPath(pathStr);
  }
}
#endif

const nsXREAppData* gAppData = nullptr;

#if defined(XP_OS2)
// because we use early returns, we use a stack-based helper to un-set the OS2 FP handler
class ScopedFPHandler {
private:
  EXCEPTIONREGISTRATIONRECORD excpreg;

public:
  ScopedFPHandler() { PR_OS2_SetFloatExcpHandler(&excpreg); }
  ~ScopedFPHandler() { PR_OS2_UnsetFloatExcpHandler(&excpreg); }
};
#endif

#ifdef MOZ_WIDGET_GTK
#include "prlink.h"
typedef void (*_g_set_application_name_fn)(const gchar *application_name);
typedef void (*_gtk_window_set_auto_startup_notification_fn)(gboolean setting);

static PRFuncPtr FindFunction(const char* aName)
{
  PRLibrary *lib = nullptr;
  PRFuncPtr result = PR_FindFunctionSymbolAndLibrary(aName, &lib);
  // Since the library was already loaded, we can safely unload it here.
  if (lib) {
    PR_UnloadLibrary(lib);
  }
  return result;
}

static void MOZ_gdk_display_close(GdkDisplay *display)
{
  // XXX wallpaper for bug 417163: don't close the Display if we're using the
  // Qt theme because we crash (in Qt code) when using jemalloc.
  bool theme_is_qt = false;
  GtkSettings* settings =
    gtk_settings_get_for_screen(gdk_display_get_default_screen(display));
  gchar *theme_name;
  g_object_get(settings, "gtk-theme-name", &theme_name, NULL);
  if (theme_name) {
    theme_is_qt = strcmp(theme_name, "Qt") == 0;
    if (theme_is_qt)
      NS_WARNING("wallpaper bug 417163 for Qt theme");
    g_free(theme_name);
  }

#if CLEANUP_MEMORY
  // Get a (new) Pango context that holds a reference to the fontmap that
  // GTK has been using.  gdk_pango_context_get() must be called while GTK
  // has a default display.
  PangoContext *pangoContext = gdk_pango_context_get();
#endif

  bool buggyCairoShutdown = cairo_version() < CAIRO_VERSION_ENCODE(1, 4, 0);

  if (!buggyCairoShutdown) {
    // We should shut down GDK before we shut down libraries it depends on
    // like Pango and cairo. But if cairo shutdown is buggy, we should
    // shut down cairo first otherwise it may crash because of dangling
    // references to Display objects (see bug 469831).
    if (!theme_is_qt)
      gdk_display_close(display);
  }

#if CLEANUP_MEMORY
  // This doesn't take a reference.
  PangoFontMap *fontmap = pango_context_get_font_map(pangoContext);
  // Do some shutdown of the fontmap, which releases the fonts, clearing a
  // bunch of circular references from the fontmap through the fonts back to
  // itself.  The shutdown that this does is much less than what's done by
  // the fontmap's finalize, though.
  if (PANGO_IS_FC_FONT_MAP(fontmap))
      pango_fc_font_map_shutdown(PANGO_FC_FONT_MAP(fontmap));
  g_object_unref(pangoContext);
  // PangoCairo still holds a reference to the fontmap.
  // Now that we have finished with GTK and Pango, we could unref fontmap,
  // which would allow us to call FcFini, but removing what is really
  // Pango's ref feels a bit evil.  Pango-1.22 will have support for
  // pango_cairo_font_map_set_default(NULL), which would release the
  // reference on the old fontmap.

  // cairo_debug_reset_static_data() is prototyped through cairo.h included
  // by gtk.h.
#ifdef cairo_debug_reset_static_data
#error "Looks like we're including Mozilla's cairo instead of system cairo"
#endif
  cairo_debug_reset_static_data();
#endif // CLEANUP_MEMORY

  if (buggyCairoShutdown) {
    if (!theme_is_qt)
      gdk_display_close(display);
  }
}
#endif // MOZ_WIDGET_GTK2

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

typedef HRESULT (WINAPI*DWriteCreateFactoryFunc)(
  DWRITE_FACTORY_TYPE factoryType,
  REFIID iid,
  IUnknown **factory
);

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

static DWORD InitDwriteBG(LPVOID lpdwThreadParam)
{
  SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
  LOGREGISTRY(L"loading dwrite.dll");
  HMODULE dwdll = LoadLibraryW(L"dwrite.dll");
  if (dwdll) {
    DWriteCreateFactoryFunc createDWriteFactory = (DWriteCreateFactoryFunc)
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
    mScopedXPCom(nullptr)
    , mAppData(nullptr)
    , mStartOffline(false)
    , mShuttingDown(false)
#ifdef MOZ_ENABLE_XREMOTE
    , mDisableRemote(false)
#endif
#if defined(MOZ_WIDGET_GTK)
    , mGdkDisplay(nullptr)
#endif
  {};

  ~XREMain() {
    if (mAppData) {
      delete mAppData;
    }
    if (mScopedXPCom) {
      NS_WARNING("Scoped xpcom should have been deleted!");
      delete mScopedXPCom;
    }
  }

  int XRE_main(int argc, char* argv[], const nsXREAppData* aAppData);
  int XRE_mainInit(bool* aExitFlag);
  int XRE_mainStartup(bool* aExitFlag);
  nsresult XRE_mainRun();
  
  nsCOMPtr<nsINativeAppSupport> mNativeApp;
  nsCOMPtr<nsIToolkitProfileService> mProfileSvc;
  nsCOMPtr<nsIFile> mProfD;
  nsCOMPtr<nsIFile> mProfLD;
  nsCOMPtr<nsIProfileLock> mProfileLock;
#ifdef MOZ_ENABLE_XREMOTE
  nsCOMPtr<nsIRemoteService> mRemoteService;
#endif

  ScopedXPCOMStartup* mScopedXPCom;
  ScopedAppData* mAppData;
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

  StartupTimeline::Record(StartupTimeline::MAIN);

  nsresult rv;
  ArgResult ar;

#ifdef DEBUG
  if (PR_GetEnv("XRE_MAIN_BREAK"))
    NS_BREAK();
#endif

#ifdef USE_GLX_TEST
  // bug 639842 - it's very important to fire this process BEFORE we set up
  // error handling. indeed, this process is expected to be crashy, and we
  // don't want the user to see its crashes. That's the whole reason for
  // doing this in a separate process.
  if (fire_glxtest_process()) {
    *aExitFlag = true;
    return 0;
  }
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
      
    OSVERSIONINFO vinfo;
    vinfo.dwOSVersionInfoSize = sizeof(vinfo);
    if (GetVersionEx(&vinfo) && vinfo.dwMajorVersion >= 6) {
      CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&InitDwriteBG, NULL, 0, NULL);
    }
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
  // Reset GTK_MODULES, strip atk-bridge if exists
  // Mozilla will load libatk-bridge.so later if necessary
  const char* gtkModules = PR_GetEnv("GTK_MODULES");
  if (gtkModules && *gtkModules) {
    nsCString gtkModulesStr(gtkModules);
    gtkModulesStr.ReplaceSubstring("atk-bridge", "");
    char* expr = PR_smprintf("GTK_MODULES=%s", gtkModulesStr.get());
    if (expr)
      PR_SetEnv(expr);
    // We intentionally leak |expr| here since it is required by PR_SetEnv.
  }

  // Suppress atk-bridge init at startup, it works after GNOME 2.24.2
  SaveToEnv("NO_AT_BRIDGE=1");
#endif

  // Check for application.ini overrides
  const char* override = nullptr;
  ar = CheckArg("override", true, &override);
  if (ar == ARG_BAD) {
    Output(true, "Incorrect number of arguments passed to -override");
    return 1;
  }
  else if (ar == ARG_FOUND) {
    nsCOMPtr<nsIFile> overrideLF;
    rv = XRE_GetFileFromPath(override, getter_AddRefs(overrideLF));
    if (NS_FAILED(rv)) {
      Output(true, "Error: unrecognized override.ini path.\n");
      return 1;
    }

    rv = XRE_ParseAppData(overrideLF, mAppData);
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

  if (!mAppData->xreDirectory) {
    nsCOMPtr<nsIFile> lf;
    rv = XRE_GetBinaryPath(gArgv[0], getter_AddRefs(lf));
    if (NS_FAILED(rv))
      return 2;

    nsCOMPtr<nsIFile> greDir;
    rv = lf->GetParent(getter_AddRefs(greDir));
    if (NS_FAILED(rv))
      return 2;
    
    rv = CallQueryInterface(greDir, &mAppData->xreDirectory);
    if (NS_FAILED(rv))
      return 2;
  }

  if (!mAppData->directory) {
    NS_IF_ADDREF(mAppData->directory = mAppData->xreDirectory);
  }

  if (mAppData->size > offsetof(nsXREAppData, minVersion)) {
    if (!mAppData->minVersion) {
      Output(true, "Error: Gecko:MinVersion not specified in application.ini\n");
      return 1;
    }

    if (!mAppData->maxVersion) {
      // If no maxVersion is specified, we assume the app is only compatible
      // with the initial preview release. Do not increment this number ever!
      SetAllocatedString(mAppData->maxVersion, "1.*");
    }

    if (mozilla::Version(mAppData->minVersion) > gToolkitVersion ||
        mozilla::Version(mAppData->maxVersion) < gToolkitVersion) {
      Output(true, "Error: Platform version '%s' is not compatible with\n"
             "minVersion >= %s\nmaxVersion <= %s\n",
             gToolkitVersion,
             mAppData->minVersion, mAppData->maxVersion);
      return 1;
    }
  }

  rv = mDirProvider.Initialize(mAppData->directory, mAppData->xreDirectory);
  if (NS_FAILED(rv))
    return 1;

#ifdef MOZ_CRASHREPORTER
  if (EnvHasValue("MOZ_CRASHREPORTER")) {
    mAppData->flags |= NS_XRE_ENABLE_CRASH_REPORTER;
  }

  if ((mAppData->flags & NS_XRE_ENABLE_CRASH_REPORTER) &&
      NS_SUCCEEDED(
         CrashReporter::SetExceptionHandler(mAppData->xreDirectory))) {
    if (mAppData->crashReporterURL)
      CrashReporter::SetServerURL(nsDependentCString(mAppData->crashReporterURL));

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
#endif

#ifdef XP_MACOSX
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
  

#if defined(XP_OS2)
  bool StartOS2App(int aArgc, char **aArgv);
  if (!StartOS2App(gArgc, gArgv))
    return 1;
  ScopedFPHandler handler;
#endif /* XP_OS2 */

  if (EnvHasValue("MOZ_SAFE_MODE_RESTART")) {
    gSafeMode = true;
    // unset the env variable
    SaveToEnv("MOZ_SAFE_MODE_RESTART=");
  }

  ar = CheckArg("safe-mode", true);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -safe-mode is invalid when argument -osint is specified\n");
    return 1;
  } else if (ar == ARG_FOUND) {
    gSafeMode = true;
  }

#ifdef XP_WIN
  // If the shift key is pressed and the ctrl and / or alt keys are not pressed
  // during startup start in safe mode. GetKeyState returns a short and the high
  // order bit will be 1 if the key is pressed. By masking the returned short
  // with 0x8000 the result will be 0 if the key is not pressed and non-zero
  // otherwise.
  if (GetKeyState(VK_SHIFT) & 0x8000 &&
      !(GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000)) {
    gSafeMode = true;
  }
#endif

#ifdef XP_MACOSX
  if (GetCurrentEventKeyModifiers() & optionKey)
    gSafeMode = true;
#endif

  // Handle -no-remote and -new-instance command line arguments. Setup
  // the environment to better accommodate other components and various
  // restart scenarios.
  ar = CheckArg("no-remote", true);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -no-remote is invalid when argument -osint is specified\n");
    return 1;
  } else if (ar == ARG_FOUND) {
    SaveToEnv("MOZ_NO_REMOTE=1");
  }

  ar = CheckArg("new-instance", true);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -new-instance is invalid when argument -osint is specified\n");
    return 1;
  } else if (ar == ARG_FOUND) {
    SaveToEnv("MOZ_NEW_INSTANCE=1");
  }

  // Handle -help and -version command line arguments.
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
    
#ifdef NS_TRACE_MALLOC
  gArgc = NS_TraceMallocStartupArgs(gArgc, gArgv);
#endif

  rv = XRE_InitCommandLine(gArgc, gArgv);
  NS_ENSURE_SUCCESS(rv, 1);

  // Check for -register, which registers chrome and then exits immediately.
  ar = CheckArg("register", true);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -register is invalid when argument -osint is specified\n");
    return 1;
  } else if (ar == ARG_FOUND) {
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

  if (PR_GetEnv("MOZ_RUN_GTEST")) {
    int result;
    // RunGTest will only be set if we're in xul-unit
    if (mozilla::RunGTest) {
      result = mozilla::RunGTest();
    } else {
      result = 1;
      printf("TEST-UNEXPECTED-FAIL | gtest | Not compiled with enable-tests\n");
    }
    *aExitFlag = true;
    return result;
  }

  return 0;
}

namespace mozilla {
  ShutdownChecksMode gShutdownChecks = SCM_NOTHING;
}

static void SetShutdownChecks() {
  // Set default first. On debug builds we crash. On nightly and local
  // builds we record. Nightlies will then send the info via telemetry,
  // but it is usefull to have the data in about:telemetry in local builds
  // too.

#ifdef DEBUG
  gShutdownChecks = SCM_CRASH;
#else
  const char* releaseChannel = NS_STRINGIFY(MOZ_UPDATE_CHANNEL);
  if (strcmp(releaseChannel, "nightly") == 0 ||
      strcmp(releaseChannel, "default") == 0) {
    gShutdownChecks = SCM_RECORD;
  } else {
    gShutdownChecks = SCM_NOTHING;
  }
#endif

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

#if defined(MOZ_WIDGET_GTK) || defined(MOZ_ENABLE_XREMOTE)
  // Stash DESKTOP_STARTUP_ID in malloc'ed memory because gtk_init will clear it.
#define HAVE_DESKTOP_STARTUP_ID
  const char* desktopStartupIDEnv = PR_GetEnv("DESKTOP_STARTUP_ID");
  if (desktopStartupIDEnv) {
    mDesktopStartupID.Assign(desktopStartupIDEnv);
  }
#endif

#if defined(MOZ_WIDGET_QT)
  nsQAppInstance::AddRef(gArgc, gArgv, true);

#if MOZ_PLATFORM_MAEMO > 5
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    // try to get the MInputContext if possible to support the MeeGo VKB
    QInputContext* inputContext = qApp->inputContext();
    if (inputContext && inputContext->identifierName() != "MInputContext") {
        QInputContext* context = QInputContextFactory::create("MInputContext",
                                                              qApp);
        if (context)
            qApp->setInputContext(context);
    }
  }
#endif
  QStringList nonQtArguments = qApp->arguments();
  gQtOnlyArgc = 1;
  gQtOnlyArgv = (char**) malloc(sizeof(char*) 
                * (gRestartArgc - nonQtArguments.size() + 2));

  // copy binary path
  gQtOnlyArgv[0] = gRestartArgv[0];

  for (int i = 1; i < gRestartArgc; ++i) {
    if (!nonQtArguments.contains(gRestartArgv[i])) {
      // copy arguments used by Qt for later
      gQtOnlyArgv[gQtOnlyArgc++] = gRestartArgv[i];
    }
  }
  gQtOnlyArgv[gQtOnlyArgc] = nullptr;
#endif
#if defined(MOZ_WIDGET_GTK)
  // setup for private colormap.  Ideally we'd like to do this
  // in nsAppShell::Create, but we need to get in before gtk
  // has been initialized to make sure everything is running
  // consistently.
#if (MOZ_WIDGET_GTK == 2)
  if (CheckArg("install"))
    gdk_rgb_set_install(TRUE);
#endif

  // Set program name to the one defined in application.ini.
  {
    nsAutoCString program(gAppData->name);
    ToLowerCase(program);
    g_set_prgname(program.get());
  }

  // Initialize GTK here for splash.

  // Open the display ourselves instead of using gtk_init, so that we can
  // close it without fear that one day gtk might clean up the display it
  // opens.
  if (!gtk_parse_args(&gArgc, &gArgv))
    return 1;

  // display_name is owned by gdk.
  const char *display_name = gdk_get_display_arg_name();
  if (display_name) {
    SaveWordToEnv("DISPLAY", nsDependentCString(display_name));
  } else {
    display_name = PR_GetEnv("DISPLAY");
    if (!display_name) {
      PR_fprintf(PR_STDERR, "Error: no display specified\n");
      return 1;
    }
  }
#endif /* MOZ_WIDGET_GTK2 */

#ifdef MOZ_ENABLE_XREMOTE
  // handle -remote now that xpcom is fired up
  bool newInstance;
  {
    char *e = PR_GetEnv("MOZ_NO_REMOTE");
    mDisableRemote = (e && *e);
    if (mDisableRemote) {
      newInstance = true;
    } else {
      e = PR_GetEnv("MOZ_NEW_INSTANCE");
      newInstance = (e && *e);
    }
  }

  const char* xremotearg;
  ArgResult ar = CheckArg("remote", true, &xremotearg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: -remote requires an argument\n");
    return 1;
  }
  const char* desktopStartupIDPtr =
    mDesktopStartupID.IsEmpty() ? nullptr : mDesktopStartupID.get();
  if (ar) {
    *aExitFlag = true;
    return HandleRemoteArgument(xremotearg, desktopStartupIDPtr);
  }

  if (!newInstance) {
    // Try to remote the entire command line. If this fails, start up normally.
    RemoteResult rr = RemoteCommandLine(desktopStartupIDPtr);
    if (rr == REMOTE_FOUND) {
      *aExitFlag = true;
      return 0;
    }
    else if (rr == REMOTE_ARG_BAD)
      return 1;
  }
#endif
#ifdef MOZ_X11
  // Init X11 in thread-safe mode. Must be called prior to the first call to XOpenDisplay 
  // (called inside gdk_display_open). This is a requirement for off main tread compositing.
  // This is done only on X11 platforms if the environment variable MOZ_USE_OMTC is set so 
  // as to avoid overhead when omtc is not used. 
  // An environment variable is used instead of a pref on X11 platforms because we start having 
  // access to prefs long after the first call to XOpenDisplay which is hard to change due to 
  // interdependencies in the initialization.
  if (PR_GetEnv("MOZ_USE_OMTC") ||
      PR_GetEnv("MOZ_OMTC_ENABLED")) {
    XInitThreads();
  }
#endif
#if defined(MOZ_WIDGET_GTK)
  mGdkDisplay = gdk_display_open(display_name);
  if (!mGdkDisplay) {
    PR_fprintf(PR_STDERR, "Error: cannot open display: %s\n", display_name);
    return 1;
  }
  gdk_display_manager_set_default_display (gdk_display_manager_get(),
                                           mGdkDisplay);
    
  // g_set_application_name () is only defined in glib2.2 and higher.
  _g_set_application_name_fn _g_set_application_name =
    (_g_set_application_name_fn)FindFunction("g_set_application_name");
  if (_g_set_application_name) {
    _g_set_application_name(mAppData->name);
  }
  _gtk_window_set_auto_startup_notification_fn _gtk_window_set_auto_startup_notification =
    (_gtk_window_set_auto_startup_notification_fn)FindFunction("gtk_window_set_auto_startup_notification");
  if (_gtk_window_set_auto_startup_notification) {
    _gtk_window_set_auto_startup_notification(false);
  }

#if (MOZ_WIDGET_GTK == 2)
  gtk_widget_set_default_colormap(gdk_rgb_get_colormap());
#endif /* (MOZ_WIDGET_GTK == 2) */
#endif /* defined(MOZ_WIDGET_GTK) */
#ifdef MOZ_X11
  // Do this after initializing GDK, or GDK will install its own handler.
  InstallX11ErrorHandler();
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

#if defined(USE_MOZ_UPDATER)
  // Check for and process any available updates
  nsCOMPtr<nsIFile> updRoot;
  bool persistent;
  rv = mDirProvider.GetFile(XRE_UPDATE_ROOT_DIR, &persistent,
                            getter_AddRefs(updRoot));
  // XRE_UPDATE_ROOT_DIR may fail. Fallback to appDir if failed
  if (NS_FAILED(rv))
    updRoot = mDirProvider.GetAppDir();

  // If the MOZ_PROCESS_UPDATES environment variable already exists, then
  // we are being called from the callback application.
  if (EnvHasValue("MOZ_PROCESS_UPDATES")) {
    // If the caller has asked us to log our arguments, do so.  This is used
    // to make sure that the maintenance service successfully launches the
    // callback application.
    const char *logFile = nullptr;
    if (ARG_FOUND == CheckArg("dump-args", false, &logFile)) {
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

  // Support for processing an update and exiting. The MOZ_PROCESS_UPDATES
  // environment variable will be part of the updater's environment and the
  // application that is relaunched by the updater. When the application is
  // relaunched by the updater it will be removed below and the application
  // will exit.
  if (CheckArg("process-updates")) {
    SaveToEnv("MOZ_PROCESS_UPDATES=1");
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
  if (EnvHasValue("MOZ_PROCESS_UPDATES")) {
    SaveToEnv("MOZ_PROCESS_UPDATES=");
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

#ifdef MOZ_CRASHREPORTER
  if (mAppData->flags & NS_XRE_ENABLE_CRASH_REPORTER)
      MakeOrSetMinidumpPath(mProfD);
#endif

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
  nsCOMPtr<nsIFile> fFlagFile;
  if (mAppData->directory) {
    rv = mAppData->directory->Clone(getter_AddRefs(fFlagFile));
  }
  flagFile = do_QueryInterface(fFlagFile);
  if (flagFile) {
    flagFile->AppendNative(FILE_INVALIDATE_CACHES);
  }

  bool cachesOK;
  bool versionOK = CheckCompatibility(mProfD, version, osABI, 
                                      mDirProvider.GetGREDir(),
                                      mAppData->directory, flagFile,
                                      &cachesOK);
  if (CheckArg("purgecaches")) {
    cachesOK = false;
  }
  if (PR_GetEnv("MOZ_PURGE_CACHES")) {
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

/*
 * XRE_mainRun - Command line startup, profile migration, and
 * the calling of appStartup->Run().
 */
nsresult
XREMain::XRE_mainRun()
{
  nsresult rv = NS_OK;
  NS_ASSERTION(mScopedXPCom, "Scoped xpcom not initialized.");

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

  rv = mScopedXPCom->SetWindowCreator(mNativeApp);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

#ifdef MOZ_CRASHREPORTER
  // tell the crash reporter to also send the release channel
  nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIPrefBranch> defaultPrefBranch;
    rv = prefs->GetDefaultBranch(nullptr, getter_AddRefs(defaultPrefBranch));

    if (NS_SUCCEEDED(rv)) {
      nsXPIDLCString sval;
      rv = defaultPrefBranch->GetCharPref("app.update.channel", getter_Copies(sval));
      if (NS_SUCCEEDED(rv)) {
        CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("ReleaseChannel"),
                                            sval);
      }
    }
  }
#endif

  if (mStartOffline) {
    nsCOMPtr<nsIIOService2> io (do_GetService("@mozilla.org/network/io-service;1"));
    NS_ENSURE_TRUE(io, NS_ERROR_FAILURE);
    io->SetManageOfflineStatus(false);
    io->SetOffline(true);
  }

  {
    nsCOMPtr<nsIObserver> startupNotifier
      (do_CreateInstance(NS_APPSTARTUPNOTIFIER_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    startupNotifier->Observe(nullptr, APPSTARTUP_TOPIC, nullptr);
  }

  nsCOMPtr<nsIAppStartup> appStartup
    (do_GetService(NS_APPSTARTUP_CONTRACTID));
  NS_ENSURE_TRUE(appStartup, NS_ERROR_FAILURE);

  if (gDoMigration) {
    nsCOMPtr<nsIFile> file;
    mDirProvider.GetAppDir()->Clone(getter_AddRefs(file));
    file->AppendNative(NS_LITERAL_CSTRING("override.ini"));
    nsINIParser parser;
    nsresult rv = parser.Init(file);
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
    nsCOMPtr<nsIToolkitProfile> selectedProfile;
    if (gDoProfileReset) {
      // At this point we can be sure that profile reset is happening on the default profile.
      rv = mProfileSvc->GetSelectedProfile(getter_AddRefs(selectedProfile));
      if (NS_FAILED(rv)) {
        gDoProfileReset = false;
        return NS_ERROR_FAILURE;
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
        pm->Migrate(&mDirProvider, aKey);
      }
    }

    if (gDoProfileReset) {
      nsresult backupCreated = ProfileResetCleanup(selectedProfile);
      if (NS_FAILED(backupCreated)) NS_WARNING("Could not cleanup the profile that was reset");

      // Set the new profile as the default after we're done cleaning up the old default.
      rv = SetCurrentProfileAsDefault(mProfileSvc, mProfD);
      if (NS_FAILED(rv)) NS_WARNING("Could not set current profile as the default");
    }
  }

  mDirProvider.DoStartup();

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
    // Set up ability to respond to system (Apple) events. This must be
    // done before setting up the command line service.
    SetupMacApplicationDelegate();

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
      mRemoteService->Startup(mAppData->name, mProfileName.get());
#endif /* MOZ_ENABLE_XREMOTE */

    mNativeApp->Enable();
  }

#ifdef MOZ_INSTRUMENT_EVENT_LOOP
  if (PR_GetEnv("MOZ_INSTRUMENT_EVENT_LOOP") || profiler_is_active()) {
    bool logToConsole = !!PR_GetEnv("MOZ_INSTRUMENT_EVENT_LOOP");
    mozilla::InitEventTracing(logToConsole);
  }
#endif /* MOZ_INSTRUMENT_EVENT_LOOP */

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
 */
int
XREMain::XRE_main(int argc, char* argv[], const nsXREAppData* aAppData)
{
  char aLocal;
  GeckoProfilerInitRAII profilerGuard(&aLocal);
  PROFILER_LABEL("Startup", "XRE_Main");

  nsresult rv = NS_OK;

  gArgc = argc;
  gArgv = argv;

  NS_ENSURE_TRUE(aAppData, 2);

  mAppData = new ScopedAppData(aAppData);
  if (!mAppData)
    return 1;
  // used throughout this file
  gAppData = mAppData;

  ScopedLogging log;

#if defined(MOZ_WIDGET_GTK)
#if defined(MOZ_MEMORY) || defined(__FreeBSD__) \
  || defined(__NetBSD__) && __NetBSD_Version__ >= 500000000
  // Disable the slice allocator, since jemalloc already uses similar layout
  // algorithms, and using a sub-allocator tends to increase fragmentation.
  // This must be done before g_thread_init() is called.
  g_slice_set_config(G_SLICE_CONFIG_ALWAYS_MALLOC, 1);
#endif
  g_thread_init(NULL);
#endif

  // init
  bool exit = false;
  int result = XRE_mainInit(&exit);
  if (result != 0 || exit)
    return result;

  // startup
  result = XRE_mainStartup(&exit);
  if (result != 0 || exit)
    return result;

  bool appInitiatedRestart = false;

  // Start the real application
  mScopedXPCom = new ScopedXPCOMStartup();
  if (!mScopedXPCom)
    return 1;

  rv = mScopedXPCom->Initialize();
  NS_ENSURE_SUCCESS(rv, 1);

  // run!
  rv = XRE_mainRun();

#ifdef MOZ_INSTRUMENT_EVENT_LOOP
  mozilla::ShutdownEventTracing();
#endif

  // Check for an application initiated restart.  This is one that
  // corresponds to nsIAppStartup.quit(eRestart)
  if (rv == NS_SUCCESS_RESTART_APP) {
    appInitiatedRestart = true;
  } else {
    // We will have a real shutdown, let ShutdownXPCOM poison writes to
    // find any late ones.
    mozilla::EnableWritePoisoning();
  }

  if (!mShuttingDown) {
#ifdef MOZ_ENABLE_XREMOTE
    // shut down the x remote proxy window
    if (mRemoteService) {
      mRemoteService->Shutdown();
    }
#endif /* MOZ_ENABLE_XREMOTE */
  }

  delete mScopedXPCom;
  mScopedXPCom = nullptr;

  // unlock the profile after ScopedXPCOMStartup object (xpcom) 
  // has gone out of scope.  see bug #386739 for more details
  mProfileLock->Unlock();
  gProfileLock = nullptr;

#if defined(MOZ_WIDGET_QT)
  nsQAppInstance::Release();
#endif

  // Restart the app after XPCOM has been shut down cleanly. 
  if (appInitiatedRestart) {
    RestoreStateForAppInitiatedRestart();

    // Ensure that these environment variables are set:
    SaveFileToEnvIfUnset("XRE_PROFILE_PATH", mProfD);
    SaveFileToEnvIfUnset("XRE_PROFILE_LOCAL_PATH", mProfLD);
    SaveWordToEnvIfUnset("XRE_PROFILE_NAME", mProfileName);

#ifdef MOZ_WIDGET_GTK
    MOZ_gdk_display_close(mGdkDisplay);
#endif

    rv = LaunchChild(mNativeApp, true);

#ifdef MOZ_CRASHREPORTER
    if (mAppData->flags & NS_XRE_ENABLE_CRASH_REPORTER)
      CrashReporter::UnsetExceptionHandler();
#endif
    return rv == NS_ERROR_LAUNCHED_CHILD_PROCESS ? 0 : 1;
  }

#ifdef MOZ_WIDGET_GTK
  // gdk_display_close also calls gdk_display_manager_set_default_display
  // appropriately when necessary.
  MOZ_gdk_display_close(mGdkDisplay);
#endif

#ifdef MOZ_CRASHREPORTER
  if (mAppData->flags & NS_XRE_ENABLE_CRASH_REPORTER)
      CrashReporter::UnsetExceptionHandler();
#endif

  XRE_DeinitCommandLine();

  return NS_FAILED(rv) ? 1 : 0;
}

#if defined(MOZ_METRO) && defined(XP_WIN)
extern bool XRE_MetroCoreApplicationRun();
static XREMain* xreMainPtr;

// must be called by the thread we want as the main thread
nsresult
XRE_metroStartup(bool runXREMain)
{
  nsresult rv;

  bool exit = false;
  if (xreMainPtr->XRE_mainStartup(&exit) != 0 || exit)
    return NS_ERROR_FAILURE;

  // Start the real application
  xreMainPtr->mScopedXPCom = new ScopedXPCOMStartup();
  if (!xreMainPtr->mScopedXPCom)
    return NS_ERROR_FAILURE;

  rv = xreMainPtr->mScopedXPCom->Initialize();
  NS_ENSURE_SUCCESS(rv, rv);

  if (runXREMain) {
    rv = xreMainPtr->XRE_mainRun();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

void
XRE_metroShutdown()
{
  delete xreMainPtr->mScopedXPCom;
  xreMainPtr->mScopedXPCom = nullptr;

#ifdef MOZ_INSTRUMENT_EVENT_LOOP
  mozilla::ShutdownEventTracing();
#endif

  // unlock the profile after ScopedXPCOMStartup object (xpcom) 
  // has gone out of scope.  see bug #386739 for more details
  xreMainPtr->mProfileLock->Unlock();
  gProfileLock = nullptr;

#ifdef MOZ_CRASHREPORTER
  if (xreMainPtr->mAppData->flags & NS_XRE_ENABLE_CRASH_REPORTER)
      CrashReporter::UnsetExceptionHandler();
#endif

  XRE_DeinitCommandLine();
}

class WinRTInitWrapper
{
public:
  WinRTInitWrapper() {
    mResult = ::RoInitialize(RO_INIT_MULTITHREADED);
  }
  ~WinRTInitWrapper() {
    if (SUCCEEDED(mResult)) {
      ::RoUninitialize();
    }
  }
  HRESULT mResult;
};

int
XRE_mainMetro(int argc, char* argv[], const nsXREAppData* aAppData)
{
  char aLocal;
  GeckoProfilerInitRAII profilerGuard(&aLocal);
  PROFILER_LABEL("Startup", "XRE_Main");

  nsresult rv = NS_OK;

  xreMainPtr = new XREMain();
  if (!xreMainPtr) {
    return 1;
  }

  // Inits Winrt and COM underneath it.
  WinRTInitWrapper wrap;

  gArgc = argc;
  gArgv = argv;

  NS_ENSURE_TRUE(aAppData, 2);

  xreMainPtr->mAppData = new ScopedAppData(aAppData);
  if (!xreMainPtr->mAppData)
    return 1;
  // used throughout this file
  gAppData = xreMainPtr->mAppData;

  ScopedLogging log;

  // init
  bool exit = false;
  if (xreMainPtr->XRE_mainInit(&exit) != 0 || exit)
    return 1;

  // Located in widget, will call back into XRE_metroStartup and
  // XRE_metroShutdown above.
  if (!XRE_MetroCoreApplicationRun()) {
    return 1;
  }

  // XRE_metroShutdown should have already been called on the worker
  // thread that called XRE_metroStartup.
  NS_ASSERTION(!xreMainPtr->mScopedXPCom,
               "XPCOM Shutdown hasn't occured, and we are exiting.");
  return 0;
}

void SetWindowsEnvironment(WindowsEnvironmentType aEnvID);
#endif // MOZ_METRO || !defined(XP_WIN)

void
XRE_DisableWritePoisoning(void) {
  mozilla::DisableWritePoisoning();
}

int
XRE_main(int argc, char* argv[], const nsXREAppData* aAppData, uint32_t aFlags)
{
#if !defined(MOZ_METRO) || !defined(XP_WIN)
  XREMain main;
  int result = main.XRE_main(argc, argv, aAppData);
  mozilla::RecordShutdownEndTimeStamp();
  return result;
#else
  if (aFlags == XRE_MAIN_FLAG_USE_METRO) {
    SetWindowsEnvironment(WindowsEnvironmentType_Metro);
  }

  // Desktop
  if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Desktop) {
    XREMain main;
    int result = main.XRE_main(argc, argv, aAppData);
    mozilla::RecordShutdownEndTimeStamp();
    return result;
  }

  // Metro
  NS_ASSERTION(XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Metro,
               "Unknown Windows environment");

  int result = XRE_mainMetro(argc, argv, aAppData);
  mozilla::RecordShutdownEndTimeStamp();
  return result;
#endif // MOZ_METRO || !defined(XP_WIN)
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
  rv = XRE_GetBinaryPath(aArgv[0], getter_AddRefs(binFile));
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
  ArgResult ar = CheckArg("greomni", false, &path);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -greomni requires a path argument\n");
    return NS_ERROR_FAILURE;
  }

  if (!path)
    return rv;

  nsCOMPtr<nsIFile> greOmni;
  rv = XRE_GetFileFromPath(path, getter_AddRefs(greOmni));
  if (NS_FAILED(rv)) {
    PR_fprintf(PR_STDERR, "Error: argument -greomni requires a valid path\n");
    return rv;
  }

  ar = CheckArg("appomni", false, &path);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -appomni requires a path argument\n");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIFile> appOmni;
  if (path) {
      rv = XRE_GetFileFromPath(path, getter_AddRefs(appOmni));
      if (NS_FAILED(rv)) {
        PR_fprintf(PR_STDERR, "Error: argument -appomni requires a valid path\n");
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

#ifndef XP_OS2
  InstallSignalHandlers(progname);
#endif

  // Unbuffer stdout, needed for tinderbox tests.
  setbuf(stdout, 0);
}

