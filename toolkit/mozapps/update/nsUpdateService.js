/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/FileUtils.jsm", this);
Cu.import("resource://gre/modules/AddonManager.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/ctypes.jsm", this);
Cu.import("resource://gre/modules/UpdateTelemetry.jsm", this);
Cu.import("resource://gre/modules/AppConstants.jsm", this);

const UPDATESERVICE_CID = Components.ID("{B3C290A6-3943-4B89-8BBE-C01EB7B3B311}");
const UPDATESERVICE_CONTRACTID = "@mozilla.org/updates/update-service;1";

const PREF_APP_UPDATE_ALTWINDOWTYPE       = "app.update.altwindowtype";
const PREF_APP_UPDATE_AUTO                = "app.update.auto";
const PREF_APP_UPDATE_BACKGROUND_INTERVAL = "app.update.download.backgroundInterval";
const PREF_APP_UPDATE_BACKGROUNDERRORS    = "app.update.backgroundErrors";
const PREF_APP_UPDATE_BACKGROUNDMAXERRORS = "app.update.backgroundMaxErrors";
const PREF_APP_UPDATE_CANCELATIONS        = "app.update.cancelations";
const PREF_APP_UPDATE_CERTS_BRANCH        = "app.update.certs.";
const PREF_APP_UPDATE_CERT_CHECKATTRS     = "app.update.cert.checkAttributes";
const PREF_APP_UPDATE_CERT_ERRORS         = "app.update.cert.errors";
const PREF_APP_UPDATE_CERT_MAXERRORS      = "app.update.cert.maxErrors";
const PREF_APP_UPDATE_CERT_REQUIREBUILTIN = "app.update.cert.requireBuiltIn";
const PREF_APP_UPDATE_CUSTOM              = "app.update.custom";
const PREF_APP_UPDATE_ENABLED             = "app.update.enabled";
const PREF_APP_UPDATE_IDLETIME            = "app.update.idletime";
const PREF_APP_UPDATE_INCOMPATIBLE_MODE   = "app.update.incompatible.mode";
const PREF_APP_UPDATE_INTERVAL            = "app.update.interval";
const PREF_APP_UPDATE_LOG                 = "app.update.log";
const PREF_APP_UPDATE_MODE                = "app.update.mode";
const PREF_APP_UPDATE_NEVER_BRANCH        = "app.update.never.";
const PREF_APP_UPDATE_NOTIFIEDUNSUPPORTED = "app.update.notifiedUnsupported";
const PREF_APP_UPDATE_POSTUPDATE          = "app.update.postupdate";
const PREF_APP_UPDATE_PROMPTWAITTIME      = "app.update.promptWaitTime";
const PREF_APP_UPDATE_SHOW_INSTALLED_UI   = "app.update.showInstalledUI";
const PREF_APP_UPDATE_SILENT              = "app.update.silent";
const PREF_APP_UPDATE_STAGING_ENABLED     = "app.update.staging.enabled";
const PREF_APP_UPDATE_URL                 = "app.update.url";
const PREF_APP_UPDATE_URL_DETAILS         = "app.update.url.details";
const PREF_APP_UPDATE_URL_OVERRIDE        = "app.update.url.override";
const PREF_APP_UPDATE_SERVICE_ENABLED     = "app.update.service.enabled";
const PREF_APP_UPDATE_SERVICE_ERRORS      = "app.update.service.errors";
const PREF_APP_UPDATE_SERVICE_MAX_ERRORS  = "app.update.service.maxErrors";
const PREF_APP_UPDATE_SOCKET_ERRORS       = "app.update.socket.maxErrors";
const PREF_APP_UPDATE_RETRY_TIMEOUT       = "app.update.socket.retryTimeout";

const PREF_APP_DISTRIBUTION               = "distribution.id";
const PREF_APP_DISTRIBUTION_VERSION       = "distribution.version";

const PREF_APP_B2G_VERSION                = "b2g.version";

const PREF_EM_HOTFIX_ID                   = "extensions.hotfix.id";

const URI_UPDATE_PROMPT_DIALOG  = "chrome://mozapps/content/update/updates.xul";
const URI_UPDATE_HISTORY_DIALOG = "chrome://mozapps/content/update/history.xul";
const URI_BRAND_PROPERTIES      = "chrome://branding/locale/brand.properties";
const URI_UPDATES_PROPERTIES    = "chrome://mozapps/locale/update/updates.properties";
const URI_UPDATE_NS             = "http://www.mozilla.org/2005/app-update";

const KEY_GRED            = "GreD";
const KEY_UPDROOT         = "UpdRootD";
const KEY_EXECUTABLE      = "XREExeF";
// Gonk only
const KEY_UPDATE_ARCHIVE_DIR = "UpdArchD";

const DIR_UPDATED         = "updated";
const DIR_UPDATED_APP     = "Updated.app";
const DIR_UPDATES         = "updates";

const FILE_UPDATE_STATUS  = "update.status";
const FILE_UPDATE_VERSION = "update.version";
const FILE_UPDATE_ARCHIVE = "update.mar";
const FILE_UPDATE_LINK    = "update.link";
const FILE_UPDATE_LOG     = "update.log";
const FILE_UPDATES_DB     = "updates.xml";
const FILE_UPDATE_ACTIVE  = "active-update.xml";
const FILE_PERMS_TEST     = "update.test";
const FILE_LAST_LOG       = "last-update.log";
const FILE_BACKUP_LOG     = "backup-update.log";
const FILE_UPDATE_LOCALE  = "update.locale";

const STATE_NONE            = "null";
const STATE_DOWNLOADING     = "downloading";
const STATE_PENDING         = "pending";
const STATE_PENDING_SVC     = "pending-service";
const STATE_APPLYING        = "applying";
const STATE_APPLIED         = "applied";
const STATE_APPLIED_OS      = "applied-os";
const STATE_APPLIED_SVC     = "applied-service";
const STATE_SUCCEEDED       = "succeeded";
const STATE_DOWNLOAD_FAILED = "download-failed";
const STATE_FAILED          = "failed";

// The values below used by this code are from common/errors.h
const WRITE_ERROR                          = 7;
const ELEVATION_CANCELED                   = 9;
const SERVICE_UPDATER_COULD_NOT_BE_STARTED = 24;
const SERVICE_NOT_ENOUGH_COMMAND_LINE_ARGS = 25;
const SERVICE_UPDATER_SIGN_ERROR           = 26;
const SERVICE_UPDATER_COMPARE_ERROR        = 27;
const SERVICE_UPDATER_IDENTITY_ERROR       = 28;
const SERVICE_STILL_APPLYING_ON_SUCCESS    = 29;
const SERVICE_STILL_APPLYING_ON_FAILURE    = 30;
const SERVICE_UPDATER_NOT_FIXED_DRIVE      = 31;
const SERVICE_COULD_NOT_LOCK_UPDATER       = 32;
const SERVICE_INSTALLDIR_ERROR             = 33;
const WRITE_ERROR_ACCESS_DENIED            = 35;
const WRITE_ERROR_CALLBACK_APP             = 37;
const FILESYSTEM_MOUNT_READWRITE_ERROR     = 43;
const SERVICE_COULD_NOT_COPY_UPDATER       = 49;
const SERVICE_STILL_APPLYING_TERMINATED    = 50;
const SERVICE_STILL_APPLYING_NO_EXIT_CODE  = 51;
const WRITE_ERROR_FILE_COPY                = 61;
const WRITE_ERROR_DELETE_FILE              = 62;
const WRITE_ERROR_OPEN_PATCH_FILE          = 63;
const WRITE_ERROR_PATCH_FILE               = 64;
const WRITE_ERROR_APPLY_DIR_PATH           = 65;
const WRITE_ERROR_CALLBACK_PATH            = 66;
const WRITE_ERROR_FILE_ACCESS_DENIED       = 67;
const WRITE_ERROR_DIR_ACCESS_DENIED        = 68;
const WRITE_ERROR_DELETE_BACKUP            = 69;
const WRITE_ERROR_EXTRACT                  = 70;

// Array of write errors to simplify checks for write errors
const WRITE_ERRORS = [WRITE_ERROR,
                      WRITE_ERROR_ACCESS_DENIED,
                      WRITE_ERROR_CALLBACK_APP,
                      WRITE_ERROR_FILE_COPY,
                      WRITE_ERROR_DELETE_FILE,
                      WRITE_ERROR_OPEN_PATCH_FILE,
                      WRITE_ERROR_PATCH_FILE,
                      WRITE_ERROR_APPLY_DIR_PATH,
                      WRITE_ERROR_CALLBACK_PATH,
                      WRITE_ERROR_FILE_ACCESS_DENIED,
                      WRITE_ERROR_DIR_ACCESS_DENIED,
                      WRITE_ERROR_DELETE_BACKUP,
                      WRITE_ERROR_EXTRACT];

// Array of write errors to simplify checks for service errors
const SERVICE_ERRORS = [SERVICE_UPDATER_COULD_NOT_BE_STARTED,
                        SERVICE_NOT_ENOUGH_COMMAND_LINE_ARGS,
                        SERVICE_UPDATER_SIGN_ERROR,
                        SERVICE_UPDATER_COMPARE_ERROR,
                        SERVICE_UPDATER_IDENTITY_ERROR,
                        SERVICE_STILL_APPLYING_ON_SUCCESS,
                        SERVICE_STILL_APPLYING_ON_FAILURE,
                        SERVICE_UPDATER_NOT_FIXED_DRIVE,
                        SERVICE_COULD_NOT_LOCK_UPDATER,
                        SERVICE_INSTALLDIR_ERROR,
                        SERVICE_COULD_NOT_COPY_UPDATER,
                        SERVICE_STILL_APPLYING_TERMINATED,
                        SERVICE_STILL_APPLYING_NO_EXIT_CODE];

// Error codes 80 through 99 are reserved for nsUpdateService.js and are not
// defined in common/errors.h
const FOTA_GENERAL_ERROR                   = 80;
const FOTA_UNKNOWN_ERROR                   = 81;
const FOTA_FILE_OPERATION_ERROR            = 82;
const FOTA_RECOVERY_ERROR                  = 83;
const INVALID_UPDATER_STATE_CODE           = 98;
const INVALID_UPDATER_STATUS_CODE          = 99;

// Custom update error codes
const CERT_ATTR_CHECK_FAILED_NO_UPDATE  = 100;
const CERT_ATTR_CHECK_FAILED_HAS_UPDATE = 101;
const BACKGROUNDCHECK_MULTIPLE_FAILURES = 110;
const NETWORK_ERROR_OFFLINE             = 111;

// Error codes should be < 1000. Errors above 1000 represent http status codes
const HTTP_ERROR_OFFSET                 = 1000;

const DOWNLOAD_CHUNK_SIZE           = 300000; // bytes
const DOWNLOAD_BACKGROUND_INTERVAL  = 600;    // seconds
const DOWNLOAD_FOREGROUND_INTERVAL  = 0;

const UPDATE_WINDOW_NAME      = "Update:Wizard";

// The number of consecutive failures when updating using the service before
// setting the app.update.service.enabled preference to false.
const DEFAULT_SERVICE_MAX_ERRORS = 10;

// The number of consecutive socket errors to allow before falling back to
// downloading a different MAR file or failing if already downloading the full.
const DEFAULT_SOCKET_MAX_ERRORS = 10;

// The number of milliseconds to wait before retrying a connection error.
const DEFAULT_UPDATE_RETRY_TIMEOUT = 2000;

var gLocale = null;
var gUpdateMutexHandle = null;

// Gonk only
var gSDCardMountLock = null;

// Gonk only
XPCOMUtils.defineLazyGetter(this, "gExtStorage", function aus_gExtStorage() {
  if (AppConstants.platform != "gonk") {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }
  return Services.env.get("EXTERNAL_STORAGE");
});

XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
                                  "resource://gre/modules/UpdateChannel.jsm");

XPCOMUtils.defineLazyGetter(this, "gLogEnabled", function aus_gLogEnabled() {
  return getPref("getBoolPref", PREF_APP_UPDATE_LOG, false);
});

XPCOMUtils.defineLazyGetter(this, "gUpdateBundle", function aus_gUpdateBundle() {
  return Services.strings.createBundle(URI_UPDATES_PROPERTIES);
});

// shared code for suppressing bad cert dialogs
XPCOMUtils.defineLazyGetter(this, "gCertUtils", function aus_gCertUtils() {
  let temp = { };
  Cu.import("resource://gre/modules/CertUtils.jsm", temp);
  return temp;
});

XPCOMUtils.defineLazyGetter(this, "gABI", function aus_gABI() {
  let abi = null;
  try {
    abi = Services.appinfo.XPCOMABI;
  }
  catch (e) {
    LOG("gABI - XPCOM ABI unknown: updates are not possible.");
  }

  if (AppConstants.platform == "macosx") {
    // Mac universal build should report a different ABI than either macppc
    // or mactel.
    let macutils = Cc["@mozilla.org/xpcom/mac-utils;1"].
                   getService(Ci.nsIMacUtils);

    if (macutils.isUniversalBinary) {
      abi += "-u-" + macutils.architecturesInBinary;
    }
  }
  return abi;
});

XPCOMUtils.defineLazyGetter(this, "gOSVersion", function aus_gOSVersion() {
  let osVersion;
  try {
    osVersion = Services.sysinfo.getProperty("name") + " " +
                Services.sysinfo.getProperty("version");
  }
  catch (e) {
    LOG("gOSVersion - OS Version unknown: updates are not possible.");
  }

  if (osVersion) {
    if (AppConstants.platform == "win") {
      const BYTE = ctypes.uint8_t;
      const WORD = ctypes.uint16_t;
      const DWORD = ctypes.uint32_t;
      const WCHAR = ctypes.char16_t;
      const BOOL = ctypes.int;

      // This structure is described at:
      // http://msdn.microsoft.com/en-us/library/ms724833%28v=vs.85%29.aspx
      const SZCSDVERSIONLENGTH = 128;
      const OSVERSIONINFOEXW = new ctypes.StructType('OSVERSIONINFOEXW',
          [
          {dwOSVersionInfoSize: DWORD},
          {dwMajorVersion: DWORD},
          {dwMinorVersion: DWORD},
          {dwBuildNumber: DWORD},
          {dwPlatformId: DWORD},
          {szCSDVersion: ctypes.ArrayType(WCHAR, SZCSDVERSIONLENGTH)},
          {wServicePackMajor: WORD},
          {wServicePackMinor: WORD},
          {wSuiteMask: WORD},
          {wProductType: BYTE},
          {wReserved: BYTE}
          ]);

      // This structure is described at:
      // http://msdn.microsoft.com/en-us/library/ms724958%28v=vs.85%29.aspx
      const SYSTEM_INFO = new ctypes.StructType('SYSTEM_INFO',
          [
          {wProcessorArchitecture: WORD},
          {wReserved: WORD},
          {dwPageSize: DWORD},
          {lpMinimumApplicationAddress: ctypes.voidptr_t},
          {lpMaximumApplicationAddress: ctypes.voidptr_t},
          {dwActiveProcessorMask: DWORD.ptr},
          {dwNumberOfProcessors: DWORD},
          {dwProcessorType: DWORD},
          {dwAllocationGranularity: DWORD},
          {wProcessorLevel: WORD},
          {wProcessorRevision: WORD}
          ]);

      let kernel32 = false;
      try {
        kernel32 = ctypes.open("Kernel32");
      } catch (e) {
        LOG("gOSVersion - Unable to open kernel32! " + e);
        osVersion += ".unknown (unknown)";
      }

      if(kernel32) {
        try {
          // Get Service pack info
          try {
            let GetVersionEx = kernel32.declare("GetVersionExW",
                                                ctypes.default_abi,
                                                BOOL,
                                                OSVERSIONINFOEXW.ptr);
            let winVer = OSVERSIONINFOEXW();
            winVer.dwOSVersionInfoSize = OSVERSIONINFOEXW.size;

            if(0 !== GetVersionEx(winVer.address())) {
              osVersion += "." + winVer.wServicePackMajor
                        +  "." + winVer.wServicePackMinor;
            } else {
              LOG("gOSVersion - Unknown failure in GetVersionEX (returned 0)");
              osVersion += ".unknown";
            }
          } catch (e) {
            LOG("gOSVersion - error getting service pack information. Exception: " + e);
            osVersion += ".unknown";
          }

          // Get processor architecture
          let arch = "unknown";
          try {
            let GetNativeSystemInfo = kernel32.declare("GetNativeSystemInfo",
                                                       ctypes.default_abi,
                                                       ctypes.void_t,
                                                       SYSTEM_INFO.ptr);
            let winSystemInfo = SYSTEM_INFO();
            // Default to unknown
            winSystemInfo.wProcessorArchitecture = 0xffff;

            GetNativeSystemInfo(winSystemInfo.address());
            switch(winSystemInfo.wProcessorArchitecture) {
              case 9:
                arch = "x64";
                break;
              case 6:
                arch = "IA64";
                break;
              case 0:
                arch = "x86";
                break;
            }
          } catch (e) {
            LOG("gOSVersion - error getting processor architecture.  Exception: " + e);
          } finally {
            osVersion += " (" + arch + ")";
          }
        } finally {
          kernel32.close();
        }
      }
    }

    try {
      osVersion += " (" + Services.sysinfo.getProperty("secondaryLibrary") + ")";
    }
    catch (e) {
      // Not all platforms have a secondary widget library, so an error is nothing to worry about.
    }
    osVersion = encodeURIComponent(osVersion);
  }
  return osVersion;
});

/**
 * Tests to make sure that we can write to a given directory.
 *
 * @param updateTestFile a test file in the directory that needs to be tested.
 * @param createDirectory whether a test directory should be created.
 * @throws if we don't have right access to the directory.
 */
function testWriteAccess(updateTestFile, createDirectory) {
  const NORMAL_FILE_TYPE = Ci.nsILocalFile.NORMAL_FILE_TYPE;
  const DIRECTORY_TYPE = Ci.nsILocalFile.DIRECTORY_TYPE;
  if (updateTestFile.exists())
    updateTestFile.remove(false);
  updateTestFile.create(createDirectory ? DIRECTORY_TYPE : NORMAL_FILE_TYPE,
                        createDirectory ? FileUtils.PERMS_DIRECTORY : FileUtils.PERMS_FILE);
  updateTestFile.remove(false);
}

/**
 * Windows only function that closes a Win32 handle.
 *
 * @param handle The handle to close
 */
function closeHandle(handle) {
  let lib = ctypes.open("kernel32.dll");
  let CloseHandle = lib.declare("CloseHandle",
                                ctypes.winapi_abi,
                                ctypes.int32_t, /* success */
                                ctypes.void_t.ptr); /* handle */
  CloseHandle(handle);
  lib.close();
}

/**
 * Windows only function that creates a mutex.
 *
 * @param  aName
 *         The name for the mutex.
 * @param  aAllowExisting
 *         If false the function will close the handle and return null.
 * @return The Win32 handle to the mutex.
 */
function createMutex(aName, aAllowExisting = true) {
  if (AppConstants.platform != "win") {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }

  const INITIAL_OWN = 1;
  const ERROR_ALREADY_EXISTS = 0xB7;
  let lib = ctypes.open("kernel32.dll");
  let CreateMutexW = lib.declare("CreateMutexW",
                                 ctypes.winapi_abi,
                                 ctypes.void_t.ptr, /* return handle */
                                 ctypes.void_t.ptr, /* security attributes */
                                 ctypes.int32_t, /* initial owner */
                                 ctypes.char16_t.ptr); /* name */

  let handle = CreateMutexW(null, INITIAL_OWN, aName);
  let alreadyExists = ctypes.winLastError == ERROR_ALREADY_EXISTS;
  if (handle && !handle.isNull() && !aAllowExisting && alreadyExists) {
    closeHandle(handle);
    handle = null;
  }
  lib.close();

  if (handle && handle.isNull()) {
    handle = null;
  }

  return handle;
}

/**
 * Windows only function that determines a unique mutex name for the
 * installation.
 *
 * @param aGlobal true if the function should return a global mutex. A global
 *                mutex is valid across different sessions
 * @return Global mutex path
 */
function getPerInstallationMutexName(aGlobal = true) {
  if (AppConstants.platform != "win") {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }

  let hasher = Cc["@mozilla.org/security/hash;1"].
               createInstance(Ci.nsICryptoHash);
  hasher.init(hasher.SHA1);

  let exeFile = Services.dirsvc.get(KEY_EXECUTABLE, Ci.nsILocalFile);

  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                  createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  var data = converter.convertToByteArray(exeFile.path.toLowerCase());

  hasher.update(data, data.length);
  return (aGlobal ? "Global\\" : "") + "MozillaUpdateMutex-" + hasher.finish(true);
}

/**
 * Whether or not the current instance has the update mutex. The update mutex
 * gives protection against 2 applications from the same installation updating:
 * 1) Running multiple profiles from the same installation path
 * 2) Two applications running in 2 different user sessions from the same path
 *
 * @return true if this instance holds the update mutex
 */
function hasUpdateMutex() {
  if (AppConstants.platform != "win") {
    return true;
  }
  if (!gUpdateMutexHandle) {
    gUpdateMutexHandle = createMutex(getPerInstallationMutexName(true), false);
  }
  return !!gUpdateMutexHandle;
}

