/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsXULAppAPI_h__
#define _nsXULAppAPI_h__

#include "prtypes.h"
#include "nsID.h"
#include "xrecore.h"
#include "nsXPCOM.h"
#include "nsISupports.h"
#include "prlog.h"
#include "nsXREAppData.h"

#include "mozilla/Assertions.h"

/**
 * A directory service key which provides the platform-correct "application
 * data" directory as follows, where $name and $vendor are as defined above and
 * $vendor is optional:
 *
 * Windows:
 *   HOME = Documents and Settings\$USER\Application Data
 *   UAppData = $HOME[\$vendor]\$name
 *
 * Unix:
 *   HOME = ~
 *   UAppData = $HOME/.[$vendor/]$name
 *
 * Mac:
 *   HOME = ~
 *   UAppData = $HOME/Library/Application Support/$name
 *
 * Note that the "profile" member above will change the value of UAppData as
 * follows:
 *
 * Windows:
 *   UAppData = $HOME\$profile
 *
 * Unix:
 *   UAppData = $HOME/.$profile
 *
 * Mac:
 *   UAppData = $HOME/Library/Application Support/$profile
 */
#define XRE_USER_APP_DATA_DIR "UAppData"

/**
 * A directory service key which provides a list of all enabled extension
 * directories and files (packed XPIs).  The list includes compatible
 * platform-specific extension subdirectories.
 *
 * @note The directory list will have no members when the application is
 *       launched in safe mode.
 */
#define XRE_EXTENSIONS_DIR_LIST "XREExtDL"

/**
 * A directory service key which provides the executable file used to
 * launch the current process.  This is the same value returned by the
 * XRE_GetBinaryPath function defined below.
 */
#define XRE_EXECUTABLE_FILE "XREExeF"

/**
 * A directory service key which specifies the profile
 * directory. Unlike the NS_APP_USER_PROFILE_50_DIR key, this key may
 * be available when the profile hasn't been "started", or after is
 * has been shut down. If the application is running without a
 * profile, such as when showing the profile manager UI, this key will
 * not be available. This key is provided by the XUL apprunner or by
 * the aAppDirProvider object passed to XRE_InitEmbedding.
 */
#define NS_APP_PROFILE_DIR_STARTUP "ProfDS"

/**
 * A directory service key which specifies the profile
 * directory. Unlike the NS_APP_USER_PROFILE_LOCAL_50_DIR key, this key may
 * be available when the profile hasn't been "started", or after is
 * has been shut down. If the application is running without a
 * profile, such as when showing the profile manager UI, this key will
 * not be available. This key is provided by the XUL apprunner or by
 * the aAppDirProvider object passed to XRE_InitEmbedding.
 */
#define NS_APP_PROFILE_LOCAL_DIR_STARTUP "ProfLDS"

/**
 * A directory service key which specifies the system extension
 * parent directory containing platform-specific extensions.
 * This key may not be available on all platforms.
 */
#define XRE_SYS_LOCAL_EXTENSION_PARENT_DIR "XRESysLExtPD"

/**
 * A directory service key which specifies the system extension
 * parent directory containing platform-independent extensions.
 * This key may not be available on all platforms.
 * Additionally, the directory may be equal to that returned by
 * XRE_SYS_LOCAL_EXTENSION_PARENT_DIR on some platforms.
 */
#define XRE_SYS_SHARE_EXTENSION_PARENT_DIR "XRESysSExtPD"

/**
 * A directory service key which specifies the user system extension
 * parent directory.
 */
#define XRE_USER_SYS_EXTENSION_DIR "XREUSysExt"

/**
 * A directory service key which specifies the distribution specific files for
 * the application.
 */
#define XRE_APP_DISTRIBUTION_DIR "XREAppDist"

/**
 * Begin an XUL application. Does not return until the user exits the
 * application.
 *
 * @param argc/argv Command-line parameters to pass to the application. On
 *                  Windows, these should be in UTF8. On unix-like platforms
 *                  these are in the "native" character set.
 *
 * @param aAppData  Information about the application to be run.
 *
 * @return         A native result code suitable for returning from main().
 *
 * @note           If the binary is linked against the standalone XPCOM glue,
 *                 XPCOMGlueStartup() should be called before this method.
 *
 */
XRE_API(int,
        XRE_main, (int argc, char* argv[], const nsXREAppData* sAppData))

/**
 * Given a path relative to the current working directory (or an absolute
 * path), return an appropriate nsILocalFile object.
 *
 * @note Pass UTF8 strings on Windows... native charset on other platforms.
 */
XRE_API(nsresult,
        XRE_GetFileFromPath, (const char *aPath, nsILocalFile* *aResult))

