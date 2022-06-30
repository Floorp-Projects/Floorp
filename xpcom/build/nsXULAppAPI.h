/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsXULAppAPI_h__
#define _nsXULAppAPI_h__

#include "js/TypeDecls.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/TimeStamp.h"
#include "nscore.h"

#if defined(MOZ_WIDGET_ANDROID)
#  include <jni.h>
#endif

class JSString;
class MessageLoop;
class nsIDirectoryServiceProvider;
class nsIFile;
class nsISupports;
struct JSContext;
struct XREChildData;
struct XREShellData;

namespace mozilla {
class XREAppData;
struct BootstrapConfig;
}  // namespace mozilla

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

#if defined(XP_UNIX) || defined(XP_MACOSX)
/**
 * Directory service keys for the system-wide and user-specific
 * directories where native manifests used by the WebExtensions
 * native messaging and managed storage features are found.
 */
#  define XRE_SYS_NATIVE_MANIFESTS "XRESysNativeManifests"
#  define XRE_USER_NATIVE_MANIFESTS "XREUserNativeManifests"
#endif

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
 * A directory service key which specifies the location for system add-ons.
 */
#define XRE_APP_FEATURES_DIR "XREAppFeat"

/**
 * A directory service key which specifies the location for app dir add-ons.
 * Should be a synonym for XCurProcD everywhere except in tests.
 */
#define XRE_ADDON_APP_DIR "XREAddonAppDir"

/**
 * A directory service key which specifies the distribution specific files for
 * the application unique for each user.
 * It's located at /run/user/$UID/<product name>/
 */
#define XRE_USER_RUNTIME_DIR "XREUserRunTimeDir"

/**
 * A directory service key which provides the update directory. Callers should
 * fall back to appDir.
 * Windows:    If vendor name exists:
 *             ProgramData\<vendor name>\updates\
 *             <hash of the path to XRE_EXECUTABLE_FILE's parent directory>
 *
 *             If vendor name doesn't exist, but product name exists:
 *             ProgramData\<product name>\updates\
 *             <hash of the path to XRE_EXECUTABLE_FILE's parent directory>
 *
 *             If neither vendor nor product name exists:
 *             ProgramData\Mozilla\updates
 *
 * Mac:        ~/Library/Caches/Mozilla/updates/<absolute path to app dir>
 *
 * All others: Parent directory of XRE_EXECUTABLE_FILE.
 */
#define XRE_UPDATE_ROOT_DIR "UpdRootD"

/**
 * A directory service key which provides the *old* update directory. This
 * path should only be used when data needs to be migrated from the old update
 * directory.
 * Windows:    If vendor name exists:
 *             Documents and Settings\<User>\Local Settings\Application Data\
 *             <vendor name>\updates\
 *             <hash of the path to XRE_EXECUTABLE_FILE's parent directory>
 *
 *             If vendor name doesn't exist, but product name exists:
 *             Documents and Settings\<User>\Local Settings\Application Data\
 *             <product name>\updates\
 *             <hash of the path to XRE_EXECUTABLE_FILE's parent directory>
 *
 *             If neither vendor nor product name exists:
 *             Documents and Settings\<User>\Local Settings\Application Data\
 *             Mozilla\updates
 *
 * This path does not exist on other operating systems
 */
#define XRE_OLD_UPDATE_ROOT_DIR "OldUpdRootD"

/**
 * Begin an XUL application. Does not return until the user exits the
 * application.
 *
 * @param argc/argv Command-line parameters to pass to the application. On
 *                  Windows, these should be in UTF8. On unix-like platforms
 *                  these are in the "native" character set.
 *
 * @param aConfig  Information about the application to be run.
 *
 * @return         A native result code suitable for returning from main().
 *
 * @note           If the binary is linked against the standalone XPCOM glue,
 *                 XPCOMGlueStartup() should be called before this method.
 */
int XRE_main(int argc, char* argv[], const mozilla::BootstrapConfig& aConfig);

/**
 * Given a path relative to the current working directory (or an absolute
 * path), return an appropriate nsIFile object.
 *
 * @note Pass UTF8 strings on Windows... native charset on other platforms.
 */
nsresult XRE_GetFileFromPath(const char* aPath, nsIFile** aResult);

/**
 * Get the path of the running application binary and store it in aResult.
 */
nsresult XRE_GetBinaryPath(nsIFile** aResult);

/**
 * Lock a profile directory using platform-specific semantics.
 *
 * @param aDirectory  The profile directory to lock.
 * @param aLockObject An opaque lock object. The directory will remain locked
 *                    as long as the XPCOM reference is held.
 */
nsresult XRE_LockProfileDirectory(nsIFile* aDirectory,
                                  nsISupports** aLockObject);

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

nsresult XRE_InitEmbedding2(nsIFile* aLibXULDirectory, nsIFile* aAppDirectory,
                            nsIDirectoryServiceProvider* aAppDirProvider);

/**
 * Register XPCOM components found in an array of files/directories.
 * This method may be called at any time before or after XRE_main or
 * XRE_InitEmbedding.
 *
 * @param aFiles An array of files or directories.
 * @param aFileCount the number of items in the aFiles array.
 * @note appdir/components is registered automatically.
 *
 * NS_APP_LOCATION specifies a location to search for binary XPCOM
 * components as well as component/chrome manifest files.
 *
 * NS_EXTENSION_LOCATION excludes binary XPCOM components but allows other
 * manifest instructions.
 *
 * NS_SKIN_LOCATION specifies a location to search for chrome manifest files
 * which are only allowed to register skin packages.
 */