XPCOMUtils.defineLazyGetter(this, "gCanApplyUpdates", function aus_gCanApplyUpdates() {
  let useService = false;
  if (shouldUseService() && isServiceInstalled()) {
    // No need to perform directory write checks, the maintenance service will
    // be able to write to all directories.
    LOG("gCanApplyUpdates - bypass the write checks because we'll use the service");
    useService = true;
  }

  if (!useService) {
    try {
      let updateTestFile = getUpdateFile([FILE_PERMS_TEST]);
      LOG("gCanApplyUpdates - testing write access " + updateTestFile.path);
      testWriteAccess(updateTestFile, false);
      if (AppConstants.platform == "macosx") {
        // Check that the application bundle can be written to.
        let appDirTestFile = getAppBaseDir();
        appDirTestFile.append(FILE_PERMS_TEST);
        LOG("gCanApplyUpdates - testing write access " + appDirTestFile.path);
        if (appDirTestFile.exists()) {
          appDirTestFile.remove(false)
        }
        appDirTestFile.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
        appDirTestFile.remove(false);
      } else if (AppConstants.platform == "win") {
        // Example windowsVersion:  Windows XP == 5.1
        let windowsVersion = Services.sysinfo.getProperty("version");
        LOG("gCanApplyUpdates - windowsVersion = " + windowsVersion);

      /**
       * For Vista, updates can be performed to a location requiring admin
       * privileges by requesting elevation via the UAC prompt when launching
       * updater.exe if the appDir is under the Program Files directory
       * (e.g. C:\Program Files\) and UAC is turned on and  we can elevate
       * (e.g. user has a split token).
       *
       * Note: this does note attempt to handle the case where UAC is turned on
       * and the installation directory is in a restricted location that
       * requires admin privileges to update other than Program Files.
       */
        let userCanElevate = false;

        if (parseFloat(windowsVersion) >= 6) {
          try {
            // KEY_UPDROOT will fail and throw an exception if
            // appDir is not under the Program Files, so we rely on that
            let dir = Services.dirsvc.get(KEY_UPDROOT, Ci.nsIFile);
            // appDir is under Program Files, so check if the user can elevate
            userCanElevate = Services.appinfo.QueryInterface(Ci.nsIWinAppHelper).
                             userCanElevate;
            LOG("gCanApplyUpdates - on Vista, userCanElevate: " + userCanElevate);
          }
          catch (ex) {
            // When the installation directory is not under Program Files,
            // fall through to checking if write access to the
            // installation directory is available.
            LOG("gCanApplyUpdates - on Vista, appDir is not under Program Files");
          }
        }

        /**
         * On Windows, we no longer store the update under the app dir.
         *
         * If we are on Windows (including Vista, if we can't elevate) we need to
         * to check that we can create and remove files from the actual app
         * directory (like C:\Program Files\Mozilla Firefox).  If we can't
         * (because this user is not an adminstrator, for example) canUpdate()
         * should return false.
         *
         * For Vista, we perform this check to enable updating the  application
         * when the user has write access to the installation directory under the
         * following scenarios:
         * 1) the installation directory is not under Program Files
         *    (e.g. C:\Program Files)
         * 2) UAC is turned off
         * 3) UAC is turned on and the user is not an admin
         *    (e.g. the user does not have a split token)
         * 4) UAC is turned on and the user is already elevated, so they can't be
         *    elevated again
         */
        if (!userCanElevate) {
          // if we're unable to create the test file this will throw an exception.
          let appDirTestFile = getAppBaseDir();
          appDirTestFile.append(FILE_PERMS_TEST);
          LOG("gCanApplyUpdates - testing write access " + appDirTestFile.path);
          if (appDirTestFile.exists())
            appDirTestFile.remove(false)
          appDirTestFile.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
          appDirTestFile.remove(false);
        }
      }
    } catch (e) {
       LOG("gCanApplyUpdates - unable to apply updates. Exception: " + e);
      // No write privileges to install directory
      return false;
    }
  } // if (!useService)

  LOG("gCanApplyUpdates - able to apply updates");
  return true;
});

/**
 * Whether or not the application can stage an update.
 *
 * @return true if updates can be staged.
 */
function getCanStageUpdates() {
  // If background updates are disabled, then just bail out!
  if (!getPref("getBoolPref", PREF_APP_UPDATE_STAGING_ENABLED, false)) {
    LOG("getCanStageUpdates - staging updates is disabled by preference " +
        PREF_APP_UPDATE_STAGING_ENABLED);
    return false;
  }


  if (AppConstants.platform == "win" && isServiceInstalled() &&
      shouldUseService()) {
    // No need to perform directory write checks, the maintenance service will
    // be able to write to all directories.
    LOG("getCanStageUpdates - able to stage updates because we'll use the service");
    return true;
  }

  // For Gonk, the updater will remount the /system partition to move staged
  // files into place.
  if (AppConstants.platform == "gonk") {
    LOG("getCanStageUpdates - able to stage updates because this is gonk");
    return true;
  }

  if (!hasUpdateMutex()) {
    LOG("getCanStageUpdates - unable to apply updates because another " +
        "instance of the application is already handling updates for this " +
        "installation.");
    return false;
  }

  /**
   * Whether or not the application can stage an update for the current session.
   * These checks are only performed once per session due to using a lazy getter.
   *
   * @return true if updates can be staged for this session.
   */
  XPCOMUtils.defineLazyGetter(this, "canStageUpdatesSession", function canStageUpdatesSession() {
    try {
      var updateTestFile = getInstallDirRoot();
      updateTestFile.append(FILE_PERMS_TEST);
      LOG("canStageUpdatesSession - testing write access " +
          updateTestFile.path);
      testWriteAccess(updateTestFile, true);
      if (AppConstants.platform != "macosx") {
        // On all platforms except Mac, we need to test the parent directory as
        // well, as we need to be able to move files in that directory during the
        // replacing step.
        updateTestFile = getInstallDirRoot().parent;
        updateTestFile.append(FILE_PERMS_TEST);
        LOG("canStageUpdatesSession - testing write access " +
            updateTestFile.path);
        updateTestFile.createUnique(Ci.nsILocalFile.DIRECTORY_TYPE,
                                    FileUtils.PERMS_DIRECTORY);
        updateTestFile.remove(false);
      }
    } catch (e) {
       LOG("canStageUpdatesSession - unable to stage updates. Exception: " +
           e);
      // No write privileges
      return false;
    }

    LOG("canStageUpdatesSession - able to stage updates");
    return true;
  });

  return canStageUpdatesSession;
}

XPCOMUtils.defineLazyGetter(this, "gCanCheckForUpdates", function aus_gCanCheckForUpdates() {
  // If the administrator has disabled app update and locked the preference so
  // users can't check for updates. This preference check is ok in this lazy
  // getter since locked prefs don't change until the application is restarted.
  var enabled = getPref("getBoolPref", PREF_APP_UPDATE_ENABLED, true);
  if (!enabled && Services.prefs.prefIsLocked(PREF_APP_UPDATE_ENABLED)) {
    LOG("gCanCheckForUpdates - unable to automatically check for updates, " +
        "the preference is disabled and admistratively locked.");
    return false;
  }

  // If we don't know the binary platform we're updating, we can't update.
  if (!gABI) {
    LOG("gCanCheckForUpdates - unable to check for updates, unknown ABI");
    return false;
  }

  // If we don't know the OS version we're updating, we can't update.
  if (!gOSVersion) {
    LOG("gCanCheckForUpdates - unable to check for updates, unknown OS " +
        "version");
    return false;
  }

  LOG("gCanCheckForUpdates - able to check for updates");
  return true;
});

/**
 * Logs a string to the error console.
 * @param   string
 *          The string to write to the error console.
 */
function LOG(string) {
  if (gLogEnabled) {
    dump("*** AUS:SVC " + string + "\n");
    Services.console.logStringMessage("AUS:SVC " + string);
  }
}

/**
 * Gets a preference value, handling the case where there is no default.
 * @param   func
 *          The name of the preference function to call, on nsIPrefBranch
 * @param   preference
 *          The name of the preference
 * @param   defaultValue
 *          The default value to return in the event the preference has
 *          no setting
 * @return  The value of the preference, or undefined if there was no
 *          user or default value.
 */
function getPref(func, preference, defaultValue) {
  try {
    return Services.prefs[func](preference);
  }
  catch (e) {
  }
  return defaultValue;
}

/**
 * Convert a string containing binary values to hex.
 */
function binaryToHex(input) {
  var result = "";
  for (var i = 0; i < input.length; ++i) {
    var hex = input.charCodeAt(i).toString(16);
    if (hex.length == 1)
      hex = "0" + hex;
    result += hex;
  }
  return result;
}

/**
 * Gets the specified directory at the specified hierarchy under the
 * update root directory and creates it if it doesn't exist.
 * @param   pathArray
 *          An array of path components to locate beneath the directory
 *          specified by |key|
 * @return  nsIFile object for the location specified.
 */
function getUpdateDirCreate(pathArray) {
  return FileUtils.getDir(KEY_UPDROOT, pathArray, true);
}

/**
 * Gets the specified directory at the specified hierarchy under the
 * update root directory and without creating it if it doesn't exist.
 * @param   pathArray
 *          An array of path components to locate beneath the directory
 *          specified by |key|
 * @return  nsIFile object for the location specified.
 */
function getUpdateDirNoCreate(pathArray) {
  return FileUtils.getDir(KEY_UPDROOT, pathArray, false);
}

/**
 * Gets the application base directory.
 *
 * @return  nsIFile object for the application base directory.
 */
function getAppBaseDir() {
  return Services.dirsvc.get(KEY_EXECUTABLE, Ci.nsIFile).parent;
}

/**
 * Gets the root of the installation directory which is the application
 * bundle directory on Mac OS X and the location of the application binary
 * on all other platforms.
 *
 * @return nsIFile object for the directory
 */
function getInstallDirRoot() {
  let dir = getAppBaseDir();
  if (AppConstants.platform == "macosx") {
    // On Mac, we store the Updated.app directory inside the bundle directory.
    dir = dir.parent.parent;
  }
  return dir;
}

/**
 * Gets the file at the specified hierarchy under the update root directory.
 * @param   pathArray
 *          An array of path components to locate beneath the directory
 *          specified by |key|. The last item in this array must be the
 *          leaf name of a file.
 * @return  nsIFile object for the file specified. The file is NOT created
 *          if it does not exist, however all required directories along
 *          the way are.
 */
function getUpdateFile(pathArray) {
  let file = getUpdateDirCreate(pathArray.slice(0, -1));
  file.append(pathArray[pathArray.length - 1]);
  return file;
}

/**
 * Returns human readable status text from the updates.properties bundle
 * based on an error code
 * @param   code
 *          The error code to look up human readable status text for
 * @param   defaultCode
 *          The default code to look up should human readable status text
 *          not exist for |code|
 * @return  A human readable status text string
 */
function getStatusTextFromCode(code, defaultCode) {
  let reason;
  try {
    reason = gUpdateBundle.GetStringFromName("check_error-" + code);
    LOG("getStatusTextFromCode - transfer error: " + reason + ", code: " +
        code);
  }
  catch (e) {
    // Use the default reason
    reason = gUpdateBundle.GetStringFromName("check_error-" + defaultCode);
    LOG("getStatusTextFromCode - transfer error: " + reason +
        ", default code: " + defaultCode);
  }
  return reason;
}

/**
 * Get the Active Updates directory
 * @return The active updates directory, as a nsIFile object
 */
function getUpdatesDir() {
  // Right now, we only support downloading one patch at a time, so we always
  // use the same target directory.
  return getUpdateDirCreate([DIR_UPDATES, "0"]);
}

/**
 * Get the Active Updates directory inside the directory where we apply the
 * staged update.
 * @return The active updates directory inside the updated directory, as a
 *         nsIFile object.
 */
function getUpdatesDirInApplyToDir() {
  let dir = getAppBaseDir();
  if (AppConstants.platform == "macosx") {
    dir = dir.parent.parent; // the bundle directory
    dir.append(DIR_UPDATED_APP);
    dir.append("Contents");
    dir.append("MacOS");
  } else {
    dir.append(DIR_UPDATED);
  }
  dir.append(DIR_UPDATES);
  if (!dir.exists()) {
    dir.create(Ci.nsILocalFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  }
  return dir;
}

/**
 * Reads the update state from the update.status file in the specified
 * directory.
 * @param   dir
 *          The dir to look for an update.status file in
 * @return  The status value of the update.
 */
function readStatusFile(dir) {
  let statusFile = dir.clone();
  statusFile.append(FILE_UPDATE_STATUS);
  let status = readStringFromFile(statusFile) || STATE_NONE;
  LOG("readStatusFile - status: " + status + ", path: " + statusFile.path);
  return status;
}

/**
 * Writes the current update operation/state to a file in the patch
 * directory, indicating to the patching system that operations need
 * to be performed.
 * @param   dir
 *          The patch directory where the update.status file should be
 *          written.
 * @param   state
 *          The state value to write.
 */
function writeStatusFile(dir, state) {
  let statusFile = dir.clone();
  statusFile.append(FILE_UPDATE_STATUS);
  writeStringToFile(statusFile, state);
}

/**
 * Writes the update's application version to a file in the patch directory. If
 * the update doesn't provide application version information via the
 * appVersion attribute the string "null" will be written to the file.
 * This value is compared during startup (in nsUpdateDriver.cpp) to determine if
 * the update should be applied. Note that this won't provide protection from
 * downgrade of the application for the nightly user case where the application
 * version doesn't change.
 * @param   dir
 *          The patch directory where the update.version file should be
 *          written.
 * @param   version
 *          The version value to write. Will be the string "null" when the
 *          update doesn't provide the appVersion attribute in the update xml.
 */
function writeVersionFile(dir, version) {
  let versionFile = dir.clone();
  versionFile.append(FILE_UPDATE_VERSION);
  writeStringToFile(versionFile, version);
}

/**
 * Gonk only function that reads the link file specified in the update.link file
 * in the specified directory and returns the nsIFile for the corresponding
 * file.
 * @param   dir
 *          The dir to look for an update.link file in
 * @return  A nsIFile for the file path specified in the
 *          update.link file or null if the update.link file
 *          doesn't exist.
 */
function getFileFromUpdateLink(dir) {
  if (AppConstants.platform != "gonk") {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }
  let linkFile = dir.clone();
  linkFile.append(FILE_UPDATE_LINK);
  let link = readStringFromFile(linkFile);
  LOG("getFileFromUpdateLink linkFile.path: " + linkFile.path + ", link: " + link);
  if (!link) {
    return null;
  }
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(link);
  return file;
}

/**
 * Gonk only function to create a link file. This allows the actual patch to
 * live in a directory different from the update directory.
 * @param   dir
 *          The patch directory where the update.link file
 *          should be written.
 * @param   patchFile
 *          The fully qualified filename of the patchfile.
 */
function writeLinkFile(dir, patchFile) {
  if (AppConstants.platform != "gonk") {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }
  let linkFile = dir.clone();
  linkFile.append(FILE_UPDATE_LINK);
  writeStringToFile(linkFile, patchFile.path);
  if (patchFile.path.indexOf(gExtStorage) == 0) {
    // The patchfile is being stored on external storage. Try to lock it
    // so that it doesn't get shared with the PC while we're downloading
    // to it.
    acquireSDCardMountLock();
  }
}

/**
 * Gonk only function to acquire a VolumeMountLock for the sdcard volume.
 *
 * This prevents the SDCard from being shared with the PC while
 * we're downloading the update.
 */
function acquireSDCardMountLock() {
  if (AppConstants.platform != "gonk") {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }
  let volsvc = Cc["@mozilla.org/telephony/volume-service;1"].
                    getService(Ci.nsIVolumeService);
  if (volsvc) {
    gSDCardMountLock = volsvc.createMountLock("sdcard");
  }
}

/**
 * Gonk only function that determines if the state corresponds to an
 * interrupted update. This could either be because the download was
 * interrupted, or because staging the update was interrupted.
 *
 * @return true if the state corresponds to an interrupted
 *         update.
 */
function isInterruptedUpdate(status) {
  if (AppConstants.platform != "gonk") {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }
  return (status == STATE_DOWNLOADING) ||
         (status == STATE_PENDING) ||
         (status == STATE_APPLYING);
}

/**
 * Releases any SDCard mount lock that we might have.
 *
 * This once again allows the SDCard to be shared with the PC.
 */
function releaseSDCardMountLock() {
  if (AppConstants.platform != "gonk") {
    throw Cr.NS_ERROR_UNEXPECTED;
  }
  if (gSDCardMountLock) {
    gSDCardMountLock.unlock();
    gSDCardMountLock = null;
  }
}

/**
 * Determines if the service should be used to attempt an update
 * or not.  For now this is only when PREF_APP_UPDATE_SERVICE_ENABLED
 * is true and we have Firefox.
 *
 * @return  true if the service should be used for updates.
 */
function shouldUseService() {
  if (AppConstants.MOZ_MAINTENANCE_SERVICE) {
    return getPref("getBoolPref",
                   PREF_APP_UPDATE_SERVICE_ENABLED, false);
  }
  return false;
}

/**
 * Determines if the service is is installed and enabled or not.
 *
 * @return  true if the service should be used for updates,
 *          is installed and enabled.
 */
function isServiceInstalled() {
  if (AppConstants.MOZ_MAINTENANCE_SERVICE && AppConstants.platform == "win") {
    let installed = 0;
    try {
      let wrk = Cc["@mozilla.org/windows-registry-key;1"].
                createInstance(Ci.nsIWindowsRegKey);
      wrk.open(wrk.ROOT_KEY_LOCAL_MACHINE,
               "SOFTWARE\\Mozilla\\MaintenanceService",
               wrk.ACCESS_READ | wrk.WOW64_64);
      installed = wrk.readIntValue("Installed");
      wrk.close();
    } catch(e) {
    }
    installed = installed == 1;  // convert to bool
    LOG("isServiceInstalled = " + installed);
    return installed;
  }
  return false;
}

/**
 * Removes the MozUpdater directory that is created when replacing an install
 * with a staged update and leftover MozUpdater-i folders in the tmp directory.
 */
function cleanUpMozUpdaterDirs() {
  try {
    // Remove the MozUpdater directory in the updates/0 directory.
    var mozUpdaterDir = getUpdatesDir();
    mozUpdaterDir.append("MozUpdater");
    if (mozUpdaterDir.exists()) {
      LOG("cleanUpMozUpdaterDirs - removing MozUpdater directory");
      mozUpdaterDir.remove(true);
    }
  } catch (e) {
    LOG("cleanUpMozUpdaterDirs - Exception: " + e);
  }

  try {
    var tmpDir = Services.dirsvc.get("TmpD", Ci.nsIFile);

    // We used to store MozUpdater-i directories in the temp directory.
    // We need to remove these directories if we detect that they still exist.
    // To check if they still exist, we simply check for MozUpdater-1.
    var mozUpdaterDir1 = tmpDir.clone();
    mozUpdaterDir1.append("MozUpdater-1");
    // Only try to delete the left over directories in "$Temp/MozUpdater-i/*" if
    // MozUpdater-1 exists.
    if (mozUpdaterDir1.exists()) {
      LOG("cleanUpMozUpdaterDirs - Removing top level tmp MozUpdater-i " +
          "directories");
      let i = 0;
      let dirEntries = tmpDir.directoryEntries;
      while (dirEntries.hasMoreElements() && i < 10) {
        let file = dirEntries.getNext().QueryInterface(Ci.nsILocalFile);
        if (file.leafName.startsWith("MozUpdater-") && file.leafName != "MozUpdater-1") {
          file.remove(true);
          i++;
        }
      }
      // If you enumerate the whole temp directory and the count of deleted
      // items is less than 10, then delete MozUpdate-1.
      if (i < 10) {
        mozUpdaterDir1.remove(true);
      }
    }
  } catch (e) {
    LOG("cleanUpMozUpdaterDirs - Exception: " + e);
  }
}

/**
 * Removes the contents of the Updates Directory
 *
 * @param aBackgroundUpdate Whether the update has been performed in the
 *        background.  If this is true, we move the update log file to the
 *        updated directory, so that it survives replacing the directories
 *        later on.
 */
function cleanUpUpdatesDir(aBackgroundUpdate) {
  // Bail out if we don't have appropriate permissions
  try {
    var updateDir = getUpdatesDir();
  } catch (e) {
    return;
  }

  // Preserve the last update log file for debugging purposes.
  let file = updateDir.clone();
  file.append(FILE_UPDATE_LOG);
  if (file.exists()) {
    let dir;
    if (aBackgroundUpdate && getUpdateDirNoCreate([]).equals(getAppBaseDir())) {
      dir = getUpdatesDirInApplyToDir();
    } else {
      dir = updateDir.parent;
    }
    let logFile = dir.clone();
    logFile.append(FILE_LAST_LOG);
    if (logFile.exists()) {
      try {
        logFile.moveTo(dir, FILE_BACKUP_LOG);
      } catch (e) {
        LOG("cleanUpUpdatesDir - failed to rename file " + logFile.path +
            " to " + FILE_BACKUP_LOG);
      }
    }

    try {
      file.moveTo(dir, FILE_LAST_LOG);
    } catch (e) {
      LOG("cleanUpUpdatesDir - failed to rename file " + file.path +
          " to " + FILE_LAST_LOG);
    }
  }

  if (!aBackgroundUpdate) {
    let e = updateDir.directoryEntries;
    while (e.hasMoreElements()) {
      let f = e.getNext().QueryInterface(Ci.nsIFile);
      if (AppConstants.platform == "gonk") {
        if (f.leafName == FILE_UPDATE_LINK) {
          let linkedFile = getFileFromUpdateLink(updateDir);
          if (linkedFile && linkedFile.exists()) {
            linkedFile.remove(false);
          }
        }
      }

      // Now, recursively remove this file.  The recursive removal is needed for
      // Mac OSX because this directory will contain a copy of updater.app,
      // which is itself a directory.
      try {
        f.remove(true);
      } catch (e) {
        LOG("cleanUpUpdatesDir - failed to remove file " + f.path);
      }
    }
  }
  if (AppConstants.platform == "gonk") {
    releaseSDCardMountLock();
  }
}

/**
 * Clean up updates list and the updates directory.
 */
function cleanupActiveUpdate() {
  // Move the update from the Active Update list into the Past Updates list.
  var um = Cc["@mozilla.org/updates/update-manager;1"].
           getService(Ci.nsIUpdateManager);
  um.activeUpdate = null;
  um.saveUpdates();

  // Now trash the updates directory, since we're done with it
  cleanUpUpdatesDir();
}

/**
 * Gets the locale from the update.locale file for replacing %LOCALE% in the
 * update url. The update.locale file can be located in the application
 * directory or the GRE directory with preference given to it being located in
 * the application directory.
 */
function getLocale() {
  if (gLocale)
    return gLocale;

  for (let res of ['app', 'gre']) {
    var channel = Services.io.newChannel2("resource://" + res + "/" + FILE_UPDATE_LOCALE,
                                          null,
                                          null,
                                          null,      // aLoadingNode
                                          Services.scriptSecurityManager.getSystemPrincipal(),
                                          null,      // aTriggeringPrincipal
                                          Ci.nsILoadInfo.SEC_NORMAL,
                                          Ci.nsIContentPolicy.TYPE_DATAREQUEST);
    try {
      var inputStream = channel.open();
      gLocale = readStringFromInputStream(inputStream);
    } catch(e) {}
    if (gLocale)
      break;
  }

  if (!gLocale)
    throw Components.Exception(FILE_UPDATE_LOCALE + " file doesn't exist in " +
                               "either the application or GRE directories",
                               Cr.NS_ERROR_FILE_NOT_FOUND);

  LOG("getLocale - getting locale from file: " + channel.originalURI.spec +
      ", locale: " + gLocale);
  return gLocale;
}

/* Get the distribution pref values, from defaults only */
function getDistributionPrefValue(aPrefName) {
  var prefValue = "default";

  try {
    prefValue = Services.prefs.getDefaultBranch(null).getCharPref(aPrefName);
  } catch (e) {
    // use default when pref not found
  }

  return prefValue;
}

/**
 * An enumeration of items in a JS array.
 * @constructor
 */
function ArrayEnumerator(aItems) {
  this._index = 0;
  if (aItems) {
    for (var i = 0; i < aItems.length; ++i) {
      if (!aItems[i])
        aItems.splice(i, 1);
    }
  }
  this._contents = aItems;
}

ArrayEnumerator.prototype = {
  _index: 0,
  _contents: [],

  hasMoreElements: function ArrayEnumerator_hasMoreElements() {
    return this._index < this._contents.length;
  },

  getNext: function ArrayEnumerator_getNext() {
    return this._contents[this._index++];
  }
};

/**
 * Writes a string of text to a file.  A newline will be appended to the data
 * written to the file.  This function only works with ASCII text.
 */
function writeStringToFile(file, text) {
  var fos = FileUtils.openSafeFileOutputStream(file)
  text += "\n";
  fos.write(text, text.length);
  FileUtils.closeSafeFileOutputStream(fos);
}

function readStringFromInputStream(inputStream) {
  var sis = Cc["@mozilla.org/scriptableinputstream;1"].
            createInstance(Ci.nsIScriptableInputStream);
  sis.init(inputStream);
  var text = sis.read(sis.available());
  sis.close();
  if (text && text[text.length - 1] == "\n") {
    text = text.slice(0, -1);
  }
  return text;
}

/**
 * Reads a string of text from a file.  A trailing newline will be removed
 * before the result is returned.  This function only works with ASCII text.
 */
function readStringFromFile(file) {
  if (!file.exists()) {
    LOG("readStringFromFile - file doesn't exist: " + file.path);
    return null;
  }
  var fis = Cc["@mozilla.org/network/file-input-stream;1"].
            createInstance(Ci.nsIFileInputStream);
  fis.init(file, FileUtils.MODE_RDONLY, FileUtils.PERMS_FILE, 0);
  return readStringFromInputStream(fis);
}

function handleUpdateFailure(update, errorCode) {
  update.errorCode = parseInt(errorCode);
  if (update.errorCode == FOTA_GENERAL_ERROR ||
      update.errorCode == FOTA_FILE_OPERATION_ERROR ||
      update.errorCode == FOTA_RECOVERY_ERROR ||
      update.errorCode == FOTA_UNKNOWN_ERROR) {
    // In the case of FOTA update errors, don't reset the state to pending. This
    // causes the FOTA update path to try again, which is not necessarily what
    // we want.
    update.statusText = gUpdateBundle.GetStringFromName("statusFailed");

    Cc["@mozilla.org/updates/update-prompt;1"].
      createInstance(Ci.nsIUpdatePrompt).
      showUpdateError(update);
    writeStatusFile(getUpdatesDir(), STATE_FAILED + ": " + errorCode);
    cleanupActiveUpdate();
    return true;
  }

  // Replace with Array.prototype.includes when it has stabilized.
  if (WRITE_ERRORS.indexOf(update.errorCode) != -1 ||
      update.errorCode == FILESYSTEM_MOUNT_READWRITE_ERROR) {
    Cc["@mozilla.org/updates/update-prompt;1"].
      createInstance(Ci.nsIUpdatePrompt).
      showUpdateError(update);
    writeStatusFile(getUpdatesDir(), update.state = STATE_PENDING);
    return true;
  }

  if (update.errorCode == ELEVATION_CANCELED) {
    writeStatusFile(getUpdatesDir(), update.state = STATE_PENDING);
    let cancelations = getPref("getIntPref", PREF_APP_UPDATE_CANCELATIONS, 0);
    cancelations++;
    Services.prefs.setIntPref(PREF_APP_UPDATE_CANCELATIONS, cancelations);
    return true;
  }
  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_SERVICE_ERRORS)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_SERVICE_ERRORS);
  }

  // Replace with Array.prototype.includes when it has stabilized.
  if (SERVICE_ERRORS.indexOf(update.errorCode) != -1) {
    var failCount = getPref("getIntPref",
                            PREF_APP_UPDATE_SERVICE_ERRORS, 0);
    var maxFail = getPref("getIntPref",
                          PREF_APP_UPDATE_SERVICE_MAX_ERRORS,
                          DEFAULT_SERVICE_MAX_ERRORS);

    // As a safety, when the service reaches maximum failures, it will
    // disable itself and fallback to using the normal update mechanism
    // without the service.
    if (failCount >= maxFail) {
      Services.prefs.setBoolPref(PREF_APP_UPDATE_SERVICE_ENABLED, false);
      Services.prefs.clearUserPref(PREF_APP_UPDATE_SERVICE_ERRORS);
    } else {
      failCount++;
      Services.prefs.setIntPref(PREF_APP_UPDATE_SERVICE_ERRORS,
                                failCount);
    }

    writeStatusFile(getUpdatesDir(), update.state = STATE_PENDING);
    return true;
  }
  return false;
}

