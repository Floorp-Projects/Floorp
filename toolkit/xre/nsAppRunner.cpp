/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Benjamin Smedberg <bsmedberg@covad.net>
 *   Ben Goodger <ben@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsAppRunner.h"

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
#include "nsIAppStartupNotifier.h"
#include "nsIArray.h"
#include "nsICategoryManager.h"
#include "nsIChromeRegistry.h"
#include "nsICmdLineHandler.h"
#include "nsICmdLineService.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIContentHandler.h"
#include "nsIDialogParamBlock.h"
#include "nsIDOMWindow.h"
#include "nsIEventQueueService.h"
#include "nsIExtensionManager.h"
#include "nsIIOService.h"
#include "nsILocaleService.h"
#include "nsILookAndFeel.h"
#include "nsIObserverService.h"
#include "nsINativeAppSupport.h"
#include "nsIPref.h"
#include "nsIProcess.h"
#include "nsIServiceManager.h"
#include "nsISupportsPrimitives.h"
#include "nsITimelineService.h"
#include "nsIToolkitProfile.h"
#include "nsIToolkitProfileService.h"
#include "nsIURI.h"
#include "nsIWindowCreator.h"
#include "nsIWindowMediator.h"
#include "nsIWindowWatcher.h"

#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsNetUtil.h"
#include "nsWidgetsCID.h"
#include "nsXPCOM.h"
#include "nsXPIDLString.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsXULAppAPI.h"
#include "nsXREDirProvider.h"
#include "nsWindowCreator.h"

#include "nsINIParser.h"

#include "InstallCleanupDefines.h"

#include <stdlib.h>

#ifdef XP_UNIX
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef XP_WIN
#include <windows.h>
#include <process.h>
#endif

#ifdef XP_OS2
#include <process.h>
#endif

#ifdef XP_MACOSX
#include "nsILocalFileMac.h"
#endif

// for X remote support
#ifdef MOZ_ENABLE_XREMOTE
#include "nsXRemoteClientCID.h"
#include "nsIXRemoteClient.h"
#include "nsIXRemoteService.h"
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

#if defined(DEBUG_pra)
#define DEBUG_CMD_LINE
#endif

#define UILOCALE_CMD_LINE_ARG "-UILocale"
#define CONTENTLOCALE_CMD_LINE_ARG "-contentLocale"

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

#ifdef _BUILD_STATIC_BIN
#include "nsStaticComponent.h"
nsresult PR_CALLBACK
app_getModuleInfo(nsStaticModuleInfo **info, PRUint32 *count);
#endif

#if defined(XP_UNIX) || defined(XP_BEOS)
  extern void InstallUnixSignalHandlers(const char *ProgramName);
#endif

int    gArgc;
char **gArgv;

static int    gRestartArgc;
static char **gRestartArgv;

#if defined(XP_BEOS)

#include <AppKit.h>
#include <AppFileInfo.h>

class nsBeOSApp : public BApplication
{
public:
  nsBeOSApp(sem_id sem)
  : BApplication(GetAppSig()), init(sem)
  {
  }

  void ReadyToRun(void)
  {
    release_sem(init);
  }

  static int32 Main(void *args)
  {
    nsBeOSApp *app = new nsBeOSApp((sem_id)args);
    if (nsnull == app)
      return B_ERROR;
    return app->Run();
  }

private:
  char *GetAppSig(void)
  {
    app_info appInfo;
    BFile file;
    BAppFileInfo appFileInfo;
    image_info info;
    int32 cookie = 0;
    static char sig[B_MIME_TYPE_LENGTH];

    sig[0] = 0;
    if (get_next_image_info(0, &cookie, &info) != B_OK ||
        file.SetTo(info.name, B_READ_ONLY) != B_OK ||
        appFileInfo.SetTo(&file) != B_OK ||
        appFileInfo.GetSignature(sig) != B_OK)
    {
      return "application/x-vnd.Mozilla";
    }
    return sig;
  }

  sem_id init;
};

static nsresult InitializeBeOSApp(void)
{
  nsresult rv = NS_OK;

  sem_id initsem = create_sem(0, "beapp init");
  if (initsem < B_OK)
    return NS_ERROR_FAILURE;

  thread_id tid = spawn_thread(nsBeOSApp::Main, "BApplication", B_NORMAL_PRIORITY, (void *)initsem);
  if (tid < B_OK || B_OK != resume_thread(tid))
    rv = NS_ERROR_FAILURE;

  if (B_OK != acquire_sem(initsem))
    rv = NS_ERROR_FAILURE;
  if (B_OK != delete_sem(initsem))
    rv = NS_ERROR_FAILURE;

  return rv;
}

#endif // XP_BEOS

#if defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_GTK2)
#include <gtk/gtk.h>
#endif //MOZ_WIDGET_GTK || MOZ_WIDGET_GTK2


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
        if (!aParam) return ARG_FOUND;

        if (*(++curarg)) {
          if (**curarg == '-'
#if defined(XP_WIN) || defined(XP_OS2)
              || **curarg == '/'
#endif
              )
            return ARG_BAD;

          *aParam = *curarg;
          return ARG_FOUND;
        }
        return ARG_BAD;
      }
    }

    ++curarg;
  }

  return ARG_NONE;
}


