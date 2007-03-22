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
 *   Fredrik Holmqvist <thesuckiestemail@yahoo.se>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


#ifdef XPCOM_GLUE
#include "nsXPCOMGlue.h"
#endif

#include "nsStringSupport.h"

#include "nsXPCOM.h"
#include "nsIServiceManager.h"
#include "nsServiceManagerUtils.h"
#include "nsIComponentManager.h"
#include "nsComponentManagerUtils.h"
#include "nsIGenericFactory.h"
#include "nsIComponentRegistrar.h"
#include "nsStaticComponents.h"

#ifdef XP_OS2
#include "private/pprthred.h"
#endif

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsILocaleService.h"
#include "prmem.h"
#include "prenv.h"
#include "prnetdb.h"

#include "nsCOMPtr.h"
#include "nsIAppShell.h"
#include "nsICmdLineService.h"
#include "nsIAppShellService.h"
#include "nsIAppStartupNotifier.h"
#include "nsIAppStartup.h"
#include "nsIObserverService.h"
#include "prprf.h"
#include "nsCRT.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIWindowMediator.h"
#include "nsIDOMWindowInternal.h"
#include "nsISupportsPrimitives.h"
#include "nsICmdLineHandler.h"
#include "nsICategoryManager.h"
#include "nsIXULWindow.h"
#include "nsIChromeRegistrySea.h"
#include "nsDirectoryServiceDefs.h"
#include "nsBuildID.h"
#include "nsIWindowCreator.h"
#include "nsIWindowWatcher.h"
#include "nsILocalFile.h"
#include "nsILookAndFeel.h"
#include "nsIProcess.h"

#ifdef MOZ_XPINSTALL
#include "InstallCleanupDefines.h"
#include "nsISoftwareUpdate.h"
#endif

// Interfaces Needed
#include "nsIXULWindow.h"
#include "nsIWebBrowserChrome.h"
#include "nsIDocShell.h"

// for X remote support
#ifdef MOZ_ENABLE_XREMOTE
#ifdef MOZ_WIDGET_PHOTON
#include "PhRemoteClient.h"
#else
#include "XRemoteClient.h"
#endif
#include "nsIRemoteService.h"
#endif

// see DoOnShutdown()
#include "nsIProfile.h"

#ifdef NS_TRACE_MALLOC
#include "nsTraceMalloc.h"
#endif

#ifdef XP_WIN32
#include <windows.h>

#ifdef DEBUG
#include <malloc.h>
#endif
#endif

#if defined (XP_MACOSX)
#include <Processes.h>
#endif

#include "nsITimelineService.h"

#if defined(DEBUG_pra)
#define DEBUG_CMD_LINE
#endif

#include "nsWidgetsCID.h"
#include "nsXPFEComponentsCID.h"

static NS_DEFINE_CID(kLookAndFeelCID,  NS_LOOKANDFEEL_CID);

#define UILOCALE_CMD_LINE_ARG "-UILocale"
#define CONTENTLOCALE_CMD_LINE_ARG "-contentLocale"

extern "C" void ShowOSAlert(const char* aMessage);

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
#ifdef WRAP_SYSTEM_INCLUDES
#pragma GCC visibility push(default)
#endif
    extern void __cxa_pure_virtual(void);
#ifdef WRAP_SYSTEM_INCLUDES
#pragma GCC visibility pop
#endif

    __cxa_pure_virtual();
  }
# endif
}
#endif

#ifndef _BUILD_STATIC_BIN
nsStaticModuleInfo const *const kPStaticModules = nsnull;
PRUint32 const kStaticModuleCount = 0;
#endif

#if defined(XP_UNIX) || defined(XP_BEOS)
  extern void InstallUnixSignalHandlers(const char *ProgramName);
#endif

#if defined(XP_OS2)
/* Adding globals that OS/2 doesn't have so we can port the DDE code */
char **__argv;
int   *__pargc;
#endif /* XP_OS2 */

#if defined(XP_MAC)

#include "macstdlibextras.h"
#include <TextServices.h>

// Set up the toolbox and (if DEBUG) the console.  Do this in a static initializer,
// to make it as unlikely as possible that somebody calls printf() before we get initialized.
static struct MacInitializer { MacInitializer() { InitializeMacToolbox(); } } gInitializer;

// Initialize profile services for both standalone and regular profiles
static nsresult InitializeProfileService(nsICmdLineService *cmdLineArgs);

// Install global locale if possible
static nsresult InstallGlobalLocale(nsICmdLineService *cmdLineArgs);
static nsresult getUILangCountry(nsAString& aUILang, nsAString& aCountry);

class stTSMCloser
{
public:
	stTSMCloser()
  {
    // TSM is initialized in InitializeMacToolbox
  };

	~stTSMCloser()
	{
#if !TARGET_CARBON
		(void)CloseTSMAwareApplication();
#endif
	}
};
#endif // XP_MAC

#if defined(XP_MACOSX)

static void InitializeMacOSXApp(int argc, char* argv[])
{
  // use the location of the executable to learn where everything is, this
  // is because the current working directory is ill-defined when the
  // application is double-clicked from the Finder.
  char* path = strdup(argv[0]);
  char* lastSlash = strrchr(path, '/');
  if (lastSlash) {
    *lastSlash = '\0';
    setenv("MOZILLA_FIVE_HOME", path, 1);
  }
  free(path);
}

#endif /* XP_MACOSX */

#if defined(MOZ_WIDGET_QT)
#include <qapplication.h>
#endif

#ifdef MOZ_X11
#include <X11/Xlib.h>
#endif /* MOZ_X11 */

#if defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_GTK2)
#include <gtk/gtk.h>
#endif //MOZ_WIDGET_GTK || MOZ_WIDGET_GTK2

#include "nsNativeAppSupport.h"

/*********************************************/
// Default implemenations for nativeAppSupport
// If your platform implements these functions if def out this code.
#if !defined(MOZ_WIDGET_COCOA) && !defined(MOZ_WIDGET_PHOTON) && !defined( XP_WIN) && !defined(XP_OS2) && !defined(MOZ_WIDGET_GTK) && !defined(MOZ_WIDGET_GTK2)