/**
 * Fall back to downloading a complete update in case an update has failed.
 *
 * @param update the update object that has failed to apply.
 * @param postStaging true if we have just attempted to stage an update.
 */
function handleFallbackToCompleteUpdate(update, postStaging) {
  cleanupActiveUpdate();

  update.statusText = gUpdateBundle.GetStringFromName("patchApplyFailure");
  var oldType = update.selectedPatch ? update.selectedPatch.type
                                     : "complete";
  if (update.selectedPatch && oldType == "partial" && update.patchCount == 2) {
    // Partial patch application failed, try downloading the complete
    // update in the background instead.
    LOG("handleFallbackToCompleteUpdate - install of partial patch " +
        "failed, downloading complete patch");
    var status = Cc["@mozilla.org/updates/update-service;1"].
                 getService(Ci.nsIApplicationUpdateService).
                 downloadUpdate(update, !postStaging);
    if (status == STATE_NONE)
      cleanupActiveUpdate();
  }
  else {
    LOG("handleFallbackToCompleteUpdate - install of complete or " +
        "only one patch offered failed.");
  }
  update.QueryInterface(Ci.nsIWritablePropertyBag);
  update.setProperty("patchingFailed", oldType);
}

function pingStateAndStatusCodes(aUpdate, aStartup, aStatus) {
  let patchType = AUSTLMY.PATCH_UNKNOWN;
  if (aUpdate && aUpdate.selectedPatch && aUpdate.selectedPatch.type) {
    if (aUpdate.selectedPatch.type == "complete") {
      patchType = AUSTLMY.PATCH_COMPLETE;
    } else if (aUpdate.selectedPatch.type == "partial") {
      patchType = AUSTLMY.PATCH_PARTIAL;
    }
  }

  let suffix = patchType + "_" + (aStartup ? AUSTLMY.STARTUP : AUSTLMY.STAGE);
  let stateCode = 0;
  let parts = aStatus.split(":");
  if (parts.length > 0) {
    switch (parts[0]) {
      case STATE_NONE:
        stateCode = 2;
        break;
      case STATE_DOWNLOADING:
        stateCode = 3;
        break;
      case STATE_PENDING:
        stateCode = 4;
        break;
      case STATE_PENDING_SVC:
        stateCode = 5;
        break;
      case STATE_APPLYING:
        stateCode = 6;
        break;
      case STATE_APPLIED:
        stateCode = 7;
        break;
      case STATE_APPLIED_OS:
        stateCode = 8;
        break;
      case STATE_APPLIED_SVC:
        stateCode = 9;
        break;
      case STATE_SUCCEEDED:
        stateCode = 10;
        break;
      case STATE_DOWNLOAD_FAILED:
        stateCode = 11;
        break;
      case STATE_FAILED:
        stateCode = 12;
        break;
      default:
        stateCode = 1;
    }

    if (parts.length > 1) {
      let statusErrorCode = INVALID_UPDATER_STATE_CODE;
      if (parts[0] == STATE_FAILED) {
        statusErrorCode = parseInt(parts[1]) || INVALID_UPDATER_STATUS_CODE;
      }
      AUSTLMY.pingStatusErrorCode(suffix, statusErrorCode);
    }
  }
  AUSTLMY.pingStateCode(suffix, stateCode);
}

/**
 * Update Patch
 * @param   patch
 *          A <patch> element to initialize this object with
 * @throws if patch has a size of 0
 * @constructor
 */
function UpdatePatch(patch) {
  this._properties = {};
  for (var i = 0; i < patch.attributes.length; ++i) {
    var attr = patch.attributes.item(i);
    attr.QueryInterface(Ci.nsIDOMAttr);
    switch (attr.name) {
      case "selected":
        this.selected = attr.value == "true";
        break;
      case "size":
        if (0 == parseInt(attr.value)) {
          LOG("UpdatePatch:init - 0-sized patch!");
          throw Cr.NS_ERROR_ILLEGAL_VALUE;
        }
        // fall through
      default:
        this[attr.name] = attr.value;
        break;
    }
  }
}
UpdatePatch.prototype = {
  /**
   * See nsIUpdateService.idl
   */
  serialize: function UpdatePatch_serialize(updates) {
    var patch = updates.createElementNS(URI_UPDATE_NS, "patch");
    patch.setAttribute("type", this.type);
    patch.setAttribute("URL", this.URL);
    // finalURL is not available until after the download has started
    if (this.finalURL) {
      patch.setAttribute("finalURL", this.finalURL);
    }
    patch.setAttribute("hashFunction", this.hashFunction);
    patch.setAttribute("hashValue", this.hashValue);
    patch.setAttribute("size", this.size);
    if (this.selected) {
      patch.setAttribute("selected", this.selected);
    }
    patch.setAttribute("state", this.state);

    for (var p in this._properties) {
      if (this._properties[p].present) {
        patch.setAttribute(p, this._properties[p].data);
      }
    }

    return patch;
  },

  /**
   * A hash of custom properties
   */
  _properties: null,

  /**
   * See nsIWritablePropertyBag.idl
   */
  setProperty: function UpdatePatch_setProperty(name, value) {
    this._properties[name] = { data: value, present: true };
  },

  /**
   * See nsIWritablePropertyBag.idl
   */
  deleteProperty: function UpdatePatch_deleteProperty(name) {
    if (name in this._properties)
      this._properties[name].present = false;
    else
      throw Cr.NS_ERROR_FAILURE;
  },

  /**
   * See nsIPropertyBag.idl
   */
  get enumerator() {
    var properties = [];
    for (var p in this._properties)
      properties.push(this._properties[p].data);
    return new ArrayEnumerator(properties);
  },

  /**
   * See nsIPropertyBag.idl
   * Note: returns null instead of throwing when the property doesn't exist to
   *       simplify code and to silence warnings in debug builds.
   */
  getProperty: function UpdatePatch_getProperty(name) {
    if (name in this._properties &&
        this._properties[name].present) {
      return this._properties[name].data;
    }
    return null;
  },

  /**
   * Returns whether or not the update.status file for this patch exists at the
   * appropriate location.
   */
  get statusFileExists() {
    var statusFile = getUpdatesDir();
    statusFile.append(FILE_UPDATE_STATUS);
    return statusFile.exists();
  },

  /**
   * See nsIUpdateService.idl
   */
  get state() {
    if (this._properties.state)
      return this._properties.state;
    return STATE_NONE;
  },
  set state(val) {
    this._properties.state = val;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUpdatePatch,
                                         Ci.nsIPropertyBag,
                                         Ci.nsIWritablePropertyBag])
};

/**
 * Update
 * Implements nsIUpdate
 * @param   update
 *          An <update> element to initialize this object with
 * @throws if the update contains no patches
 * @constructor
 */
function Update(update) {
  this._properties = {};
  this._patches = [];
  this.isCompleteUpdate = false;
  this.isOSUpdate = false;
  this.showPrompt = false;
  this.showNeverForVersion = false;
  this.unsupported = false;
  this.channel = "default";
  this.promptWaitTime = getPref("getIntPref", PREF_APP_UPDATE_PROMPTWAITTIME, 43200);

  // Null <update>, assume this is a message container and do no
  // further initialization
  if (!update) {
    return;
  }

  const ELEMENT_NODE = Ci.nsIDOMNode.ELEMENT_NODE;
  for (var i = 0; i < update.childNodes.length; ++i) {
    var patchElement = update.childNodes.item(i);
    if (patchElement.nodeType != ELEMENT_NODE ||
        patchElement.localName != "patch") {
      continue;
    }

    patchElement.QueryInterface(Ci.nsIDOMElement);
    try {
      var patch = new UpdatePatch(patchElement);
    } catch (e) {
      continue;
    }
    this._patches.push(patch);
  }

  if (this._patches.length == 0 && !update.hasAttribute("unsupported")) {
    throw Cr.NS_ERROR_ILLEGAL_VALUE;
  }

  // Fallback to the behavior prior to bug 530872 if the update does not have an
  // appVersion attribute.
  if (!update.hasAttribute("appVersion")) {
    if (update.getAttribute("type") == "major") {
      if (update.hasAttribute("detailsURL")) {
        this.billboardURL = update.getAttribute("detailsURL");
        this.showPrompt = true;
        this.showNeverForVersion = true;
      }
    }
  }

  // Set the installDate value with the current time. If the update has an
  // installDate attribute this will be replaced with that value if it doesn't
  // equal 0.
  this.installDate = (new Date()).getTime();

  for (var i = 0; i < update.attributes.length; ++i) {
    var attr = update.attributes.item(i);
    attr.QueryInterface(Ci.nsIDOMAttr);
    if (attr.value == "undefined") {
      continue;
    } else if (attr.name == "detailsURL") {
      this._detailsURL = attr.value;
    } else if (attr.name == "extensionVersion") {
      // Prevent extensionVersion from replacing appVersion if appVersion is
      // present in the update xml.
      if (!this.appVersion) {
        this.appVersion = attr.value;
      }
    } else if (attr.name == "installDate" && attr.value) {
      let val = parseInt(attr.value);
      if (val) {
        this.installDate = val;
      }
    } else if (attr.name == "isCompleteUpdate") {
      this.isCompleteUpdate = attr.value == "true";
    } else if (attr.name == "isSecurityUpdate") {
      this.isSecurityUpdate = attr.value == "true";
    } else if (attr.name == "isOSUpdate") {
      this.isOSUpdate = attr.value == "true";
    } else if (attr.name == "showNeverForVersion") {
      this.showNeverForVersion = attr.value == "true";
    } else if (attr.name == "showPrompt") {
      this.showPrompt = attr.value == "true";
    } else if (attr.name == "promptWaitTime") {
      if(!isNaN(attr.value)) {
        this.promptWaitTime = parseInt(attr.value);
      }
    } else if (attr.name == "unsupported") {
      this.unsupported = attr.value == "true";
    } else if (attr.name == "version") {
      // Prevent version from replacing displayVersion if displayVersion is
      // present in the update xml.
      if (!this.displayVersion) {
        this.displayVersion = attr.value;
      }
    } else {
      this[attr.name] = attr.value;

      switch (attr.name) {
        case "appVersion":
        case "billboardURL":
        case "buildID":
        case "channel":
        case "displayVersion":
        case "licenseURL":
        case "name":
        case "platformVersion":
        case "previousAppVersion":
        case "serviceURL":
        case "statusText":
        case "type":
          break;
        default:
          // Save custom attributes when serializing to the local xml file but
          // don't use this method for the expected attributes which are already
          // handled in serialize.
          this.setProperty(attr.name, attr.value);
          break;
      }
    }
  }

  // The Update Name is either the string provided by the <update> element, or
  // the string: "<App Name> <Update App Version>"
  var name = "";
  if (update.hasAttribute("name")) {
    name = update.getAttribute("name");
  } else {
    var brandBundle = Services.strings.createBundle(URI_BRAND_PROPERTIES);
    var appName = brandBundle.GetStringFromName("brandShortName");
    name = gUpdateBundle.formatStringFromName("updateName",
                                              [appName, this.displayVersion], 2);
  }
  this.name = name;
}
Update.prototype = {
  /**
   * See nsIUpdateService.idl
   */
  get patchCount() {
    return this._patches.length;
  },

  /**
   * See nsIUpdateService.idl
   */
  getPatchAt: function Update_getPatchAt(index) {
    return this._patches[index];
  },

  /**
   * See nsIUpdateService.idl
   *
   * We use a copy of the state cached on this object in |_state| only when
   * there is no selected patch, i.e. in the case when we could not load
   * |.activeUpdate| from the update manager for some reason but still have
   * the update.status file to work with.
   */
  _state: "",
  set state(state) {
    if (this.selectedPatch)
      this.selectedPatch.state = state;
    this._state = state;
    return state;
  },
  get state() {
    if (this.selectedPatch)
      return this.selectedPatch.state;
    return this._state;
  },

  /**
   * See nsIUpdateService.idl
   */
  errorCode: 0,

  /**
   * See nsIUpdateService.idl
   */
  get selectedPatch() {
    for (var i = 0; i < this.patchCount; ++i) {
      if (this._patches[i].selected)
        return this._patches[i];
    }
    return null;
  },

  /**
   * See nsIUpdateService.idl
   */
  get detailsURL() {
    if (!this._detailsURL) {
      try {
        // Try using a default details URL supplied by the distribution
        // if the update XML does not supply one.
        return Services.urlFormatter.formatURLPref(PREF_APP_UPDATE_URL_DETAILS);
      }
      catch (e) {
      }
    }
    return this._detailsURL || "";
  },

  /**
   * See nsIUpdateService.idl
   */
  serialize: function Update_serialize(updates) {
    // If appVersion isn't defined just return null. This happens when a
    // temporary nsIUpdate is passed to the UI when the
    // app.update.showInstalledUI prefence is set to true.
    if (!this.appVersion) {
      return null;
    }
    var update = updates.createElementNS(URI_UPDATE_NS, "update");
    update.setAttribute("appVersion", this.appVersion);
    update.setAttribute("buildID", this.buildID);
    update.setAttribute("channel", this.channel);
    update.setAttribute("displayVersion", this.displayVersion);
    // for backwards compatibility in case the user downgrades
    update.setAttribute("extensionVersion", this.appVersion);
    update.setAttribute("installDate", this.installDate);
    update.setAttribute("isCompleteUpdate", this.isCompleteUpdate);
    update.setAttribute("isOSUpdate", this.isOSUpdate);
    update.setAttribute("name", this.name);
    update.setAttribute("serviceURL", this.serviceURL);
    update.setAttribute("showNeverForVersion", this.showNeverForVersion);
    update.setAttribute("showPrompt", this.showPrompt);
    update.setAttribute("promptWaitTime", this.promptWaitTime);
    update.setAttribute("type", this.type);
    // for backwards compatibility in case the user downgrades
    update.setAttribute("version", this.displayVersion);

    // Optional attributes
    if (this.billboardURL) {
      update.setAttribute("billboardURL", this.billboardURL);
    }
    if (this.detailsURL) {
      update.setAttribute("detailsURL", this.detailsURL);
    }
    if (this.licenseURL) {
      update.setAttribute("licenseURL", this.licenseURL);
    }
    if (this.platformVersion) {
      update.setAttribute("platformVersion", this.platformVersion);
    }
    if (this.previousAppVersion) {
      update.setAttribute("previousAppVersion", this.previousAppVersion);
    }
    if (this.statusText) {
      update.setAttribute("statusText", this.statusText);
    }
    if (this.unsupported) {
      update.setAttribute("unsupported", this.unsupported);
    }
    updates.documentElement.appendChild(update);

    for (var p in this._properties) {
      if (this._properties[p].present) {
        update.setAttribute(p, this._properties[p].data);
      }
    }

    for (let i = 0; i < this.patchCount; ++i) {
      update.appendChild(this.getPatchAt(i).serialize(updates));
    }

    return update;
  },

  /**
   * A hash of custom properties
   */
  _properties: null,

  /**
   * See nsIWritablePropertyBag.idl
   */
  setProperty: function Update_setProperty(name, value) {
    this._properties[name] = { data: value, present: true };
  },

  /**
   * See nsIWritablePropertyBag.idl
   */
  deleteProperty: function Update_deleteProperty(name) {
    if (name in this._properties)
      this._properties[name].present = false;
    else
      throw Cr.NS_ERROR_FAILURE;
  },

  /**
   * See nsIPropertyBag.idl
   */
  get enumerator() {
    var properties = [];
    for (var p in this._properties)
      properties.push(this._properties[p].data);
    return new ArrayEnumerator(properties);
  },

  /**
   * See nsIPropertyBag.idl
   * Note: returns null instead of throwing when the property doesn't exist to
   *       simplify code and to silence warnings in debug builds.
   */
  getProperty: function Update_getProperty(name) {
    if (name in this._properties && this._properties[name].present) {
      return this._properties[name].data;
    }
    return null;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUpdate,
                                         Ci.nsIPropertyBag,
                                         Ci.nsIWritablePropertyBag])
};