/**
 * Get the path of the running application binary and store it in aResult.
 * @param argv0   The value passed as argv[0] of main(). This value is only
 *                used on *nix, and only when other methods of determining
 *                the binary path have failed.
 */
XRE_API(nsresult,
        XRE_GetBinaryPath, (const char *argv0, nsILocalFile* *aResult))

/**
 * Get the static module built in to libxul.
 */
XRE_API(const mozilla::Module*,
        XRE_GetStaticModule, ())

/**
 * Lock a profile directory using platform-specific semantics.
 *
 * @param aDirectory  The profile directory to lock.
 * @param aLockObject An opaque lock object. The directory will remain locked
 *                    as long as the XPCOM reference is held.
 */
XRE_API(nsresult,
        XRE_LockProfileDirectory, (nsILocalFile* aDirectory,
                                   nsISupports* *aLockObject))

/**
 * Initialize libXUL for embedding purposes.
 *
 * @param aLibXULDirectory   The directory in which the libXUL shared library
 *                           was found.
 * @param aAppDirectory      The directory in which the application components
 *                           and resources can be found. This will map to
 *                           the NS_OS_CURRENT_PROCESS_DIR directory service
 *                           key.
 * @param aAppDirProvider    A directory provider for the application. This
 *                           provider will be aggregated by a libxul provider
 *                           which will provide the base required GRE keys.
 *
 * @note This function must be called from the "main" thread.
 *
 * @note At the present time, this function may only be called once in
 * a given process. Use XRE_TermEmbedding to clean up and free
 * resources allocated by XRE_InitEmbedding.
 */

XRE_API(nsresult,
        XRE_InitEmbedding2, (nsILocalFile *aLibXULDirectory,
                             nsILocalFile *aAppDirectory,
                             nsIDirectoryServiceProvider *aAppDirProvider))

/**
 * Register static XPCOM component information.
 * This method may be called at any time before or after XRE_main or
 * XRE_InitEmbedding.
 */
XRE_API(nsresult,
        XRE_AddStaticComponent, (const mozilla::Module* aComponent))

/**
 * Register XPCOM components found in an array of files/directories.
 * This method may be called at any time before or after XRE_main or
 * XRE_InitEmbedding.
 *
 * @param aFiles An array of files or directories.
 * @param aFileCount the number of items in the aFiles array.
 * @note appdir/components is registered automatically.
 *
 * NS_COMPONENT_LOCATION specifies a location to search for binary XPCOM
 * components as well as component/chrome manifest files.
 *
 * NS_SKIN_LOCATION specifies a location to search for chrome manifest files
 * which are only allowed to register only skin packages and style overlays.
 */
enum NSLocationType
{
  NS_COMPONENT_LOCATION,
  NS_SKIN_LOCATION,
  NS_BOOTSTRAPPED_LOCATION
};

XRE_API(nsresult,
        XRE_AddManifestLocation, (NSLocationType aType,
                                  nsILocalFile* aLocation))

/**
 * Register XPCOM components found in a JAR.
 * This is similar to XRE_AddManifestLocation except the file specified
 * must be a zip archive with a manifest named chrome.manifest
 * This method may be called at any time before or after XRE_main or
 * XRE_InitEmbedding.
 *
 * @param aFiles An array of files or directories.
 * @param aFileCount the number of items in the aFiles array.
 * @note appdir/components is registered automatically.
 *
 * NS_COMPONENT_LOCATION specifies a location to search for binary XPCOM
 * components as well as component/chrome manifest files.
 *
 * NS_SKIN_LOCATION specifies a location to search for chrome manifest files
 * which are only allowed to register only skin packages and style overlays.
 */
XRE_API(nsresult,
        XRE_AddJarManifestLocation, (NSLocationType aType,
                                     nsILocalFile* aLocation))

/**
 * Fire notifications to inform the toolkit about a new profile. This
 * method should be called after XRE_InitEmbedding if the embedder
 * wishes to run with a profile. Normally the embedder should call
 * XRE_LockProfileDirectory to lock the directory before calling this
 * method.
 *
 * @note There are two possibilities for selecting a profile:
 *
 * 1) Select the profile before calling XRE_InitEmbedding. The aAppDirProvider
 *    object passed to XRE_InitEmbedding should provide the
 *    NS_APP_USER_PROFILE_50_DIR key, and may also provide the following keys:
 *    - NS_APP_USER_PROFILE_LOCAL_50_DIR
 *    - NS_APP_PROFILE_DIR_STARTUP
 *    - NS_APP_PROFILE_LOCAL_DIR_STARTUP
 *    In this scenario XRE_NotifyProfile should be called immediately after
 *    XRE_InitEmbedding. Component registration information will be stored in
 *    the profile and JS components may be stored in the fastload cache.
 *
 * 2) Select a profile some time after calling XRE_InitEmbedding. In this case
 *    the embedder must install a directory service provider which provides
 *    NS_APP_USER_PROFILE_50_DIR and optionally
 *    NS_APP_USER_PROFILE_LOCAL_50_DIR. Component registration information
 *    will be stored in the application directory and JS components will not
 *    fastload.
 */