enum NSLocationType {
  NS_APP_LOCATION,
  NS_EXTENSION_LOCATION,
  NS_SKIN_LOCATION,
  NS_BOOTSTRAPPED_LOCATION
};

nsresult XRE_AddManifestLocation(NSLocationType aType, nsIFile* aLocation);

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
 * which are only allowed to register skin packages.
 */
nsresult XRE_AddJarManifestLocation(NSLocationType aType, nsIFile* aLocation);

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
void XRE_NotifyProfile();

/**
 * Terminate embedding started with XRE_InitEmbedding or XRE_InitEmbedding2
 */
void XRE_TermEmbedding();

/**
 * Parse an INI file (application.ini or override.ini) into an existing
 * nsXREAppData structure.
 *
 * @param aINIFile The INI file to parse
 * @param aAppData The nsXREAppData structure to fill.
 */
nsresult XRE_ParseAppData(nsIFile* aINIFile, mozilla::XREAppData& aAppData);

// This enum is not dense.  See GeckoProcessTypes.h for details.
enum GeckoProcessType {
#define GECKO_PROCESS_TYPE(enum_value, enum_name, string_name, proc_typename, \
                           process_bin_type, procinfo_typename,               \
                           webidl_typename, allcaps_name)                     \
  GeckoProcessType_##enum_name = enum_value,
#include "mozilla/GeckoProcessTypes.h"
#undef GECKO_PROCESS_TYPE
  GeckoProcessType_End,
  GeckoProcessType_Invalid = GeckoProcessType_End
};

const char* XRE_GeckoProcessTypeToString(GeckoProcessType aProcessType);
const char* XRE_ChildProcessTypeToAnnotation(GeckoProcessType aProcessType);

#if defined(MOZ_WIDGET_ANDROID)
struct XRE_AndroidChildFds {
  int mPrefsFd;
  int mPrefMapFd;
  int mIpcFd;
  int mCrashFd;
  int mCrashAnnotationFd;
};

void XRE_SetAndroidChildFds(JNIEnv* env, const XRE_AndroidChildFds& fds);
#endif  // defined(MOZ_WIDGET_ANDROID)

void XRE_SetProcessType(const char* aProcessTypeString);

nsresult XRE_InitChildProcess(int aArgc, char* aArgv[],
                              const XREChildData* aChildData);

/**
 * Return the GeckoProcessType of the current process.
 */
GeckoProcessType XRE_GetProcessType();

/**
 * Return the string representation of the GeckoProcessType of the current
 * process.
 */
const char* XRE_GetProcessTypeString();

/**
 * Returns true when called in the e10s parent process.  Does *NOT* return true
 * when called in the main process if e10s is disabled.
 */
bool XRE_IsE10sParentProcess();

/**
 * Defines XRE_IsParentProcess, XRE_IsContentProcess, etc.
 *
 * XRE_IsParentProcess is unique in that it returns true when called in
 * the e10s parent process or called in the main process when e10s is
 * disabled.
 */
#define GECKO_PROCESS_TYPE(enum_value, enum_name, string_name, proc_typename, \
                           process_bin_type, procinfo_typename,               \
                           webidl_typename, allcaps_name)                     \
  bool XRE_Is##proc_typename##Process();
#include "mozilla/GeckoProcessTypes.h"
#undef GECKO_PROCESS_TYPE

bool XRE_IsSocketProcess();

/**
 * Returns true if the appshell should run its own native event loop. Returns
 * false if we should rely solely on the Gecko event loop.
 */
bool XRE_UseNativeEventProcessing();

typedef void (*MainFunction)(void* aData);

nsresult XRE_InitParentProcess(int aArgc, char* aArgv[],
                               MainFunction aMainFunction,
                               void* aMainFunctionExtraData);

int XRE_RunIPDLTest(int aArgc, char* aArgv[]);

nsresult XRE_RunAppShell();

nsresult XRE_InitCommandLine(int aArgc, char* aArgv[]);

nsresult XRE_DeinitCommandLine();

void XRE_ShutdownChildProcess();

MessageLoop* XRE_GetIOMessageLoop();

bool XRE_SendTestShellCommand(JSContext* aCx, JSString* aCommand,
                              JS::Value* aCallback);
bool XRE_ShutdownTestShell();

void XRE_InstallX11ErrorHandler();
void XRE_CleanupX11ErrorHandler();

void XRE_TelemetryAccumulate(int aID, uint32_t aSample);

void XRE_StartupTimelineRecord(int aEvent, mozilla::TimeStamp aWhen);

void XRE_InitOmnijar(nsIFile* aGreOmni, nsIFile* aAppOmni);
void XRE_StopLateWriteChecks(void);

void XRE_EnableSameExecutableForContentProc();

namespace mozilla {
enum class BinPathType { Self, PluginContainer };
}
mozilla::BinPathType XRE_GetChildProcBinPathType(GeckoProcessType aProcessType);

int XRE_XPCShellMain(int argc, char** argv, char** envp,
                     const XREShellData* aShellData);

#ifdef LIBFUZZER
#  include "FuzzerRegistry.h"

void XRE_LibFuzzerSetDriver(LibFuzzerDriver);

#endif  // LIBFUZZER

#ifdef MOZ_ENABLE_FORKSERVER

int XRE_ForkServer(int* aArgc, char*** aArgv);

#endif  // MOZ_ENABLE_FORKSERVER

#endif  // _nsXULAppAPI_h__