const UpdateServiceFactory = {
  _instance: null,
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return this._instance == null ? this._instance = new UpdateService() :
                                    this._instance;
  }
};

/**
 * UpdateService
 * A Service for managing the discovery and installation of software updates.
 * @constructor
 */
function UpdateService() {
  LOG("Creating UpdateService");
  Services.obs.addObserver(this, "xpcom-shutdown", false);
  Services.prefs.addObserver(PREF_APP_UPDATE_LOG, this, false);
  if (AppConstants.platform == "gonk") {
    // PowerManagerService::SyncProfile (which is called for Reboot, PowerOff
    // and Restart) sends the profile-change-net-teardown event. We can then
    // pause the download in a similar manner to xpcom-shutdown.
    Services.obs.addObserver(this, "profile-change-net-teardown", false);
  }
}

UpdateService.prototype = {
  /**
   * The downloader we are using to download updates. There is only ever one of
   * these.
   */
  _downloader: null,

  /**
   * Incompatible add-on count.
   */
  _incompatAddonsCount: 0,

  /**
   * Whether or not the service registered the "online" observer.
   */
  _registeredOnlineObserver: false,

  /**
   * The current number of consecutive socket errors
   */
  _consecutiveSocketErrors: 0,

  /**
   * A timer used to retry socket errors
   */
  _retryTimer: null,

  /**
   * Whether or not a background update check was initiated by the
   * application update timer notification.
   */
  _isNotify: true,

  /**
   * Handle Observer Service notifications
   * @param   subject
   *          The subject of the notification
   * @param   topic
   *          The notification name
   * @param   data
   *          Additional data
   */
  observe: function AUS_observe(subject, topic, data) {
    switch (topic) {
      case "post-update-processing":
        // Clean up any extant updates
        this._postUpdateProcessing();
        break;
      case "network:offline-status-changed":
        this._offlineStatusChanged(data);
        break;
      case "nsPref:changed":
        if (data == PREF_APP_UPDATE_LOG) {
          gLogEnabled = getPref("getBoolPref", PREF_APP_UPDATE_LOG, false);
        }
        break;
      case "profile-change-net-teardown": // fall thru
      case "xpcom-shutdown":
        Services.obs.removeObserver(this, topic);
        Services.prefs.removeObserver(PREF_APP_UPDATE_LOG, this);

        if (AppConstants.platform == "win" && gUpdateMutexHandle) {
          // If we hold the update mutex, let it go!
          // The OS would clean this up sometime after shutdown,
          // but that would have no guarantee on timing.
          closeHandle(gUpdateMutexHandle);
        }
        if (this._retryTimer) {
          this._retryTimer.cancel();
        }

        this.pauseDownload();
        // Prevent leaking the downloader (bug 454964)
        this._downloader = null;
        break;
    }
  },

  /**
   * The following needs to happen during the post-update-processing
   * notification from nsUpdateServiceStub.js:
   * 1. post update processing
   * 2. resume of a download that was in progress during a previous session
   * 3. start of a complete update download after the failure to apply a partial
   *    update
   */

  /**
   * Perform post-processing on updates lingering in the updates directory
   * from a previous application session - either report install failures (and
   * optionally attempt to fetch a different version if appropriate) or
   * notify the user of install success.
   */
  _postUpdateProcessing: function AUS__postUpdateProcessing() {
    if (!this.canCheckForUpdates) {
      LOG("UpdateService:_postUpdateProcessing - unable to check for " +
          "updates... returning early");
      return;
    }

    if (!this.canApplyUpdates) {
      LOG("UpdateService:_postUpdateProcessing - unable to apply " +
          "updates... returning early");
      // If the update is present in the update directly somehow,
      // it would prevent us from notifying the user of futher updates.
      cleanupActiveUpdate();
      return;
    }

    var um = Cc["@mozilla.org/updates/update-manager;1"].
             getService(Ci.nsIUpdateManager);
    var update = um.activeUpdate;
    var status = readStatusFile(getUpdatesDir());
    pingStateAndStatusCodes(update, true, status);
    // STATE_NONE status typically means that the update.status file is present
    // but a background download error occurred.
    if (status == STATE_NONE) {
      LOG("UpdateService:_postUpdateProcessing - no status, no update");
      cleanupActiveUpdate();
      return;
    }

    if (AppConstants.platform == "gonk") {
      // This code is called very early in the boot process, before we've even
      // had a chance to setup the UI so we can give feedback to the user.
      //
      // Since the download may be occuring over a link which has associated
      // cost, we want to require user-consent before resuming the download.
      // Also, applying an already downloaded update now is undesireable,
      // since the phone will look dead while the update is being applied.
      // Applying the update can take several minutes. Instead we wait until
      // the UI is initialized so it is possible to give feedback to and get
      // consent to update from the user.
      if (isInterruptedUpdate(status)) {
        LOG("UpdateService:_postUpdateProcessing - interrupted update detected - wait for user consent");
        return;
      }
    }

    if (status == STATE_DOWNLOADING) {
      LOG("UpdateService:_postUpdateProcessing - patch found in downloading " +
          "state");
      if (update && update.state != STATE_SUCCEEDED) {
        // Resume download
        var status = this.downloadUpdate(update, true);
        if (status == STATE_NONE)
          cleanupActiveUpdate();
      }
      return;
    }

    if (status == STATE_APPLYING) {
      // This indicates that the background updater service is in either of the
      // following two states:
      // 1. It is in the process of applying an update in the background, and
      //    we just happen to be racing against that.
      // 2. It has failed to apply an update for some reason, and we hit this
      //    case because the updater process has set the update status to
      //    applying, but has never finished.
      // In order to differentiate between these two states, we look at the
      // state field of the update object.  If it's "pending", then we know
      // that this is the first time we're hitting this case, so we switch
      // that state to "applying" and we just wait and hope for the best.
      // If it's "applying", we know that we've already been here once, so
      // we really want to start from a clean state.
      if (update &&
          (update.state == STATE_PENDING || update.state == STATE_PENDING_SVC)) {
        LOG("UpdateService:_postUpdateProcessing - patch found in applying " +
            "state for the first time");
        update.state = STATE_APPLYING;
        um.saveUpdates();
      } else { // We get here even if we don't have an update object
        LOG("UpdateService:_postUpdateProcessing - patch found in applying " +
            "state for the second time");
        cleanupActiveUpdate();
      }
      return;
    }

    if (AppConstants.platform == "gonk") {
      // The update is only applied but not selected to be installed
      if (status == STATE_APPLIED && update && update.isOSUpdate) {
        LOG("UpdateService:_postUpdateProcessing - update staged as applied found");
        return;
      }

      if (status == STATE_APPLIED_OS && update && update.isOSUpdate) {
        // In gonk, we need to check for OS update status after startup, since
        // the recovery partition won't write to update.status for us
        let recoveryService = Cc["@mozilla.org/recovery-service;1"].
                              getService(Ci.nsIRecoveryService);
        let fotaStatus = recoveryService.getFotaUpdateStatus();
        switch (fotaStatus) {
          case Ci.nsIRecoveryService.FOTA_UPDATE_SUCCESS:
            status = STATE_SUCCEEDED;
            break;
          case Ci.nsIRecoveryService.FOTA_UPDATE_FAIL:
            status = STATE_FAILED + ": " + FOTA_GENERAL_ERROR;
            break;
          case Ci.nsIRecoveryService.FOTA_UPDATE_UNKNOWN:
          default:
            status = STATE_FAILED + ": " + FOTA_UNKNOWN_ERROR;
            break;
        }
      }
    }

    if (!update) {
      if (status != STATE_SUCCEEDED) {
        LOG("UpdateService:_postUpdateProcessing - previous patch failed " +
            "and no patch available");
        cleanupActiveUpdate();
        return;
      }
      update = new Update(null);
    }

    var prompter = Cc["@mozilla.org/updates/update-prompt;1"].
                   createInstance(Ci.nsIUpdatePrompt);

    update.state = status;

    if (status == STATE_SUCCEEDED) {
      update.statusText = gUpdateBundle.GetStringFromName("installSuccess");

      // Update the patch's metadata.
      um.activeUpdate = update;
      Services.prefs.setBoolPref(PREF_APP_UPDATE_POSTUPDATE, true);
      prompter.showUpdateInstalled();

      // Done with this update. Clean it up.
      cleanupActiveUpdate();
    }
    else {
      // If we hit an error, then the error code will be included in the status
      // string following a colon and a space. If we had an I/O error, then we
      // assume that the patch is not invalid, and we re-stage the patch so that
      // it can be attempted again the next time we restart. This will leave a
      // space at the beginning of the error code when there is a failure which
      // will be removed by using parseInt below. This prevents panic which has
      // occurred numerous times previously (see bug 569642 comment #9 for one
      // example) when testing releases due to forgetting to include the space.
      var ary = status.split(":");
      update.state = ary[0];
      if (update.state == STATE_FAILED && ary[1]) {
        if (handleUpdateFailure(update, ary[1])) {
          return;
        }
      }

      // Something went wrong with the patch application process.
      handleFallbackToCompleteUpdate(update, false);

      prompter.showUpdateError(update);
    }

    // Now trash the MozUpdater directory created when replacing an install with
    // a staged update.
    cleanUpMozUpdaterDirs();
  },

  /**
   * Register an observer when the network comes online, so we can short-circuit
   * the app.update.interval when there isn't connectivity
   */
  _registerOnlineObserver: function AUS__registerOnlineObserver() {
    if (this._registeredOnlineObserver) {
      LOG("UpdateService:_registerOnlineObserver - observer already registered");
      return;
    }

    LOG("UpdateService:_registerOnlineObserver - waiting for the network to " +
        "be online, then forcing another check");

    Services.obs.addObserver(this, "network:offline-status-changed", false);
    this._registeredOnlineObserver = true;
  },

  /**
   * Called from the network:offline-status-changed observer.
   */
  _offlineStatusChanged: function AUS__offlineStatusChanged(status) {
    if (status !== "online") {
      return;
    }

    Services.obs.removeObserver(this, "network:offline-status-changed");
    this._registeredOnlineObserver = false;

    LOG("UpdateService:_offlineStatusChanged - network is online, forcing " +
        "another background check");

    // the background checker is contained in notify
    this._attemptResume();
  },

  onCheckComplete: function AUS_onCheckComplete(request, updates, updateCount) {
    this._selectAndInstallUpdate(updates);
  },

  onError: function AUS_onError(request, update) {
    LOG("UpdateService:onError - error during background update. error code: " +
        update.errorCode + ", status text: " + update.statusText);

    var maxErrors;
    var errCount;
    if (update.errorCode == NETWORK_ERROR_OFFLINE) {
      // Register an online observer to try again
      this._registerOnlineObserver();
      if (this._pingSuffix) {
        AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_OFFLINE);
      }
      return;
    }

    if (update.errorCode == CERT_ATTR_CHECK_FAILED_NO_UPDATE ||
        update.errorCode == CERT_ATTR_CHECK_FAILED_HAS_UPDATE) {
      errCount = getPref("getIntPref", PREF_APP_UPDATE_CERT_ERRORS, 0);
      errCount++;
      Services.prefs.setIntPref(PREF_APP_UPDATE_CERT_ERRORS, errCount);
      maxErrors = getPref("getIntPref", PREF_APP_UPDATE_CERT_MAXERRORS, 5);
    } else {
      update.errorCode = BACKGROUNDCHECK_MULTIPLE_FAILURES;
      errCount = getPref("getIntPref", PREF_APP_UPDATE_BACKGROUNDERRORS, 0);
      errCount++;
      Services.prefs.setIntPref(PREF_APP_UPDATE_BACKGROUNDERRORS, errCount);
      maxErrors = getPref("getIntPref", PREF_APP_UPDATE_BACKGROUNDMAXERRORS,
                          10);
    }

    let checkCode;
    if (errCount >= maxErrors) {
      var prompter = Cc["@mozilla.org/updates/update-prompt;1"].
                     createInstance(Ci.nsIUpdatePrompt);
      prompter.showUpdateError(update);

      switch (update.errorCode) {
        case CERT_ATTR_CHECK_FAILED_NO_UPDATE:
          checkCode = AUSTLMY.CHK_CERT_ATTR_NO_UPDATE_PROMPT;
          break;
        case CERT_ATTR_CHECK_FAILED_HAS_UPDATE:
          checkCode = AUSTLMY.CHK_CERT_ATTR_WITH_UPDATE_PROMPT;
          break;
        default:
          checkCode = AUSTLMY.CHK_GENERAL_ERROR_PROMPT;
          AUSTLMY.pingCheckExError(this._pingSuffix, update.errorCode);
      }
    } else {
      switch (update.errorCode) {
        case CERT_ATTR_CHECK_FAILED_NO_UPDATE:
          checkCode = AUSTLMY.CHK_CERT_ATTR_NO_UPDATE_SILENT;
          break;
        case CERT_ATTR_CHECK_FAILED_HAS_UPDATE:
          checkCode = AUSTLMY.CHK_CERT_ATTR_WITH_UPDATE_SILENT;
          break;
        default:
          checkCode = AUSTLMY.CHK_GENERAL_ERROR_SILENT;
          AUSTLMY.pingCheckExError(this._pingSuffix, update.errorCode);
      }
    }
    AUSTLMY.pingCheckCode(this._pingSuffix, checkCode);
  },

  /**
   * Called when a connection should be resumed
   */
  _attemptResume: function AUS_attemptResume() {
    LOG("UpdateService:_attemptResume")
    // If a download is in progress, then resume it.
    if (this._downloader && this._downloader._patch &&
        this._downloader._patch.state == STATE_DOWNLOADING &&
        this._downloader._update) {
      LOG("UpdateService:_attemptResume - _patch.state: " +
          this._downloader._patch.state);
      // Make sure downloading is the state for selectPatch to work correctly
      writeStatusFile(getUpdatesDir(), STATE_DOWNLOADING);
      var status = this.downloadUpdate(this._downloader._update,
                                       this._downloader.background);
      LOG("UpdateService:_attemptResume - downloadUpdate status: " + status);
      if (status == STATE_NONE) {
        cleanupActiveUpdate();
      }
      return;
    }

    this.backgroundChecker.checkForUpdates(this, false);
  },

  /**
   * Notified when a timer fires
   * @param   timer
   *          The timer that fired
   */
  notify: function AUS_notify(timer) {
    this._checkForBackgroundUpdates(true);
  },

  /**
   * See nsIUpdateService.idl
   */
  checkForBackgroundUpdates: function AUS_checkForBackgroundUpdates() {
    this._checkForBackgroundUpdates(false);
  },

  // The suffix used for background update check telemetry histogram ID's.
  get _pingSuffix() {
    return this._isNotify ? AUSTLMY.NOTIFY : AUSTLMY.EXTERNAL;
  },

  /**
   * Checks for updates in the background.
   * @param   isNotify
   *          Whether or not a background update check was initiated by the
   *          application update timer notification.
   */
  _checkForBackgroundUpdates: function AUS__checkForBackgroundUpdates(isNotify) {
    this._isNotify = isNotify;

    // Histogram IDs:
    // UPDATE_PING_COUNT_EXTERNAL
    // UPDATE_PING_COUNT_NOTIFY
    AUSTLMY.pingGeneric("UPDATE_PING_COUNT_" + this._pingSuffix,
                        true, false);

    // Histogram IDs:
    // UPDATE_UNABLE_TO_APPLY_EXTERNAL
    // UPDATE_UNABLE_TO_APPLY_NOTIFY
    AUSTLMY.pingGeneric("UPDATE_UNABLE_TO_APPLY_" + this._pingSuffix,
                        gCanApplyUpdates, true);
    // Histogram IDs:
    // UPDATE_CANNOT_STAGE_EXTERNAL
    // UPDATE_CANNOT_STAGE_NOTIFY
    AUSTLMY.pingGeneric("UPDATE_CANNOT_STAGE_" + this._pingSuffix,
                        getCanStageUpdates(), true);
    // Histogram IDs:
    // UPDATE_INVALID_LASTUPDATETIME_EXTERNAL
    // UPDATE_INVALID_LASTUPDATETIME_NOTIFY
    // UPDATE_LAST_NOTIFY_INTERVAL_DAYS_EXTERNAL
    // UPDATE_LAST_NOTIFY_INTERVAL_DAYS_NOTIFY
    AUSTLMY.pingLastUpdateTime(this._pingSuffix);
    // Histogram IDs:
    // UPDATE_NOT_PREF_UPDATE_ENABLED_EXTERNAL
    // UPDATE_NOT_PREF_UPDATE_ENABLED_NOTIFY
    AUSTLMY.pingBoolPref("UPDATE_NOT_PREF_UPDATE_ENABLED_" + this._pingSuffix,
                         PREF_APP_UPDATE_ENABLED, true, true);
    // Histogram IDs:
    // UPDATE_NOT_PREF_UPDATE_AUTO_EXTERNAL
    // UPDATE_NOT_PREF_UPDATE_AUTO_NOTIFY
    AUSTLMY.pingBoolPref("UPDATE_NOT_PREF_UPDATE_AUTO_" + this._pingSuffix,
                         PREF_APP_UPDATE_AUTO, true, true);
    // Histogram IDs:
    // UPDATE_NOT_PREF_UPDATE_STAGING_ENABLED_EXTERNAL
    // UPDATE_NOT_PREF_UPDATE_STAGING_ENABLED_NOTIFY
    AUSTLMY.pingBoolPref("UPDATE_NOT_PREF_UPDATE_STAGING_ENABLED_" +
                         this._pingSuffix,
                         PREF_APP_UPDATE_STAGING_ENABLED, true, true);
    if (AppConstants.platform == "win") {
      // Histogram IDs:
      // UPDATE_PREF_UPDATE_CANCELATIONS_EXTERNAL
      // UPDATE_PREF_UPDATE_CANCELATIONS_NOTIFY
      AUSTLMY.pingIntPref("UPDATE_PREF_UPDATE_CANCELATIONS_" + this._pingSuffix,
                          PREF_APP_UPDATE_CANCELATIONS, 0, 0);
    }
    if (AppConstants.MOZ_MAINTENANCE_SERVICE) {
      // Histogram IDs:
      // UPDATE_NOT_PREF_UPDATE_SERVICE_ENABLED_EXTERNAL
      // UPDATE_NOT_PREF_UPDATE_SERVICE_ENABLED_NOTIFY
      AUSTLMY.pingBoolPref("UPDATE_NOT_PREF_UPDATE_SERVICE_ENABLED_" +
                           this._pingSuffix,
                           PREF_APP_UPDATE_SERVICE_ENABLED, true);
      // Histogram IDs:
      // UPDATE_PREF_SERVICE_ERRORS_EXTERNAL
      // UPDATE_PREF_SERVICE_ERRORS_NOTIFY
      AUSTLMY.pingIntPref("UPDATE_PREF_SERVICE_ERRORS_" + this._pingSuffix,
                          PREF_APP_UPDATE_SERVICE_ERRORS, 0, 0);
      if (AppConstants.platform == "win") {
        // Histogram IDs:
        // UPDATE_SERVICE_INSTALLED_EXTERNAL
        // UPDATE_SERVICE_INSTALLED_NOTIFY
        // UPDATE_SERVICE_MANUALLY_UNINSTALLED_EXTERNAL
        // UPDATE_SERVICE_MANUALLY_UNINSTALLED_NOTIFY
        AUSTLMY.pingServiceInstallStatus(this._pingSuffix, isServiceInstalled());
      }
    }

    let prefType = Services.prefs.getPrefType(PREF_APP_UPDATE_URL_OVERRIDE);
    let overridePrefHasValue = prefType != Ci.nsIPrefBranch.PREF_INVALID;
    // Histogram IDs:
    // UPDATE_HAS_PREF_URL_OVERRIDE_EXTERNAL
    // UPDATE_HAS_PREF_URL_OVERRIDE_NOTIFY
    AUSTLMY.pingGeneric("UPDATE_HAS_PREF_URL_OVERRIDE_" + this._pingSuffix,
                        overridePrefHasValue, false);

    // If a download is in progress or the patch has been staged do nothing.
    if (this.isDownloading) {
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_IS_DOWNLOADING);
      return;
    }

    if (this._downloader && this._downloader.patchIsStaged) {
      let readState = readStatusFile(getUpdatesDir());
      if (readState == STATE_PENDING || readState == STATE_PENDING_SVC) {
        AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_IS_DOWNLOADED);
      } else {
        AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_IS_STAGED);
      }
      return;
    }

    let validUpdateURL = true;
    try {
      this.backgroundChecker.getUpdateURL(false);
    } catch (e) {
      validUpdateURL = false;
    }
    // The following checks are done here so they can be differentiated from
    // foreground checks.
    if (!gOSVersion) {
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_NO_OS_VERSION);
    } else if (!gABI) {
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_NO_OS_ABI);
    } else if (!validUpdateURL) {
      if (overridePrefHasValue) {
        if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_URL_OVERRIDE)) {
          AUSTLMY.pingCheckCode(this._pingSuffix,
                                AUSTLMY.CHK_INVALID_USER_OVERRIDE_URL);
        } else {
          AUSTLMY.pingCheckCode(this._pingSuffix,
                                AUSTLMY.CHK_INVALID_DEFAULT_OVERRIDE_URL);
        }
      } else {
        AUSTLMY.pingCheckCode(this._pingSuffix,
                              AUSTLMY.CHK_INVALID_DEFAULT_URL);
      }
    } else if (!getPref("getBoolPref", PREF_APP_UPDATE_ENABLED, true)) {
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_PREF_DISABLED);
    } else if (!hasUpdateMutex()) {
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_NO_MUTEX);
    } else if (!gCanCheckForUpdates) {
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_UNABLE_TO_CHECK);
    } else if (!this.backgroundChecker._enabled) {
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_DISABLED_FOR_SESSION);
    }

    this.backgroundChecker.checkForUpdates(this, false);
  },

  /**
   * Determine the update from the specified updates that should be offered.
   * If both valid major and minor updates are available the minor update will
   * be offered.
   * @param   updates
   *          An array of available nsIUpdate items
   * @return  The nsIUpdate to offer.
   */
  selectUpdate: function AUS_selectUpdate(updates) {
    if (updates.length == 0) {
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_NO_UPDATE_FOUND);
      return null;
    }

    // The ping for unsupported is sent after the call to showPrompt.
    if (updates.length == 1 && updates[0].unsupported) {
      return updates[0];
    }

    // Choose the newest of the available minor and major updates.
    var majorUpdate = null;
    var minorUpdate = null;
    var vc = Services.vc;
    let lastCheckCode = AUSTLMY.CHK_NO_COMPAT_UPDATE_FOUND;

    updates.forEach(function(aUpdate) {
      // Ignore updates for older versions of the application and updates for
      // the same version of the application with the same build ID.
      if (vc.compare(aUpdate.appVersion, Services.appinfo.version) < 0 ||
          vc.compare(aUpdate.appVersion, Services.appinfo.version) == 0 &&
          aUpdate.buildID == Services.appinfo.appBuildID) {
        LOG("UpdateService:selectUpdate - skipping update because the " +
            "update's application version is less than the current " +
            "application version");
        lastCheckCode = AUSTLMY.CHK_UPDATE_PREVIOUS_VERSION;
        return;
      }

      // Skip the update if the user responded with "never" to this update's
      // application version and the update specifies showNeverForVersion
      // (see bug 350636).
      let neverPrefName = PREF_APP_UPDATE_NEVER_BRANCH + aUpdate.appVersion;
      if (aUpdate.showNeverForVersion &&
          getPref("getBoolPref", neverPrefName, false)) {
        LOG("UpdateService:selectUpdate - skipping update because the " +
            "preference " + neverPrefName + " is true");
        lastCheckCode = AUSTLMY.CHK_UPDATE_NEVER_PREF;
        return;
      }

      switch (aUpdate.type) {
        case "major":
          if (!majorUpdate)
            majorUpdate = aUpdate;
          else if (vc.compare(majorUpdate.appVersion, aUpdate.appVersion) <= 0)
            majorUpdate = aUpdate;
          break;
        case "minor":
          if (!minorUpdate)
            minorUpdate = aUpdate;
          else if (vc.compare(minorUpdate.appVersion, aUpdate.appVersion) <= 0)
            minorUpdate = aUpdate;
          break;
        default:
          LOG("UpdateService:selectUpdate - skipping unknown update type: " +
              aUpdate.type);
          lastCheckCode = AUSTLMY.CHK_UPDATE_INVALID_TYPE;
          break;
      }
    });

    var update = minorUpdate || majorUpdate;
    if (!update) {
      AUSTLMY.pingCheckCode(this._pingSuffix, lastCheckCode);
    }

    return update;
  },

  /**
   * Reference to the currently selected update for when add-on compatibility
   * is checked.
   */
  _update: null,

  /**
   * Determine which of the specified updates should be installed and begin the
   * download/installation process or notify the user about the update.
   * @param   updates
   *          An array of available updates
   */
  _selectAndInstallUpdate: function AUS__selectAndInstallUpdate(updates) {
    // Return early if there's an active update. The user is already aware and
    // is downloading or performed some user action to prevent notification.
    var um = Cc["@mozilla.org/updates/update-manager;1"].
             getService(Ci.nsIUpdateManager);
    if (um.activeUpdate) {
      if (AppConstants.platform == "gonk") {
        // For gonk, the user isn't necessarily aware of the update, so we need
        // to show the prompt to make sure.
        this._showPrompt(um.activeUpdate);
      }
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_HAS_ACTIVEUPDATE);
      return;
    }

    var updateEnabled = getPref("getBoolPref", PREF_APP_UPDATE_ENABLED, true);
    if (!updateEnabled) {
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_PREF_DISABLED);
      LOG("UpdateService:_selectAndInstallUpdate - not prompting because " +
          "update is disabled");
      return;
    }

    var update = this.selectUpdate(updates, updates.length);
    if (!update) {
      return;
    }

    if (update.unsupported) {
      LOG("UpdateService:_selectAndInstallUpdate - update not supported for " +
          "this system");
      if (!getPref("getBoolPref", PREF_APP_UPDATE_NOTIFIEDUNSUPPORTED, false)) {
        LOG("UpdateService:_selectAndInstallUpdate - notifying that the " +
            "update is not supported for this system");
        this._showPrompt(update);
      }
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_UNSUPPORTED);
      return;
    }

    if (!gCanApplyUpdates) {
      LOG("UpdateService:_selectAndInstallUpdate - the user is unable to " +
          "apply updates... prompting");
      this._showPrompt(update);
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_UNABLE_TO_APPLY);
      return;
    }

    /**
     * From this point on there are two possible outcomes:
     * 1. download and install the update automatically
     * 2. notify the user about the availability of an update
     *
     * Notes:
     * a) if the app.update.auto preference is false then automatic download and
     *    install is disabled and the user will be notified.
     * b) if the update has a showPrompt attribute the user will be notified.
     * c) Mode is determined by the value of the app.update.mode preference.
     *
     * If the update when it is first read has an appVersion attribute the
     * following behavior implemented in bug 530872 will occur:
     * Mode   Incompatible Add-ons   Outcome
     * 0      N/A                    Auto Install
     * 1      Yes                    Notify
     * 1      No                     Auto Install
     *
     * If the update when it is first read does not have an appVersion attribute
     * the following deprecated behavior will occur:
     * Update Type   Mode   Incompatible Add-ons   Outcome
     * Major         all    N/A                    Notify
     * Minor         0      N/A                    Auto Install
     * Minor         1      Yes                    Notify
     * Minor         1      No                     Auto Install
     */
    if (update.showPrompt) {
      LOG("UpdateService:_selectAndInstallUpdate - prompting because the " +
          "update snippet specified showPrompt");
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_SHOWPROMPT_SNIPPET);
      this._showPrompt(update);
      return;
    }

    if (!getPref("getBoolPref", PREF_APP_UPDATE_AUTO, true)) {
      LOG("UpdateService:_selectAndInstallUpdate - prompting because silent " +
          "install is disabled");
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_SHOWPROMPT_PREF);
      this._showPrompt(update);
      return;
    }

    if (getPref("getIntPref", PREF_APP_UPDATE_MODE, 1) == 0) {
      // Do not prompt regardless of add-on incompatibilities
      LOG("UpdateService:_selectAndInstallUpdate - add-on compatibility " +
          "check disabled by preference, just download the update");
      var status = this.downloadUpdate(update, true);
      if (status == STATE_NONE)
        cleanupActiveUpdate();
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_ADDON_PREF_DISABLED);
      return;
    }

    // Only check add-on compatibility when the version changes.
    if (update.appVersion &&
        Services.vc.compare(update.appVersion, Services.appinfo.version) != 0) {
      this._update = update;
      this._checkAddonCompatibility();
    }
    else {
      LOG("UpdateService:_selectAndInstallUpdate - add-on compatibility " +
          "check not performed due to the update version being the same as " +
          "the current application version, just download the update");
      let status = this.downloadUpdate(update, true);
      if (status == STATE_NONE) {
        cleanupActiveUpdate();
      }
      AUSTLMY.pingCheckCode(this._pingSuffix,AUSTLMY.CHK_ADDON_SAME_APP_VER);
    }
  },

  _showPrompt: function AUS__showPrompt(update) {
    var prompter = Cc["@mozilla.org/updates/update-prompt;1"].
                   createInstance(Ci.nsIUpdatePrompt);
    prompter.showUpdateAvailable(update);
  },

  _checkAddonCompatibility: function AUS__checkAddonCompatibility() {
    try {
      var hotfixID = Services.prefs.getCharPref(PREF_EM_HOTFIX_ID);
    }
    catch (e) { }

    // Get all the installed add-ons
    var self = this;
    AddonManager.getAllAddons(function(addons) {
      self._incompatibleAddons = [];
      addons.forEach(function(addon) {
        // Protect against code that overrides the add-ons manager and doesn't
        // implement the isCompatibleWith or the findUpdates method.
        if (!("isCompatibleWith" in addon) || !("findUpdates" in addon)) {
          let errMsg = "Add-on doesn't implement either the isCompatibleWith " +
                       "or the findUpdates method!";
          if (addon.id) {
            errMsg += " Add-on ID: " + addon.id;
          }
          Cu.reportError(errMsg);
          return;
        }

        // If an add-on isn't appDisabled and isn't userDisabled then it is
        // either active now or the user expects it to be active after the
        // restart. If that is the case and the add-on is not installed by the
        // application and is not compatible with the new application version
        // then the user should be warned that the add-on will become
        // incompatible. If an addon's type equals plugin it is skipped since
        // checking plugins compatibility information isn't supported and
        // getting the scope property of a plugin breaks in some environments
        // (see bug 566787). The hotfix add-on is also ignored as it shouldn't
        // block the user from upgrading.
        try {
          if (addon.type != "plugin" && addon.id != hotfixID &&
              !addon.appDisabled && !addon.userDisabled &&
              addon.scope != AddonManager.SCOPE_APPLICATION &&
              addon.isCompatible &&
              !addon.isCompatibleWith(self._update.appVersion,
                                      self._update.platformVersion)) {
            self._incompatibleAddons.push(addon);
          }
        } catch (e) {
          Cu.reportError(e);
        }
      });

      if (self._incompatibleAddons.length > 0) {
      /**
       * PREF_APP_UPDATE_INCOMPATIBLE_MODE
       * Controls the mode in which we check for updates as follows.
       *
       *   PREF_APP_UPDATE_INCOMPATIBLE_MODE != 1
       *   We check for VersionInfo _and_ NewerVersion updates for the
       *   incompatible add-ons - i.e. if Foo 1.2 is installed and it is
       *   incompatible with the update, and we find Foo 2.0 which is but has
       *   not been installed, then we do NOT prompt because the user can
       *   download Foo 2.0 when they restart after the update during the add-on
       *   mismatch checking UI. This is the default, since it suppresses most
       *   prompt dialogs.
       *
       *   PREF_APP_UPDATE_INCOMPATIBLE_MODE == 1
       *   We check for VersionInfo updates for the incompatible add-ons - i.e.
       *   if the situation above with Foo 1.2 and available update to 2.0
       *   applies, we DO show the prompt since a download operation will be
       *   required after the update. This is not the default and is supplied
       *   only as a hidden option for those that want it.
       */
        self._updateCheckCount = self._incompatibleAddons.length;
        LOG("UpdateService:_checkAddonCompatibility - checking for " +
            "incompatible add-ons");

        self._incompatibleAddons.forEach(function(addon) {
          addon.findUpdates(this, AddonManager.UPDATE_WHEN_NEW_APP_DETECTED,
                            this._update.appVersion, this._update.platformVersion);
        }, self);
      }
      else {
        LOG("UpdateService:_checkAddonCompatibility - no incompatible " +
            "add-ons found, just download the update");
        var status = self.downloadUpdate(self._update, true);
        if (status == STATE_NONE)
          cleanupActiveUpdate();
        self._update = null;
        AUSTLMY.pingCheckCode(self._pingSuffix, AUSTLMY.CHK_ADDON_NO_INCOMPAT);
      }
    });
  },

  // AddonUpdateListener
  onCompatibilityUpdateAvailable: function(addon) {
    // Remove the add-on from the list of add-ons that will become incompatible
    // with the new version of the application.
    for (var i = 0; i < this._incompatibleAddons.length; ++i) {
      if (this._incompatibleAddons[i].id == addon.id) {
        LOG("UpdateService:onCompatibilityUpdateAvailable - found update for " +
            "add-on ID: " + addon.id);
        this._incompatibleAddons.splice(i, 1);
      }
    }
  },

  onUpdateAvailable: function(addon, install) {
    if (getPref("getIntPref", PREF_APP_UPDATE_INCOMPATIBLE_MODE, 0) == 1) {
      return;
    }

    // If the new version of this add-on is blocklisted for the new application
    // then it isn't a valid update and the user should still be warned that
    // the add-on will become incompatible.
    if (Services.blocklist.isAddonBlocklisted(addon, this._update.appVersion,
                                              this._update.platformVersion)) {
      return;
    }

    // Compatibility or new version updates mean the same thing here.
    this.onCompatibilityUpdateAvailable(addon);
  },

  onUpdateFinished: function(addon) {
    if (--this._updateCheckCount > 0) {
      return;
    }

    if (this._incompatibleAddons.length > 0 || !gCanApplyUpdates) {
      LOG("UpdateService:onUpdateEnded - prompting because there are " +
          "incompatible add-ons");
      if (this._incompatibleAddons.length > 0) {
        AUSTLMY.pingCheckCode(this._pingSuffix,
                              AUSTLMY.CHK_ADDON_HAVE_INCOMPAT);
      } else {
        AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_UNABLE_TO_APPLY);
      }
      this._showPrompt(this._update);
    } else {
      LOG("UpdateService:_selectAndInstallUpdate - updates for all " +
          "incompatible add-ons found, just download the update");
      var status = this.downloadUpdate(this._update, true);
      if (status == STATE_NONE)
        cleanupActiveUpdate();
      AUSTLMY.pingCheckCode(this._pingSuffix,
                            AUSTLMY.CHK_ADDON_UPDATES_FOR_INCOMPAT);
    }
    this._update = null;
  },

  /**
   * The Checker used for background update checks.
   */
  _backgroundChecker: null,

  /**
   * See nsIUpdateService.idl
   */
  get backgroundChecker() {
    if (!this._backgroundChecker)
      this._backgroundChecker = new Checker();
    return this._backgroundChecker;
  },

  /**
   * See nsIUpdateService.idl
   */
  get canCheckForUpdates() {
    return gCanCheckForUpdates && hasUpdateMutex();
  },

  /**
   * See nsIUpdateService.idl
   */
  get canApplyUpdates() {
    return gCanApplyUpdates && hasUpdateMutex();
  },

  /**
   * See nsIUpdateService.idl
   */
  get canStageUpdates() {
    return getCanStageUpdates();
  },

  /**
   * See nsIUpdateService.idl
   */
  get isOtherInstanceHandlingUpdates() {
    return !hasUpdateMutex();
  },


  /**
   * See nsIUpdateService.idl
   */
  addDownloadListener: function AUS_addDownloadListener(listener) {
    if (!this._downloader) {
      LOG("UpdateService:addDownloadListener - no downloader!");
      return;
    }
    this._downloader.addDownloadListener(listener);
  },

  /**
   * See nsIUpdateService.idl
   */
  removeDownloadListener: function AUS_removeDownloadListener(listener) {
    if (!this._downloader) {
      LOG("UpdateService:removeDownloadListener - no downloader!");
      return;
    }
    this._downloader.removeDownloadListener(listener);
  },

  /**
   * See nsIUpdateService.idl
   */
  downloadUpdate: function AUS_downloadUpdate(update, background) {
    if (!update)
      throw Cr.NS_ERROR_NULL_POINTER;

    // Don't download the update if the update's version is less than the
    // current application's version or the update's version is the same as the
    // application's version and the build ID is the same as the application's
    // build ID.
    if (update.appVersion &&
        (Services.vc.compare(update.appVersion, Services.appinfo.version) < 0 ||
         update.buildID && update.buildID == Services.appinfo.appBuildID &&
         update.appVersion == Services.appinfo.version)) {
      LOG("UpdateService:downloadUpdate - canceling download of update since " +
          "it is for an earlier or same application version and build ID.\n" +
          "current application version: " + Services.appinfo.version + "\n" +
          "update application version : " + update.appVersion + "\n" +
          "current build ID: " + Services.appinfo.appBuildID + "\n" +
          "update build ID : " + update.buildID);
      cleanupActiveUpdate();
      return STATE_NONE;
    }

    // If a download request is in progress vs. a download ready to resume
    if (this.isDownloading) {
      if (update.isCompleteUpdate == this._downloader.isCompleteUpdate &&
          background == this._downloader.background) {
        LOG("UpdateService:downloadUpdate - no support for downloading more " +
            "than one update at a time");
        return readStatusFile(getUpdatesDir());
      }
      this._downloader.cancel();
    }
    if (AppConstants.platform == "gonk") {
      let um = Cc["@mozilla.org/updates/update-manager;1"].
               getService(Ci.nsIUpdateManager);
      let activeUpdate = um.activeUpdate;
      if (activeUpdate &&
          (activeUpdate.appVersion != update.appVersion ||
           activeUpdate.buildID != update.buildID)) {
        // We have an activeUpdate (which presumably was interrupted), and are
        // about start downloading a new one. Make sure we remove all traces
        // of the active one (otherwise we'll start appending the new update.mar
        // the the one that's been partially downloaded).
        LOG("UpdateService:downloadUpdate - removing stale active update.");
        cleanupActiveUpdate();
      }
    }
    // Set the previous application version prior to downloading the update.
    update.previousAppVersion = Services.appinfo.version;
    this._downloader = new Downloader(background, this);
    return this._downloader.downloadUpdate(update);
  },

  /**
   * See nsIUpdateService.idl
   */
  pauseDownload: function AUS_pauseDownload() {
    if (this.isDownloading) {
      this._downloader.cancel();
    } else if (this._retryTimer) {
      // Download status is still consider as 'downloading' during retry.
      // We need to cancel both retry and download at this stage.
      this._retryTimer.cancel();
      this._retryTimer = null;
      this._downloader.cancel();
    }
  },

  /**
   * See nsIUpdateService.idl
   */
  getUpdatesDirectory: getUpdatesDir,

  /**
   * See nsIUpdateService.idl
   */
  get isDownloading() {
    return this._downloader && this._downloader.isBusy;
  },

  /**
   * See nsIUpdateService.idl
   */
  applyOsUpdate: function AUS_applyOsUpdate(aUpdate) {
    if (!aUpdate.isOSUpdate || aUpdate.state != STATE_APPLIED) {
      aUpdate.statusText = "fota-state-error";
      throw Cr.NS_ERROR_FAILURE;
    }

    aUpdate.QueryInterface(Ci.nsIWritablePropertyBag);
    let osApplyToDir = aUpdate.getProperty("osApplyToDir");

    if (!osApplyToDir) {
      LOG("UpdateService:applyOsUpdate - Error: osApplyToDir is not defined" +
          "in the nsIUpdate!");
      pingStateAndStatusCodes(aUpdate, false,
                              STATE_FAILED + ": " + FOTA_FILE_OPERATION_ERROR);
      handleUpdateFailure(aUpdate, FOTA_FILE_OPERATION_ERROR);
      return;
    }

    let updateFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    updateFile.initWithPath(osApplyToDir + "/update.zip");
    if (!updateFile.exists()) {
      LOG("UpdateService:applyOsUpdate - Error: OS update is not found at " +
          updateFile.path);
      pingStateAndStatusCodes(aUpdate, false,
                              STATE_FAILED + ": " + FOTA_FILE_OPERATION_ERROR);
      handleUpdateFailure(aUpdate, FOTA_FILE_OPERATION_ERROR);
      return;
    }

    writeStatusFile(getUpdatesDir(), aUpdate.state = STATE_APPLIED_OS);
    LOG("UpdateService:applyOsUpdate - Rebooting into recovery to apply " +
        "FOTA update: " + updateFile.path);
    try {
      let recoveryService = Cc["@mozilla.org/recovery-service;1"]
                            .getService(Ci.nsIRecoveryService);
      recoveryService.installFotaUpdate(updateFile.path);
    } catch (e) {
      LOG("UpdateService:applyOsUpdate - Error: Couldn't reboot into recovery" +
          " to apply FOTA update " + updateFile.path);
      pingStateAndStatusCodes(aUpdate, false,
                              STATE_FAILED + ": " + FOTA_RECOVERY_ERROR);
      writeStatusFile(getUpdatesDir(), aUpdate.state = STATE_APPLIED);
      handleUpdateFailure(aUpdate, FOTA_RECOVERY_ERROR);
    }
  },

  classID: UPDATESERVICE_CID,
  classInfo: XPCOMUtils.generateCI({classID: UPDATESERVICE_CID,
                                    contractID: UPDATESERVICE_CONTRACTID,
                                    interfaces: [Ci.nsIApplicationUpdateService,
                                                 Ci.nsITimerCallback,
                                                 Ci.nsIObserver],
                                    flags: Ci.nsIClassInfo.SINGLETON}),

  _xpcom_factory: UpdateServiceFactory,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIApplicationUpdateService,
                                         Ci.nsIUpdateCheckListener,
                                         Ci.nsITimerCallback,
                                         Ci.nsIObserver])
};

