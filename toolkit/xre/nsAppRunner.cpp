/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Benjamin Smedberg <benjamin@smedbergs.us>
 *   Ben Goodger <ben@mozilla.org>
 *   Fredrik Holmqvist <thesuckiestemail@yahoo.se>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#define XPCOM_TRANSLATE_NSGM_ENTRY_POINT 1

#include "nsAppRunner.h"
#include "nsUpdateDriver.h"
#include "nsBuildID.h"

#ifdef XP_MACOSX
#include "MacLaunchHelper.h"
#endif

#ifdef XP_OS2
#include "private/pprthred.h"
#endif
#include "plevent.h"
#include "prmem.h"
#include "prnetdb.h"
#include "prprf.h"
#include "prproces.h"
#include "prenv.h"

#include "nsIAppShellService.h"
#include "nsIAppStartup.h"
#include "nsIAppStartupNotifier.h"
#include "nsIArray.h"
#include "nsICategoryManager.h"
#include "nsIChromeRegistry.h"
#include "nsICommandLineRunner.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIContentHandler.h"
#include "nsIDialogParamBlock.h"
#include "nsIDOMWindow.h"
#include "nsIEventQueueService.h"
#include "nsIExtensionManager.h"
#include "nsIFastLoadService.h" // for PLATFORM_FASL_SUFFIX
#include "nsIGenericFactory.h"
#include "nsIIOService.h"
#include "nsIObserverService.h"
#include "nsINativeAppSupport.h"
#include "nsIProcess.h"
#include "nsIProfileUnlocker.h"
#include "nsIPromptService.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsISupportsPrimitives.h"
#include "nsITimelineService.h"
#include "nsIToolkitChromeRegistry.h"
#include "nsIToolkitProfile.h"
#include "nsIToolkitProfileService.h"
#include "nsIURI.h"
#include "nsIWindowCreator.h"
#include "nsIWindowMediator.h"
#include "nsIWindowWatcher.h"
#include "nsIXULAppInfo.h"
#include "nsIXULRuntime.h"

#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsEmbedCID.h"
#include "nsNetUtil.h"
#include "nsStaticComponents.h"
#include "nsXPCOM.h"
#include "nsXPIDLString.h"
#include "nsXPFEComponentsCID.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsXULAppAPI.h"
#include "nsXREDirProvider.h"
#include "nsToolkitCompsCID.h"

#include "nsINIParser.h"

#include "InstallCleanupDefines.h"

#include <stdlib.h>

#ifdef XP_UNIX
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef XP_BEOS
//BeOS has execve in unistd, but it doesn't work well enough.
//It leaves zombies around until the app that launched Firefox is closed.
//#include <unistd.h>
#include <AppKit.h>
#include <AppFileInfo.h>
#endif //XP_BEOS

#ifdef XP_WIN
#include <windows.h>
#include <process.h>
#endif

#ifdef XP_OS2
#include <process.h>
#endif

#ifdef XP_MACOSX
#include "nsILocalFileMac.h"
#include "nsCommandLineServiceMac.h"
#endif

// for X remote support
#ifdef MOZ_ENABLE_XREMOTE
#ifdef MOZ_WIDGET_PHOTON
#include "PhRemoteClient.h"
#else
#include "XRemoteClient.h"
#endif
#include "nsIRemoteService.h"
#endif

#ifdef NS_TRACE_MALLOC
#include "nsTraceMalloc.h"
#endif

#if defined(DEBUG) && defined(XP_WIN32)
#include <malloc.h>
#endif

#if defined (XP_MACOSX)
#include <Processes.h>
#endif

extern "C" void ShowOSAlert(const char* aMessage);

#define HELP_SPACER_1   "\t"
#define HELP_SPACER_2   "\t\t"
#define HELP_SPACER_4   "\t\t\t\t"

#ifdef DEBUG
#include "prlog.h"
#endif

#ifdef MOZ_JPROF
#include "jprof.h"
#endif

// on x86 linux, the current builds of some popular plugins (notably
// flashplayer and real) expect a few builtin symbols from libgcc
// which were available in some older versions of gcc.  However,
// they're _NOT_ available in newer versions of gcc (eg 3.1), so if
// we want those plugin to work with a gcc-3.1 built binary, we need
// to provide these symbols.  MOZ_ENABLE_OLD_ABI_COMPAT_WRAPPERS defaults
// to true on x86 linux, and false everywhere else.
//
// The fact that the new and free operators are mismatched 
// mirrors the way the original functions in egcs 1.1.2 worked.

#ifdef MOZ_ENABLE_OLD_ABI_COMPAT_WRAPPERS

extern "C" {

# ifndef HAVE___BUILTIN_VEC_NEW
  void *__builtin_vec_new(size_t aSize, const std::nothrow_t &aNoThrow) throw()
  {
    return ::operator new(aSize, aNoThrow);
  }
# endif

# ifndef HAVE___BUILTIN_VEC_DELETE
  void __builtin_vec_delete(void *aPtr, const std::nothrow_t &) throw ()
  {
    if (aPtr) {
      free(aPtr);
    }
  }
# endif

# ifndef HAVE___BUILTIN_NEW
	void *__builtin_new(int aSize)
  {
    return malloc(aSize);
  }
# endif

# ifndef HAVE___BUILTIN_DELETE
	void __builtin_delete(void *aPtr)
  {
    free(aPtr);
  }
# endif

# ifndef HAVE___PURE_VIRTUAL
  void __pure_virtual(void) {
    extern void __cxa_pure_virtual(void);

    __cxa_pure_virtual();
  }
# endif
}
#endif

#if defined(XP_UNIX) || defined(XP_BEOS)
  extern void InstallUnixSignalHandlers(const char *ProgramName);
#endif

#if defined(XP_WIN)
/**
 * Return a malloc'd copy of the given string surrounded with double quotes.
 */
static char *NewQuotedString(const char *input)
{
  int len = strlen(input);
  char *p = (char *) malloc(len + 3);
  p[0] = '\"';
  memcpy(&p[1], input, len);
  p[len + 1] = '\"';
  p[len + 2] = '\0';
  return p;
}
#endif

int    gArgc;
char **gArgv;

static int    gRestartArgc;
static char **gRestartArgv;

#if defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_GTK2)
#include <gtk/gtk.h>
#endif //MOZ_WIDGET_GTK || MOZ_WIDGET_GTK2

#if defined(MOZ_WIDGET_QT)
#include <qapplication.h>
#endif

static PRBool
strimatch(const char* lowerstr, const char* mixedstr)
{
  while(*lowerstr) {
    if (!*mixedstr) return PR_FALSE; // mixedstr is shorter
    if (tolower(*mixedstr) != *lowerstr) return PR_FALSE; // no match

    ++lowerstr;
    ++mixedstr;
  }

  if (*mixedstr) return PR_FALSE; // lowerstr is shorter

  return PR_TRUE;
}

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
 * @param if non-null, the -arg <data> will be stored in this pointer. This is *not*
 *        allocated, but rather a pointer to the argv data.
 */
static ArgResult
CheckArg(const char* aArg, const char **aParam = nsnull)
{
  char **curarg = gArgv + 1; // skip argv[0]

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
        RemoveArg(curarg);
        if (!aParam) {
          return ARG_FOUND;
        }

        if (*curarg) {
          if (**curarg == '-'
#if defined(XP_WIN) || defined(XP_OS2)
              || **curarg == '/'
#endif
              )
            return ARG_BAD;

          *aParam = *curarg;
          RemoveArg(curarg);
          return ARG_FOUND;
        }
        return ARG_BAD;
      }
    }

    ++curarg;
  }

  return ARG_NONE;
}

PRBool gSafeMode = PR_FALSE;

/**
 * The nsXULAppInfo object implements nsIFactory so that it can be its own
 * singleton.
 */
class nsXULAppInfo : public nsIXULAppInfo,
                     public nsIXULRuntime
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIXULAPPINFO
  NS_DECL_NSIXULRUNTIME
};