static const nsXREAppData* LoadAppData(const char* appDataFile)
{
  static char vendor[256], name[256], version[32], buildID[32], copyright[512];
  static nsXREAppData data = {
    vendor, name, version, buildID, copyright, PR_FALSE, PR_FALSE, PR_FALSE
  };
  
  nsCOMPtr<nsILocalFile> lf;
  NS_GetFileFromPath(appDataFile, getter_AddRefs(lf));
  if (!lf)
    return nsnull;

  nsINIParser parser; 
  if (NS_FAILED(parser.Init(lf)))
    return nsnull;

  // Ensure that this file specifies a compatible XRE version.
  char xreVersion[32];
  nsresult rv = parser.GetString("XRE", "Version", xreVersion, sizeof(xreVersion));
  if (NS_FAILED(rv) || xreVersion[0] != '0' || xreVersion[1] != '.') {
    fprintf(stderr, "Error: XRE version requirement not met.\n");
    return nsnull;
  }

  int i;

  // Read string-valued fields
  const struct {
    const char* key;
    char* buf;
    size_t bufLen;
    PRBool required;
  } string_fields[] = {
    { "Vendor",    vendor,    sizeof(vendor),    PR_FALSE },
    { "Name",      name,      sizeof(name),      PR_TRUE  },
    { "Version",   version,   sizeof(version),   PR_FALSE },
    { "BuildID",   buildID,   sizeof(buildID),   PR_TRUE  },
    { "Copyright", copyright, sizeof(copyright), PR_FALSE }
  };
  for (i=0; i<5; ++i) {
    rv = parser.GetString("App", string_fields[i].key, string_fields[i].buf,
                          string_fields[i].bufLen);
    if (NS_FAILED(rv)) {
      if (string_fields[i].required) {
        fprintf(stderr, "Error: %x: No \"%s\" field.\n", rv,
                string_fields[i].key);
        return nsnull;
      } else {
        string_fields[i].buf[0] = '\0';
      }
    }
  }

  // Read boolean-valued fields
  const struct {
    const char* key;
    PRBool* value;
  } boolean_fields[] = {
    { "EnableProfileMigrator",  &data.enableProfileMigrator },
    { "EnableExtensionManager", &data.enableExtensionManager }
  };
  char buf[2];
  for (i=0; i<2; ++i) {
    rv = parser.GetString("XRE", boolean_fields[i].key, buf, sizeof(buf));
    if (NS_SUCCEEDED(rv))
      *(boolean_fields[i].value) =
          buf[0] == '1' || buf[0] == 't' || buf[0] == 'T';
  } 

#ifdef DEBUG
  printf("---------------------------------------------------------\n");
  printf("     Vendor %s\n", data.appVendor);
  printf("       Name %s\n", data.appName);
  printf("    Version %s\n", data.appVersion);
  printf("    BuildID %s\n", data.appBuildID);
  printf("  Copyright %s\n", data.copyright);
  printf("   EnablePM %u\n", data.enableProfileMigrator);
  printf("   EnableEM %u\n", data.enableExtensionManager);
  printf("---------------------------------------------------------\n");
#endif

  return &data;
}


static nsresult OpenWindow(const nsAFlatCString& aChromeURL,
                           const nsAFlatString& aAppArgs,
                           PRInt32 aWidth, PRInt32 aHeight);

static nsresult OpenWindow(const nsAFlatCString& aChromeURL,
                           const nsAFlatString& aAppArgs)
{
  return OpenWindow(aChromeURL, aAppArgs,
                    nsIAppShellService::SIZE_TO_CONTENT,
                    nsIAppShellService::SIZE_TO_CONTENT);
}

static nsresult OpenWindow(const nsAFlatCString& aChromeURL,
                           PRInt32 aWidth, PRInt32 aHeight)
{
  return OpenWindow(aChromeURL, EmptyString(), aWidth, aHeight);
}