nsresult NS_CreateSplashScreen(nsISplashScreen **aResult)
{
    nsresult rv = NS_OK;
    if (aResult) {
        *aResult = nsnull;
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

PRBool NS_CanRun()
{
	return PR_TRUE;
}
#endif

/*********************************************/
// Default implementation for new and improved
// native app support.  If your platform
// implements nsINativeAppSupport then implement
// this function and if def out this code.
//
// Note: For now, the default imiplementation returns 0 and
//       the code that calls this will defalt to use the old
//       nsISplashScreen interface directly.  At some point
//       this function will return an instance of
//       nsNativeAppSupportBase which will use the older
//       "splash screen" interface.  The code below will
//       then rely on nsINativeAppSupport and its use of
//       nsISplashScreen will be removed.
//

#if !defined(XP_WIN) && !defined(XP_OS2)&& !defined( XP_BEOS ) && !defined(MOZ_WIDGET_GTK) && !defined(MOZ_WIDGET_GTK2) && !defined(XP_MAC) && (!defined(XP_MACOSX) || defined(MOZ_WIDGET_COCOA))

nsresult NS_CreateNativeAppSupport(nsINativeAppSupport **aResult)
{
    nsresult rv = NS_OK;
    if (aResult) {
        *aResult = nsnull;
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

#endif

static nsresult GetNativeAppSupport(nsINativeAppSupport** aNativeApp)
{
    NS_ENSURE_ARG_POINTER(aNativeApp);
    *aNativeApp = nsnull;

    nsCOMPtr<nsIAppStartup> appStartup(do_GetService(NS_APPSTARTUP_CONTRACTID));
    if (appStartup)
        appStartup->GetNativeAppSupport(aNativeApp);

    return *aNativeApp ? NS_OK : NS_ERROR_FAILURE;
}

#ifdef XP_MAC
#include "nsCommandLineServiceMac.h"
#endif

static inline void
PrintUsage(void)
{
  fprintf(stderr, "Usage: apprunner <url>\n"
                  "\t<url>:  a fully defined url string like http:// etc..\n");
}

static nsresult OpenWindow(const nsCString& aChromeURL,
                           const nsString& aAppArgs,
                           PRInt32 aWidth, PRInt32 aHeight);

static nsresult OpenWindow(const nsCString& aChromeURL,
                           const nsString& aAppArgs)
{
  return OpenWindow(aChromeURL, aAppArgs,
                    nsIAppShellService::SIZE_TO_CONTENT,
                    nsIAppShellService::SIZE_TO_CONTENT);
}

static nsresult OpenWindow(const nsCString& aChromeURL,
                           PRInt32 aWidth, PRInt32 aHeight)
{
  return OpenWindow(aChromeURL, EmptyString(), aWidth, aHeight);
}

static nsresult OpenWindow(const nsCString& aChromeURL,
                           const nsString& aAppArgs,
                           PRInt32 aWidth, PRInt32 aHeight)
{

#ifdef DEBUG_CMD_LINE
  printf("OpenWindow(%s, %s, %d, %d)\n", aChromeURL.get(),
                                         NS_ConvertUTF16toUTF8(aAppArgs).get(),
                                         aWidth, aHeight);
#endif /* DEBUG_CMD_LINE */

  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  nsCOMPtr<nsISupportsString> sarg(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  if (!wwatch || !sarg)
    return NS_ERROR_FAILURE;

  // Make sure a profile is selected.

  // We need the native app support object, which we get from
  // the app shell service.  If this fails, we still proceed.
  // That's because some platforms don't have a native app
  // support implementation.  On those platforms, "ensuring a
  // profile" is moot (because they don't support "-turbo",
  // basically).  Specifically, because they don't do turbo, they will
  // *always* have a profile selected.
  nsCOMPtr<nsIAppStartup> appStartup(do_GetService(NS_APPSTARTUP_CONTRACTID));
  nsCOMPtr <nsICmdLineService> cmdLine(do_GetService(NS_COMMANDLINESERVICE_CONTRACTID));
  if (appStartup && cmdLine)
  {
    nsCOMPtr<nsINativeAppSupport> nativeApp;
    if (NS_SUCCEEDED(appStartup->GetNativeAppSupport(getter_AddRefs(nativeApp))))
    {
      // Make sure profile has been selected.
      // At this point, we have to look for failure.  That
      // handles the case where the user chooses "Exit" on
      // the profile manager window.
      if (NS_FAILED(nativeApp->EnsureProfile(cmdLine)))
        return NS_ERROR_NOT_INITIALIZED;
    }
  }

  sarg->SetData(aAppArgs);

  nsCAutoString features("chrome,dialog=no,all");
  if (aHeight != nsIAppShellService::SIZE_TO_CONTENT) {
    features.Append(",height=");
    AppendIntToString(features, aHeight);
  }
  if (aWidth != nsIAppShellService::SIZE_TO_CONTENT) {
    features.Append(",width=");
    AppendIntToString(features, aWidth);
  }

#ifdef DEBUG_CMD_LINE
  printf("features: %s...\n", features.get());
#endif /* DEBUG_CMD_LINE */

  nsCOMPtr<nsIDOMWindow> newWindow;
  return wwatch->OpenWindow(0, aChromeURL.get(), "_blank",
                            features.get(), sarg,
                            getter_AddRefs(newWindow));
}

static void DumpArbitraryHelp()
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  if(NS_SUCCEEDED(rv) && catman) {
    nsCOMPtr<nsISimpleEnumerator> e;
    rv = catman->EnumerateCategory(COMMAND_LINE_ARGUMENT_HANDLERS, getter_AddRefs(e));
    if(NS_SUCCEEDED(rv) && e) {
      while (PR_TRUE) {
        nsCOMPtr<nsISupportsCString> catEntry;
        rv = e->GetNext(getter_AddRefs(catEntry));
        if (NS_FAILED(rv) || !catEntry) break;

        nsCAutoString entryString;
        rv = catEntry->GetData(entryString);
        if (NS_FAILED(rv) || entryString.IsEmpty()) break;

        nsXPIDLCString contractidString;
        rv = catman->GetCategoryEntry(COMMAND_LINE_ARGUMENT_HANDLERS,
                                      entryString.get(),
                                      getter_Copies(contractidString));
        if (NS_FAILED(rv) || contractidString.IsEmpty()) break;

#ifdef DEBUG_CMD_LINE
        printf("cmd line handler contractid = %s\n", contractidString.get());
#endif /* DEBUG_CMD_LINE */

        nsCOMPtr <nsICmdLineHandler> handler(do_GetService(contractidString.get(), &rv));

        if (handler) {
          nsXPIDLCString commandLineArg;
          rv = handler->GetCommandLineArgument(getter_Copies(commandLineArg));
          if (NS_FAILED(rv)) continue;

          nsXPIDLCString helpText;
          rv = handler->GetHelpText(getter_Copies(helpText));
          if (NS_FAILED(rv)) continue;

          if (!commandLineArg.IsEmpty()) {
            printf("\t%s", commandLineArg.get());

            PRBool handlesArgs = PR_FALSE;
            rv = handler->GetHandlesArgs(&handlesArgs);
            if (NS_SUCCEEDED(rv) && handlesArgs) {
              printf(" <url>");
            }
            if (!helpText.IsEmpty()) {
              printf("\t\t%s\n", helpText.get());
            }
          }
        }

      }
    }
  }
  return;
}

static nsresult
LaunchApplicationWithArgs(const char *commandLineArg,
                          nsICmdLineService *cmdLineArgs,
                          const char *aParam,
                          PRInt32 height, PRInt32 width, PRBool *windowOpened)
{
  NS_ENSURE_ARG(commandLineArg);
  NS_ENSURE_ARG(cmdLineArgs);
  NS_ENSURE_ARG(aParam);
  NS_ENSURE_ARG(windowOpened);

  nsresult rv;

  nsCOMPtr <nsICmdLineHandler> handler;
  rv = cmdLineArgs->GetHandlerForParam(aParam, getter_AddRefs(handler));
  if (NS_FAILED(rv)) return rv;

  if (!handler) return NS_ERROR_FAILURE;

  nsXPIDLCString chromeUrlForTask;
  rv = handler->GetChromeUrlForTask(getter_Copies(chromeUrlForTask));
  if (NS_FAILED(rv)) return rv;

#ifdef DEBUG_CMD_LINE
  printf("XXX got this one:\t%s\n\t%s\n\n",commandLineArg,chromeUrlForTask.get());
#endif /* DEBUG_CMD_LINE */

  nsXPIDLCString cmdResult;
  rv = cmdLineArgs->GetCmdLineValue(commandLineArg, getter_Copies(cmdResult));
  if (NS_FAILED(rv)) return rv;
#ifdef DEBUG_CMD_LINE
  printf("%s, cmdResult = %s\n",commandLineArg,cmdResult.get());
#endif /* DEBUG_CMD_LINE */

  PRBool handlesArgs = PR_FALSE;
  rv = handler->GetHandlesArgs(&handlesArgs);
  if (handlesArgs) {
    if (!cmdResult.IsEmpty()) {
      if (strcmp("1", cmdResult.get())) {
        PRBool openWindowWithArgs = PR_TRUE;
        rv = handler->GetOpenWindowWithArgs(&openWindowWithArgs);
        if (NS_FAILED(rv)) return rv;

        if (openWindowWithArgs) {
          nsAutoString cmdArgs;
          NS_CopyNativeToUnicode(cmdResult, cmdArgs);
#ifdef DEBUG_CMD_LINE
          printf("opening %s with %s\n", chromeUrlForTask.get(), "OpenWindow");
#endif /* DEBUG_CMD_LINE */
          rv = OpenWindow(chromeUrlForTask, cmdArgs);
        }
        else {
#ifdef DEBUG_CMD_LINE
          printf("opening %s with %s\n", cmdResult.get(), "OpenWindow");
#endif /* DEBUG_CMD_LINE */
          rv = OpenWindow(cmdResult, width, height);
          if (NS_FAILED(rv)) return rv;
        }
        // If we get here without an error, then a window was opened OK.
        if (NS_SUCCEEDED(rv)) {
          *windowOpened = PR_TRUE;
        }
      }
      else {
        nsXPIDLString defaultArgs;
        rv = handler->GetDefaultArgs(getter_Copies(defaultArgs));
        if (NS_FAILED(rv)) return rv;

        rv = OpenWindow(chromeUrlForTask, defaultArgs);
        if (NS_FAILED(rv)) return rv;
        // Window was opened OK.
        *windowOpened = PR_TRUE;
      }
    }
  }
  else {
    if (NS_SUCCEEDED(rv) && !cmdResult.IsEmpty()) {
      if (strcmp("1", cmdResult.get()) == 0) {
        rv = OpenWindow(chromeUrlForTask, width, height);
        if (NS_FAILED(rv)) return rv;
      }
      else {
        rv = OpenWindow(cmdResult, width, height);
        if (NS_FAILED(rv)) return rv;
      }
      // If we get here without an error, then a window was opened OK.
      if (NS_SUCCEEDED(rv)) {
        *windowOpened = PR_TRUE;
      }
    }
  }

  return NS_OK;
}

static PRBool IsStartupCommand(const char *arg)
{
  if (!arg || (arg[0] == '\0') || (arg[1] == '\0'))
    return PR_FALSE;

  // windows allows /mail or -mail
  if ((arg[0] == '-')
#if defined(XP_WIN) || defined(XP_OS2)
      || (arg[0] == '/')
#endif /* XP_WIN || XP_OS2 */
      ) {
    return PR_TRUE;
  }

  return PR_FALSE;
}


// This should be done by app shell enumeration someday
nsresult DoCommandLines(nsICmdLineService* cmdLineArgs, PRBool *windowOpened)
{
  NS_ENSURE_ARG(windowOpened);
  *windowOpened = PR_FALSE;

  nsresult rv;

	PRInt32 height = nsIAppShellService::SIZE_TO_CONTENT;
	PRInt32 width  = nsIAppShellService::SIZE_TO_CONTENT;
	nsXPIDLCString tempString;

	// Get the value of -width option
	rv = cmdLineArgs->GetCmdLineValue("-width", getter_Copies(tempString));
	if (NS_SUCCEEDED(rv) && !tempString.IsEmpty())
    PR_sscanf(tempString.get(), "%d", &width);

	// Get the value of -height option
	rv = cmdLineArgs->GetCmdLineValue("-height", getter_Copies(tempString));
	if (NS_SUCCEEDED(rv) && !tempString.IsEmpty())
    PR_sscanf(tempString.get(), "%d", &height);
  
    PRInt32 argc = 0;
    rv = cmdLineArgs->GetArgc(&argc);
  if (NS_FAILED(rv)) 
    return rv;

    char **argv = nsnull;
    rv = cmdLineArgs->GetArgv(&argv);
  if (NS_FAILED(rv)) 
    return rv;

  // first apply command line args
  // if there is any window opened, startup preferences are ignored
    PRInt32 i = 0;
    for (i=1;i<argc;i++) {
#ifdef DEBUG_CMD_LINE
      printf("XXX argv[%d] = %s\n",i,argv[i]);
#endif /* DEBUG_CMD_LINE */
      if (IsStartupCommand(argv[i])) {

        // skip over the - (or / on windows)
        char *command = argv[i] + 1;
#ifdef XP_UNIX
        // unix allows -mail and --mail
        if ((argv[i][0] == '-') && (argv[i][1] == '-')) {
          command = argv[i] + 2;
        }
#endif /* XP_UNIX */

        // this can fail, as someone could do -foo, where -foo is not handled
        rv = LaunchApplicationWithArgs((const char *)(argv[i]),
                                       cmdLineArgs, command,
                                       height, width, windowOpened);
        if (rv == NS_ERROR_NOT_AVAILABLE || rv == NS_ERROR_ABORT)
          return rv;
      }
    }
   // second if an URL is given in the command line
   // then open a browser window with the URL
  nsXPIDLCString urlToLoad;
  rv = cmdLineArgs->GetURLToLoad(getter_Copies(urlToLoad));
  if (NS_SUCCEEDED(rv) && !urlToLoad.IsEmpty()) {
    nsCOMPtr<nsICmdLineHandler> handler(
      do_GetService("@mozilla.org/commandlinehandler/general-startup;1?type=browser"));

    nsXPIDLCString chromeUrlForTask;
    rv = handler->GetChromeUrlForTask(getter_Copies(chromeUrlForTask));

    if (NS_SUCCEEDED(rv)) {
      // convert the cmdLine URL to Unicode
      nsAutoString url;
      NS_CopyNativeToUnicode(urlToLoad, url);
      rv = OpenWindow(chromeUrlForTask, url, width, height);
    }

    if (NS_SUCCEEDED(rv)) {
      *windowOpened = PR_TRUE;
    }
  }

  // third if no window is opened then apply the startup preferences
  if (!*windowOpened) {
    nsCOMPtr<nsIAppStartup> appStartup(do_GetService(NS_APPSTARTUP_CONTRACTID, &rv));
    if (NS_FAILED(rv))
      return rv;
    rv = appStartup->CreateStartupState(width, height, windowOpened);
    if (NS_FAILED(rv))
      return rv;
  }
  return NS_OK;
}

static nsresult DoOnShutdown()
{
  nsresult rv;

  // save the prefs, in case they weren't saved
  {
    // scoping this in a block to force release
    nsCOMPtr<nsIPrefService> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get prefs, so unable to save them");
    if (NS_SUCCEEDED(rv))
      prefs->SavePrefFile(nsnull);
  }

  // call ShutDownCurrentProfile() so we update the last modified time of the profile
  {
    // scoping this in a block to force release
    nsCOMPtr<nsIProfile> profileMgr(do_GetService(NS_PROFILE_CONTRACTID, &rv));
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get profile manager, so unable to update last modified time");
    if (NS_SUCCEEDED(rv)) {
      profileMgr->ShutDownCurrentProfile(nsIProfile::SHUTDOWN_PERSIST);
    }
  }

  return rv;
}

// match OS locale
static char kMatchOSLocalePref[] = "intl.locale.matchOS";

nsresult
getCountry(const nsAString& lc_name, nsAString& aCountry)
{
#ifndef MOZILLA_INTERNAL_API
  const PRUnichar *begin = lc_name.BeginReading();
  const PRUnichar *end   = lc_name.EndReading();
  while (begin != end) {
    if (*begin == '-')
      break;
    ++begin;
  }

  if (begin == end)
    return NS_ERROR_FAILURE;

  aCountry.Assign(begin + 1, end - begin);
#else
  PRInt32 i = lc_name.FindChar('-');
  if (i == kNotFound)
    return NS_ERROR_FAILURE;
  aCountry = Substring(lc_name, i + 1, PR_UINT32_MAX);
#endif
  return NS_OK;
}

static nsresult
getUILangCountry(nsAString& aUILang, nsAString& aCountry)
{
  nsresult	 result;
  // get a locale service 
  nsCOMPtr<nsILocaleService> localeService = do_GetService(NS_LOCALESERVICE_CONTRACTID, &result);
  NS_ASSERTION(NS_SUCCEEDED(result),"getUILangCountry: get locale service failed");

  result = localeService->GetLocaleComponentForUserAgent(aUILang);
  NS_ASSERTION(NS_SUCCEEDED(result), "getUILangCountry: get locale component failed");
  result = getCountry(aUILang, aCountry);
  return result;
}

// update global locale if possible (in case when user-*.rdf can be updated)
// so that any apps after this can be invoked in the UILocale and contentLocale
static nsresult InstallGlobalLocale(nsICmdLineService *cmdLineArgs)
{
    nsresult rv = NS_OK;

    // check the pref first
    nsCOMPtr<nsIPrefBranch> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID));
    PRBool matchOS = PR_FALSE;
    if (prefService)
      prefService->GetBoolPref(kMatchOSLocalePref, &matchOS);

    // match os locale
    nsAutoString uiLang;
    nsAutoString country;
    if (matchOS) {
      // compute lang and region code only when needed!
      rv = getUILangCountry(uiLang, country);
    }

    nsXPIDLCString cmdUI;
    rv = cmdLineArgs->GetCmdLineValue(UILOCALE_CMD_LINE_ARG, getter_Copies(cmdUI));
    if (NS_SUCCEEDED(rv)){
        if (!cmdUI.IsEmpty()) {
            nsCOMPtr<nsIChromeRegistrySea> chromeRegistry = do_GetService(NS_CHROMEREGISTRY_CONTRACTID, &rv);
            if (chromeRegistry)
                rv = chromeRegistry->SelectLocale(cmdUI, PR_FALSE);
        }
    }
    // match OS when no cmdline override
    if (cmdUI.IsEmpty() && matchOS) {
      nsCOMPtr<nsIChromeRegistrySea> chromeRegistry = do_GetService(NS_CHROMEREGISTRY_CONTRACTID, &rv);
      if (chromeRegistry) {
        chromeRegistry->SetRuntimeProvider(PR_TRUE);
        rv = chromeRegistry->SelectLocale(NS_ConvertUTF16toUTF8(uiLang), PR_FALSE);
      }
    }

    nsXPIDLCString cmdContent;
    rv = cmdLineArgs->GetCmdLineValue(CONTENTLOCALE_CMD_LINE_ARG, getter_Copies(cmdContent));
    if (NS_SUCCEEDED(rv)){
        if (!cmdContent.IsEmpty()) {
            nsCOMPtr<nsIChromeRegistrySea> chromeRegistry = do_GetService(NS_CHROMEREGISTRY_CONTRACTID, &rv);
            if(chromeRegistry)
                rv = chromeRegistry->SelectLocale(cmdContent, PR_FALSE);
        }
    }
    // match OS when no cmdline override
    if (cmdContent.IsEmpty() && matchOS) {
      nsCOMPtr<nsIChromeRegistrySea> chromeRegistry = do_GetService(NS_CHROMEREGISTRY_CONTRACTID, &rv);
      if (chromeRegistry) {
        chromeRegistry->SetRuntimeProvider(PR_TRUE);        
        rv = chromeRegistry->SelectLocale(NS_ConvertUTF16toUTF8(country), PR_FALSE);
      }
    }

    return NS_OK;
}

// Use classic skin if OS has indicated that the 
// current theme is being used for accessibility.
static void CheckUseAccessibleSkin()
{
    PRInt32 useAccessibilityTheme = 0;

    nsCOMPtr<nsILookAndFeel> lookAndFeel = do_GetService(kLookAndFeelCID);
    if (lookAndFeel) {
      lookAndFeel->GetMetric(nsILookAndFeel::eMetric_UseAccessibilityTheme, 
                             useAccessibilityTheme);
    }

    if (useAccessibilityTheme) {
      // Use classic skin, it obeys the system's accessibility theme
      nsCOMPtr<nsIChromeRegistrySea> chromeRegistry = do_GetService(NS_CHROMEREGISTRY_CONTRACTID);
      if (chromeRegistry) {
        chromeRegistry->SetRuntimeProvider(PR_TRUE);  // The skin change isn't permanent
        chromeRegistry->SelectSkin(NS_LITERAL_CSTRING("classic/1.0"), PR_TRUE);
      }
    }
}

static nsresult InitializeProfileService(nsICmdLineService *cmdLineArgs)
{
    // If we are being launched in -turbo mode, we cannot show UI
    PRBool shouldShowUI = PR_TRUE;
    nsCOMPtr<nsINativeAppSupport> nativeApp;
    if (NS_SUCCEEDED(GetNativeAppSupport(getter_AddRefs(nativeApp))))
      nativeApp->GetShouldShowUI(&shouldShowUI);
    // If we were launched with -silent, we cannot show UI, either.
    if (shouldShowUI) {
      nsXPIDLCString arg;
      if (NS_SUCCEEDED(cmdLineArgs->GetCmdLineValue("-silent", getter_Copies(arg)))) {
        if (!arg.IsEmpty()) {
          shouldShowUI = PR_FALSE;
        }
      }
    }
    nsresult rv;
    nsCOMPtr<nsIAppStartup> appStartup(do_GetService(NS_APPSTARTUP_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;
    rv = appStartup->DoProfileStartup(cmdLineArgs, shouldShowUI);

    return rv;
}

// Maximum allowed / used length of alert message is 255 chars, due to restrictions on Mac.
// Please make sure that file contents and fallback_alert_text are at most 255 chars.
// Fallback_alert_text must be non-const, because of inplace conversion on Mac.
static void ShowOSAlertFromFile(int argc, char **argv, const char *alert_filename, const char* fallback_alert_text)
{
  char message[256] = { 0 };
  PRInt32 numRead = 0;
  const char *messageToShow = fallback_alert_text;
  nsresult rv;
  nsCOMPtr<nsILocalFile> fileName;
  nsCOMPtr<nsIProperties> directoryService;

  directoryService = do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = directoryService->Get(NS_GRE_DIR,
                               NS_GET_IID(nsIFile),
                               getter_AddRefs(fileName));
    if (NS_SUCCEEDED(rv) && fileName) {
      fileName->AppendNative(NS_LITERAL_CSTRING("res"));
      fileName->AppendNative(nsDependentCString(alert_filename));
      PRFileDesc* fd = nsnull;
      fileName->OpenNSPRFileDesc(PR_RDONLY, 0664, &fd);
      if (fd) {
        numRead = PR_Read(fd, message, sizeof(message)-1);
        if (numRead > 0) {
          message[numRead] = 0;
          messageToShow = message;
        }
      }
    }
  }

  ShowOSAlert(messageToShow);
}

static nsresult VerifyInstallation(int argc, char **argv)
{
#ifdef MOZ_XPINSTALL
  nsresult rv;
  nsCOMPtr<nsILocalFile> registryFile;

  nsCOMPtr<nsIProperties> directoryService =
           do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return NS_OK;
  rv = directoryService->Get(NS_APP_INSTALL_CLEANUP_DIR,
                             NS_GET_IID(nsIFile),
                             getter_AddRefs(registryFile));
  if (NS_FAILED(rv) || !registryFile)
    return NS_ERROR_FAILURE;

  registryFile->AppendNative(CLEANUP_REGISTRY);

  PRBool exists;
  registryFile->Exists(&exists);
  if (exists)
  {
    nsCOMPtr<nsIFile> binPath;
    const char lastResortMessage[] = "A previous install did not complete correctly.  Finishing install.";

    ShowOSAlertFromFile(argc, argv, CLEANUP_MESSAGE_FILENAME.get(), lastResortMessage);

    nsCOMPtr<nsIFile> cleanupUtility;
    registryFile->Clone(getter_AddRefs(cleanupUtility));
    cleanupUtility->SetNativeLeafName(CLEANUP_UTIL);

    //Create the process framework to run the cleanup utility
    nsCOMPtr<nsIProcess> cleanupProcess = do_CreateInstance(NS_PROCESS_CONTRACTID);
    rv = cleanupProcess->Init(cleanupUtility);
    if (NS_SUCCEEDED(rv))
      rv = cleanupProcess->Run(PR_FALSE,nsnull, 0, nsnull);

    //We must exit because all open files must be released by the system
    return NS_ERROR_FAILURE;
  }
#endif
  return NS_OK;
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

// Note: nativeApp is an owning reference that this function has responsibility
//       to release.  This responsibility is delegated to the app startup service
//       (see nsAppStartup::Initialize call, below).
static nsresult main1(int argc, char* argv[], nsISupports *nativeApp )
{
  nsresult rv;
  NS_TIMELINE_ENTER("main1");
  nsCOMPtr<nsISupports> nativeAppOwner(dont_AddRef(nativeApp));

  //----------------------------------------------------------------
  // First we need to check if a previous installation occured and
  // if so, make sure it finished and cleaned up correctly.
  //
  // If there is an xpicleanup.dat file left around, that means the
  // previous installation did not finish correctly. We must cleanup
  // before a valid mozilla can run.
  //
  // Show the user a platform-specific Alert message, then spawn the
  // xpicleanup utility, then exit.
  //----------------------------------------------------------------
  rv = VerifyInstallation(argc, argv);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

#ifdef DEBUG_warren
//  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
#endif

#ifndef XP_MAC
  // Unbuffer debug output (necessary for automated QA performance scripts).
  setbuf(stdout, 0);
#endif

#if defined(FREEBSD)
  // Disable all SIGFPE's on FreeBSD, as it has non-IEEE-conformant fp
  // trap behavior that trips up on floating-point tests performed by
  // the JS engine.  See bugzilla bug 9967 details.
  fpsetmask(0);
#endif

  // Setup an autoreg obserer, so that we can update a progress
  // string in the splash screen
  nsCOMPtr<nsIObserverService> obsService(do_GetService("@mozilla.org/observer-service;1"));
  if (obsService)
  {
    nsCOMPtr<nsIObserver> splashScreenObserver(do_QueryInterface(nativeAppOwner));
    if (splashScreenObserver)
    {
      obsService->AddObserver(splashScreenObserver, NS_XPCOM_AUTOREGISTRATION_OBSERVER_ID, PR_FALSE);
      obsService->AddObserver(splashScreenObserver, "startup_user_notifcations", PR_FALSE);
    }
  }

#if XP_MAC
  stTSMCloser  tsmCloser;

  rv = InitializeMacCommandLine(argc, argv);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Initializing AppleEvents failed");
#endif

#ifdef DEBUG
  // _Always_ autoreg if we're in a debug build, under the assumption
  // that people are busily modifying components and will be angry if
  // their changes aren't noticed.
  nsCOMPtr<nsIComponentRegistrar> registrar;
  NS_GetComponentRegistrar(getter_AddRefs(registrar));
  registrar->AutoRegister(nsnull);
  registrar = nsnull;
#endif

  NS_TIMELINE_ENTER("startupNotifier");

  // Start up the core services:
  {
    // Please do not add new things to main1() - please hook into the
    // nsIAppStartupNotifier service.
    nsCOMPtr<nsIObserver> startupNotifier = do_CreateInstance(NS_APPSTARTUPNOTIFIER_CONTRACTID, &rv);
    if(NS_FAILED(rv))
      return rv;
    startupNotifier->Observe(nsnull, APPSTARTUP_TOPIC, nsnull);
  }
  NS_TIMELINE_LEAVE("startupNotifier");

  NS_TIMELINE_ENTER("cmdLineArgs");

  // Initialize the cmd line service
  nsCOMPtr<nsICmdLineService> cmdLineArgs(do_GetService(NS_COMMANDLINESERVICE_CONTRACTID, &rv));
  NS_ASSERTION(NS_SUCCEEDED(rv), "Could not obtain CmdLine processing service\n");
  if (NS_FAILED(rv))
    return rv;

  rv = cmdLineArgs->Initialize(argc, argv);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to initialize command line args");
  if (rv == NS_ERROR_INVALID_ARG) {
    PrintUsage();
    return rv;
  }

  NS_TIMELINE_LEAVE("cmdLineArgs");

  NS_TIMELINE_ENTER("InstallGlobalLocale");
  rv = InstallGlobalLocale(cmdLineArgs);
  if(NS_FAILED(rv))
    return rv;
  NS_TIMELINE_LEAVE("InstallGlobalLocale");

  NS_TIMELINE_ENTER("appStartup");

  nsCOMPtr<nsIAppStartup> appStartup(do_GetService(NS_APPSTARTUP_CONTRACTID));
  NS_ASSERTION(appStartup, "failed to get the appstartup service");

  /* if we couldn't get the nsIAppStartup service, then we should hide the
     splash screen and return (and something is seriously screwed up) */
  if (!appStartup)
  {
    // See if platform supports nsINativeAppSupport.
    nsCOMPtr<nsINativeAppSupport> nativeAppSupport(do_QueryInterface(nativeAppOwner));
    if (nativeAppSupport) 
    {
      // Use that interface to remove splash screen.
      nativeAppSupport->HideSplashScreen();
    }
    else
    {
      // See if platform supports nsISplashScreen, instead.
      nsCOMPtr<nsISplashScreen> splashScreen(do_QueryInterface(nativeAppOwner));
      if (splashScreen)
      {
        splashScreen->Hide();
      }
    }
    return rv;
  }

  NS_TIMELINE_LEAVE("appStartup");

  NS_TIMELINE_ENTER("appStartup->Initialize");

  // Create the Application Shell instance...
  rv = appStartup->Initialize(nativeAppOwner);

  NS_TIMELINE_LEAVE("appStartup->Initialize");

  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to initialize appstartup");
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIWindowCreator> wcreator (do_QueryInterface(appStartup));
  NS_ASSERTION(wcreator, "appstartup doesn't do nsIWindowCreator?");
  NS_ENSURE_TRUE(wcreator, NS_ERROR_FAILURE);

  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  NS_ASSERTION(wwatch, "Couldn't get the window watcher!");
  NS_ENSURE_TRUE(wwatch, NS_ERROR_FAILURE);

  wwatch->SetWindowCreator(wcreator);

  // So we can open and close windows during startup
  appStartup->EnterLastWindowClosingSurvivalArea();

  // Initialize Profile Service here.
  NS_TIMELINE_ENTER("InitializeProfileService");
  rv = InitializeProfileService(cmdLineArgs);
  NS_TIMELINE_LEAVE("InitializeProfileService");
  if (NS_FAILED(rv)) return rv;

  NS_TIMELINE_ENTER("CheckUseAccessibleSkin");
  // Need to do this after profile service init'd
  // If OS accessibility theme is used, then we always load classic theme 
  // which follows the system appearance.
  CheckUseAccessibleSkin();
  NS_TIMELINE_LEAVE("CheckUseAccessibleSkin");

  NS_TIMELINE_ENTER("appStartup->CreateHiddenWindow");
  appStartup->CreateHiddenWindow();
  NS_TIMELINE_LEAVE("appStartup->CreateHiddenWindow");

  // This will go away once Components are handling there own commandlines

  PRBool windowOpened = PR_FALSE;
  rv = DoCommandLines(cmdLineArgs, &windowOpened);
  if (NS_FAILED(rv))
  {
    NS_WARNING("failed to process command line");
    return rv;
  }
  
	if (obsService)
  {
    NS_NAMED_LITERAL_STRING(userMessage, "Creating first window...");
    obsService->NotifyObservers(nsnull, "startup_user_notifcations", userMessage.get());
  }
  

  // Make sure there exists at least 1 window.
  NS_TIMELINE_ENTER("Ensure1Window");
  rv = appStartup->Ensure1Window(cmdLineArgs);
  NS_TIMELINE_LEAVE("Ensure1Window");
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to Ensure1Window");
  if (NS_FAILED(rv)) return rv;

#if !defined(XP_MAC) && !defined(XP_MACOSX)
  appStartup->ExitLastWindowClosingSurvivalArea();
#endif

#ifdef MOZ_ENABLE_XREMOTE
  // if we have X remote support and we have our one window up and
  // running start listening for requests on the proxy window.
  // It will shut itself down before the event queue stops processing events.
  nsCOMPtr<nsIRemoteService> remoteService
    (do_GetService("@mozilla.org/toolkit/remote-service;1"));
  NS_ASSERTION(remoteService, "Couldn't create remote service?");
  if (remoteService) {
    nsCAutoString pname;

    nsCOMPtr<nsIProfile> pm (do_GetService(NS_PROFILE_CONTRACTID));
    if (pm) {
      nsXPIDLString name;
      pm->GetCurrentProfile(getter_Copies(name));
      if (name) {
        CopyUTF16toUTF8(name, pname);
      }
    }

    remoteService->Startup(NS_STRINGIFY(MOZ_APP_NAME), pname.IsEmpty() ? nsnull : pname.get());
  }
#endif /* MOZ_ENABLE_XREMOTE */

  // remove the nativeApp as an XPCOM autoreg observer
  if (obsService)
  {
    nsCOMPtr<nsIObserver> splashScreenObserver(do_QueryInterface(nativeAppOwner));
    if (splashScreenObserver)
    {
      obsService->RemoveObserver(splashScreenObserver, NS_XPCOM_AUTOREGISTRATION_OBSERVER_ID);
      obsService->RemoveObserver(splashScreenObserver, "startup_user_notifcations");
    }
  }

  // We are done with the native app (or splash screen) object here;
  // the app shell owns it now.
  nativeAppOwner = nsnull;

  // Start main event loop
  NS_TIMELINE_ENTER("appStartup->Run");
  rv = appStartup->Run();
  NS_TIMELINE_LEAVE("appStartup->Run");
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to run appstartup");

#ifdef MOZ_TIMELINE
  // Make sure we print this out even if timeline is runtime disabled
  if (NS_FAILED(NS_TIMELINE_LEAVE("main1")))
      NS_TimelineForceMark("...main1");
#endif

  return rv;
}

// English text needs to go into a dtd file.
// But when this is called we have no components etc. These strings must either be
// here, or in a native resource file.
static void DumpHelp(char *appname)
{
  printf("Usage: %s [ options ... ] [URL]\n"
         "       where options include:\n\n", appname);

#ifdef MOZ_WIDGET_GTK
  /* insert gtk options above moz options, like any other gtk app
   *
   * note: this isn't a very cool way to do things -- i'd rather get
   * these straight from a user's gtk version -- but it seems to be
   * what most gtk apps do. -dr
   */
  printf("GTK options\n"
         "\t--gdk-debug=FLAGS\t\tGdk debugging flags to set\n"
         "\t--gdk-no-debug=FLAGS\t\tGdk debugging flags to unset\n"
         "\t--gtk-debug=FLAGS\t\tGtk+ debugging flags to set\n"
         "\t--gtk-no-debug=FLAGS\t\tGtk+ debugging flags to unset\n"
         "\t--gtk-module=MODULE\t\tLoad an additional Gtk module\n"
         "\t-install\t\tInstall a private colormap\n");

  /* end gtk toolkit options */
#endif /* MOZ_WIDGET_GTK */
#if MOZ_WIDGET_XLIB
  printf("Xlib options\n"
         "\t-display=DISPLAY\t\tX display to use\n"
         "\t-visual=VISUALID\t\tX visual to use\n"
         "\t-install_colormap\t\tInstall own colormap\n"
         "\t-sync\t\tMake X calls synchronous\n"
         "\t-no-xshm\t\tDon't use X shared memory extension\n");

#endif /* MOZ_WIDGET_XLIB */
#ifdef MOZ_X11
  printf("X11 options\n"
         "\t--display=DISPLAY\t\tX display to use\n"
         "\t--sync\t\tMake X calls synchronous\n"
         "\t--no-xshm\t\tDon't use X shared memory extension\n"
         "\t--xim-preedit=STYLE\n"
         "\t--xim-status=STYLE\n");
#endif
#ifdef XP_UNIX
  printf("\t--g-fatal-warnings\t\tMake all warnings fatal\n"
         "\n%s options\n", NS_STRINGIFY(MOZ_APP_DISPLAYNAME));
#endif

  printf("\t-height <value>\t\tSet height of startup window to <value>.\n"
         "\t-h or -help\t\tPrint this message.\n"
         "\t-installer\t\tStart with 4.x migration window.\n"
         "\t-width <value>\t\tSet width of startup window to <value>.\n"
         "\t-v or -version\t\tPrint %s version.\n"
         "\t-CreateProfile <profile>\t\tCreate <profile>.\n"
         "\t-P <profile>\t\tStart with <profile>.\n"
         "\t-ProfileWizard\t\tStart with profile wizard.\n"
         "\t-ProfileManager\t\tStart with profile manager.\n"
         "\t-SelectProfile\t\tStart with profile selection dialog.\n"
         "\t-UILocale <locale>\t\tStart with <locale> resources as UI Locale.\n"
         "\t-contentLocale <locale>\t\tStart with <locale> resources as content Locale.\n", appname);
#if defined(XP_WIN32) || defined(XP_OS2)
  printf("\t-console\t\tStart %s with a debugging console.\n",NS_STRINGIFY(MOZ_APP_DISPLAYNAME));
#endif
#ifdef MOZ_ENABLE_XREMOTE
  printf("\t-remote <command>\t\tExecute <command> in an already running\n"
         "\t\t\t\t%s process.  For more info, see:\n"
         "\n\t\thttp://www.mozilla.org/unix/remote.html\n\n"
         "\t-splash\t\tEnable splash screen.\n",
         NS_STRINGIFY(MOZ_APP_DISPLAYNAME));
#else
  printf("\t-nosplash\t\tDisable splash screen.\n");
#if defined(XP_WIN) || defined(XP_OS2)
  printf("\t-quiet\t\tDisable splash screen.\n");
#endif
#endif

  // this works, but only after the components have registered.  so if you drop in a new command line handler, -help
  // won't not until the second run.
  // out of the bug, because we ship a component.reg file, it works correctly.
  DumpArbitraryHelp();
}


static inline void DumpVersion()
{
  long buildID = NS_BUILD_ID;  // 10-digit number
  printf("%s %s, Copyright (c) 2003-2007 mozilla.org", 
         NS_STRINGIFY(MOZ_APP_DISPLAYNAME), NS_STRINGIFY(MOZ_APP_VERSION));
  if(buildID) {
    printf(", build %u\n", (unsigned int)buildID);
  } else {
    printf(" <developer build>\n");
  }
}

#ifdef MOZ_ENABLE_XREMOTE
// use int here instead of a PR type since it will be returned
// from main - just to keep types consistent
static int HandleRemoteArguments(int argc, char* argv[], PRBool *aArgUsed)
{
  const char *profile = nsnull;
  const char *program = nsnull;
  const char *remote = nsnull;
  const char *username = nsnull;

  for (int i = 1; i < argc; ++i) {
    if (PL_strcasecmp(argv[i], "-remote") == 0) {
      // someone used a -remote flag
      *aArgUsed = PR_TRUE;
      // check to make sure there's another arg
      if (argc-1 == i) {
        PR_fprintf(PR_STDERR, "Error: -remote requires an argument\n");
        return 1;
      }

      // Get the remote argument and advance past it.
      remote = argv[++i];
    }
    else if (PL_strcasecmp(argv[i], "-p") == 0) {
      // someone used the -p <profile> flag - save the contents
      if (argc-1 == i) {
        continue;
      }

      // Get the argument
      profile = argv[++i];
    }
    else if (PL_strcasecmp(argv[i], "-a") == 0) {
      // someone used the -a <application> flag - save the contents
      if (argc-1 == i) {
        continue;
      }

      // Get the argument
      program = argv[++i];
    }
    else if (PL_strcasecmp(argv[i], "-u") == 0) {
      // someone used the -u <username> flag - save the contents
      if (argc-1 == i) {
        continue;
      }

      // Get the argument
      username = argv[++i];
    }
  }

  if (!remote)
    return 0; // No remote argument == success

  // try to get the X remote client
  XRemoteClient client;

  nsresult rv;
  // try to init - connects to the X server and stuff
  rv = client.Init();
  if (NS_FAILED(rv)) {
    PR_fprintf(PR_STDERR, "Error: Failed to connect to X server.\n");
    return 1;
  }

  // Make sure to set a username if possible
  if (!username) {
    username = PR_GetEnv("LOGNAME");
  }

  // Same with the program name
  if (!program) {
    program = NS_STRINGIFY(MOZ_APP_NAME);
  }

  char *response = NULL;
  PRBool success = PR_FALSE;
  rv = client.SendCommand(program, username, profile, remote,
                          nsnull, &response, &success);

  // did the command fail?
  if (NS_FAILED(rv)) {
    PR_fprintf(PR_STDERR, "Error: Failed to send command: ");
    if (response) {
      PR_fprintf(PR_STDERR, "%s\n", response);
      free(response);
    }
    else {
      PR_fprintf(PR_STDERR, "No response included.\n");
    }

    return 1;
  }

  // was there no window running?
  if (!success) {
    PR_fprintf(PR_STDERR, "Error: No running window found.\n");
    return 2;
  }

  // success
  return 0;
}
#endif /* XP_UNIX */

static PRBool HandleDumpArguments(int argc, char* argv[])
{
  for (int i=1; i<argc; i++) {
    if ((PL_strcasecmp(argv[i], "-h") == 0)
        || (PL_strcasecmp(argv[i], "-help") == 0)
#if defined(XP_UNIX) || defined(XP_BEOS)
        || (PL_strcasecmp(argv[i], "--help") == 0)
#endif /* XP_UNIX || XP_BEOS*/
#if defined(XP_WIN) || defined(XP_OS2)
        || (PL_strcasecmp(argv[i], "/h") == 0)
        || (PL_strcasecmp(argv[i], "/help") == 0)
        || (PL_strcasecmp(argv[i], "/?") == 0)
#endif /* XP_WIN || XP_OS2 */
      ) {
      DumpHelp(argv[0]);
      return PR_TRUE;
    }
    if ((PL_strcasecmp(argv[i], "-v") == 0)
        || (PL_strcasecmp(argv[i], "-version") == 0)
#if defined(XP_UNIX) || defined(XP_BEOS)
        || (PL_strcasecmp(argv[i], "--version") == 0)
#endif /* XP_UNIX || XP_BEOS */
#if defined(XP_WIN) || defined(XP_OS2)
        || (PL_strcasecmp(argv[i], "/v") == 0)
        || (PL_strcasecmp(argv[i], "/version") == 0)
#endif /* XP_WIN || XP_OS2 */
      ) {
      DumpVersion();
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}


static PRBool GetWantSplashScreen(int argc, char* argv[])
{
  int i;
  PRBool dosplash;
  // We can't use the command line service here because it isn't running yet
#if defined(XP_UNIX) && !defined(MOZ_WIDGET_PHOTON) 
  dosplash = PR_FALSE;
  for (i=1; i<argc; i++)
    if ((PL_strcasecmp(argv[i], "-splash") == 0)
        || (PL_strcasecmp(argv[i], "--splash") == 0))
      dosplash = PR_TRUE;
#else
  dosplash = PR_TRUE;
  for (i=1; i<argc; i++)
    if ((PL_strcasecmp(argv[i], "-nosplash") == 0)
#ifdef XP_BEOS
		|| (PL_strcasecmp(argv[i], "--nosplash") == 0)
#endif /* XP_BEOS */
#if defined(XP_WIN) || defined(XP_OS2)
        || (PL_strcasecmp(argv[i], "/nosplash") == 0)
#endif /* XP_WIN || XP_OS2 */
	) {
      dosplash = PR_FALSE;
	}
#endif

  return dosplash;
}

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

int main(int argc, char* argv[])
{
  NS_TIMELINE_MARK("enter main");

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
#endif

#if defined(XP_OS2)
  __pargc = &argc;
  __argv = argv;

  PRBool StartOS2App(int aArgc, char **aArgv);
  if (!StartOS2App(argc, argv))
    return 1;

  ScopedFPHandler handler;
#endif /* XP_OS2 */

#if defined(XP_MACOSX)
  InitializeMacOSXApp(argc, argv);
#endif

  // Handle -help and -version command line arguments.
  // They should% return quick, so we deal with them here.
  if (HandleDumpArguments(argc, argv))
    return 0;

#ifdef NS_TRACE_MALLOC
  argc = NS_TraceMallocStartupArgs(argc, argv);
#endif

#if defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_GTK2) || defined(MOZ_X11)
  int i;
#endif

#ifdef MOZ_X11
  /* Init threadsafe mode of Xlib API on demand
   * (currently this is only for testing, future builds may use this by
   * default) */
  PRBool x11threadsafe;
  const char *xInitThreads = PR_GetEnv("MOZILLA_X11_XINITTHREADS");
  if (xInitThreads && *xInitThreads)
    x11threadsafe = PR_TRUE;
  else {
    x11threadsafe = PR_FALSE;
    for (i = 1; i < argc; ++i)
      if (PL_strcmp(argv[i], "-xinitthreads") == 0) {
        x11threadsafe = PR_TRUE;
        break;
      }
  }
  if (x11threadsafe) {
    if (XInitThreads() == False) {
      fprintf(stderr, "%s: XInitThreads failure.", argv[0]);
      exit(EXIT_FAILURE);
    }
  }
#endif /* MOZ_X11 */

#if defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_GTK2)
  // setup for private colormap.  Ideally we'd like to do this
  // in nsAppShell::Create, but we need to get in before gtk
  // has been initialized to make sure everything is running
  // consistently.
  for (i=1; i<argc; i++)
    if ((PL_strcasecmp(argv[i], "-install") == 0)
        || (PL_strcasecmp(argv[i], "--install") == 0)) {
      gdk_rgb_set_install(TRUE);
      break;
    }

  // Initialize GTK+1/2 here for splash
#if defined(MOZ_WIDGET_GTK)
  gtk_set_locale();
#endif
  gtk_init(&argc, &argv);

  gtk_widget_set_default_visual(gdk_rgb_get_visual());
  gtk_widget_set_default_colormap(gdk_rgb_get_cmap());
#endif /* MOZ_WIDGET_GTK || MOZ_WIDGET_GTK2 */

#if defined(MOZ_WIDGET_QT)
  QApplication qapp(argc, argv);
#endif

// #if defined(MOZ_WIDGET_XLIB)
// XXXtimeless fix me! Because we don't have external shared libs holding global
// refs, there isn't an easy way to get a Display from here to the widget component.
// #endif
    
  // Call the code to install our handler
#ifdef MOZ_JPROF
  setupProfilingStuff();
#endif
    

#ifdef XPCOM_GLUE
  NS_TIMELINE_MARK("GRE_Startup...");
  nsresult rv = GRE_Startup();
  NS_TIMELINE_MARK("...GRE_Startup done");
  if (NS_FAILED(rv)) {
       // We should be displaying a dialog here with the reason why we failed.
    NS_WARNING("GRE_Startup failed");
    return 1;
  }
#else
  NS_TIMELINE_MARK("NS_InitXPCOM3...");
  nsresult rv = NS_InitXPCOM3(nsnull, nsnull, nsnull,
                              kPStaticModules, kStaticModuleCount);
  NS_TIMELINE_MARK("...NS_InitXPCOM3 done");
  if (NS_FAILED(rv)) {
    // We should be displaying a dialog here with the reason why we failed.
    NS_WARNING("NS_InitXPCOM3 failed");
    return 1;
  }
#endif


  // Try to allocate "native app support."
  // Note: this object is not released here.  It is passed to main1 which
  //       has responsibility to release it.
  nsINativeAppSupport *nativeApp = nsnull;
  rv = NS_CreateNativeAppSupport(&nativeApp);

  // See if we can run.
  if (nativeApp)
  {
    PRBool canRun = PR_FALSE;
    rv = nativeApp->Start(&canRun);
    if (!canRun) {
        return 1;
    }
  } else {
    // If platform doesn't implement nsINativeAppSupport, fall
    // back to old method.
    if (!NS_CanRun())
      return 1;
  }
  // Note: this object is not released here.  It is passed to main1 which
  //       has responsibility to release it.
  nsISplashScreen *splash = nsnull;
  PRBool dosplash = GetWantSplashScreen(argc, argv);

  if (dosplash && !nativeApp) {
    // If showing splash screen and platform doesn't implement
    // nsINativeAppSupport, then use older nsISplashScreen interface.
    rv = NS_CreateSplashScreen(&splash);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_CreateSplashScreen failed");
  }
  // If the platform has a splash screen, show it ASAP.
  if (dosplash && nativeApp) {
    nativeApp->ShowSplashScreen();
  } else if (splash) {
    splash->Show();
  }

#ifdef MOZ_ENABLE_XREMOTE
  // handle -remote now that xpcom is fired up
  int remoterv;
  PRBool argused = PR_FALSE;
  // argused will be true if someone tried to use a -remote flag.  We
  // always exit in that case.
  remoterv = HandleRemoteArguments(argc, argv, &argused);

  if (argused) {
#ifdef XPCOM_GLUE
    GRE_Shutdown();
#else
    NS_ShutdownXPCOM(nsnull);
#endif
    return remoterv;
  }
#endif

  nsresult mainResult = main1(argc, argv, nativeApp ? (nsISupports*)nativeApp : (nsISupports*)splash);

  /* if main1() didn't succeed, then don't bother trying to shut down clipboard, etc */
  if (NS_SUCCEEDED(mainResult)) {
    rv = DoOnShutdown();
    NS_ASSERTION(NS_SUCCEEDED(rv), "DoOnShutdown failed");
  }
#ifdef XPCOM_GLUE
  rv = GRE_Shutdown();
  NS_ASSERTION(NS_SUCCEEDED(rv), "GRE_Shutdown failed");
#else
  rv = NS_ShutdownXPCOM(nsnull);
  NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
#endif

  return NS_SUCCEEDED(mainResult) ? 0 : 1;
}

#if defined( XP_WIN ) && defined( WIN32 ) && !defined(__GNUC__)
// We need WinMain in order to not be a console app.  This function is
// unused if we are a console application.
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR args, int)
{
    // Do the real work.
    return main(__argc, __argv);
}
#endif // XP_WIN && WIN32