NS_INTERFACE_MAP_BEGIN(nsXULAppInfo)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXULRuntime)
  NS_INTERFACE_MAP_ENTRY(nsIXULRuntime)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIXULAppInfo, gAppData)
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
  aResult.Assign(gAppData->vendor);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetName(nsACString& aResult)
{
  aResult.Assign(gAppData->name);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetID(nsACString& aResult)
{
  aResult.Assign(gAppData->ID);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetVersion(nsACString& aResult)
{
  aResult.Assign(gAppData->version);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetPlatformVersion(nsACString& aResult)
{
  aResult.AssignLiteral(TOOLKIT_EM_VERSION);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetAppBuildID(nsACString& aResult)
{
  aResult.Assign(gAppData->buildID);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetPlatformBuildID(nsACString& aResult)
{
  aResult.Assign(NS_STRINGIFY(BUILD_ID));

  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetLogConsoleErrors(PRBool *aResult)
{
  *aResult = gLogConsoleErrors;
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::SetLogConsoleErrors(PRBool aValue)
{
  gLogConsoleErrors = aValue;
  return NS_OK;
}

NS_IMETHODIMP
nsXULAppInfo::GetInSafeMode(PRBool *aResult)
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

static const nsXULAppInfo kAppInfo;
static NS_METHOD AppInfoConstructor(nsISupports* aOuter,
                                    REFNSIID aIID, void **aResult)
{
  NS_ENSURE_NO_AGGREGATION(aOuter);

  return NS_CONST_CAST(nsXULAppInfo*, &kAppInfo)->
    QueryInterface(aIID, aResult);
}

PRBool gLogConsoleErrors
#ifdef DEBUG
         = PR_TRUE;
#else
         = PR_FALSE;
#endif

#define NS_ENSURE_TRUE_LOG(x, ret)               \
  PR_BEGIN_MACRO                                 \
  if (NS_UNLIKELY(!(x))) {                       \
    NS_WARNING("NS_ENSURE_TRUE(" #x ") failed"); \
    gLogConsoleErrors = PR_TRUE;                 \
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
    mServiceManager(nsnull) { }
  ~ScopedXPCOMStartup();

  nsresult Initialize();
  nsresult DoAutoreg();
  nsresult RegisterProfileService(nsIToolkitProfileService* aProfileService);
  nsresult InitEventQueue();
  nsresult SetWindowCreator(nsINativeAppSupport* native);

private:
  nsIServiceManager* mServiceManager;
};

ScopedXPCOMStartup::~ScopedXPCOMStartup()
{
  if (mServiceManager) {
    gDirServiceProvider->DoShutdown();

    WriteConsoleLog();

    NS_ShutdownXPCOM(mServiceManager);
    mServiceManager = nsnull;
  }
}

// {95d89e3e-a169-41a3-8e56-719978e15b12}
#define APPINFO_CID \
  { 0x95d89e3e, 0xa169, 0x41a3, { 0x8e, 0x56, 0x71, 0x99, 0x78, 0xe1, 0x5b, 0x12 } }

static nsModuleComponentInfo kComponents[] =
{
  {
    "nsXULAppInfo",
    APPINFO_CID,
    XULAPPINFO_SERVICE_CONTRACTID,
    AppInfoConstructor
  }
};

NS_IMPL_NSGETMODULE(Apprunner, kComponents)

#if !defined(_BUILD_STATIC_BIN) && !defined(MOZ_ENABLE_LIBXUL)
static nsStaticModuleInfo const kXREStaticModules[] =
{
  {
    "Apprunner",
    Apprunner_NSGetModule
  }
};

nsStaticModuleInfo const *const kPStaticModules = kXREStaticModules;
PRUint32 const kStaticModuleCount = NS_ARRAY_LENGTH(kXREStaticModules);
#endif

nsresult
ScopedXPCOMStartup::Initialize()
{
  NS_ASSERTION(gDirServiceProvider, "Should not get here!");

  nsresult rv;
  rv = NS_InitXPCOM3(&mServiceManager, gDirServiceProvider->GetAppDir(),
                     gDirServiceProvider,
                     kPStaticModules, kStaticModuleCount);
  if (NS_FAILED(rv)) {
    NS_ERROR("Couldn't start xpcom!");
    mServiceManager = nsnull;
  }
  else {
    nsCOMPtr<nsIComponentRegistrar> reg =
      do_QueryInterface(mServiceManager);
    NS_ASSERTION(reg, "Service Manager doesn't QI to Registrar.");
  }

  return rv;
}

// {0C4A446C-EE82-41f2-8D04-D366D2C7A7D4}
static const nsCID kNativeAppSupportCID =
  { 0xc4a446c, 0xee82, 0x41f2, { 0x8d, 0x4, 0xd3, 0x66, 0xd2, 0xc7, 0xa7, 0xd4 } };

// {5F5E59CE-27BC-47eb-9D1F-B09CA9049836}
static const nsCID kProfileServiceCID =
  { 0x5f5e59ce, 0x27bc, 0x47eb, { 0x9d, 0x1f, 0xb0, 0x9c, 0xa9, 0x4, 0x98, 0x36 } };

nsresult
ScopedXPCOMStartup::RegisterProfileService(nsIToolkitProfileService* aProfileService)
{
  NS_ASSERTION(mServiceManager, "Not initialized!");

  nsCOMPtr<nsIFactory> factory = do_QueryInterface(aProfileService);
  NS_ASSERTION(factory, "Supposed to be an nsIFactory!");

  nsCOMPtr<nsIComponentRegistrar> reg (do_QueryInterface(mServiceManager));
  if (!reg) return NS_ERROR_NO_INTERFACE;

  return reg->RegisterFactory(kProfileServiceCID,
                              "Toolkit Profile Service",
                              NS_PROFILESERVICE_CONTRACTID,
                              factory);
}

nsresult
ScopedXPCOMStartup::InitEventQueue()
{
  NS_TIMELINE_ENTER("init event service");
  nsresult rv;

  nsCOMPtr<nsIEventQueueService> eventQService(do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = eventQService->CreateThreadEventQueue();
  NS_TIMELINE_LEAVE("init event service");

  return rv;
}

nsresult
ScopedXPCOMStartup::DoAutoreg()
{
#ifdef DEBUG
  // _Always_ autoreg if we're in a debug build, under the assumption
  // that people are busily modifying components and will be angry if
  // their changes aren't noticed.
  nsCOMPtr<nsIComponentRegistrar> registrar
    (do_QueryInterface(mServiceManager));
  NS_ASSERTION(registrar, "Where's the component registrar?");

  registrar->AutoRegister(nsnull);
#endif

  return NS_OK;
}

/**
 * This is a little factory class that serves as a singleton-service-factory
 * for the nativeappsupport object.
 */
class nsSingletonFactory : public nsIFactory
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
nsSingletonFactory::LockFactory(PRBool)
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

  nsCOMPtr<nsIComponentRegistrar> registrar
    (do_QueryInterface(mServiceManager));
  NS_ASSERTION(registrar, "Where's the component registrar?");

  nsCOMPtr<nsIFactory> nativeFactory = new nsSingletonFactory(native);
  NS_ENSURE_TRUE(nativeFactory, NS_ERROR_OUT_OF_MEMORY);

  rv = registrar->RegisterFactory(kNativeAppSupportCID,
                                  "Native App Support",
                                  NS_NATIVEAPPSUPPORT_CONTRACTID,
                                  nativeFactory);
  NS_ENSURE_SUCCESS(rv, rv);

  // Inform the chrome registry about OS accessibility
  nsCOMPtr<nsIToolkitChromeRegistry> cr (do_GetService(NS_CHROMEREGISTRY_CONTRACTID));
  if (cr)
    cr->CheckForOSAccessibility();

  nsCOMPtr<nsIWindowCreator> creator (do_GetService(NS_APPSTARTUP_CONTRACTID));
  if (!creator) return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIWindowWatcher> wwatch
    (do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  return wwatch->SetWindowCreator(creator);
}

static void DumpArbitraryHelp()
{
  nsresult rv;

  nsXREDirProvider dirProvider;
  dirProvider.Initialize(nsnull);

  ScopedXPCOMStartup xpcom;
  xpcom.Initialize();
  xpcom.DoAutoreg();

  nsCOMPtr<nsICommandLineRunner> cmdline
    (do_CreateInstance("@mozilla.org/toolkit/command-line;1"));
  if (!cmdline)
    return;

  nsCString text;
  rv = cmdline->GetHelpText(text);
  if (NS_SUCCEEDED(rv))
    printf("%s", text.get());
}

// English text needs to go into a dtd file.
// But when this is called we have no components etc. These strings must either be
// here, or in a native resource file.
static void
DumpHelp()
{
  printf("Usage: %s [ options ... ] [URL]\n", gArgv[0]);
  printf("       where options include:\n");
  printf("\n");

#ifdef MOZ_WIDGET_GTK
  /* insert gtk options above moz options, like any other gtk app
   *
   * note: this isn't a very cool way to do things -- i'd rather get
   * these straight from a user's gtk version -- but it seems to be
   * what most gtk apps do. -dr
   */

  printf("GTK options\n");
  printf("%s--gdk-debug=FLAGS%sGdk debugging flags to set\n", HELP_SPACER_1, HELP_SPACER_2);
  printf("%s--gdk-no-debug=FLAGS%sGdk debugging flags to unset\n", HELP_SPACER_1, HELP_SPACER_2);
  printf("%s--gtk-debug=FLAGS%sGtk+ debugging flags to set\n", HELP_SPACER_1, HELP_SPACER_2);
  printf("%s--gtk-no-debug=FLAGS%sGtk+ debugging flags to unset\n", HELP_SPACER_1, HELP_SPACER_2);
  printf("%s--gtk-module=MODULE%sLoad an additional Gtk module\n", HELP_SPACER_1, HELP_SPACER_2);
  printf("%s-install%sInstall a private colormap\n", HELP_SPACER_1, HELP_SPACER_2);

  /* end gtk toolkit options */
#endif /* MOZ_WIDGET_GTK */
#if MOZ_WIDGET_XLIB
  printf("Xlib options\n");
  printf("%s-display=DISPLAY%sX display to use\n", HELP_SPACER_1, HELP_SPACER_2);
  printf("%s-visual=VISUALID%sX visual to use\n", HELP_SPACER_1, HELP_SPACER_2);
  printf("%s-install_colormap%sInstall own colormap\n", HELP_SPACER_1, HELP_SPACER_2);
  printf("%s-sync%sMake X calls synchronous\n", HELP_SPACER_1, HELP_SPACER_2);
  printf("%s-no-xshm%sDon't use X shared memory extension\n", HELP_SPACER_1, HELP_SPACER_2);

  /* end xlib toolkit options */
#endif /* MOZ_WIDGET_XLIB */
#ifdef MOZ_X11
  printf("X11 options\n");
  printf("%s--display=DISPLAY%sX display to use\n", HELP_SPACER_1, HELP_SPACER_2);
  printf("%s--sync%sMake X calls synchronous\n", HELP_SPACER_1, HELP_SPACER_2);
  printf("%s--no-xshm%sDon't use X shared memory extension\n", HELP_SPACER_1, HELP_SPACER_2);
  printf("%s--xim-preedit=STYLE\n", HELP_SPACER_1);
  printf("%s--xim-status=STYLE\n", HELP_SPACER_1);
#endif
#ifdef XP_UNIX
  printf("%s--g-fatal-warnings%sMake all warnings fatal\n", HELP_SPACER_1, HELP_SPACER_2);

  printf("\nMozilla options\n");
#endif

  printf("%s-height <value>%sSet height of startup window to <value>.\n",HELP_SPACER_1,HELP_SPACER_2);
  printf("%s-h or -help%sPrint this message.\n",HELP_SPACER_1,HELP_SPACER_2);
  printf("%s-width <value>%sSet width of startup window to <value>.\n",HELP_SPACER_1,HELP_SPACER_2);
  printf("%s-v or -version%sPrint %s version.\n",HELP_SPACER_1,HELP_SPACER_2, gAppData->name);
  printf("%s-P <profile>%sStart with <profile>.\n",HELP_SPACER_1,HELP_SPACER_2);
  printf("%s-ProfileManager%sStart with profile manager.\n",HELP_SPACER_1,HELP_SPACER_2);
  printf("%s-UILocale <locale>%sStart with <locale> resources as UI Locale.\n",HELP_SPACER_1,HELP_SPACER_2);
  printf("%s-contentLocale <locale>%sStart with <locale> resources as content Locale.\n",HELP_SPACER_1,HELP_SPACER_2);
#if defined(XP_WIN) || defined(XP_OS2)
  printf("%s-console%sStart %s with a debugging console.\n",HELP_SPACER_1,HELP_SPACER_2,gAppData->name);
#endif

  // this works, but only after the components have registered.  so if you drop in a new command line handler, -help
  // won't not until the second run.
  // out of the bug, because we ship a component.reg file, it works correctly.
  DumpArbitraryHelp();
}

// don't modify aAppDir directly... clone it first
static int
VerifyInstallation(nsIFile* aAppDir)
{
  static const char lastResortMessage[] =
    "A previous install did not complete correctly.  Finishing install.";

  // Maximum allowed / used length of alert message is 255 chars, due to restrictions on Mac.
  // Please make sure that file contents and fallback_alert_text are at most 255 chars.

  char message[256];
  PRInt32 numRead = 0;
  const char *messageToShow = lastResortMessage;

  nsresult rv;
  nsCOMPtr<nsIFile> messageFile;
  rv = aAppDir->Clone(getter_AddRefs(messageFile));
  if (NS_SUCCEEDED(rv)) {
    messageFile->AppendNative(NS_LITERAL_CSTRING("res"));
    messageFile->AppendNative(CLEANUP_MESSAGE_FILENAME);
    PRFileDesc* fd = 0;

    nsCOMPtr<nsILocalFile> lf (do_QueryInterface(messageFile));
    if (lf) {
      rv = lf->OpenNSPRFileDesc(PR_RDONLY, 0664, &fd);
      if (NS_SUCCEEDED(rv)) {
        numRead = PR_Read(fd, message, sizeof(message)-1);
        if (numRead > 0) {
          message[numRead] = 0;
          messageToShow = message;
        }
      }
    }
  }

  ShowOSAlert(messageToShow);

  nsCOMPtr<nsIFile> cleanupUtility;
  aAppDir->Clone(getter_AddRefs(cleanupUtility));
  if (!cleanupUtility) return 1;

  cleanupUtility->AppendNative(CLEANUP_UTIL);

  ScopedXPCOMStartup xpcom;
  rv = xpcom.Initialize();
  if (NS_FAILED(rv)) return 1;

  { // extra scoping needed to release things before xpcom shutdown
    //Create the process framework to run the cleanup utility
    nsCOMPtr<nsIProcess> cleanupProcess
      (do_CreateInstance(NS_PROCESS_CONTRACTID));
    rv = cleanupProcess->Init(cleanupUtility);
    if (NS_FAILED(rv)) return 1;

    rv = cleanupProcess->Run(PR_FALSE,nsnull, 0, nsnull);
    if (NS_FAILED(rv)) return 1;
  }

  return 0;
}

#ifdef DEBUG_warren
#ifdef XP_WIN
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#endif

#if defined(FREEBSD)
// pick up fpsetmask prototype.
#include <ieeefp.h>
#endif

static void
DumpVersion()
{
  printf("%s %s %s, %s\n", gAppData->vendor ? gAppData->vendor : "", gAppData->name, gAppData->version, gAppData->copyright);
}

#ifdef MOZ_ENABLE_XREMOTE
// use int here instead of a PR type since it will be returned
// from main - just to keep types consistent
static int
HandleRemoteArgument(const char* remote)
{
  nsresult rv;
  ArgResult ar;

  const char *profile = 0;
  nsCAutoString program(gAppData->name);
  ToLowerCase(program);
  const char *username = getenv("LOGNAME");

  ar = CheckArg("p", &profile);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -p requires a profile name\n");
    return 1;
  }

  const char *temp = nsnull;
  ar = CheckArg("a", &temp);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -a requires an application name\n");
    return 1;
  } else if (ar == ARG_FOUND) {
    program.Assign(temp);
  }

  ar = CheckArg("u", &username);
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
  PRBool success = PR_FALSE;
  rv = client.SendCommand(program.get(), username, profile, remote,
                           getter_Copies(response), &success);
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

static PRBool
RemoteCommandLine()
{
  nsresult rv;
  ArgResult ar;

  nsCAutoString program(gAppData->name);
  ToLowerCase(program);
  const char *username = getenv("LOGNAME");

  const char *temp = nsnull;
  ar = CheckArg("a", &temp);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -a requires an application name\n");
    return PR_FALSE;
  } else if (ar == ARG_FOUND) {
    program.Assign(temp);
  }

  ar = CheckArg("u", &username);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -u requires a username\n");
    return PR_FALSE;
  }

  XRemoteClient client;
  rv = client.Init();
  if (NS_FAILED(rv))
    return PR_FALSE;
 
  nsXPIDLCString response;
  PRBool success = PR_FALSE;
  rv = client.SendCommandLine(program.get(), username, nsnull,
                              gArgc, gArgv,
                              getter_Copies(response), &success);
  // did the command fail?
  if (NS_FAILED(rv) || !success)
    return PR_FALSE;

  return PR_TRUE;
}
#endif // MOZ_ENABLE_XREMOTE

#ifdef XP_MACOSX
static char const *gBinaryPath;
#endif

nsresult
XRE_GetBinaryPath(const char* argv0, nsILocalFile* *aResult)
{
  nsresult rv;
  nsCOMPtr<nsILocalFile> lf;

  // We need to use platform-specific hackery to find the
  // path of this executable. This is copied, with some modifications, from
  // nsGREDirServiceProvider.cpp

#ifdef XP_WIN
  char exePath[MAXPATHLEN];

  if (!::GetModuleFileName(0, exePath, MAXPATHLEN))
    return NS_ERROR_FAILURE;

  rv = NS_NewNativeLocalFile(nsDependentCString(exePath), PR_TRUE,
                             getter_AddRefs(lf));
  if (NS_FAILED(rv))
    return rv;

#elif defined(XP_MACOSX)
  if (gBinaryPath)
    return NS_NewNativeLocalFile(nsDependentCString(gBinaryPath), PR_FALSE,
                                 aResult);

  NS_NewNativeLocalFile(EmptyCString(), PR_TRUE, getter_AddRefs(lf));
  nsCOMPtr<nsILocalFileMac> lfm (do_QueryInterface(lf));
  if (!lfm)
    return NS_ERROR_FAILURE;

  // Works even if we're not bundled.
  CFBundleRef appBundle = CFBundleGetMainBundle();
  if (!appBundle)
    return NS_ERROR_FAILURE;

  CFURLRef bundleURL = CFBundleCopyExecutableURL(appBundle);
  if (!bundleURL)
    return NS_ERROR_FAILURE;

  FSRef fileRef;
  if (!CFURLGetFSRef(bundleURL, &fileRef)) {
    CFRelease(bundleURL);
    return NS_ERROR_FAILURE;
  }

  rv = lfm->InitWithFSRef(&fileRef);
  CFRelease(bundleURL);

  if (NS_FAILED(rv))
    return rv;

#elif defined(XP_UNIX)
  struct stat fileStat;
  char exePath[MAXPATHLEN];

  rv = NS_ERROR_FAILURE;

  // on unix, there is no official way to get the path of the current binary.
  // instead of using the MOZILLA_FIVE_HOME hack, which doesn't scale to
  // multiple applications, we will try a series of techniques:
  //
  // 1) look for /proc/<pid>/exe which is a symlink to the executable on newer
  //    Linux kernels
  // 2) use realpath() on argv[0], which works unless we're loaded from the
  //    PATH
  // 3) manually walk through the PATH and look for ourself
  // 4) give up

// #ifdef __linux__
#if 0
  int r = readlink("/proc/self/exe", exePath, MAXPATHLEN);

  // apparently, /proc/self/exe can sometimes return weird data... check it
  if (r > 0 && r < MAXPATHLEN && stat(exePath, &fileStat) == 0) {
    rv = NS_OK;
  }

#endif
  if (NS_FAILED(rv) &&
      realpath(argv0, exePath) && stat(exePath, &fileStat) == 0) {
    rv = NS_OK;
  }

  if (NS_FAILED(rv)) {
    const char *path = getenv("PATH");
    if (!path)
      return NS_ERROR_FAILURE;

    char *pathdup = strdup(path);
    if (!pathdup)
      return NS_ERROR_OUT_OF_MEMORY;

    PRBool found = PR_FALSE;
    char *newStr = pathdup;
    char *token;
    while ( (token = nsCRT::strtok(newStr, ":", &newStr)) ) {
      sprintf(exePath, "%s/%s", token, argv0);
      if (stat(exePath, &fileStat) == 0) {
        found = PR_TRUE;
        break;
      }
    }
    free(pathdup);
    if (!found)
      return NS_ERROR_FAILURE;
  }

  rv = NS_NewNativeLocalFile(nsDependentCString(exePath), PR_TRUE,
                             getter_AddRefs(lf));
  if (NS_FAILED(rv))
    return rv;

#elif defined(XP_OS2)
  PPIB ppib;
  PTIB ptib;
  char exePath[MAXPATHLEN];

  DosGetInfoBlocks( &ptib, &ppib);
  DosQueryModuleName( ppib->pib_hmte, MAXPATHLEN, exePath);
  rv = NS_NewNativeLocalFile(nsDependentCString(exePath), PR_TRUE,
                             getter_AddRefs(lf));
  if (NS_FAILED(rv))
    return rv;

#elif defined(XP_BEOS)
  int32 cookie = 0;
  image_info info;

  if(get_next_image_info(0, &cookie, &info) != B_OK)
    return NS_ERROR_FAILURE;

  rv = NS_NewNativeLocalFile(nsDependentCString(info.name), PR_TRUE,
                             getter_AddRefs(lf));
  if (NS_FAILED(rv))
    return rv;

#elif
#error Oops, you need platform-specific code here
#endif

  NS_ADDREF(*aResult = lf);
  return NS_OK;
}

#define NS_ERROR_LAUNCHED_CHILD_PROCESS NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_PROFILE, 200)

// If aBlankCommandLine is true, then the application will be launched with a
// blank command line instead of being launched with the same command line that
// it was initially started with.
static nsresult LaunchChild(nsINativeAppSupport* aNative,
                            PRBool aBlankCommandLine = PR_FALSE)
{
  aNative->Quit(); // release DDE mutex, if we're holding it

  // Restart this process by exec'ing it into the current process
  // if supported by the platform.  Otherwise, use NSPR.
 
  if (aBlankCommandLine) {
    gRestartArgc = 1;
    gRestartArgv[gRestartArgc] = nsnull;
  }

#if defined(XP_MACOSX)
  LaunchChildMac(gRestartArgc, gRestartArgv);
#else
  nsCOMPtr<nsILocalFile> lf;
  nsresult rv = XRE_GetBinaryPath(gArgv[0], getter_AddRefs(lf));
  if (NS_FAILED(rv))
    return rv;

  nsCAutoString exePath;
  rv = lf->GetNativePath(exePath);
  if (NS_FAILED(rv))
    return rv;

#if defined(XP_WIN)
  // We must shorten the path to a 8.3 path, since that's all _execv can
  // handle (otherwise segments after any spaces that might exist in the path
  // will be converted into parameters, and really weird things will happen)
  char shortPath[MAXPATHLEN];
  ::GetShortPathName(exePath.get(), shortPath, MAXPATHLEN);

  gRestartArgv[0] = shortPath;

  if (_execv(shortPath, gRestartArgv) == -1)
    return NS_ERROR_FAILURE;
#elif defined(XP_OS2)
  if (_execv(exePath.get(), gRestartArgv) == -1)
    return NS_ERROR_FAILURE;
#elif defined(XP_UNIX)
  if (execv(exePath.get(), gRestartArgv) == -1)
    return NS_ERROR_FAILURE;
#else
  PRProcess* process = PR_CreateProcess(exePath.get(), gRestartArgv,
                                        nsnull, nsnull);
  if (!process) return NS_ERROR_FAILURE;

  PRInt32 exitCode;
  PRStatus failed = PR_WaitProcess(process, &exitCode);
  if (failed || exitCode)
    return NS_ERROR_FAILURE;
#endif
#endif

  return NS_ERROR_LAUNCHED_CHILD_PROCESS;
}

static const char kProfileProperties[] =
  "chrome://mozapps/locale/profile/profileSelection.properties";

static nsresult
ProfileLockedDialog(nsILocalFile* aProfileDir, nsILocalFile* aProfileLocalDir,
                    nsIProfileUnlocker* aUnlocker,
                    nsINativeAppSupport* aNative, nsIProfileLock* *aResult)
{
  nsresult rv;

  ScopedXPCOMStartup xpcom;
  rv = xpcom.Initialize();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = xpcom.DoAutoreg();
  rv |= xpcom.InitEventQueue();
  rv |= xpcom.SetWindowCreator(aNative);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  { //extra scoping is needed so we release these components before xpcom shutdown
    nsCOMPtr<nsIStringBundleService> sbs
      (do_GetService(NS_STRINGBUNDLE_CONTRACTID));
    NS_ENSURE_TRUE(sbs, NS_ERROR_FAILURE);

    nsCOMPtr<nsIStringBundle> sb;
    sbs->CreateBundle(kProfileProperties, getter_AddRefs(sb));
    NS_ENSURE_TRUE_LOG(sbs, NS_ERROR_FAILURE);

    NS_ConvertUTF8toUTF16 appName(gAppData->name);
    const PRUnichar* params[] = {appName.get(), appName.get()};

    nsXPIDLString killMessage;
    static const PRUnichar kRestartNoUnlocker[] = {'r','e','s','t','a','r','t','M','e','s','s','a','g','e','N','o','U','n','l','o','c','k','e','r','\0'};
    static const PRUnichar kRestartUnlocker[] = {'r','e','s','t','a','r','t','M','e','s','s','a','g','e','U','n','l','o','c','k','e','r','\0'};

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

    PRUint32 flags = nsIPromptService::BUTTON_TITLE_OK * nsIPromptService::BUTTON_POS_0;

    if (aUnlocker) {
      flags =
        nsIPromptService::BUTTON_TITLE_CANCEL * nsIPromptService::BUTTON_POS_0 +
        nsIPromptService::BUTTON_TITLE_IS_STRING * nsIPromptService::BUTTON_POS_1 +
        nsIPromptService::BUTTON_POS_1_DEFAULT;
    }

    PRInt32 button;
    rv = ps->ConfirmEx(nsnull, killTitle, killMessage, flags,
                       killTitle, nsnull, nsnull, nsnull, nsnull, &button);
    NS_ENSURE_SUCCESS_LOG(rv, rv);

    if (button == 1 && aUnlocker) {
      rv = aUnlocker->Unlock(nsIProfileUnlocker::FORCE_QUIT);
      if (NS_FAILED(rv)) return rv;

      return NS_LockProfilePath(aProfileDir, aProfileLocalDir, nsnull, aResult);
    }

    return NS_ERROR_ABORT;
  }
}

static const char kProfileManagerURL[] =
  "chrome://mozapps/content/profile/profileSelection.xul";

static nsresult
ShowProfileManager(nsIToolkitProfileService* aProfileSvc,
                   nsINativeAppSupport* aNative)
{
  nsresult rv;

  nsCOMPtr<nsILocalFile> profD, profLD;

  {
    ScopedXPCOMStartup xpcom;
    rv = xpcom.Initialize();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = xpcom.RegisterProfileService(aProfileSvc);
    rv |= xpcom.DoAutoreg();
    rv |= xpcom.InitEventQueue();
    rv |= xpcom.SetWindowCreator(aNative);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

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

      appStartup->EnterLastWindowClosingSurvivalArea();

      nsCOMPtr<nsIDOMWindow> newWindow;
      rv = windowWatcher->OpenWindow(nsnull,
                                     kProfileManagerURL,
                                     "_blank",
                                     "centerscreen,chrome,modal,titlebar",
                                     ioParamBlock,
                                     getter_AddRefs(newWindow));

      appStartup->ExitLastWindowClosingSurvivalArea();

      NS_ENSURE_SUCCESS_LOG(rv, rv);

      aProfileSvc->Flush();

      PRInt32 dialogConfirmed;
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

      lock->Unlock();
    }
  }

  nsCAutoString path1;
  nsCAutoString path2;
  profD->GetNativePath(path1);
  profLD->GetNativePath(path2);

  static char kEnvVar1[MAXPATHLEN], kEnvVar2[MAXPATHLEN];
  sprintf(kEnvVar1, "XRE_PROFILE_PATH=%s", path1.get());
  sprintf(kEnvVar2, "XRE_PROFILE_LOCAL_PATH=%s", path2.get());
  PR_SetEnv(kEnvVar1);
  PR_SetEnv(kEnvVar2);

  PRBool offline = PR_FALSE;
  aProfileSvc->GetStartOffline(&offline);
  if (offline) {
    PR_SetEnv("XRE_START_OFFLINE=1");
  }

  return LaunchChild(aNative);
}

static nsresult
ImportProfiles(nsIToolkitProfileService* aPService,
               nsINativeAppSupport* aNative)
{
  nsresult rv;

  PR_SetEnv("XRE_IMPORT_PROFILES=1");

  // try to import old-style profiles
  { // scope XPCOM
    ScopedXPCOMStartup xpcom;
    rv = xpcom.Initialize();
    if (NS_SUCCEEDED(rv)) {
      xpcom.DoAutoreg();
      xpcom.RegisterProfileService(aPService);

      nsCOMPtr<nsIProfileMigrator> migrator
        (do_GetService(NS_PROFILEMIGRATOR_CONTRACTID));
      if (migrator) {
        migrator->Import();
      }
    }
  }

  aPService->Flush();
  return LaunchChild(aNative);
}

// Pick a profile. We need to end up with a profile lock.
//
// 1) check for -profile <path>
// 2) check for -P <name>
// 3) check for -ProfileManager
// 4) use the default profile, if there is one
// 5) if there are *no* profiles, set up profile-migration
// 6) display the profile-manager UI

static PRBool gDoMigration = PR_FALSE;

static nsresult
SelectProfile(nsIProfileLock* *aResult, nsINativeAppSupport* aNative,
              PRBool* aStartOffline)
{
  nsresult rv;
  ArgResult ar;
  const char* arg;
  *aResult = nsnull;
  *aStartOffline = PR_FALSE;

  arg = PR_GetEnv("XRE_START_OFFLINE");
  if ((arg && *arg) || CheckArg("offline"))
    *aStartOffline = PR_TRUE;

  arg = PR_GetEnv("XRE_PROFILE_PATH");
  if (arg && *arg) {
    nsCOMPtr<nsILocalFile> lf;
    rv = NS_NewNativeLocalFile(nsDependentCString(arg), PR_TRUE,
                               getter_AddRefs(lf));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILocalFile> localDir;
    arg = PR_GetEnv("XRE_PROFILE_LOCAL_PATH");
    if (arg && *arg) {
      rv = NS_NewNativeLocalFile(nsDependentCString(arg), PR_TRUE,
                                 getter_AddRefs(localDir));
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      localDir = lf;
    }

    // Clear out flags that we handled (or should have handled!) last startup.
    const char *dummy;
    CheckArg("p", &dummy);
    CheckArg("profile", &dummy);
    CheckArg("profilemanager", &dummy);

    return NS_LockProfilePath(lf, localDir, nsnull, aResult);
  }

  if (CheckArg("migration"))
    gDoMigration = PR_TRUE;

  ar = CheckArg("profile", &arg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -profile requires a path\n");
    return NS_ERROR_FAILURE;
  }
  if (ar) {
    nsCOMPtr<nsILocalFile> lf;
    rv = XRE_GetFileFromPath(arg, getter_AddRefs(lf));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIProfileUnlocker> unlocker;

    // If a profile path is specified directory on the command line, then
    // assume that the temp directory is the same as the given directory.
    rv = NS_LockProfilePath(lf, lf, getter_AddRefs(unlocker), aResult);
    if (NS_SUCCEEDED(rv))
      return rv;

    return ProfileLockedDialog(lf, lf, unlocker, aNative, aResult);
  }

  nsCOMPtr<nsIToolkitProfileService> profileSvc;
  rv = NS_NewToolkitProfileService(getter_AddRefs(profileSvc));
  NS_ENSURE_SUCCESS(rv, rv);

  ar = CheckArg("createprofile", &arg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument -createprofile requires a profile name\n");
    return NS_ERROR_FAILURE;
  }
  if (ar) {
    nsCOMPtr<nsIToolkitProfile> profile;

    const char* delim = strchr(arg, ' ');
    if (delim) {
      nsCOMPtr<nsILocalFile> lf;
      rv = NS_NewNativeLocalFile(nsDependentCString(delim + 1),
                                   PR_TRUE, getter_AddRefs(lf));
      if (NS_FAILED(rv)) {
        PR_fprintf(PR_STDERR, "Error: profile path not valid.");
        return rv;
      }
      
      // As with -profile, assume that the given path will be used for both the
      // main profile directory and the temp profile directory.
      rv = profileSvc->CreateProfile(lf, lf, nsDependentCSubstring(arg, delim),
                                     getter_AddRefs(profile));
    } else {
      rv = profileSvc->CreateProfile(nsnull, nsnull, nsDependentCString(arg),
                                     getter_AddRefs(profile));
    }
    if (NS_SUCCEEDED(rv)) {
      rv = NS_ERROR_ABORT;
      PR_fprintf(PR_STDERR, "Success: created profile '%s'\n", arg);
    }
    profileSvc->Flush();

    // XXXben need to ensure prefs.js exists here so the tinderboxes will
    //        not go orange.
    nsCOMPtr<nsILocalFile> prefsJSFile;
    profile->GetRootDir(getter_AddRefs(prefsJSFile));
    prefsJSFile->AppendNative(NS_LITERAL_CSTRING("prefs.js"));
    PRBool exists;
    prefsJSFile->Exists(&exists);
    if (!exists)
      prefsJSFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
    // XXXdarin perhaps 0600 would be better?

    return rv;
  }

  PRUint32 count;
  rv = profileSvc->GetProfileCount(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  if (gAppData->flags & NS_XRE_ENABLE_PROFILE_MIGRATOR) {
    arg = PR_GetEnv("XRE_IMPORT_PROFILES");
    if (!count && (!arg || !*arg)) {
      return ImportProfiles(profileSvc, aNative);
    }
  }

  ar = CheckArg("p", &arg);
  if (ar == ARG_BAD) {
    return ShowProfileManager(profileSvc, aNative);
  }
  if (ar) {
    nsCOMPtr<nsIToolkitProfile> profile;
    rv = profileSvc->GetProfileByName(nsDependentCString(arg),
                                      getter_AddRefs(profile));
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIProfileUnlocker> unlocker;
      rv = profile->Lock(nsnull, aResult);
      if (NS_SUCCEEDED(rv))
        return NS_OK;

      nsCOMPtr<nsILocalFile> profileDir;
      rv = profile->GetRootDir(getter_AddRefs(profileDir));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsILocalFile> profileLocalDir;
      rv = profile->GetLocalDir(getter_AddRefs(profileLocalDir));
      NS_ENSURE_SUCCESS(rv, rv);

      return ProfileLockedDialog(profileDir, profileLocalDir, unlocker,
                                 aNative, aResult);
    }

    return ShowProfileManager(profileSvc, aNative);
  }

  if (CheckArg("profilemanager")) {
    return ShowProfileManager(profileSvc, aNative);
  }

  if (!count) {
    gDoMigration = PR_TRUE;

    // create a default profile
    nsCOMPtr<nsIToolkitProfile> profile;
    nsresult rv = profileSvc->CreateProfile(nsnull, // choose a default dir for us
                                            nsnull, // choose a default dir for us
                                            NS_LITERAL_CSTRING("default"),
                                            getter_AddRefs(profile));
    if (NS_SUCCEEDED(rv)) {
      profileSvc->Flush();
      rv = profile->Lock(nsnull, aResult);
      if (NS_SUCCEEDED(rv))
        return NS_OK;
    }
  }

  PRBool useDefault = PR_TRUE;
  if (count > 1)
    profileSvc->GetStartWithLastProfile(&useDefault);

  if (useDefault) {
    nsCOMPtr<nsIToolkitProfile> profile;
    // GetSelectedProfile will auto-select the only profile if there's just one
    profileSvc->GetSelectedProfile(getter_AddRefs(profile));
    if (profile) {
      nsCOMPtr<nsIProfileUnlocker> unlocker;
      rv = profile->Lock(getter_AddRefs(unlocker), aResult);
      if (NS_SUCCEEDED(rv))
        return NS_OK;

      nsCOMPtr<nsILocalFile> profileDir;
      rv = profile->GetRootDir(getter_AddRefs(profileDir));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsILocalFile> profileLocalDir;
      rv = profile->GetRootDir(getter_AddRefs(profileLocalDir));
      NS_ENSURE_SUCCESS(rv, rv);

      return ProfileLockedDialog(profileDir, profileLocalDir, unlocker,
                                 aNative, aResult);
    }
  }

  return ShowProfileManager(profileSvc, aNative);
}

#define FILE_COMPATIBILITY_INFO NS_LITERAL_CSTRING("compatibility.ini")

static PRBool
CheckCompatibility(nsIFile* aProfileDir, const nsCString& aVersion,
                   nsIFile* aXULRunnerDir, nsIFile* aAppDir)
{
  nsCOMPtr<nsIFile> file;
  aProfileDir->Clone(getter_AddRefs(file));
  if (!file)
    return PR_FALSE;
  file->AppendNative(FILE_COMPATIBILITY_INFO);

  nsINIParser parser;
  nsCOMPtr<nsILocalFile> localFile(do_QueryInterface(file));
  nsresult rv = parser.Init(localFile);
  if (NS_FAILED(rv))
    return PR_FALSE;

  nsCAutoString buf;
  rv = parser.GetString("Compatibility", "LastVersion", buf);
  if (NS_FAILED(rv))
    return PR_FALSE;

  if (!aVersion.Equals(buf))
    return PR_FALSE;

  rv = parser.GetString("Compatibility", "LastPlatformDir", buf);
  if (NS_FAILED(rv))
    return PR_FALSE;

  nsCOMPtr<nsILocalFile> lf;
  rv = NS_NewNativeLocalFile(buf, PR_FALSE,
                             getter_AddRefs(lf));
  if (NS_FAILED(rv))
    return PR_FALSE;

  PRBool eq;
  rv = lf->Equals(aXULRunnerDir, &eq);
  if (NS_FAILED(rv) || !eq)
    return PR_FALSE;

  if (aAppDir) {
    rv = parser.GetString("Compatibility", "LastAppDir", buf);
    if (NS_FAILED(rv))
      return PR_FALSE;

    rv = NS_NewNativeLocalFile(buf, PR_FALSE,
                               getter_AddRefs(lf));
    if (NS_FAILED(rv))
      return PR_FALSE;

    rv = lf->Equals(aAppDir, &eq);
    if (NS_FAILED(rv) || !eq)
      return PR_FALSE;
  }

  return PR_TRUE;
}

static void BuildVersion(nsCString &aBuf)
{
  aBuf.Assign(gAppData->version);
  aBuf.Append('_');
  aBuf.Append(gAppData->buildID);
  aBuf.Append('/');
  aBuf.AppendLiteral(GRE_BUILD_ID);
}

static void
WriteVersion(nsIFile* aProfileDir, const nsCString& aVersion,
             nsIFile* aXULRunnerDir, nsIFile* aAppDir)
{
  nsCOMPtr<nsIFile> file;
  aProfileDir->Clone(getter_AddRefs(file));
  if (!file)
    return;
  file->AppendNative(FILE_COMPATIBILITY_INFO);

  nsCOMPtr<nsILocalFile> lf = do_QueryInterface(file);

  nsCAutoString platformDir;
  aXULRunnerDir->GetNativePath(platformDir);

  nsCAutoString appDir;
  if (aAppDir)
    aAppDir->GetNativePath(appDir);

  PRFileDesc *fd = nsnull;
  lf->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0600, &fd);
  if (!fd) {
    NS_ERROR("could not create output stream");
    return;
  }

  static const char kHeader[] = "[Compatibility]" NS_LINEBREAK
                                "LastVersion=";

  PR_Write(fd, kHeader, sizeof(kHeader) - 1);
  PR_Write(fd, aVersion.get(), aVersion.Length());

  static const char kPlatformDirHeader[] = NS_LINEBREAK "LastPlatformDir=";

  PR_Write(fd, kPlatformDirHeader, sizeof(kPlatformDirHeader) - 1);
  PR_Write(fd, platformDir.get(), platformDir.Length());

  static const char kAppDirHeader[] = NS_LINEBREAK "LastAppDir=";
  if (aAppDir) {
    PR_Write(fd, kAppDirHeader, sizeof(kAppDirHeader) - 1);
    PR_Write(fd, appDir.get(), appDir.Length());
  }

  static const char kNL[] = NS_LINEBREAK;
  PR_Write(fd, kNL, sizeof(kNL) - 1);

  PR_Close(fd);
}

static PRBool ComponentsListChanged(nsIFile* aProfileDir)
{
  nsCOMPtr<nsIFile> file;
  aProfileDir->Clone(getter_AddRefs(file));
  if (!file)
    return PR_TRUE;
  file->AppendNative(NS_LITERAL_CSTRING(".autoreg"));

  PRBool exists = PR_FALSE;
  file->Exists(&exists);
  return exists;
}

static void RemoveComponentRegistries(nsIFile* aProfileDir, nsIFile* aLocalProfileDir,
                                      PRBool aRemoveEMFiles)
{
  nsCOMPtr<nsIFile> file;
  aProfileDir->Clone(getter_AddRefs(file));
  if (!file)
    return;

  file->AppendNative(NS_LITERAL_CSTRING("compreg.dat"));
  file->Remove(PR_FALSE);

  file->SetNativeLeafName(NS_LITERAL_CSTRING("xpti.dat"));
  file->Remove(PR_FALSE);

  file->SetNativeLeafName(NS_LITERAL_CSTRING(".autoreg"));
  file->Remove(PR_FALSE);

  if (aRemoveEMFiles) {
    file->SetNativeLeafName(NS_LITERAL_CSTRING("extensions.ini"));
    file->Remove(PR_FALSE);
  }

  aLocalProfileDir->Clone(getter_AddRefs(file));
  if (!file)
    return;

  file->AppendNative(NS_LITERAL_CSTRING("XUL" PLATFORM_FASL_SUFFIX));
  file->Remove(PR_FALSE);
}

// To support application initiated restart via nsIAppStartup.quit, we
// need to save various environment variables, and then restore them
// before re-launching the application.

static struct {
  const char *name;
  char *value;
} gSavedVars[] = {
  {"XUL_APP_FILE", nsnull}
};

static void SaveStateForAppInitiatedRestart()
{
  for (size_t i = 0; i < NS_ARRAY_LENGTH(gSavedVars); ++i) {
    const char *s = PR_GetEnv(gSavedVars[i].name);
    if (s)
      gSavedVars[i].value = PR_smprintf("%s=%s", gSavedVars[i].name, s);
  }
}

static void RestoreStateForAppInitiatedRestart()
{
  for (size_t i = 0; i < NS_ARRAY_LENGTH(gSavedVars); ++i) {
    if (gSavedVars[i].value)
      PR_SetEnv(gSavedVars[i].value);
  }
}

const nsXREAppData* gAppData = nsnull;

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

#ifdef MOZ_WIDGET_GTK2
#include "prlink.h"
typedef void (*_g_set_application_name_fn)(const gchar *application_name);
#endif

int
XRE_main(int argc, char* argv[], const nsXREAppData* aAppData)
{
  nsresult rv;
  NS_TIMELINE_MARK("enter main");

#ifdef DEBUG
  if (PR_GetEnv("XRE_MAIN_BREAK"))
    NS_BREAK();
#endif

#ifdef XP_WIN32
  // Suppress the "DLL Foo could not be found" dialog, such that if dependent
  // libraries (such as GDI+) are not preset, we gracefully fail to load those
  // XPCOM components, instead of being ungraceful.
  SetErrorMode(SEM_FAILCRITICALERRORS);

#ifdef DEBUG
  // Disable small heap allocator to get heapwalk() giving us
  // accurate heap numbers. Win2k non-debug does not use small heap allocator.
  // Win2k debug seems to be still using it.
  // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/vclib/html/_crt__set_sbh_threshold.asp
  _set_sbh_threshold(0);
#endif
#endif

#if defined(XP_UNIX) || defined(XP_BEOS)
  InstallUnixSignalHandlers(argv[0]);
#endif

  // Unbuffer stdout, needed for tinderbox tests.
  setbuf(stdout, 0);

#if defined(FREEBSD)
  // Disable all SIGFPE's on FreeBSD, as it has non-IEEE-conformant fp
  // trap behavior that trips up on floating-point tests performed by
  // the JS engine.  See bugzilla bug 9967 details.
  fpsetmask(0);
#endif

  gArgc = argc;
  gArgv = argv;

  NS_ASSERTION(aAppData, "must specify XUL app data");

  // In the future when nsXREAppData is extended, this check will need to
  // have more finesse.
  if (aAppData->size < sizeof(nsXREAppData)) {
    NS_ERROR("aAppdata.size isn't set properly!");
    return 1;
  }

#ifdef XP_MACOSX
  // The xulrunner stub executable tricks CFBundleGetMainBundle on
  // purpose into lying about the main bundle path. It will set
  // XRE_BINARY_PATH to inform us of our real location.
  gBinaryPath = getenv("XRE_BINARY_PATH");

  if (gBinaryPath && !*gBinaryPath)
    gBinaryPath = nsnull;
#endif

  gAppData = aAppData;

  gRestartArgc = argc;
  gRestartArgv = (char**) malloc(sizeof(char*) * (argc + 1));
  if (!gRestartArgv) return 1;

  int i;
  for (i = 0; i < argc; ++i) {
#if defined(XP_WIN)
    if (strchr(argv[i], ' ')) {
      gRestartArgv[i] = NewQuotedString(argv[i]);
      if (!gRestartArgv[i]) return 1;
    }
    else
#endif
    gRestartArgv[i] = argv[i];
  }
  gRestartArgv[argc] = nsnull;

#if defined(XP_OS2)
  PRBool StartOS2App(int aArgc, char **aArgv);
  if (!StartOS2App(gArgc, gArgv))
    return 1;
  ScopedFPHandler handler;
#endif /* XP_OS2 */

  if (CheckArg("safe-mode"))
    gSafeMode = PR_TRUE;

  // Handle -help and -version command line arguments.
  // They should return quickly, so we deal with them here.
  if (CheckArg("h") || CheckArg("help") || CheckArg("?")) {
    DumpHelp();
    return 0;
  }

  if (CheckArg("v") || CheckArg("version")) {
    DumpVersion();
    return 0;
  }
    
#ifdef NS_TRACE_MALLOC
  gArgc = argc = NS_TraceMallocStartupArgs(argc, argv);
#endif

  nsXREDirProvider dirProvider;
  {
    rv = dirProvider.Initialize(gAppData->directory);
    if (NS_FAILED(rv))
      return 1;
  }

  // Check for -register, which registers chrome and then exits immediately.
  if (CheckArg("register")) {
    ScopedXPCOMStartup xpcom;
    rv = xpcom.Initialize();
    NS_ENSURE_SUCCESS(rv, 1);

    {
      nsCOMPtr<nsIChromeRegistry> chromeReg
        (do_GetService("@mozilla.org/chrome/chrome-registry;1"));
      NS_ENSURE_TRUE(chromeReg, 1);

      chromeReg->CheckForNewChrome();
    }
    return 0;
  }

#if defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_GTK2)
  // setup for private colormap.  Ideally we'd like to do this
  // in nsAppShell::Create, but we need to get in before gtk
  // has been initialized to make sure everything is running
  // consistently.
  if (CheckArg("install"))
    gdk_rgb_set_install(TRUE);

  // Initialize GTK+1/2 here for splash
#if defined(MOZ_WIDGET_GTK)
  gtk_set_locale();
#endif
  gtk_init(&gArgc, &gArgv);

#if defined(MOZ_WIDGET_GTK2)
  // g_set_application_name () is only defined in glib2.2 and higher.
  PRLibrary *glib2 = nsnull;
  _g_set_application_name_fn _g_set_application_name =
      (_g_set_application_name_fn)PR_FindFunctionSymbolAndLibrary("g_set_application_name", &glib2);
  if (_g_set_application_name) {
    _g_set_application_name(gAppData->name);
  }
  if (glib2) {
    PR_UnloadLibrary(glib2);
  }
#endif

  gtk_widget_set_default_visual(gdk_rgb_get_visual());
  gtk_widget_set_default_colormap(gdk_rgb_get_cmap());
#endif /* MOZ_WIDGET_GTK || MOZ_WIDGET_GTK2 */

#if defined(MOZ_WIDGET_QT)
  QApplication qapp(argc, argv);
#endif

// #if defined(MOZ_WIDGET_XLIB)
// XXXtimeless fix me! How do we get a Display from here to nsAppShell.cpp ?
// #endif
    
  // Call the code to install our handler
#ifdef MOZ_JPROF
  setupProfilingStuff();
#endif

  // Try to allocate "native app support."
  nsCOMPtr<nsINativeAppSupport> nativeApp;
  rv = NS_CreateNativeAppSupport(getter_AddRefs(nativeApp));
  if (NS_FAILED(rv))
    return 1;

  PRBool canRun = PR_FALSE;
  rv = nativeApp->Start(&canRun);
  if (NS_FAILED(rv) || !canRun) {
    return 1;
  }

  //----------------------------------------------------------------
  // We need to check if a previous installation occured and
  // if so, make sure it finished and cleaned up correctly.
  //
  // If there is an xpicleanup.dat file left around, that means the
  // previous installation did not finish correctly. We must cleanup
  // before a valid mozilla can run.
  //
  // Show the user a platform-specific Alert message, then spawn the
  // xpicleanup utility, then exit.
  //----------------------------------------------------------------
  nsCOMPtr<nsIFile> registryFile;
  rv = dirProvider.GetAppDir()->Clone(getter_AddRefs(registryFile));
  if (NS_SUCCEEDED(rv)) {
    registryFile->AppendNative(CLEANUP_REGISTRY);

    PRBool exists;
    rv = registryFile->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists) {
      return VerifyInstallation(dirProvider.GetAppDir());
    }
  }

#ifdef MOZ_ENABLE_XREMOTE
  // handle -remote now that xpcom is fired up

  const char* xremotearg;
  ArgResult ar = CheckArg("remote", &xremotearg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: -remote requires an argument\n");
    return 1;
  }
  if (ar) {
    return HandleRemoteArgument(xremotearg);
  }

  if (!PR_GetEnv("MOZ_NO_REMOTE")) {
    // Try to remote the entire command line. If this fails, start up normally.
    if (RemoteCommandLine())
      return 0;
  }
#endif

#if defined(MOZ_UPDATER)
  // Check for and process any available updates
  ProcessUpdates(dirProvider.GetAppDir(), gRestartArgc, gRestartArgv);
#endif

  nsCOMPtr<nsIProfileLock> profileLock;
  PRBool startOffline = PR_FALSE;

  rv = SelectProfile(getter_AddRefs(profileLock), nativeApp, &startOffline);
  if (rv == NS_ERROR_LAUNCHED_CHILD_PROCESS ||
      rv == NS_ERROR_ABORT) return 0;
  if (NS_FAILED(rv)) return 1;

  nsCOMPtr<nsILocalFile> profD;
  rv = profileLock->GetDirectory(getter_AddRefs(profD));
  NS_ENSURE_SUCCESS(rv, 1);

  nsCOMPtr<nsILocalFile> profLD;
  rv = profileLock->GetLocalDirectory(getter_AddRefs(profLD));
  NS_ENSURE_SUCCESS(rv, 1);

  rv = dirProvider.SetProfile(profD, profLD);
  NS_ENSURE_SUCCESS(rv, 1);

  //////////////////////// NOW WE HAVE A PROFILE ////////////////////////

  PRBool upgraded = PR_FALSE;

  nsCAutoString version;
  BuildVersion(version);

  // Check for version compatibility with the last version of the app this 
  // profile was started with.  The format of the version stamp is defined
  // by the BuildVersion function.
  PRBool versionOK = CheckCompatibility(profD, version,
                                        dirProvider.GetAppDir(),
                                        gAppData->directory);

  // Every time a profile is loaded by a build with a different version,
  // it updates the compatibility.ini file saying what version last wrote
  // the compreg.dat.  On subsequent launches if the version matches, 
  // there is no need for re-registration.  If the user loads the same
  // profile in different builds the component registry must be
  // re-generated to prevent mysterious component loading failures.
  //
  if (gSafeMode) {
    RemoveComponentRegistries(profD, profLD, PR_FALSE);
    WriteVersion(profD, NS_LITERAL_CSTRING("Safe Mode"),
                 dirProvider.GetAppDir(), gAppData->directory);
  }
  else if (versionOK) {
    if (ComponentsListChanged(profD)) {
      // Remove compreg.dat and xpti.dat, forcing component re-registration,
      // with the new list of additional components directories specified
      // in "components.ini" which we have just discovered changed since the
      // last time the application was run. 
      RemoveComponentRegistries(profD, profLD, PR_FALSE);
    }
    // Nothing need be done for the normal startup case.
  }
  else {
    // Remove compreg.dat and xpti.dat, forcing component re-registration
    // with the default set of components (this disables any potentially
    // troublesome incompatible XPCOM components). 
    RemoveComponentRegistries(profD, profLD, PR_TRUE);

    // Tell the Extension Manager it should check for incompatible 
    // Extensions and re-write the Components manifest ("components.ini")
    // with a list of XPCOM components for compatible extensions
    upgraded = PR_TRUE;

    // Write out version
    WriteVersion(profD, version,
                 dirProvider.GetAppDir(), gAppData->directory);
  }

  PRBool needsRestart = PR_FALSE;
  PRBool appInitiatedRestart = PR_FALSE;

  // Allows the user to forcefully bypass the restart process at their
  // own risk. Useful for debugging or for tinderboxes where child 
  // processes can be problematic.
  {
    // Start the real application
    ScopedXPCOMStartup xpcom;
    rv = xpcom.Initialize();
    NS_ENSURE_SUCCESS(rv, 1); 
    rv = xpcom.DoAutoreg();
    rv |= xpcom.InitEventQueue();
    rv |= xpcom.SetWindowCreator(nativeApp);
    NS_ENSURE_SUCCESS(rv, 1);

    {
      if (startOffline) {
        nsCOMPtr<nsIIOService> io (do_GetService("@mozilla.org/network/io-service;1"));
        NS_ENSURE_TRUE(io, 1);
        io->SetOffline(PR_TRUE);
      }

      {
        NS_TIMELINE_ENTER("startupNotifier");
        nsCOMPtr<nsIObserver> startupNotifier
          (do_CreateInstance(NS_APPSTARTUPNOTIFIER_CONTRACTID, &rv));
        NS_ENSURE_SUCCESS(rv, 1);

        startupNotifier->Observe(nsnull, APPSTARTUP_TOPIC, nsnull);
        NS_TIMELINE_LEAVE("startupNotifier");
      }

      nsCOMPtr<nsIAppStartup> appStartup
        (do_GetService(NS_APPSTARTUP_CONTRACTID));
      NS_ENSURE_TRUE(appStartup, 1);

      // So we can open and close windows during startup
      appStartup->EnterLastWindowClosingSurvivalArea();

      // Profile Migration
      if (gAppData->flags & NS_XRE_ENABLE_PROFILE_MIGRATOR && gDoMigration) {
        gDoMigration = PR_FALSE;
        nsCOMPtr<nsIProfileMigrator> pm
          (do_CreateInstance(NS_PROFILEMIGRATOR_CONTRACTID));
        if (pm)
          pm->Migrate(&dirProvider);
      }
      dirProvider.DoStartup();

      nsCOMPtr<nsICommandLineRunner> cmdLine
        (do_CreateInstance("@mozilla.org/toolkit/command-line;1"));
      NS_ENSURE_TRUE(cmdLine, 1);

      nsCOMPtr<nsIFile> workingDir;
      rv = NS_GetSpecialDirectory(NS_OS_CURRENT_WORKING_DIR, getter_AddRefs(workingDir));
      NS_ENSURE_SUCCESS(rv, 1);

      rv = cmdLine->Init(gArgc, gArgv,
                         workingDir, nsICommandLine::STATE_INITIAL_LAUNCH);
      NS_ENSURE_SUCCESS(rv, 1);

      /* Special-case services that need early access to the command
         line. */
      nsCOMPtr<nsIObserver> chromeObserver
        (do_GetService("@mozilla.org/chrome/chrome-registry;1"));
      if (chromeObserver) {
        chromeObserver->Observe(cmdLine, "command-line-startup", nsnull);
      }

      NS_TIMELINE_ENTER("appStartup->CreateHiddenWindow");
      rv = appStartup->CreateHiddenWindow();
      NS_TIMELINE_LEAVE("appStartup->CreateHiddenWindow");
      NS_ENSURE_SUCCESS(rv, 1);

      // Extension Compatibility Checking and Startup
      if (gAppData->flags & NS_XRE_ENABLE_EXTENSION_MANAGER) {
        nsCOMPtr<nsIExtensionManager> em(do_GetService("@mozilla.org/extensions/manager;1"));
        NS_ENSURE_TRUE(em, 1);

        if (CheckArg("install-global-extension") || CheckArg("install-global-theme")) {
          // Do the required processing and then shut down.
          em->HandleCommandLineArgs(cmdLine);
          return 0;
        }

        if (upgraded) {
          rv = em->CheckForMismatches(&needsRestart);
          if (NS_FAILED(rv)) {
            needsRestart = PR_FALSE;
            upgraded = PR_FALSE;
          }
        }

        if (!upgraded || !needsRestart)
          em->Start(cmdLine, &needsRestart);
      }

      // We want to restart no more than 2 times. The first restart,
      // NO_EM_RESTART == "0" , and the second time, "1".
      char* noEMRestart = PR_GetEnv("NO_EM_RESTART");
      if (noEMRestart && *noEMRestart && *noEMRestart == '1') {
        if (upgraded || needsRestart) {
          NS_WARNING("EM tried to force us to restart twice! Forcefully preventing that.");
        }
        needsRestart = upgraded = PR_FALSE;
      }

      if (!upgraded && !needsRestart) {
        SaveStateForAppInitiatedRestart();

        // clear out any environment variables which may have been set 
        // during the relaunch process now that we know we won't be relaunching.
        PR_SetEnv("XRE_PROFILE_PATH=");
        PR_SetEnv("XRE_PROFILE_LOCAL_PATH=");
        PR_SetEnv("XRE_START_OFFLINE=");
        PR_SetEnv("XRE_IMPORT_PROFILES=");
        PR_SetEnv("NO_EM_RESTART=");
        PR_SetEnv("XUL_APP_FILE=");
        PR_SetEnv("XRE_BINARY_PATH=");

#ifdef XP_MACOSX
        // we re-initialize the command-line service and do appleevents munging
        // after we are sure that we're not restarting
        cmdLine = do_CreateInstance("@mozilla.org/toolkit/command-line;1");
        NS_ENSURE_TRUE(cmdLine, 1);

        rv = InitializeMacCommandLine(gArgc, gArgv);
        NS_ENSURE_SUCCESS(rv, 1);

        rv = cmdLine->Init(gArgc, gArgv,
                           workingDir, nsICommandLine::STATE_INITIAL_LAUNCH);
        NS_ENSURE_SUCCESS(rv, 1);

        // Kick off the prebinding update now that we know we won't be
        // relaunching.

        UpdatePrebinding();
#endif

        rv = cmdLine->Run();
        NS_ENSURE_SUCCESS_LOG(rv, 1);

        nsCOMPtr<nsIWindowMediator> windowMediator
          (do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv));
        NS_ENSURE_SUCCESS(rv, 1);

        // Make sure there exists at least 1 window.
        nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
        rv = windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));
        NS_ENSURE_SUCCESS(rv, 1);

        PRBool more;
        windowEnumerator->HasMoreElements(&more);
        if (!more) {
          // We didn't open any windows. This is normally not a good thing,
          // so we force console logging to file.
          gLogConsoleErrors = PR_TRUE;
        }
        else {
#ifndef XP_MACOSX
          appStartup->ExitLastWindowClosingSurvivalArea();
#endif

#ifdef MOZ_ENABLE_XREMOTE
          // if we have X remote support and we have our one window up and
          // running start listening for requests on the proxy window.
          nsCOMPtr<nsIRemoteService> remoteService;
          remoteService = do_GetService("@mozilla.org/toolkit/remote-service;1");
          if (remoteService)
            remoteService->Startup(gAppData->name, nsnull);
#endif /* MOZ_ENABLE_XREMOTE */

          // enable win32 DDE responses and Mac appleevents responses
          nativeApp->Enable();

          // Start main event loop
          NS_TIMELINE_ENTER("appStartup->Run");
          rv = appStartup->Run();
          NS_TIMELINE_LEAVE("appStartup->Run");
          if (NS_FAILED(rv)) {
            NS_ERROR("failed to run appstartup");
            gLogConsoleErrors = PR_TRUE;
          }

          // Check for an application initiated restart.  This is one that
          // corresponds to nsIAppStartup.quit(eRestart)
          if (rv == NS_SUCCESS_RESTART_APP) {
            needsRestart = PR_TRUE;
            appInitiatedRestart = PR_TRUE;
          }

#ifdef MOZ_ENABLE_XREMOTE
          // shut down the x remote proxy window
          if (remoteService)
            remoteService->Shutdown();
#endif /* MOZ_ENABLE_XREMOTE */
        }

#ifdef MOZ_TIMELINE
        // Make sure we print this out even if timeline is runtime disabled
        if (NS_FAILED(NS_TIMELINE_LEAVE("main1")))
          NS_TimelineForceMark("...main1");
#endif
      }
      else {
        // Upgrade condition (build id changes), but the restart hint was 
        // not set by the Extension Manager. This is because the compatibility
        // resolution for Extensions is different than for the component 
        // registry - major milestone vs. build id. 
        needsRestart = PR_TRUE;
      }
    }

    profileLock->Unlock();
  }

  // Restart the app after XPCOM has been shut down cleanly. 
  if (needsRestart) {
    if (appInitiatedRestart) {
      RestoreStateForAppInitiatedRestart();
    }
    else {
      char* noEMRestart = PR_GetEnv("NO_EM_RESTART");
      if (noEMRestart && *noEMRestart) {
        PR_SetEnv("NO_EM_RESTART=1");
      }
      else {
        PR_SetEnv("NO_EM_RESTART=0");
      }
    }

    nsCAutoString path1, path2;
    profD->GetNativePath(path1);
    profLD->GetNativePath(path2);

    static char kEnvVar1[MAXPATHLEN], kEnvVar2[MAXPATHLEN];
    sprintf(kEnvVar1, "XRE_PROFILE_PATH=%s", path1.get());
    sprintf(kEnvVar2, "XRE_PROFILE_LOCAL_PATH=%s", path2.get());
    PR_SetEnv(kEnvVar1);
    PR_SetEnv(kEnvVar2);

#ifdef XP_MACOSX
    if (gBinaryPath) {
      static char kEnvVar3[MAXPATHLEN];
      sprintf(kEnvVar3, "XRE_BINARY_PATH=%s", gBinaryPath);
      PR_SetEnv(kEnvVar3);
    }
#endif

    rv = LaunchChild(nativeApp, appInitiatedRestart);
    return rv == NS_ERROR_LAUNCHED_CHILD_PROCESS ? 0 : 1;
  }

  return NS_FAILED(rv) ? 1 : 0;
}