/**
 * A service to manage active and past updates.
 * @constructor
 */
function UpdateManager() {
  // Ensure the Active Update file is loaded
  var updates = this._loadXMLFileIntoArray(getUpdateFile(
                  [FILE_UPDATE_ACTIVE]));
  if (updates.length > 0) {
    // Under some edgecases such as Windows system restore the active-update.xml
    // will contain a pending update without the status file which will return
    // STATE_NONE. To recover from this situation clean the updates dir and
    // rewrite the active-update.xml file without the broken update.
    if (readStatusFile(getUpdatesDir()) == STATE_NONE) {
      cleanUpUpdatesDir();
      this._writeUpdatesToXMLFile([], getUpdateFile([FILE_UPDATE_ACTIVE]));
    }
    else
      this._activeUpdate = updates[0];
  }
}
UpdateManager.prototype = {
  /**
   * All previously downloaded and installed updates, as an array of nsIUpdate
   * objects.
   */
  _updates: null,

  /**
   * The current actively downloading/installing update, as a nsIUpdate object.
   */
  _activeUpdate: null,

  /**
   * Handle Observer Service notifications
   * @param   subject
   *          The subject of the notification
   * @param   topic
   *          The notification name
   * @param   data
   *          Additional data
   */
  observe: function UM_observe(subject, topic, data) {
    // Hack to be able to run and cleanup tests by reloading the update data.
    if (topic == "um-reload-update-data") {
      this._updates = this._loadXMLFileIntoArray(getUpdateFile(
                        [FILE_UPDATES_DB]));
      this._activeUpdate = null;
      var updates = this._loadXMLFileIntoArray(getUpdateFile(
                      [FILE_UPDATE_ACTIVE]));
      if (updates.length > 0)
        this._activeUpdate = updates[0];
    }
  },

  /**
   * Loads an updates.xml formatted file into an array of nsIUpdate items.
   * @param   file
   *          A nsIFile for the updates.xml file
   * @return  The array of nsIUpdate items held in the file.
   */
  _loadXMLFileIntoArray: function UM__loadXMLFileIntoArray(file) {
    if (!file.exists()) {
      LOG("UpdateManager:_loadXMLFileIntoArray: XML file does not exist");
      return [];
    }

    var result = [];
    var fileStream = Cc["@mozilla.org/network/file-input-stream;1"].
                     createInstance(Ci.nsIFileInputStream);
    fileStream.init(file, FileUtils.MODE_RDONLY, FileUtils.PERMS_FILE, 0);
    try {
      var parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                   createInstance(Ci.nsIDOMParser);
      var doc = parser.parseFromStream(fileStream, "UTF-8",
                                       fileStream.available(), "text/xml");

      const ELEMENT_NODE = Ci.nsIDOMNode.ELEMENT_NODE;
      var updateCount = doc.documentElement.childNodes.length;
      for (var i = 0; i < updateCount; ++i) {
        var updateElement = doc.documentElement.childNodes.item(i);
        if (updateElement.nodeType != ELEMENT_NODE ||
            updateElement.localName != "update")
          continue;

        updateElement.QueryInterface(Ci.nsIDOMElement);
        try {
          var update = new Update(updateElement);
        } catch (e) {
          LOG("UpdateManager:_loadXMLFileIntoArray - invalid update");
          continue;
        }
        result.push(update);
      }
    }
    catch (e) {
      LOG("UpdateManager:_loadXMLFileIntoArray - error constructing update " +
          "list. Exception: " + e);
    }
    fileStream.close();
    return result;
  },

  /**
   * Load the update manager, initializing state from state files.
   */
  _ensureUpdates: function UM__ensureUpdates() {
    if (!this._updates) {
      this._updates = this._loadXMLFileIntoArray(getUpdateFile(
                        [FILE_UPDATES_DB]));
      var activeUpdates = this._loadXMLFileIntoArray(getUpdateFile(
                            [FILE_UPDATE_ACTIVE]));
      if (activeUpdates.length > 0)
        this._activeUpdate = activeUpdates[0];
    }
  },

  /**
   * See nsIUpdateService.idl
   */
  getUpdateAt: function UM_getUpdateAt(index) {
    this._ensureUpdates();
    return this._updates[index];
  },

  /**
   * See nsIUpdateService.idl
   */
  get updateCount() {
    this._ensureUpdates();
    return this._updates.length;
  },

  /**
   * See nsIUpdateService.idl
   */
  get activeUpdate() {
    if (this._activeUpdate &&
        this._activeUpdate.channel != UpdateChannel.get()) {
      LOG("UpdateManager:get activeUpdate - channel has changed, " +
          "reloading default preferences to workaround bug 802022");
      // Workaround to get distribution preferences loaded (Bug 774618). This
      // can be removed after bug 802022 is fixed.
      let prefSvc = Services.prefs.QueryInterface(Ci.nsIObserver);
      prefSvc.observe(null, "reload-default-prefs", null);
      if (this._activeUpdate.channel != UpdateChannel.get()) {
        // User switched channels, clear out any old active updates and remove
        // partial downloads
        this._activeUpdate = null;
        this.saveUpdates();

        // Destroy the updates directory, since we're done with it.
        cleanUpUpdatesDir();
      }
    }
    return this._activeUpdate;
  },
  set activeUpdate(activeUpdate) {
    this._addUpdate(activeUpdate);
    this._activeUpdate = activeUpdate;
    if (!activeUpdate) {
      // If |activeUpdate| is null, we have updated both lists - the active list
      // and the history list, so we want to write both files.
      this.saveUpdates();
    }
    else
      this._writeUpdatesToXMLFile([this._activeUpdate],
                                  getUpdateFile([FILE_UPDATE_ACTIVE]));
    return activeUpdate;
  },

  /**
   * Add an update to the Updates list. If the item already exists in the list,
   * replace the existing value with the new value.
   * @param   update
   *          The nsIUpdate object to add.
   */
  _addUpdate: function UM__addUpdate(update) {
    if (!update)
      return;
    this._ensureUpdates();
    if (this._updates) {
      for (var i = 0; i < this._updates.length; ++i) {
        if (this._updates[i] &&
            this._updates[i].appVersion == update.appVersion &&
            this._updates[i].buildID == update.buildID) {
          // Replace the existing entry with the new value, updating
          // all metadata.
          this._updates[i] = update;
          return;
        }
      }
    }
    // Otherwise add it to the front of the list.
    this._updates.unshift(update);
  },

  /**
   * Serializes an array of updates to an XML file
   * @param   updates
   *          An array of nsIUpdate objects
   * @param   file
   *          The nsIFile object to serialize to
   */
  _writeUpdatesToXMLFile: function UM__writeUpdatesToXMLFile(updates, file) {
    var fos = Cc["@mozilla.org/network/safe-file-output-stream;1"].
              createInstance(Ci.nsIFileOutputStream);
    var modeFlags = FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE |
                    FileUtils.MODE_TRUNCATE;
    if (!file.exists()) {
      file.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
    }
    fos.init(file, modeFlags, FileUtils.PERMS_FILE, 0);

    try {
      var parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                   createInstance(Ci.nsIDOMParser);
      const EMPTY_UPDATES_DOCUMENT = "<?xml version=\"1.0\"?><updates xmlns=\"http://www.mozilla.org/2005/app-update\"></updates>";
      var doc = parser.parseFromString(EMPTY_UPDATES_DOCUMENT, "text/xml");

      for (var i = 0; i < updates.length; ++i) {
        // If appVersion isn't defined don't add the update. This happens when a
        // temporary nsIUpdate is passed to the UI when the
        // app.update.showInstalledUI prefence is set to true.
        if (updates[i] && updates[i].appVersion) {
          doc.documentElement.appendChild(updates[i].serialize(doc));
        }
      }

      var serializer = Cc["@mozilla.org/xmlextras/xmlserializer;1"].
                       createInstance(Ci.nsIDOMSerializer);
      serializer.serializeToStream(doc.documentElement, fos, null);
    } catch (e) {
    }

    FileUtils.closeSafeFileOutputStream(fos);
  },

  /**
   * See nsIUpdateService.idl
   */
  saveUpdates: function UM_saveUpdates() {
    this._writeUpdatesToXMLFile([this._activeUpdate],
                                getUpdateFile([FILE_UPDATE_ACTIVE]));
    if (this._activeUpdate)
      this._addUpdate(this._activeUpdate);

    this._ensureUpdates();
    // Don't write updates that have a temporary state to the updates.xml file.
    if (this._updates) {
      let updates = this._updates.slice();
      for (let i = updates.length - 1; i >= 0; --i) {
        let state = updates[i].state;
        if (state == STATE_NONE || state == STATE_DOWNLOADING ||
            state == STATE_APPLIED || state == STATE_APPLIED_SVC ||
            state == STATE_PENDING || state == STATE_PENDING_SVC) {
          updates.splice(i, 1);
        }
      }

      this._writeUpdatesToXMLFile(updates.slice(0, 10),
                                  getUpdateFile([FILE_UPDATES_DB]));
    }
  },

  /**
   * See nsIUpdateService.idl
   */
  refreshUpdateStatus: function UM_refreshUpdateStatus() {
    var update = this._activeUpdate;
    if (!update) {
      return;
    }
    var updateSucceeded = true;
    var status = readStatusFile(getUpdatesDir());
    pingStateAndStatusCodes(update, false, status);
    var parts = status.split(":");
    update.state = parts[0];

    if (update.state == STATE_FAILED && parts[1]) {
      updateSucceeded = false;
      if (!handleUpdateFailure(update, parts[1])) {
        handleFallbackToCompleteUpdate(update, true);
      }
    }
    if (update.state == STATE_APPLIED && shouldUseService()) {
      writeStatusFile(getUpdatesDir(), update.state = STATE_APPLIED_SVC);
    }
    var um = Cc["@mozilla.org/updates/update-manager;1"].
             getService(Ci.nsIUpdateManager);
    um.saveUpdates();

    if (update.state != STATE_PENDING && update.state != STATE_PENDING_SVC) {
      // Destroy the updates directory, since we're done with it.
      // Make sure to not do this when the updater has fallen back to
      // non-staged updates.
      cleanUpUpdatesDir(updateSucceeded);
    }

    // Send an observer notification which the update wizard uses in
    // order to update its UI.
    LOG("UpdateManager:refreshUpdateStatus - Notifying observers that " +
        "the update was staged. state: " + update.state + ", status: " + status);
    Services.obs.notifyObservers(null, "update-staged", update.state);

    // Do this after *everything* else, since it will likely cause the app
    // to shut down.
    if (AppConstants.platform == "gonk") {
      if (update.state == STATE_APPLIED) {
        // Notify the user that an update has been staged and is ready for
        // installation (i.e. that they should restart the application). We do
        // not notify on failed update attempts.
        let prompter = Cc["@mozilla.org/updates/update-prompt;1"].
                       createInstance(Ci.nsIUpdatePrompt);
        prompter.showUpdateDownloaded(update, true);
      } else {
        releaseSDCardMountLock();
      }
      return;
    }
    // Only prompt when the UI isn't already open.
    let windowType = getPref("getCharPref", PREF_APP_UPDATE_ALTWINDOWTYPE, null);
    if (Services.wm.getMostRecentWindow(UPDATE_WINDOW_NAME) ||
        windowType && Services.wm.getMostRecentWindow(windowType)) {
      return;
    }

    if (update.state == STATE_APPLIED || update.state == STATE_APPLIED_SVC ||
        update.state == STATE_PENDING || update.state == STATE_PENDING_SVC) {
        // Notify the user that an update has been staged and is ready for
        // installation (i.e. that they should restart the application).
      let prompter = Cc["@mozilla.org/updates/update-prompt;1"].
                     createInstance(Ci.nsIUpdatePrompt);
      prompter.showUpdateDownloaded(update, true);
    }
  },

  classID: Components.ID("{093C2356-4843-4C65-8709-D7DBCBBE7DFB}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUpdateManager, Ci.nsIObserver])
};

/**
 * Checker
 * Checks for new Updates
 * @constructor
 */
function Checker() {
}
Checker.prototype = {
  /**
   * The XMLHttpRequest object that performs the connection.
   */
  _request  : null,

  /**
   * The nsIUpdateCheckListener callback
   */
  _callback : null,

  /**
   * The URL of the update service XML file to connect to that contains details
   * about available updates.
   */
  getUpdateURL: function UC_getUpdateURL(force) {
    this._forced = force;

    // Use the override URL if specified.
    let url = getPref("getCharPref", PREF_APP_UPDATE_URL_OVERRIDE, null);

    // Otherwise, construct the update URL from component parts.
    if (!url) {
      try {
        url = Services.prefs.getDefaultBranch(null).
              getCharPref(PREF_APP_UPDATE_URL);
      } catch (e) {
      }
    }

    if (!url || url == "") {
      LOG("Checker:getUpdateURL - update URL not defined");
      return null;
    }

    url = url.replace(/%PRODUCT%/g, Services.appinfo.name);
    url = url.replace(/%VERSION%/g, Services.appinfo.version);
    url = url.replace(/%BUILD_ID%/g, Services.appinfo.appBuildID);
    url = url.replace(/%BUILD_TARGET%/g, Services.appinfo.OS + "_" + gABI);
    url = url.replace(/%OS_VERSION%/g, gOSVersion);
    if (/%LOCALE%/.test(url)) {
      url = url.replace(/%LOCALE%/g, getLocale());
    }
    url = url.replace(/%CHANNEL%/g, UpdateChannel.get());
    url = url.replace(/%PLATFORM_VERSION%/g, Services.appinfo.platformVersion);
    url = url.replace(/%DISTRIBUTION%/g,
                      getDistributionPrefValue(PREF_APP_DISTRIBUTION));
    url = url.replace(/%DISTRIBUTION_VERSION%/g,
                      getDistributionPrefValue(PREF_APP_DISTRIBUTION_VERSION));
    url = url.replace(/%CUSTOM%/g, getPref("getCharPref", PREF_APP_UPDATE_CUSTOM, ""));
    url = url.replace(/\+/g, "%2B");

    if (AppConstants.platform == "gonk") {
      let sysLibs = {};
      Cu.import("resource://gre/modules/systemlibs.js", sysLibs);
      url = url.replace(/%PRODUCT_MODEL%/g,
                        sysLibs.libcutils.property_get("ro.product.model"));
      url = url.replace(/%PRODUCT_DEVICE%/g,
                        sysLibs.libcutils.property_get("ro.product.device"));
      url = url.replace(/%B2G_VERSION%/g,
                        getPref("getCharPref", PREF_APP_B2G_VERSION, null));
    }

    if (force) {
      url += (url.indexOf("?") != -1 ? "&" : "?") + "force=1";
    }

    LOG("Checker:getUpdateURL - update URL: " + url);
    return url;
  },

  /**
   * See nsIUpdateService.idl
   */
  checkForUpdates: function UC_checkForUpdates(listener, force) {
    LOG("Checker: checkForUpdates, force: " + force);
    if (!listener)
      throw Cr.NS_ERROR_NULL_POINTER;

    Services.obs.notifyObservers(null, "update-check-start", null);

    var url = this.getUpdateURL(force);
    if (!url || (!this.enabled && !force))
      return;

    this._request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                    createInstance(Ci.nsISupports);
    // This is here to let unit test code override XHR
    if (this._request.wrappedJSObject) {
      this._request = this._request.wrappedJSObject;
    }
    this._request.open("GET", url, true);
    var allowNonBuiltIn = !getPref("getBoolPref",
                                   PREF_APP_UPDATE_CERT_REQUIREBUILTIN, true);
    this._request.channel.notificationCallbacks = new gCertUtils.BadCertHandler(allowNonBuiltIn);
    // Prevent the request from reading from the cache.
    this._request.channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
    // Prevent the request from writing to the cache.
    this._request.channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;

    this._request.overrideMimeType("text/xml");
    // The Cache-Control header is only interpreted by proxies and the
    // final destination. It does not help if a resource is already
    // cached locally.
    this._request.setRequestHeader("Cache-Control", "no-cache");
    // HTTP/1.0 servers might not implement Cache-Control and
    // might only implement Pragma: no-cache
    this._request.setRequestHeader("Pragma", "no-cache");

    var self = this;
    this._request.addEventListener("error", function(event) { self.onError(event); } ,false);
    this._request.addEventListener("load", function(event) { self.onLoad(event); }, false);

    LOG("Checker:checkForUpdates - sending request to: " + url);
    this._request.send(null);

    this._callback = listener;
  },

  /**
   * Returns an array of nsIUpdate objects discovered by the update check.
   * @throws if the XML document element node name is not updates.
   */
  get _updates() {
    var updatesElement = this._request.responseXML.documentElement;
    if (!updatesElement) {
      LOG("Checker:_updates get - empty updates document?!");
      return [];
    }

    if (updatesElement.nodeName != "updates") {
      LOG("Checker:_updates get - unexpected node name!");
      throw new Error("Unexpected node name, expected: updates, got: " +
                      updatesElement.nodeName);
    }

    const ELEMENT_NODE = Ci.nsIDOMNode.ELEMENT_NODE;
    var updates = [];
    for (var i = 0; i < updatesElement.childNodes.length; ++i) {
      var updateElement = updatesElement.childNodes.item(i);
      if (updateElement.nodeType != ELEMENT_NODE ||
          updateElement.localName != "update")
        continue;

      updateElement.QueryInterface(Ci.nsIDOMElement);
      try {
        var update = new Update(updateElement);
      } catch (e) {
        LOG("Checker:_updates get - invalid <update/>, ignoring...");
        continue;
      }
      update.serviceURL = this.getUpdateURL(this._forced);
      update.channel = UpdateChannel.get();
      updates.push(update);
    }

    return updates;
  },

  /**
   * Returns the status code for the XMLHttpRequest
   */
  _getChannelStatus: function UC__getChannelStatus(request) {
    var status = 0;
    try {
      status = request.status;
    }
    catch (e) {
    }

    if (status == 0)
      status = request.channel.QueryInterface(Ci.nsIRequest).status;
    return status;
  },

  _isHttpStatusCode: function UC__isHttpStatusCode(status) {
    return status >= 100 && status <= 599;
  },

  /**
   * The XMLHttpRequest succeeded and the document was loaded.
   * @param   event
   *          The nsIDOMEvent for the load
   */
  onLoad: function UC_onLoad(event) {
    LOG("Checker:onLoad - request completed downloading document");

    var prefs = Services.prefs;
    var certs = null;
    if (!prefs.prefHasUserValue(PREF_APP_UPDATE_URL_OVERRIDE) &&
        getPref("getBoolPref", PREF_APP_UPDATE_CERT_CHECKATTRS, true)) {
      certs = gCertUtils.readCertPrefs(PREF_APP_UPDATE_CERTS_BRANCH);
    }

    try {
      // Analyze the resulting DOM and determine the set of updates.
      var updates = this._updates;
      LOG("Checker:onLoad - number of updates available: " + updates.length);
      var allowNonBuiltIn = !getPref("getBoolPref",
                                     PREF_APP_UPDATE_CERT_REQUIREBUILTIN, true);
      gCertUtils.checkCert(this._request.channel, allowNonBuiltIn, certs);

      if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_CERT_ERRORS))
        Services.prefs.clearUserPref(PREF_APP_UPDATE_CERT_ERRORS);

      if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_BACKGROUNDERRORS))
        Services.prefs.clearUserPref(PREF_APP_UPDATE_BACKGROUNDERRORS);

      // Tell the callback about the updates
      this._callback.onCheckComplete(event.target, updates, updates.length);
    }
    catch (e) {
      LOG("Checker:onLoad - there was a problem checking for updates. " +
          "Exception: " + e);
      var request = event.target;
      var status = this._getChannelStatus(request);
      LOG("Checker:onLoad - request.status: " + status);
      var update = new Update(null);
      update.errorCode = status;
      update.statusText = getStatusTextFromCode(status, 404);

      if (this._isHttpStatusCode(status)) {
        update.errorCode = HTTP_ERROR_OFFSET + status;
      }
      if (e.result && e.result == Cr.NS_ERROR_ILLEGAL_VALUE) {
        update.errorCode = updates[0] ? CERT_ATTR_CHECK_FAILED_HAS_UPDATE
                                      : CERT_ATTR_CHECK_FAILED_NO_UPDATE;
      }

      this._callback.onError(request, update);
    }

    this._callback = null;
    this._request = null;
  },

  /**
   * There was an error of some kind during the XMLHttpRequest
   * @param   event
   *          The nsIDOMEvent for the error
   */
  onError: function UC_onError(event) {
    var request = event.target;
    var status = this._getChannelStatus(request);
    LOG("Checker:onError - request.status: " + status);

    // If we can't find an error string specific to this status code,
    // just use the 200 message from above, which means everything
    // "looks" fine but there was probably an XML error or a bogus file.
    var update = new Update(null);
    update.errorCode = status;
    update.statusText = getStatusTextFromCode(status, 200);

    if (status == Cr.NS_ERROR_OFFLINE) {
      // We use a separate constant here because nsIUpdate.errorCode is signed
      update.errorCode = NETWORK_ERROR_OFFLINE;
    } else if (this._isHttpStatusCode(status)) {
      update.errorCode = HTTP_ERROR_OFFSET + status;
    }

    this._callback.onError(request, update);
    this._request = null;
  },

  /**
   * Whether or not we are allowed to do update checking.
   */
  _enabled: true,
  get enabled() {
    return getPref("getBoolPref", PREF_APP_UPDATE_ENABLED, true) &&
           gCanCheckForUpdates && hasUpdateMutex() && this._enabled;
  },

  /**
   * See nsIUpdateService.idl
   */
  stopChecking: function UC_stopChecking(duration) {
    // Always stop the current check
    if (this._request)
      this._request.abort();

    switch (duration) {
      case Ci.nsIUpdateChecker.CURRENT_SESSION:
        this._enabled = false;
        break;
      case Ci.nsIUpdateChecker.ANY_CHECKS:
        this._enabled = false;
        Services.prefs.setBoolPref(PREF_APP_UPDATE_ENABLED, this._enabled);
        break;
    }

    this._callback = null;
  },

  classID: Components.ID("{898CDC9B-E43F-422F-9CC4-2F6291B415A3}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUpdateChecker])
};