XRE_API(void,
        XRE_NotifyProfile, ())

/**
 * Terminate embedding started with XRE_InitEmbedding or XRE_InitEmbedding2
 */
XRE_API(void,
        XRE_TermEmbedding, ())

/**
 * Create a new nsXREAppData structure from an application.ini file.
 *
 * @param aINIFile The application.ini file to parse.
 * @param aAppData A newly-allocated nsXREAppData structure. The caller is
 *                 responsible for freeing this structure using
 *                 XRE_FreeAppData.
 */
XRE_API(nsresult,
        XRE_CreateAppData, (nsILocalFile* aINIFile,
                            nsXREAppData **aAppData))

/**
 * Parse an INI file (application.ini or override.ini) into an existing
 * nsXREAppData structure.
 *
 * @param aINIFile The INI file to parse
 * @param aAppData The nsXREAppData structure to fill.
 */
XRE_API(nsresult,
        XRE_ParseAppData, (nsILocalFile* aINIFile,
                           nsXREAppData *aAppData))

/**
 * Free a nsXREAppData structure that was allocated with XRE_CreateAppData.
 */
XRE_API(void,
        XRE_FreeAppData, (nsXREAppData *aAppData))

enum GeckoProcessType {
  GeckoProcessType_Default = 0,

  GeckoProcessType_Plugin,
  GeckoProcessType_Content,

  GeckoProcessType_IPDLUnitTest,

  GeckoProcessType_End,
  GeckoProcessType_Invalid = GeckoProcessType_End
};

static const char* const kGeckoProcessTypeString[] = {
  "default",
  "plugin",
  "tab",
  "ipdlunittest"
};

// Oddly, NS_ARRAY_LENGTH causes an internal compiler error with MSVC10, so
// compute the length manually.
MOZ_STATIC_ASSERT(sizeof(kGeckoProcessTypeString) /
                  sizeof(kGeckoProcessTypeString[0]) ==
                  GeckoProcessType_End,
                  "Array length mismatch");

XRE_API(const char*,
        XRE_ChildProcessTypeToString, (GeckoProcessType aProcessType))

XRE_API(GeckoProcessType,
        XRE_StringToChildProcessType, (const char* aProcessTypeString))

#if defined(MOZ_CRASHREPORTER)
// Used in the "master" parent process hosting the crash server
XRE_API(bool,
        XRE_TakeMinidumpForChild, (PRUint32 aChildPid, nsILocalFile** aDump))

// Used in child processes.
XRE_API(bool,
        XRE_SetRemoteExceptionHandler, (const char* aPipe))
#endif

XRE_API(nsresult,
        XRE_InitChildProcess, (int aArgc,
                               char* aArgv[],
                               GeckoProcessType aProcess))

XRE_API(GeckoProcessType,
        XRE_GetProcessType, ())

typedef void (*MainFunction)(void* aData);

XRE_API(nsresult,
        XRE_InitParentProcess, (int aArgc,
                                char* aArgv[],
                                MainFunction aMainFunction,
                                void* aMainFunctionExtraData))

XRE_API(int,
        XRE_RunIPDLTest, (int aArgc,
                          char* aArgv[]))

XRE_API(nsresult,
        XRE_RunAppShell, ())

XRE_API(nsresult,
        XRE_InitCommandLine, (int aArgc, char* aArgv[]))

XRE_API(nsresult,
        XRE_DeinitCommandLine, ())

class MessageLoop;

XRE_API(void,
        XRE_ShutdownChildProcess, ())

XRE_API(MessageLoop*,
        XRE_GetIOMessageLoop, ())

struct JSContext;
class JSString;

XRE_API(bool,
        XRE_SendTestShellCommand, (JSContext* aCx,
                                   JSString* aCommand,
                                   void* aCallback))
struct JSObject;

XRE_API(bool,
        XRE_GetChildGlobalObject, (JSContext* aCx,
                                   JSObject** globalp))

XRE_API(bool,
        XRE_ShutdownTestShell, ())

XRE_API(void,
        XRE_InstallX11ErrorHandler, ())

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#define XRE_HAS_DLL_BLOCKLIST
XRE_API(void,
        XRE_SetupDllBlocklist, ())
#endif

XRE_API(void,
        XRE_TelemetryAccumulate, (int aID, PRUint32 aSample))


XRE_API(void,
        XRE_InitOmnijar, (nsILocalFile* greOmni,
                          nsILocalFile* appOmni))
#endif // _nsXULAppAPI_h__