static nsresult OpenWindow(const nsAFlatCString& aChromeURL,
                           const nsAFlatString& aAppArgs,
                           PRInt32 aWidth, PRInt32 aHeight)
{

#ifdef DEBUG_CMD_LINE
  printf("OpenWindow(%s, %s, %d, %d)\n", aChromeURL.get(),
                                         NS_ConvertUCS2toUTF8(aAppArgs).get(),
                                         aWidth, aHeight);
#endif /* DEBUG_CMD_LINE */

  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  nsCOMPtr<nsISupportsString> sarg(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  if (!wwatch || !sarg)
    return NS_ERROR_FAILURE;

  sarg->SetData(aAppArgs);

  nsCAutoString features("chrome,dialog=no,all");
  if (aHeight != nsIAppShellService::SIZE_TO_CONTENT) {
    features.Append(",height=");
    features.AppendInt(aHeight);
  }
  if (aWidth != nsIAppShellService::SIZE_TO_CONTENT) {
    features.Append(",width=");
    features.AppendInt(aWidth);
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
        if (NS_FAILED(rv) || !((const char *)contractidString)) break;

#ifdef DEBUG_CMD_LINE
        printf("cmd line handler contractid = %s\n", (const char *)contractidString);
#endif /* DEBUG_CMD_LINE */

        nsCOMPtr <nsICmdLineHandler> handler(do_GetService((const char *)contractidString, &rv));

        if (handler) {
          nsXPIDLCString commandLineArg;
          rv = handler->GetCommandLineArgument(getter_Copies(commandLineArg));
          if (NS_FAILED(rv)) continue;

          nsXPIDLCString helpText;
          rv = handler->GetHelpText(getter_Copies(helpText));
          if (NS_FAILED(rv)) continue;

          if ((const char *)commandLineArg) {
            printf("%s%s", HELP_SPACER_1,(const char *)commandLineArg);

            PRBool handlesArgs = PR_FALSE;
            rv = handler->GetHandlesArgs(&handlesArgs);
            if (NS_SUCCEEDED(rv) && handlesArgs) {
              printf(" <url>");
            }
            if ((const char *)helpText) {
              printf("%s%s\n",HELP_SPACER_2,(const char *)helpText);
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

  nsCOMPtr<nsICmdLineService> cmdLine =
    do_GetService("@mozilla.org/appshell/commandLineService;1",&rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr <nsICmdLineHandler> handler;
  rv = cmdLine->GetHandlerForParam(aParam, getter_AddRefs(handler));
  if (NS_FAILED(rv)) return rv;

  if (!handler) return NS_ERROR_FAILURE;

  nsXPIDLCString chromeUrlForTask;
  rv = handler->GetChromeUrlForTask(getter_Copies(chromeUrlForTask));
  if (NS_FAILED(rv)) return rv;

#ifdef DEBUG_CMD_LINE
  printf("XXX got this one:\t%s\n\t%s\n\n",commandLineArg,(const char *)chromeUrlForTask);
#endif /* DEBUG_CMD_LINE */

  nsXPIDLCString cmdResult;
  rv = cmdLineArgs->GetCmdLineValue(commandLineArg, getter_Copies(cmdResult));
  if (NS_FAILED(rv)) return rv;
#ifdef DEBUG_CMD_LINE
  printf("%s, cmdResult = %s\n",commandLineArg,(const char *)cmdResult);
#endif /* DEBUG_CMD_LINE */

  PRBool handlesArgs = PR_FALSE;
  rv = handler->GetHandlesArgs(&handlesArgs);
  if (handlesArgs) {
    if ((const char *)cmdResult) {
      if (PL_strcmp("1",(const char *)cmdResult)) {
        PRBool openWindowWithArgs = PR_TRUE;
        rv = handler->GetOpenWindowWithArgs(&openWindowWithArgs);
        if (NS_FAILED(rv)) return rv;

        if (openWindowWithArgs) {
          nsAutoString cmdArgs; cmdArgs.AssignWithConversion(cmdResult);
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
    if (NS_SUCCEEDED(rv) && (const char*)cmdResult) {
      if (PL_strcmp("1",cmdResult) == 0) {
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
  if (!arg) return PR_FALSE;

  if (PL_strlen(arg) <= 1) return PR_FALSE;

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
nsresult
DoCommandLines(nsICmdLineService* cmdLineArgs, PRBool heedGeneralStartupPrefs, PRBool *windowOpened)
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
  
  if (heedGeneralStartupPrefs) {
    nsCOMPtr<nsIAppShellService> appShellService
      (do_GetService("@mozilla.org/appshell/appShellService;1", &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = appShellService->CreateStartupState(width, height, windowOpened);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    PRInt32 argc = 0;
    rv = cmdLineArgs->GetArgc(&argc);
    if (NS_FAILED(rv)) return rv;

    char **argv = nsnull;
    rv = cmdLineArgs->GetArgv(&argv);
    if (NS_FAILED(rv)) return rv;

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
  }
  return NS_OK;
}

// match OS locale
static char kMatchOSLocalePref[] = "intl.locale.matchOS";

nsresult
getCountry(const nsAString& lc_name, nsAString& aCountry)
{

  nsresult        result = NS_OK;

  PRInt32 dash = lc_name.FindChar('-');
  if (dash > 0)
    aCountry = Substring(lc_name, dash+1, lc_name.Length()-dash);
  else
    result = NS_ERROR_FAILURE;

  return result;
}

static nsresult
getUILangCountry(nsAString& aUILang, nsAString& aCountry)
{
  nsresult	 result;
  // get a locale service 
  nsCOMPtr<nsILocaleService> localeService = do_GetService(NS_LOCALESERVICE_CONTRACTID, &result);
  NS_ASSERTION(NS_SUCCEEDED(result),"getUILangCountry: get locale service failed");

  result = localeService->GetLocaleComponentForUserAgent(aUILang);
  NS_ASSERTION(NS_SUCCEEDED(result),
          "getUILangCountry: get locale componet for user agent failed");
  result = getCountry(aUILang, aCountry);
  return result;
}

// update global locale if possible (in case when user-*.rdf can be updated)
// so that any apps after this can be invoked in the UILocale and contentLocale
static nsresult InstallGlobalLocale(nsICmdLineService *cmdLineArgs)
{
    nsresult rv = NS_OK;

    // check the pref first
    nsCOMPtr<nsIPref> prefService(do_GetService(NS_PREF_CONTRACTID));
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
        if (cmdUI) {
            nsCAutoString UILocaleName(cmdUI);
            nsCOMPtr<nsIXULChromeRegistry> chromeRegistry = do_GetService(NS_CHROMEREGISTRY_CONTRACTID, &rv);
            if (chromeRegistry)
                rv = chromeRegistry->SelectLocale(UILocaleName, PR_FALSE);
        }
    }
    // match OS when no cmdline override
    if (!cmdUI && matchOS) {
      nsCOMPtr<nsIXULChromeRegistry> chromeRegistry = do_GetService(NS_CHROMEREGISTRY_CONTRACTID, &rv);
      if (chromeRegistry) {
        chromeRegistry->SetRuntimeProvider(PR_TRUE);
        rv = chromeRegistry->SelectLocale(NS_ConvertUCS2toUTF8(uiLang), PR_FALSE);
      }
    }

    nsXPIDLCString cmdContent;
    rv = cmdLineArgs->GetCmdLineValue(CONTENTLOCALE_CMD_LINE_ARG, getter_Copies(cmdContent));
    if (NS_SUCCEEDED(rv)){
        if (cmdContent) {
            nsCAutoString contentLocaleName(cmdContent);
            nsCOMPtr<nsIXULChromeRegistry> chromeRegistry = do_GetService(NS_CHROMEREGISTRY_CONTRACTID, &rv);
            if(chromeRegistry)
                rv = chromeRegistry->SelectLocale(contentLocaleName, PR_FALSE);
        }
    }
    // match OS when no cmdline override
    if (!cmdContent && matchOS) {
      nsCOMPtr<nsIXULChromeRegistry> chromeRegistry = do_GetService(NS_CHROMEREGISTRY_CONTRACTID, &rv);
      if (chromeRegistry) {
        chromeRegistry->SetRuntimeProvider(PR_TRUE);        
        rv = chromeRegistry->SelectLocale(NS_ConvertUCS2toUTF8(country), PR_FALSE);
      }
    }

    return NS_OK;
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
  printf("%s-v or -version%sPrint %s version.\n",HELP_SPACER_1,HELP_SPACER_2, gAppData->appName);
  printf("%s-P <profile>%sStart with <profile>.\n",HELP_SPACER_1,HELP_SPACER_2);
  printf("%s-ProfileManager%sStart with profile manager.\n",HELP_SPACER_1,HELP_SPACER_2);
  printf("%s-UILocale <locale>%sStart with <locale> resources as UI Locale.\n",HELP_SPACER_1,HELP_SPACER_2);
  printf("%s-contentLocale <locale>%sStart with <locale> resources as content Locale.\n",HELP_SPACER_1,HELP_SPACER_2);
#ifdef XP_WIN
  printf("%s-console%sStart Mozilla with a debugging console.\n",HELP_SPACER_1,HELP_SPACER_2);
#endif
#ifdef MOZ_ENABLE_XREMOTE
  printf("%s-remote <command>%sExecute <command> in an already running\n"
         "%sMozilla process.  For more info, see:\n"
         "\n%shttp://www.mozilla.org/unix/remote.html\n\n",
         HELP_SPACER_1,HELP_SPACER_1,HELP_SPACER_4,HELP_SPACER_2);
#endif

  // this works, but only after the components have registered.  so if you drop in a new command line handler, -help
  // won't not until the second run.
  // out of the bug, because we ship a component.reg file, it works correctly.
  DumpArbitraryHelp();
}


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
  void     CheckAccessibleSkin();

private:
  nsIServiceManager* mServiceManager;
};

ScopedXPCOMStartup::~ScopedXPCOMStartup()
{
  if (mServiceManager) {
    gDirServiceProvider->DoShutdown();

    NS_ShutdownXPCOM(mServiceManager);
    mServiceManager = nsnull;
  }
}

nsresult
ScopedXPCOMStartup::Initialize()
{
  NS_ASSERTION(gDirServiceProvider, "Should not get here!");

  nsresult rv;
  rv = NS_InitXPCOM2(&mServiceManager, gDirServiceProvider->GetAppDir(),
                     gDirServiceProvider);
  if (NS_FAILED(rv)) {
    NS_ERROR("Couldn't start xpcom!");
    mServiceManager = nsnull;
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
 * Set our windowcreator on the WindowWatcher service.
 */
nsresult
ScopedXPCOMStartup::SetWindowCreator(nsINativeAppSupport* native)
{
  nsresult rv;

  // Initialize the cmd line service
  nsCOMPtr<nsICmdLineService> cmdLineArgs
    (do_GetService("@mozilla.org/appshell/commandLineService;1"));
  NS_ENSURE_TRUE(cmdLineArgs, NS_ERROR_FAILURE);

  rv = cmdLineArgs->Initialize(gArgc, gArgv);

  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_INVALID_ARG)
      DumpHelp();

    return rv;
  }

  nsCOMPtr<nsIAppShellService> appShellService
    (do_GetService("@mozilla.org/appshell/appShellService;1"));
  NS_ENSURE_TRUE(appShellService, NS_ERROR_UNEXPECTED);

  rv = appShellService->Initialize(cmdLineArgs, native);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIWindowCreator> creator = new nsWindowCreator();
  if (!creator) return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIWindowWatcher> wwatch
    (do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  return wwatch->SetWindowCreator(creator);
}

NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

void
ScopedXPCOMStartup::CheckAccessibleSkin()
{
  nsCOMPtr<nsILookAndFeel> lookAndFeel (do_GetService(kLookAndFeelCID));

  if (lookAndFeel) {
    PRInt32 useAccessibilityTheme = 0;

    lookAndFeel->GetMetric(nsILookAndFeel::eMetric_UseAccessibilityTheme,
                           useAccessibilityTheme);

    if (useAccessibilityTheme) {
      // If OS accessibility is active, use the classic skin, which obeys the
      // system accessibility colors.
      nsCOMPtr<nsIXULChromeRegistry> chromeRegistry
        (do_GetService(NS_CHROMEREGISTRY_CONTRACTID));
      if (chromeRegistry) {
        // Make change this session only
        chromeRegistry->SetRuntimeProvider(PR_TRUE);
        chromeRegistry->SelectSkin(NS_LITERAL_CSTRING("classic/1.0"), PR_TRUE);
      }
    }
  }
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
  printf("%s %s %s, %s\n", gAppData->appVendor, gAppData->appName, gAppData->appVersion, gAppData->copyright);
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
  nsCAutoString program(gAppData->appName);
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

  // start XPCOM
  ScopedXPCOMStartup xpcom;
  rv = xpcom.Initialize();
  NS_ENSURE_SUCCESS(rv, 1);

  { // scope the comptr so we don't hold on to XPCOM objects beyond shutdown
    // try to get the X remote client
    nsCOMPtr<nsIXRemoteClient> client (do_CreateInstance(NS_XREMOTECLIENT_CONTRACTID));
    NS_ENSURE_TRUE(client, 1);

    // try to init - connects to the X server and stuff
    rv = client->Init();
    if (NS_FAILED(rv)) {
      PR_fprintf(PR_STDERR, "Error: Failed to connect to X server.\n");
      return 1;
    }

    nsXPIDLCString response;
    PRBool success = PR_FALSE;
    rv = client->SendCommand(program.get(), username, profile, remote,
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

    client->Shutdown();
  }

  return 0;
}
#endif // MOZ_ENABLE_XREMOTE

#if defined(XP_UNIX) && !defined(XP_MACOSX)
char gBinaryPath[MAXPATHLEN];

static PRBool
GetBinaryPath(const char* argv0)
{
  struct stat fileStat;

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
  int r = readlink("/proc/self/exe", gBinaryPath, MAXPATHLEN);

  // apparently, /proc/self/exe can sometimes return weird data... check it
  if (r > 0 && r < MAXPATHLEN && stat(gBinaryPath, &fileStat) == 0)
    return PR_TRUE;
#endif

  if (realpath(argv0, gBinaryPath) && stat(gBinaryPath, &fileStat) == 0)
    return PR_TRUE;

  const char *path = getenv("PATH");
  if (!path) return PR_FALSE;

  char *pathdup = strdup(path);
  if (!pathdup) return PR_FALSE;

  PRBool found = PR_FALSE;
  char *newStr = pathdup;
  char *token;
  while ( (token = nsCRT::strtok(newStr, ":", &newStr)) ) {
    sprintf(gBinaryPath, "%s/%s", token, argv0);
    if (stat(gBinaryPath, &fileStat) == 0) {
      found = PR_TRUE;
      break;
    }
  }
  free(pathdup);
  return found;
}
#endif

#define NS_ERROR_LAUNCHED_CHILD_PROCESS NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_PROFILE, 200)

static nsresult LaunchChild(nsINativeAppSupport* aNative)
{
  aNative->Quit(); // release DDE mutex, if we're holding it

  // We need to use platform-specific hackery to find the
  // path of this executable. This is copied, with some modifications, from
  // nsGREDirServiceProvider.cpp
#ifdef XP_WIN
  // We must shorten the path to a 8.3 path, since that's all _execv can
  // handle (otherwise segments after any spaces that might exist in the path
  // will be converted into parameters, and really weird things will happen)
  char exePath[MAXPATHLEN];
  if (!::GetModuleFileName(0, exePath, MAXPATHLEN) ||
      !::GetShortPathName(exePath, exePath, sizeof(exePath)))
    return NS_ERROR_FAILURE;
  gRestartArgv[0] = (char*)exePath;

#elif defined(XP_MACOSX)
  // Works even if we're not bundled.
  CFBundleRef appBundle = CFBundleGetMainBundle();
  if (!appBundle) return NS_ERROR_FAILURE;

  CFURLRef bundleURL = CFBundleCopyExecutableURL(appBundle);
  if (!bundleURL) return NS_ERROR_FAILURE;

  FSRef fileRef;
  if (!CFURLGetFSRef(bundleURL, &fileRef)) {
    CFRelease(bundleURL);
    return NS_ERROR_FAILURE;
  }

  char exePath[MAXPATHLEN];
  if (FSRefMakePath(&fileRef, exePath, MAXPATHLEN)) {
    CFRelease(bundleURL);
    return NS_ERROR_FAILURE;
  }

  CFRelease(bundleURL);

#elif defined(XP_UNIX)
  char* exePath = gBinaryPath;

#elif defined(XP_OS2)
  PPIB ppib;
  PTIB ptib;
  char exePath[MAXPATHLEN];
  DosGetInfoBlocks( &ptib, &ppib);
  DosQueryModuleName( ppib->pib_hmte, MAXPATHLEN, exePath);

#elif defined(XP_BEOS)
  int32 cookie = 0;
  image_info info;

  if(get_next_image_info(0, &cookie, &info) != B_OK)
    return NS_ERROR_FAILURE;

  char *exePath = info.name;
#elif
#error Oops, you need platform-specific code here
#endif

  // restart this process by exec'ing it into the current process
  // if supported by the platform.  otherwise, use nspr ;-)

#if defined(XP_WIN) || defined(XP_OS2)
  if (_execv(exePath, gRestartArgv) == -1)
    return NS_ERROR_FAILURE;
#elif defined(XP_MACOSX)
  LaunchChildMac(gRestartArgc, gRestartArgv);
#elif defined(XP_UNIX)
  if (execv(exePath, gRestartArgv) == -1)
    return NS_ERROR_FAILURE;
#else
  PRProcess* process = PR_CreateProcess(exePath, gRestartArgv,
                                        nsnull, nsnull);
  if (!process) return NS_ERROR_FAILURE;

  PRInt32 exitCode;
  PRStatus failed = PR_WaitProcess(process, &exitCode);
  if (failed || exitCode)
    return NS_ERROR_FAILURE;
#endif

  return NS_ERROR_LAUNCHED_CHILD_PROCESS;
}

static const char kProfileManagerURL[] = "chrome://mozapps/content/profile/profileSelection.xul";

static nsresult
ShowProfileManager(nsIToolkitProfileService* aProfileSvc,
                   nsINativeAppSupport* aNative)
{
  nsresult rv;

  nsCOMPtr<nsILocalFile> lf;

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
      xpcom.CheckAccessibleSkin();

      nsCOMPtr<nsIWindowWatcher> windowWatcher
        (do_GetService(NS_WINDOWWATCHER_CONTRACTID));
      nsCOMPtr<nsIDialogParamBlock> ioParamBlock
        (do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID));
      nsCOMPtr<nsIMutableArray> dlgArray (do_CreateInstance(NS_ARRAY_CONTRACTID));
      NS_ENSURE_TRUE(windowWatcher && ioParamBlock && dlgArray, NS_ERROR_FAILURE);

      ioParamBlock->SetObjects(dlgArray);

      nsCOMPtr<nsIAppShellService> appShellService
        (do_GetService("@mozilla.org/appshell/appShellService;1"));
      NS_ENSURE_TRUE(appShellService, NS_ERROR_FAILURE);

      appShellService->EnterLastWindowClosingSurvivalArea();

      nsCOMPtr<nsIDOMWindow> newWindow;
      rv = windowWatcher->OpenWindow(nsnull,
                                     kProfileManagerURL,
                                     "_blank",
                                     "centerscreen,chrome,modal,titlebar",
                                     ioParamBlock,
                                     getter_AddRefs(newWindow));

      appShellService->ExitLastWindowClosingSurvivalArea();

      NS_ENSURE_SUCCESS(rv, rv);

      aProfileSvc->Flush();

      PRInt32 dialogConfirmed;
      rv = ioParamBlock->GetInt(0, &dialogConfirmed);
      if (NS_FAILED(rv) || dialogConfirmed == 0) return NS_ERROR_ABORT;

      nsCOMPtr<nsIProfileLock> lock;
      rv = dlgArray->QueryElementAt(0, NS_GET_IID(nsIProfileLock),
                                    getter_AddRefs(lock));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = lock->GetDirectory(getter_AddRefs(lf));
      NS_ENSURE_SUCCESS(rv, rv);

      lock->Unlock();
    }
  }

  nsCAutoString path;
  lf->GetNativePath(path);

  static char kEnvVar[MAXPATHLEN];
  sprintf(kEnvVar, "XRE_PROFILE_PATH=%s", path.get());
  PR_SetEnv(kEnvVar);

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

    return NS_LockProfilePath(lf, aResult);
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
    rv = NS_GetFileFromPath(arg, getter_AddRefs(lf));
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_LockProfilePath(lf, aResult);
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
      
      rv = profileSvc->CreateProfile(lf, nsDependentCSubstring(arg, delim),
                                     getter_AddRefs(profile));
    } else {
      rv = profileSvc->CreateProfile(nsnull, nsDependentCString(arg),
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

    return rv;
  }

  PRUint32 count;
  rv = profileSvc->GetProfileCount(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  arg = PR_GetEnv("XRE_IMPORT_PROFILES");
  if (!count && (!arg || !*arg)) {
    return ImportProfiles(profileSvc, aNative);
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
      rv = profile->Lock(aResult);
      if (NS_SUCCEEDED(rv))
        return NS_OK;
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
                                            NS_LITERAL_CSTRING("default"),
                                            getter_AddRefs(profile));
    if (NS_SUCCEEDED(rv)) {
      profileSvc->Flush();
      rv = profile->Lock(aResult);
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
      rv = profile->Lock(aResult);
      if (NS_SUCCEEDED(rv))
        return NS_OK;
    }
  }

  return ShowProfileManager(profileSvc, aNative);
}

#define FILE_COMPATIBILITY_INFO NS_LITERAL_STRING("compatibility.ini")

static void GetVersion(nsIFile* aProfileDir, char* aVersion, int aVersionLength)
{
  nsCOMPtr<nsIFile> compatibilityFile;
  aProfileDir->Clone(getter_AddRefs(compatibilityFile));
  compatibilityFile->Append(FILE_COMPATIBILITY_INFO);

  nsINIParser parser;
  nsCOMPtr<nsILocalFile> lf(do_QueryInterface(compatibilityFile));
  parser.Init(lf);

  parser.GetString("Compatibility", "Build ID", aVersion, aVersionLength);
}

static PRBool ComponentsListChanged(nsIFile* aProfileDir)
{
  nsCOMPtr<nsIFile> compatibilityFile;
  aProfileDir->Clone(getter_AddRefs(compatibilityFile));
  compatibilityFile->Append(FILE_COMPATIBILITY_INFO);

  nsINIParser parser;
  nsCOMPtr<nsILocalFile> lf(do_QueryInterface(compatibilityFile));
  parser.Init(lf);

  char parserBuf[MAXPATHLEN];
  nsresult rv = parser.GetString("Compatibility", "Components List Changed", parserBuf, MAXPATHLEN);
  return NS_SUCCEEDED(rv) && !strcmp(parserBuf, "1");
}

static void RemoveComponentRegistries(nsIFile* aProfileDir)
{
  nsCOMPtr<nsIFile> compregFile;
  aProfileDir->Clone(getter_AddRefs(compregFile));
  compregFile->Append(NS_LITERAL_STRING("compreg.dat"));
  compregFile->Remove(PR_FALSE);

  nsCOMPtr<nsIFile> xptiFile;
  aProfileDir->Clone(getter_AddRefs(xptiFile));
  xptiFile->Append(NS_LITERAL_STRING("xpti.dat"));
  xptiFile->Remove(PR_FALSE);
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

int xre_main(int argc, char* argv[], const nsXREAppData* aAppData)
{
  nsresult rv;
  NS_TIMELINE_MARK("enter main");

#if defined(DEBUG) && defined(XP_WIN32)
  // Disable small heap allocator to get heapwalk() giving us
  // accurate heap numbers. Win2k non-debug does not use small heap allocator.
  // Win2k debug seems to be still using it.
  // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/vclib/html/_crt__set_sbh_threshold.asp
  _set_sbh_threshold(0);
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

#if defined(XP_UNIX) && !defined(XP_MACOSX)
  if (!GetBinaryPath(argv[0]))
    return 1;
#endif

  gArgc = argc;
  gArgv = argv;

  // allow -app argument to override default app data
  const char *appDataFile = nsnull;
  if (CheckArg("app", &appDataFile))
    aAppData = LoadAppData(appDataFile);

  if (!aAppData) {
    fprintf(stderr, "Error: Invalid or missing application data!\n");
    return 1;
  }
  gAppData = aAppData;

  gRestartArgc = argc;
  gRestartArgv = (char**) malloc(sizeof(char*) * (argc + 1));
  if (!gRestartArgv) return 1;

  int i;
  for (i = 0; i < argc; ++i) {
    gRestartArgv[i] = argv[i];
  }
  gRestartArgv[argc] = nsnull;

#if defined(XP_OS2)
  ULONG    ulMaxFH = 0;
  LONG     ulReqCount = 0;

  DosSetRelMaxFH(&ulReqCount,
                 &ulMaxFH);

  if (ulMaxFH < 256) {
    DosSetMaxFH(256);
  }

  ScopedFPHandler handler();
#endif /* XP_OS2 */

#if defined(XP_BEOS)
  if (NS_OK != InitializeBeOSApp())
    return 1;
#endif

#ifdef _BUILD_STATIC_BIN
  // Initialize XPCOM's module info table
  NSGetStaticModuleInfo = app_getModuleInfo;
#endif

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
    nsCOMPtr<nsIFile> xulAppDir;

    if (appDataFile) {
      nsCOMPtr<nsILocalFile> lf;
      NS_GetFileFromPath(appDataFile, getter_AddRefs(lf));
      lf->GetParent(getter_AddRefs(xulAppDir));
    }

    rv = dirProvider.Initialize(xulAppDir);
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

      if (gAppData->enableExtensionManager) {
        nsCOMPtr<nsIExtensionManager> em
          (do_GetService("@mozilla.org/extensions/manager;1"));
        NS_ENSURE_TRUE(em, 1);

        em->Register();
      }
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
  gtk_init(&argc, &argv);

  gtk_widget_set_default_visual(gdk_rgb_get_visual());
  gtk_widget_set_default_colormap(gdk_rgb_get_cmap());
#endif /* MOZ_WIDGET_GTK || MOZ_WIDGET_GTK2 */
    
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
#endif

  nsCOMPtr<nsIProfileLock> profileLock;
  PRBool startOffline = PR_FALSE;

  rv = SelectProfile(getter_AddRefs(profileLock), nativeApp, &startOffline);
  if (rv == NS_ERROR_LAUNCHED_CHILD_PROCESS ||
      rv == NS_ERROR_ABORT) return 0;
  if (NS_FAILED(rv)) return 1;

  nsCOMPtr<nsILocalFile> lf;
  rv = profileLock->GetDirectory(getter_AddRefs(lf));
  NS_ENSURE_SUCCESS(rv, 1);

  rv = dirProvider.SetProfileDir(lf);
  NS_ENSURE_SUCCESS(rv, 1);

  //////////////////////// NOW WE HAVE A PROFILE ////////////////////////

  PRBool upgraded = PR_FALSE;
  PRBool componentsListChanged = PR_FALSE;

  if (gAppData->enableExtensionManager) {
    // Check for version compatibility with the last version of the app this 
    // profile was started with.
    char version[MAXPATHLEN];
    GetVersion(lf, version, MAXPATHLEN);

    // Extensions are deemed compatible for all builds in the "x.x.x+" 
    // period in between milestones for developer convenience (even though
    // ongoing code changes might actually make that a poor assumption. 
    // The size and expertise of the nightly build testing community is 
    // expected to be sufficient to deal with this issue. 
    //
    // Every time a profile is loaded by a build with a different build id, 
    // it updates the compatibility.ini file saying what build last wrote
    // the compreg.dat. On subsequent launches if the build id matches, 
    // there is no need for re-registration. If the user loads the same
    // profile in different builds the component registry must be
    // re-generated to prevent mysterious component loading failures.
    // 
    if (!strcmp(version, aAppData->appBuildID)) {
      componentsListChanged = ComponentsListChanged(lf);
      if (componentsListChanged) {
        // Remove compreg.dat and xpti.dat, forcing component re-registration,
        // with the new list of additional components directories specified
        // in "components.ini" which we have just discovered changed since the
        // last time the application was run. 
        RemoveComponentRegistries(lf);
        
        // Tell the dir provider to supply the list of components locations 
        // specified in "components.ini" when it is asked for the "ComsDL"
        // enumeration.
        dirProvider.RegisterExtraComponents();
      }
      // Nothing need be done for the normal startup case.
    }
    else {
      // Remove compreg.dat and xpti.dat, forcing component re-registration
      // with the default set of components (this disables any potentially
      // troublesome incompatible XPCOM components). 
      RemoveComponentRegistries(lf);

      // Tell the Extension Manager it should check for incompatible 
      // Extensions and re-write the Components manifest ("components.ini")
      // with a list of XPCOM components for compatible extensions
      upgraded = PR_TRUE;

      // The Extension Manager will write the Compatibility manifest with
      // the current app version. 
    }
  }

  PRBool needsRestart = PR_FALSE;

  // Allows the user to forcefully bypass the restart process at their
  // own risk. Useful for debugging or for tinderboxes where child 
  // processes can be problematic.
  PRBool noRestart = PR_FALSE;
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

      xpcom.CheckAccessibleSkin();

      {
        NS_TIMELINE_ENTER("startupNotifier");
        nsCOMPtr<nsIObserver> startupNotifier
          (do_CreateInstance(NS_APPSTARTUPNOTIFIER_CONTRACTID, &rv));
        NS_ENSURE_SUCCESS(rv, 1);

        startupNotifier->Observe(nsnull, APPSTARTUP_TOPIC, nsnull);
        NS_TIMELINE_LEAVE("startupNotifier");
      }

      nsCOMPtr<nsIAppShellService> appShellService;
      appShellService = do_GetService("@mozilla.org/appshell/appShellService;1");
      NS_ENSURE_TRUE(appShellService, 1);

      // So we can open and close windows during startup
      appShellService->EnterLastWindowClosingSurvivalArea();

      // Profile Migration
      if (gAppData->enableProfileMigrator && gDoMigration) {
        gDoMigration = PR_FALSE;
        nsCOMPtr<nsIProfileMigrator> pm
          (do_CreateInstance(NS_PROFILEMIGRATOR_CONTRACTID));
        if (pm)
          pm->Migrate(&dirProvider);
      }
      dirProvider.DoStartup();

      NS_TIMELINE_ENTER("appShellService->CreateHiddenWindow");
      rv = appShellService->CreateHiddenWindow();
      NS_TIMELINE_LEAVE("appShellService->CreateHiddenWindow");
      NS_ENSURE_SUCCESS(rv, 1);

      // Extension Compatibility Checking and Startup
      if (gAppData->enableExtensionManager) {
        nsCOMPtr<nsIExtensionManager> em(do_GetService("@mozilla.org/extensions/manager;1"));
        NS_ENSURE_TRUE(em, 1);

        if (CheckArg("install-global-extension") || 
            CheckArg("install-global-theme") || CheckArg("list-global-items") || 
            CheckArg("lock-item") || CheckArg("unlock-item")) {
          // Do the required processing and then shut down.
          em->HandleCommandLineArgs();
          return 0;
        }

        char* noEMRestart = PR_GetEnv("NO_EM_RESTART");
        noRestart = noEMRestart && !strcmp("1", noEMRestart);
        if (!noRestart && upgraded) {
          rv = em->CheckForMismatches(&needsRestart);
          NS_ASSERTION(NS_SUCCEEDED(rv), "Oops, looks like you have a extensions.rdf file generated by a buggy nightly build. Please remove it!");
          if (NS_FAILED(rv)) {
            needsRestart = PR_FALSE;
            upgraded = PR_FALSE;
          }
        }

        if (noRestart || (!upgraded || !needsRestart))
          em->Start(componentsListChanged, &needsRestart);
      }

      if (noRestart || (!upgraded && !needsRestart)) {

        // clear out any environment variables which may have been set 
        // during the relaunch process now that we know we won't be relaunching.
        PR_SetEnv("XRE_PROFILE_PATH=");
        PR_SetEnv("XRE_START_OFFLINE=");
        PR_SetEnv("XRE_IMPORT_PROFILES=");
        

        // Kick off the prebinding update now that we know we won't be
        // relaunching.

#ifdef XP_MACOSX
        UpdatePrebinding();
#endif

        nsCOMPtr<nsICmdLineService> cmdLineArgs
          (do_GetService("@mozilla.org/appshell/commandLineService;1"));
        NS_ENSURE_TRUE(cmdLineArgs, 1);

        NS_TIMELINE_ENTER("InstallGlobalLocale");
        InstallGlobalLocale(cmdLineArgs);
        NS_TIMELINE_LEAVE("InstallGlobalLocale");

        // This will go away once Components are handling there own commandlines
        // if we have no command line arguments, we need to heed the
        // "general.startup.*" prefs
        // if we had no command line arguments, argc == 1.

        PRBool windowOpened = PR_FALSE;
        rv = DoCommandLines(cmdLineArgs, aAppData->useStartupPrefs, &windowOpened);
        NS_ENSURE_SUCCESS(rv, 1);
      
        // Make sure there exists at least 1 window.
        NS_TIMELINE_ENTER("Ensure1Window");
        rv = appShellService->Ensure1Window(cmdLineArgs);
        NS_TIMELINE_LEAVE("Ensure1Window");
        NS_ENSURE_SUCCESS(rv, 1);

#ifndef XP_MACOSX
        appShellService->ExitLastWindowClosingSurvivalArea();
#endif

#ifdef MOZ_ENABLE_XREMOTE
        // if we have X remote support and we have our one window up and
        // running start listening for requests on the proxy window.
        nsCOMPtr<nsIXRemoteService> remoteService;
        remoteService = do_GetService(NS_IXREMOTESERVICE_CONTRACTID);
        if (remoteService)
          remoteService->Startup(aAppData->appName);
#endif /* MOZ_ENABLE_XREMOTE */

        // enable win32 DDE responses and Mac appleevents responses
        nativeApp->SetShouldShowUI(PR_TRUE);

        // Start main event loop
        NS_TIMELINE_ENTER("appShell->Run");
        rv = appShellService->Run();
        NS_TIMELINE_LEAVE("appShell->Run");
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to run appshell");

#ifdef MOZ_ENABLE_XREMOTE
        // shut down the x remote proxy window
        if (remoteService)
          remoteService->Shutdown();
#endif /* MOZ_ENABLE_XREMOTE */

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
  if (!noRestart && needsRestart) {
    nsCAutoString path;
    lf->GetNativePath(path);

    static char kEnvVar[MAXPATHLEN];
    sprintf(kEnvVar, "XRE_PROFILE_PATH=%s", path.get());
    PR_SetEnv(kEnvVar);

    return LaunchChild(nativeApp) == NS_ERROR_LAUNCHED_CHILD_PROCESS ? 0 : 1;
  }

  return NS_FAILED(rv) ? 1 : 0;
}