/**
 * Manages the download of updates
 * @param   background
 *          Whether or not this downloader is operating in background
 *          update mode.
 * @param   updateService
 *          The update service that created this downloader.
 * @constructor
 */
function Downloader(background, updateService) {
  LOG("Creating Downloader");
  this.background = background;
  this.updateService = updateService;
}
Downloader.prototype = {
  /**
   * The nsIUpdatePatch that we are downloading
   */
  _patch: null,

  /**
   * The nsIUpdate that we are downloading
   */
  _update: null,

  /**
   * The nsIIncrementalDownload object handling the download
   */
  _request: null,

  /**
   * Whether or not the update being downloaded is a complete replacement of
   * the user's existing installation or a patch representing the difference
   * between the new version and the previous version.
   */
  isCompleteUpdate: null,

  /**
   * Cancels the active download.
   */
  cancel: function Downloader_cancel(cancelError) {
    LOG("Downloader: cancel");
    if (cancelError === undefined) {
      cancelError = Cr.NS_BINDING_ABORTED;
    }
    if (this._request && this._request instanceof Ci.nsIRequest) {
      this._request.cancel(cancelError);
    }
    if (AppConstants.platform == "gonk") {
      releaseSDCardMountLock();
    }
  },

  /**
   * Whether or not a patch has been downloaded and staged for installation.
   */
  get patchIsStaged() {
    var readState = readStatusFile(getUpdatesDir());
    // Note that if we decide to download and apply new updates after another
    // update has been successfully applied in the background, we need to stop
    // checking for the APPLIED state here.
    return readState == STATE_PENDING || readState == STATE_PENDING_SVC ||
           readState == STATE_APPLIED || readState == STATE_APPLIED_SVC;
  },

  /**
   * Verify the downloaded file.  We assume that the download is complete at
   * this point.
   */
  _verifyDownload: function Downloader__verifyDownload() {
    LOG("Downloader:_verifyDownload called");
    if (!this._request) {
      AUSTLMY.pingDownloadCode(this.isCompleteUpdate,
                               AUSTLMY.DWNLD_ERR_VERIFY_NO_REQUEST);
      return false;
    }

    let destination = this._request.destination;

    // Ensure that the file size matches the expected file size.
    if (destination.fileSize != this._patch.size) {
      LOG("Downloader:_verifyDownload downloaded size != expected size.");
      AUSTLMY.pingDownloadCode(this.isCompleteUpdate,
                               AUSTLMY.DWNLD_ERR_VERIFY_PATCH_SIZE_NOT_EQUAL);
      return false;
    }

    LOG("Downloader:_verifyDownload downloaded size == expected size.");
    let fileStream = Cc["@mozilla.org/network/file-input-stream;1"].
                     createInstance(Ci.nsIFileInputStream);
    fileStream.init(destination, FileUtils.MODE_RDONLY, FileUtils.PERMS_FILE, 0);

    let digest;
    try {
      let hash = Cc["@mozilla.org/security/hash;1"].
                 createInstance(Ci.nsICryptoHash);
      var hashFunction = Ci.nsICryptoHash[this._patch.hashFunction.toUpperCase()];
      if (hashFunction == undefined) {
        throw Cr.NS_ERROR_UNEXPECTED;
      }
      hash.init(hashFunction);
      hash.updateFromStream(fileStream, -1);
      // NOTE: For now, we assume that the format of _patch.hashValue is hex
      // encoded binary (such as what is typically output by programs like
      // sha1sum).  In the future, this may change to base64 depending on how
      // we choose to compute these hashes.
      digest = binaryToHex(hash.finish(false));
    } catch (e) {
      LOG("Downloader:_verifyDownload - failed to compute hash of the " +
          "downloaded update archive");
      digest = "";
    }

    fileStream.close();

    if (digest == this._patch.hashValue.toLowerCase()) {
      LOG("Downloader:_verifyDownload hashes match.");
      return true;
    }

    LOG("Downloader:_verifyDownload hashes do not match. ");
    AUSTLMY.pingDownloadCode(this.isCompleteUpdate,
                             AUSTLMY.DWNLD_ERR_VERIFY_NO_HASH_MATCH);
    return false;
  },

  /**
   * Select the patch to use given the current state of updateDir and the given
   * set of update patches.
   * @param   update
   *          A nsIUpdate object to select a patch from
   * @param   updateDir
   *          A nsIFile representing the update directory
   * @return  A nsIUpdatePatch object to download
   */
  _selectPatch: function Downloader__selectPatch(update, updateDir) {
    // Given an update to download, we will always try to download the patch
    // for a partial update over the patch for a full update.

    /**
     * Return the first UpdatePatch with the given type.
     * @param   type
     *          The type of the patch ("complete" or "partial")
     * @return  A nsIUpdatePatch object matching the type specified
     */
    function getPatchOfType(type) {
      for (var i = 0; i < update.patchCount; ++i) {
        var patch = update.getPatchAt(i);
        if (patch && patch.type == type)
          return patch;
      }
      return null;
    }

    // Look to see if any of the patches in the Update object has been
    // pre-selected for download, otherwise we must figure out which one
    // to select ourselves.
    var selectedPatch = update.selectedPatch;

    var state = readStatusFile(updateDir);

    // If this is a patch that we know about, then select it.  If it is a patch
    // that we do not know about, then remove it and use our default logic.
    var useComplete = false;
    if (selectedPatch) {
      LOG("Downloader:_selectPatch - found existing patch with state: " +
          state);
      if (state == STATE_DOWNLOADING) {
        LOG("Downloader:_selectPatch - resuming download");
        return selectedPatch;
      }

      if (AppConstants.platform == "gonk") {
        if (state == STATE_PENDING || state == STATE_APPLYING) {
          LOG("Downloader:_selectPatch - resuming interrupted apply");
          return selectedPatch;
        }
        if (state == STATE_APPLIED) {
          LOG("Downloader:_selectPatch - already downloaded and staged");
          return null;
        }
      } else if (state == STATE_PENDING || state == STATE_PENDING_SVC) {
        LOG("Downloader:_selectPatch - already downloaded and staged");
        return null;
      }

      if (update && selectedPatch.type == "complete") {
        // This is a pretty fatal error.  Just bail.
        LOG("Downloader:_selectPatch - failed to apply complete patch!");
        writeStatusFile(updateDir, STATE_NONE);
        writeVersionFile(getUpdatesDir(), null);
        return null;
      }

      // Something went wrong when we tried to apply the previous patch.
      // Try the complete patch next time.
      useComplete = true;
      selectedPatch = null;
    }

    // If we were not able to discover an update from a previous download, we
    // select the best patch from the given set.
    var partialPatch = getPatchOfType("partial");
    if (!useComplete)
      selectedPatch = partialPatch;
    if (!selectedPatch) {
      if (partialPatch)
        partialPatch.selected = false;
      selectedPatch = getPatchOfType("complete");
    }

    // Restore the updateDir since we may have deleted it.
    updateDir = getUpdatesDir();

    // if update only contains a partial patch, selectedPatch == null here if
    // the partial patch has been attempted and fails and we're trying to get a
    // complete patch
    if (selectedPatch)
      selectedPatch.selected = true;

    update.isCompleteUpdate = useComplete;

    // Reset the Active Update object on the Update Manager and flush the
    // Active Update DB.
    var um = Cc["@mozilla.org/updates/update-manager;1"].
             getService(Ci.nsIUpdateManager);
    um.activeUpdate = update;

    return selectedPatch;
  },

  /**
   * Whether or not we are currently downloading something.
   */
  get isBusy() {
    return this._request != null;
  },

  /**
   * Get the nsIFile to use for downloading the active update's selected patch
   */
  _getUpdateArchiveFile: function Downloader__getUpdateArchiveFile() {
    var updateArchive;
    if (AppConstants.platform == "gonk") {
      try {
        updateArchive = FileUtils.getDir(KEY_UPDATE_ARCHIVE_DIR, [], true);
      } catch (e) {
        return null;
      }
    } else {
      updateArchive = getUpdatesDir().clone();
    }

    updateArchive.append(FILE_UPDATE_ARCHIVE);
    return updateArchive;
  },

  /**
   * Download and stage the given update.
   * @param   update
   *          A nsIUpdate object to download a patch for. Cannot be null.
   */
  downloadUpdate: function Downloader_downloadUpdate(update) {
    LOG("UpdateService:_downloadUpdate");
    if (!update) {
      AUSTLMY.pingDownloadCode(undefined, AUSTLMY.DWNLD_ERR_NO_UPDATE);
      throw Cr.NS_ERROR_NULL_POINTER;
    }

    var updateDir = getUpdatesDir();

    this._update = update;

    // This function may return null, which indicates that there are no patches
    // to download.
    this._patch = this._selectPatch(update, updateDir);
    if (!this._patch) {
      LOG("Downloader:downloadUpdate - no patch to download");
      AUSTLMY.pingDownloadCode(undefined, AUSTLMY.DWNLD_ERR_NO_UPDATE_PATCH);
      return readStatusFile(updateDir);
    }
    this.isCompleteUpdate = this._patch.type == "complete";

    let patchFile = null;

    // Only used by gonk
    let status = STATE_NONE;
    if (AppConstants.platform == "gonk") {
      status = readStatusFile(updateDir);
      if (isInterruptedUpdate(status)) {
        LOG("Downloader:downloadUpdate - interruptted update");
        // The update was interrupted. Try to locate the existing patch file.
        // For an interrupted download, this allows a resume rather than a
        // re-download.
        patchFile = getFileFromUpdateLink(updateDir);
        if (!patchFile) {
          // No link file. We'll just assume that the update.mar is in the
          // update directory.
          patchFile = updateDir.clone();
          patchFile.append(FILE_UPDATE_ARCHIVE);
        }
        if (patchFile.exists()) {
          LOG("Downloader:downloadUpdate - resuming with patchFile " + patchFile.path);
          if (patchFile.fileSize == this._patch.size) {
            LOG("Downloader:downloadUpdate - patchFile appears to be fully downloaded");
            // Bump the status along so that we don't try to redownload again.
            status = STATE_PENDING;
          }
        } else {
          LOG("Downloader:downloadUpdate - patchFile " + patchFile.path +
              " doesn't exist - performing full download");
          // The patchfile doesn't exist, we might as well treat this like
          // a new download.
          patchFile = null;
        }
        if (patchFile && status != STATE_DOWNLOADING) {
          // It looks like the patch was downloaded, but got interrupted while it
          // was being verified or applied. So we'll fake the downloading portion.

          writeStatusFile(updateDir, STATE_PENDING);

          // Since the code expects the onStopRequest callback to happen
          // asynchronously (And you have to call AUS_addDownloadListener
          // after calling AUS_downloadUpdate) we need to defer this.

          this._downloadTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
          this._downloadTimer.initWithCallback(function() {
            this._downloadTimer = null;
            // Send a fake onStopRequest. Filling in the destination allows
            // _verifyDownload to work, and then the update will be applied.
            this._request = {destination: patchFile};
            this.onStopRequest(this._request, null, Cr.NS_OK);
          }.bind(this), 0, Ci.nsITimer.TYPE_ONE_SHOT);

          // Returning STATE_DOWNLOADING makes UpdatePrompt think we're
          // downloading. The onStopRequest that we spoofed above will make it
          // look like the download finished.
          return STATE_DOWNLOADING;
        }
      }
    }

    if (!patchFile) {
      // Find a place to put the patchfile that we're going to download.
      patchFile = this._getUpdateArchiveFile();
    }
    if (!patchFile) {
      AUSTLMY.pingDownloadCode(this.isCompleteUpdate,
                               AUSTLMY.DWNLD_ERR_NO_PATCH_FILE);
      return STATE_NONE;
    }

    if (AppConstants.platform == "gonk") {
      if (patchFile.path.indexOf(updateDir.path) != 0) {
        // The patchFile is in a directory which is different from the
        // updateDir, create a link file.
        writeLinkFile(updateDir, patchFile);

        if (!isInterruptedUpdate(status) && patchFile.exists()) {
          // Remove stale patchFile
          patchFile.remove(false);
        }
      }
    }

    var uri = Services.io.newURI(this._patch.URL, null, null);

    this._request = Cc["@mozilla.org/network/incremental-download;1"].
                    createInstance(Ci.nsIIncrementalDownload);

    LOG("Downloader:downloadUpdate - downloading from " + uri.spec + " to " +
        patchFile.path);
    var interval = this.background ? getPref("getIntPref",
                                             PREF_APP_UPDATE_BACKGROUND_INTERVAL,
                                             DOWNLOAD_BACKGROUND_INTERVAL)
                                   : DOWNLOAD_FOREGROUND_INTERVAL;
    this._request.init(uri, patchFile, DOWNLOAD_CHUNK_SIZE, interval);
    this._request.start(this, null);

    writeStatusFile(updateDir, STATE_DOWNLOADING);
    this._patch.QueryInterface(Ci.nsIWritablePropertyBag);
    this._patch.state = STATE_DOWNLOADING;
    var um = Cc["@mozilla.org/updates/update-manager;1"].
             getService(Ci.nsIUpdateManager);
    um.saveUpdates();
    return STATE_DOWNLOADING;
  },

  /**
   * An array of download listeners to notify when we receive
   * nsIRequestObserver or nsIProgressEventSink method calls.
   */
  _listeners: [],

  /**
   * Adds a listener to the download process
   * @param   listener
   *          A download listener, implementing nsIRequestObserver and
   *          nsIProgressEventSink
   */
  addDownloadListener: function Downloader_addDownloadListener(listener) {
    for (var i = 0; i < this._listeners.length; ++i) {
      if (this._listeners[i] == listener)
        return;
    }
    this._listeners.push(listener);
  },

  /**
   * Removes a download listener
   * @param   listener
   *          The listener to remove.
   */
  removeDownloadListener: function Downloader_removeDownloadListener(listener) {
    for (var i = 0; i < this._listeners.length; ++i) {
      if (this._listeners[i] == listener) {
        this._listeners.splice(i, 1);
        return;
      }
    }
  },

  /**
   * When the async request begins
   * @param   request
   *          The nsIRequest object for the transfer
   * @param   context
   *          Additional data
   */
  onStartRequest: function Downloader_onStartRequest(request, context) {
    if (request instanceof Ci.nsIIncrementalDownload)
      LOG("Downloader:onStartRequest - original URI spec: " + request.URI.spec +
          ", final URI spec: " + request.finalURI.spec);
    // Always set finalURL in onStartRequest since it can change.
    this._patch.finalURL = request.finalURI.spec;
    var um = Cc["@mozilla.org/updates/update-manager;1"].
             getService(Ci.nsIUpdateManager);
    um.saveUpdates();

    var listeners = this._listeners.concat();
    var listenerCount = listeners.length;
    for (var i = 0; i < listenerCount; ++i)
      listeners[i].onStartRequest(request, context);
  },

  /**
   * When new data has been downloaded
   * @param   request
   *          The nsIRequest object for the transfer
   * @param   context
   *          Additional data
   * @param   progress
   *          The current number of bytes transferred
   * @param   maxProgress
   *          The total number of bytes that must be transferred
   */
  onProgress: function Downloader_onProgress(request, context, progress,
                                             maxProgress) {
    LOG("Downloader:onProgress - progress: " + progress + "/" + maxProgress);

    if (progress > this._patch.size) {
      LOG("Downloader:onProgress - progress: " + progress +
          " is higher than patch size: " + this._patch.size);
      // It's important that we use a different code than
      // NS_ERROR_CORRUPTED_CONTENT so that tests can verify the difference
      // between a hash error and a wrong download error.
      AUSTLMY.pingDownloadCode(this.isCompleteUpdate,
                               AUSTLMY.DWNLD_ERR_PATCH_SIZE_LARGER);
      this.cancel(Cr.NS_ERROR_UNEXPECTED);
      return;
    }

    if (maxProgress != this._patch.size) {
      LOG("Downloader:onProgress - maxProgress: " + maxProgress +
          " is not equal to expectd patch size: " + this._patch.size);
      // It's important that we use a different code than
      // NS_ERROR_CORRUPTED_CONTENT so that tests can verify the difference
      // between a hash error and a wrong download error.
      AUSTLMY.pingDownloadCode(this.isCompleteUpdate,
                               AUSTLMY.DWNLD_ERR_PATCH_SIZE_NOT_EQUAL);
      this.cancel(Cr.NS_ERROR_UNEXPECTED);
      return;
    }

    var listeners = this._listeners.concat();
    var listenerCount = listeners.length;
    for (var i = 0; i < listenerCount; ++i) {
      var listener = listeners[i];
      if (listener instanceof Ci.nsIProgressEventSink)
        listener.onProgress(request, context, progress, maxProgress);
    }
    this.updateService._consecutiveSocketErrors = 0;
  },

  /**
   * When we have new status text
   * @param   request
   *          The nsIRequest object for the transfer
   * @param   context
   *          Additional data
   * @param   status
   *          A status code
   * @param   statusText
   *          Human readable version of |status|
   */
  onStatus: function Downloader_onStatus(request, context, status, statusText) {
    LOG("Downloader:onStatus - status: " + status + ", statusText: " +
        statusText);

    var listeners = this._listeners.concat();
    var listenerCount = listeners.length;
    for (var i = 0; i < listenerCount; ++i) {
      var listener = listeners[i];
      if (listener instanceof Ci.nsIProgressEventSink)
        listener.onStatus(request, context, status, statusText);
    }
  },

  /**
   * When data transfer ceases
   * @param   request
   *          The nsIRequest object for the transfer
   * @param   context
   *          Additional data
   * @param   status
   *          Status code containing the reason for the cessation.
   */
  onStopRequest: function  Downloader_onStopRequest(request, context, status) {
    if (request instanceof Ci.nsIIncrementalDownload)
      LOG("Downloader:onStopRequest - original URI spec: " + request.URI.spec +
          ", final URI spec: " + request.finalURI.spec + ", status: " + status);

    // XXX ehsan shouldShowPrompt should always be false here.
    // But what happens when there is already a UI showing?
    var state = this._patch.state;
    var shouldShowPrompt = false;
    var shouldRegisterOnlineObserver = false;
    var shouldRetrySoon = false;
    var deleteActiveUpdate = false;
    var retryTimeout = getPref("getIntPref", PREF_APP_UPDATE_RETRY_TIMEOUT,
                               DEFAULT_UPDATE_RETRY_TIMEOUT);
    var maxFail = getPref("getIntPref", PREF_APP_UPDATE_SOCKET_ERRORS,
                          DEFAULT_SOCKET_MAX_ERRORS);
    LOG("Downloader:onStopRequest - status: " + status + ", " +
        "current fail: " + this.updateService._consecutiveSocketErrors + ", " +
        "max fail: " + maxFail + ", " + "retryTimeout: " + retryTimeout);
    if (Components.isSuccessCode(status)) {
      if (this._verifyDownload()) {
        state = shouldUseService() ? STATE_PENDING_SVC : STATE_PENDING;
        if (this.background) {
          shouldShowPrompt = !getCanStageUpdates();
        }
        AUSTLMY.pingDownloadCode(this.isCompleteUpdate, AUSTLMY.DWNLD_SUCCESS);

        // Tell the updater.exe we're ready to apply.
        writeStatusFile(getUpdatesDir(), state);
        writeVersionFile(getUpdatesDir(), this._update.appVersion);
        this._update.installDate = (new Date()).getTime();
        this._update.statusText = gUpdateBundle.GetStringFromName("installPending");
      }
      else {
        LOG("Downloader:onStopRequest - download verification failed");
        state = STATE_DOWNLOAD_FAILED;
        status = Cr.NS_ERROR_CORRUPTED_CONTENT;

        // Yes, this code is a string.
        const vfCode = "verification_failed";
        var message = getStatusTextFromCode(vfCode, vfCode);
        this._update.statusText = message;

        if (this._update.isCompleteUpdate || this._update.patchCount != 2)
          deleteActiveUpdate = true;

        // Destroy the updates directory, since we're done with it.
        cleanUpUpdatesDir();
      }
    } else {
      if (status == Cr.NS_ERROR_OFFLINE) {
        // Register an online observer to try again.
        // The online observer will continue the incremental download by
        // calling downloadUpdate on the active update which continues
        // downloading the file from where it was.
        LOG("Downloader:onStopRequest - offline, register online observer: true");
        AUSTLMY.pingDownloadCode(this.isCompleteUpdate,
                                 AUSTLMY.DWNLD_RETRY_OFFLINE);
        shouldRegisterOnlineObserver = true;
        deleteActiveUpdate = false;
      // Each of NS_ERROR_NET_TIMEOUT, ERROR_CONNECTION_REFUSED, and
      // NS_ERROR_NET_RESET can be returned when disconnecting the internet while
      // a download of a MAR is in progress.  There may be others but I have not
      // encountered them during testing.
      } else if ((status == Cr.NS_ERROR_NET_TIMEOUT ||
                  status == Cr.NS_ERROR_CONNECTION_REFUSED ||
                  status == Cr.NS_ERROR_NET_RESET) &&
                 this.updateService._consecutiveSocketErrors < maxFail) {
        LOG("Downloader:onStopRequest - socket error, shouldRetrySoon: true");
        let dwnldCode = AUSTLMY.DWNLD_RETRY_CONNECTION_REFUSED;
        if (status == Cr.NS_ERROR_NET_TIMEOUT) {
          dwnldCode = AUSTLMY.DWNLD_RETRY_NET_TIMEOUT;
        } else if (status == Cr.NS_ERROR_NET_RESET) {
          dwnldCode = AUSTLMY.DWNLD_RETRY_NET_RESET;
        }
        AUSTLMY.pingDownloadCode(this.isCompleteUpdate, dwnldCode);
        shouldRetrySoon = true;
        deleteActiveUpdate = false;
      } else if (status != Cr.NS_BINDING_ABORTED &&
                 status != Cr.NS_ERROR_ABORT &&
                 status != Cr.NS_ERROR_DOCUMENT_NOT_CACHED) {
        LOG("Downloader:onStopRequest - non-verification failure");
        let dwnldCode = AUSTLMY.DWNLD_ERR_DOCUMENT_NOT_CACHED;
        if (status == Cr.NS_BINDING_ABORTED) {
          dwnldCode = AUSTLMY.DWNLD_ERR_BINDING_ABORTED;
        } else if (status == Cr.NS_ERROR_ABORT) {
          dwnldCode = AUSTLMY.DWNLD_ERR_ABORT;
        }
        AUSTLMY.pingDownloadCode(this.isCompleteUpdate, dwnldCode);

        // Some sort of other failure, log this in the |statusText| property
        state = STATE_DOWNLOAD_FAILED;

        // XXXben - if |request| (The Incremental Download) provided a means
        // for accessing the http channel we could do more here.

        this._update.statusText = getStatusTextFromCode(status,
                                                        Cr.NS_BINDING_FAILED);

        if (AppConstants.platform == "gonk") {
          // bug891009: On FirefoxOS, manaully retry OTA download will reuse
          // the Update object. We need to remove selected patch so that download
          // can be triggered again successfully.
          this._update.selectedPatch.selected = false;
        }

        // Destroy the updates directory, since we're done with it.
        cleanUpUpdatesDir();

        deleteActiveUpdate = true;
      }
    }
    LOG("Downloader:onStopRequest - setting state to: " + state);
    this._patch.state = state;
    var um = Cc["@mozilla.org/updates/update-manager;1"].
             getService(Ci.nsIUpdateManager);
    if (deleteActiveUpdate) {
      this._update.installDate = (new Date()).getTime();
      um.activeUpdate = null;
    }
    else {
      if (um.activeUpdate) {
        um.activeUpdate.state = state;
      }
    }
    um.saveUpdates();

    // Only notify listeners about the stopped state if we
    // aren't handling an internal retry.
    if (!shouldRetrySoon && !shouldRegisterOnlineObserver) {
      var listeners = this._listeners.concat();
      var listenerCount = listeners.length;
      for (var i = 0; i < listenerCount; ++i) {
        listeners[i].onStopRequest(request, context, status);
      }
    }

    this._request = null;

    if (state == STATE_DOWNLOAD_FAILED) {
      var allFailed = true;
      // Check if there is a complete update patch that can be downloaded.
      if (!this._update.isCompleteUpdate && this._update.patchCount == 2) {
        LOG("Downloader:onStopRequest - verification of patch failed, " +
            "downloading complete update patch");
        this._update.isCompleteUpdate = true;
        let updateStatus = this.downloadUpdate(this._update);

        if (updateStatus == STATE_NONE) {
          cleanupActiveUpdate();
        } else {
          allFailed = false;
        }
      }

      if (allFailed) {
        LOG("Downloader:onStopRequest - all update patch downloads failed");
        // If the update UI is not open (e.g. the user closed the window while
        // downloading) and if at any point this was a foreground download
        // notify the user about the error. If the update was a background
        // update there is no notification since the user won't be expecting it.
        if (!Services.wm.getMostRecentWindow(UPDATE_WINDOW_NAME)) {
          this._update.QueryInterface(Ci.nsIWritablePropertyBag);
          if (this._update.getProperty("foregroundDownload") == "true") {
            var prompter = Cc["@mozilla.org/updates/update-prompt;1"].
                           createInstance(Ci.nsIUpdatePrompt);
            prompter.showUpdateError(this._update);
          }
        }

        if (AppConstants.platform == "gonk") {
          // We always forward errors in B2G, since Gaia controls the update UI
          let prompter = Cc["@mozilla.org/updates/update-prompt;1"].
                         createInstance(Ci.nsIUpdatePrompt);
          prompter.showUpdateError(this._update);
        }

        // Prevent leaking the update object (bug 454964).
        this._update = null;
      }
      // A complete download has been initiated or the failure was handled.
      return;
    }

    if (state == STATE_PENDING || state == STATE_PENDING_SVC) {
      if (getCanStageUpdates()) {
        LOG("Downloader:onStopRequest - attempting to stage update: " +
            this._update.name);

        // Initiate the update in the background
        try {
          Cc["@mozilla.org/updates/update-processor;1"].
            createInstance(Ci.nsIUpdateProcessor).
            processUpdate(this._update);
        } catch (e) {
          // Fail gracefully in case the application does not support the update
          // processor service.
          LOG("Downloader:onStopRequest - failed to stage update. Exception: " +
              e);
          if (this.background) {
            shouldShowPrompt = true;
          }
        }
      }
    }

    // Do this after *everything* else, since it will likely cause the app
    // to shut down.
    if (shouldShowPrompt) {
      // Notify the user that an update has been downloaded and is ready for
      // installation (i.e. that they should restart the application). We do
      // not notify on failed update attempts.
      let prompter = Cc["@mozilla.org/updates/update-prompt;1"].
                     createInstance(Ci.nsIUpdatePrompt);
      prompter.showUpdateDownloaded(this._update, true);
    }

    if (shouldRegisterOnlineObserver) {
      LOG("Downloader:onStopRequest - Registering online observer");
      this.updateService._registerOnlineObserver();
    } else if (shouldRetrySoon) {
      LOG("Downloader:onStopRequest - Retrying soon");
      this.updateService._consecutiveSocketErrors++;
      if (this.updateService._retryTimer) {
        this.updateService._retryTimer.cancel();
      }
      this.updateService._retryTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      this.updateService._retryTimer.initWithCallback(function() {
        this._attemptResume();
      }.bind(this.updateService), retryTimeout, Ci.nsITimer.TYPE_ONE_SHOT);
    } else {
      // Prevent leaking the update object (bug 454964)
      this._update = null;
    }
  },

  /**
   * See nsIInterfaceRequestor.idl
   */
  getInterface: function Downloader_getInterface(iid) {
    // The network request may require proxy authentication, so provide the
    // default nsIAuthPrompt if requested.
    if (iid.equals(Ci.nsIAuthPrompt)) {
      var prompt = Cc["@mozilla.org/network/default-auth-prompt;1"].
                   createInstance();
      return prompt.QueryInterface(iid);
    }
    throw Cr.NS_NOINTERFACE;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIRequestObserver,
                                         Ci.nsIProgressEventSink,
                                         Ci.nsIInterfaceRequestor])
};

/**
 * UpdatePrompt
 * An object which can prompt the user with information about updates, request
 * action, etc. Embedding clients can override this component with one that
 * invokes a native front end.
 * @constructor
 */
function UpdatePrompt() {
}
UpdatePrompt.prototype = {
  /**
   * See nsIUpdateService.idl
   */
  checkForUpdates: function UP_checkForUpdates() {
    if (this._getAltUpdateWindow())
      return;

    this._showUI(null, URI_UPDATE_PROMPT_DIALOG, null, UPDATE_WINDOW_NAME,
                 null, null);
  },

  /**
   * See nsIUpdateService.idl
   */
  showUpdateAvailable: function UP_showUpdateAvailable(update) {
    if (getPref("getBoolPref", PREF_APP_UPDATE_SILENT, false) ||
        this._getUpdateWindow() || this._getAltUpdateWindow())
      return;

    var stringsPrefix = "updateAvailable_" + update.type + ".";
    var title = gUpdateBundle.formatStringFromName(stringsPrefix + "title",
                                                   [update.name], 1);
    var text = gUpdateBundle.GetStringFromName(stringsPrefix + "text");
    var imageUrl = "";
    this._showUnobtrusiveUI(null, URI_UPDATE_PROMPT_DIALOG, null,
                           UPDATE_WINDOW_NAME, "updatesavailable", update,
                           title, text, imageUrl);
  },

  /**
   * See nsIUpdateService.idl
   */
  showUpdateDownloaded: function UP_showUpdateDownloaded(update, background) {
    if (this._getAltUpdateWindow())
      return;

    if (background) {
      if (getPref("getBoolPref", PREF_APP_UPDATE_SILENT, false))
        return;

      var stringsPrefix = "updateDownloaded_" + update.type + ".";
      var title = gUpdateBundle.formatStringFromName(stringsPrefix + "title",
                                                     [update.name], 1);
      var text = gUpdateBundle.GetStringFromName(stringsPrefix + "text");
      var imageUrl = "";
      this._showUnobtrusiveUI(null, URI_UPDATE_PROMPT_DIALOG, null,
                              UPDATE_WINDOW_NAME, "finishedBackground", update,
                              title, text, imageUrl);
    } else {
      this._showUI(null, URI_UPDATE_PROMPT_DIALOG, null,
                   UPDATE_WINDOW_NAME, "finishedBackground", update);
    }
  },

  /**
   * See nsIUpdateService.idl
   */
  showUpdateInstalled: function UP_showUpdateInstalled() {
    if (getPref("getBoolPref", PREF_APP_UPDATE_SILENT, false) ||
        !getPref("getBoolPref", PREF_APP_UPDATE_SHOW_INSTALLED_UI, false) ||
        this._getUpdateWindow())
      return;

    var page = "installed";
    var win = this._getUpdateWindow();
    if (win) {
      if (page && "setCurrentPage" in win)
        win.setCurrentPage(page);
      win.focus();
    }
    else {
      var openFeatures = "chrome,centerscreen,dialog=no,resizable=no,titlebar,toolbar=no";
      var arg = Cc["@mozilla.org/supports-string;1"].
                createInstance(Ci.nsISupportsString);
      arg.data = page;
      Services.ww.openWindow(null, URI_UPDATE_PROMPT_DIALOG, null, openFeatures, arg);
    }
  },

  /**
   * See nsIUpdateService.idl
   */
  showUpdateError: function UP_showUpdateError(update) {
    if (getPref("getBoolPref", PREF_APP_UPDATE_SILENT, false) ||
        this._getAltUpdateWindow())
      return;

    // In some cases, we want to just show a simple alert dialog.
    // Replace with Array.prototype.includes when it has stabilized.
    if (update.state == STATE_FAILED &&
        (WRITE_ERRORS.indexOf(update.errorCode) != -1 ||
         update.errorCode == FILESYSTEM_MOUNT_READWRITE_ERROR ||
         update.errorCode == FOTA_GENERAL_ERROR ||
         update.errorCode == FOTA_FILE_OPERATION_ERROR ||
         update.errorCode == FOTA_RECOVERY_ERROR ||
         update.errorCode == FOTA_UNKNOWN_ERROR)) {
      var title = gUpdateBundle.GetStringFromName("updaterIOErrorTitle");
      var text = gUpdateBundle.formatStringFromName("updaterIOErrorMsg",
                                                    [Services.appinfo.name,
                                                     Services.appinfo.name], 2);
      Services.ww.getNewPrompter(null).alert(title, text);
      return;
    }

    if (update.errorCode == CERT_ATTR_CHECK_FAILED_NO_UPDATE ||
        update.errorCode == CERT_ATTR_CHECK_FAILED_HAS_UPDATE ||
        update.errorCode == BACKGROUNDCHECK_MULTIPLE_FAILURES) {
      this._showUIWhenIdle(null, URI_UPDATE_PROMPT_DIALOG, null,
                           UPDATE_WINDOW_NAME, null, update);
      return;
    }

    this._showUI(null, URI_UPDATE_PROMPT_DIALOG, null, UPDATE_WINDOW_NAME,
                 "errors", update);
  },

  /**
   * See nsIUpdateService.idl
   */
  showUpdateHistory: function UP_showUpdateHistory(parent) {
    this._showUI(parent, URI_UPDATE_HISTORY_DIALOG, "modal,dialog=yes",
                 "Update:History", null, null);
  },

  /**
   * Returns the update window if present.
   */
  _getUpdateWindow: function UP__getUpdateWindow() {
    return Services.wm.getMostRecentWindow(UPDATE_WINDOW_NAME);
  },

  /**
   * Returns an alternative update window if present. When a window with this
   * windowtype is open the application update service won't open the normal
   * application update user interface window.
   */
  _getAltUpdateWindow: function UP__getAltUpdateWindow() {
    let windowType = getPref("getCharPref", PREF_APP_UPDATE_ALTWINDOWTYPE, null);
    if (!windowType)
      return null;
    return Services.wm.getMostRecentWindow(windowType);
  },

  /**
   * Initiate a less obtrusive UI, starting with a non-modal notification alert
   * @param   parent
   *          A parent window, can be null
   * @param   uri
   *          The URI string of the dialog to show
   * @param   name
   *          The Window Name of the dialog to show, in case it is already open
   *          and can merely be focused
   * @param   page
   *          The page of the wizard to be displayed, if one is already open.
   * @param   update
   *          An update to pass to the UI in the window arguments.
   *          Can be null
   * @param   title
   *          The title for the notification alert.
   * @param   text
   *          The contents of the notification alert.
   * @param   imageUrl
   *          A URL identifying the image to put in the notification alert.
   */
  _showUnobtrusiveUI: function UP__showUnobUI(parent, uri, features, name, page,
                                              update, title, text, imageUrl) {
    var observer = {
      updatePrompt: this,
      service: null,
      timer: null,
      notify: function () {
        // the user hasn't restarted yet => prompt when idle
        this.service.removeObserver(this, "quit-application");
        // If the update window is already open skip showing the UI
        if (this.updatePrompt._getUpdateWindow())
          return;
        this.updatePrompt._showUIWhenIdle(parent, uri, features, name, page, update);
      },
      observe: function (aSubject, aTopic, aData) {
        switch (aTopic) {
          case "alertclickcallback":
            this.updatePrompt._showUI(parent, uri, features, name, page, update);
            // fall thru
          case "quit-application":
            if (this.timer)
              this.timer.cancel();
            this.service.removeObserver(this, "quit-application");
            break;
        }
      }
    };

    // bug 534090 - show the UI for update available notifications when the
    // the system has been idle for at least IDLE_TIME without displaying an
    // alert notification.
    if (page == "updatesavailable") {
      var idleService = Cc["@mozilla.org/widget/idleservice;1"].
                        getService(Ci.nsIIdleService);

      const IDLE_TIME = getPref("getIntPref", PREF_APP_UPDATE_IDLETIME, 60);
      if (idleService.idleTime / 1000 >= IDLE_TIME) {
        this._showUI(parent, uri, features, name, page, update);
        return;
      }
    }

    try {
      var notifier = Cc["@mozilla.org/alerts-service;1"].
                     getService(Ci.nsIAlertsService);
      notifier.showAlertNotification(imageUrl, title, text, true, "", observer);
    }
    catch (e) {
      // Failed to retrieve alerts service, platform unsupported
      this._showUIWhenIdle(parent, uri, features, name, page, update);
      return;
    }

    observer.service = Services.obs;
    observer.service.addObserver(observer, "quit-application", false);

    // bug 534090 - show the UI when idle for update available notifications.
    if (page == "updatesavailable") {
      this._showUIWhenIdle(parent, uri, features, name, page, update);
      return;
    }

    // Give the user x seconds to react before prompting as defined by
    // promptWaitTime
    observer.timer = Cc["@mozilla.org/timer;1"].
                     createInstance(Ci.nsITimer);
    observer.timer.initWithCallback(observer, update.promptWaitTime * 1000,
                                    observer.timer.TYPE_ONE_SHOT);
  },

  /**
   * Show the UI when the user was idle
   * @param   parent
   *          A parent window, can be null
   * @param   uri
   *          The URI string of the dialog to show
   * @param   name
   *          The Window Name of the dialog to show, in case it is already open
   *          and can merely be focused
   * @param   page
   *          The page of the wizard to be displayed, if one is already open.
   * @param   update
   *          An update to pass to the UI in the window arguments.
   *          Can be null
   */
  _showUIWhenIdle: function UP__showUIWhenIdle(parent, uri, features, name,
                                               page, update) {
    var idleService = Cc["@mozilla.org/widget/idleservice;1"].
                      getService(Ci.nsIIdleService);

    const IDLE_TIME = getPref("getIntPref", PREF_APP_UPDATE_IDLETIME, 60);
    if (idleService.idleTime / 1000 >= IDLE_TIME) {
      this._showUI(parent, uri, features, name, page, update);
    } else {
      var observer = {
        updatePrompt: this,
        observe: function (aSubject, aTopic, aData) {
          switch (aTopic) {
            case "idle":
              // If the update window is already open skip showing the UI
              if (!this.updatePrompt._getUpdateWindow())
                this.updatePrompt._showUI(parent, uri, features, name, page, update);
              // fall thru
            case "quit-application":
              idleService.removeIdleObserver(this, IDLE_TIME);
              Services.obs.removeObserver(this, "quit-application");
              break;
          }
        }
      };
      idleService.addIdleObserver(observer, IDLE_TIME);
      Services.obs.addObserver(observer, "quit-application", false);
    }
  },

  /**
   * Show the Update Checking UI
   * @param   parent
   *          A parent window, can be null
   * @param   uri
   *          The URI string of the dialog to show
   * @param   name
   *          The Window Name of the dialog to show, in case it is already open
   *          and can merely be focused
   * @param   page
   *          The page of the wizard to be displayed, if one is already open.
   * @param   update
   *          An update to pass to the UI in the window arguments.
   *          Can be null
   */
  _showUI: function UP__showUI(parent, uri, features, name, page, update) {
    var ary = null;
    if (update) {
      ary = Cc["@mozilla.org/supports-array;1"].
            createInstance(Ci.nsISupportsArray);
      ary.AppendElement(update);
    }

    var win = this._getUpdateWindow();
    if (win) {
      if (page && "setCurrentPage" in win)
        win.setCurrentPage(page);
      win.focus();
    }
    else {
      var openFeatures = "chrome,centerscreen,dialog=no,resizable=no,titlebar,toolbar=no";
      if (features)
        openFeatures += "," + features;
      Services.ww.openWindow(parent, uri, "", openFeatures, ary);
    }
  },

  classDescription: "Update Prompt",
  contractID: "@mozilla.org/updates/update-prompt;1",
  classID: Components.ID("{27ABA825-35B5-4018-9FDD-F99250A0E722}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUpdatePrompt])
};

var components = [UpdateService, Checker, UpdatePrompt, UpdateManager];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
