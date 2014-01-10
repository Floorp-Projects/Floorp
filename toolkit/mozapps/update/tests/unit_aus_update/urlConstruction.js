/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General URL Construction Tests */

Components.utils.import("resource://gre/modules/ctypes.jsm")

const URL_PREFIX = URL_HOST + "/";

var gAppInfo;

function run_test() {
  setupTestCommon(true);

  // The mock XMLHttpRequest is MUCH faster
  overrideXHR(callHandleEvent);
  standardInit();
  gAppInfo = AUS_Cc["@mozilla.org/xre/app-info;1"].
             getService(AUS_Ci.nsIXULAppInfo).
             QueryInterface(AUS_Ci.nsIXULRuntime);
  do_execute_soon(run_test_pt1);
}

function end_test() {
  cleanupTestCommon();
}

// Callback function used by the custom XMLHttpRequest implementation to
// call the nsIDOMEventListener's handleEvent method for onload.
function callHandleEvent() {
  var e = { target: gXHR };
  gXHR.onload(e);
}

// Helper function for parsing the result from the contructed url
function getResult(url) {
  return url.substr(URL_PREFIX.length).split("/")[0];
}

// url constructed with %PRODUCT%
function run_test_pt1() {
  gCheckFunc = check_test_pt1;
  var url = URL_PREFIX + "%PRODUCT%/";
  logTestInfo("testing url constructed with %PRODUCT% - " + url);
  setUpdateURLOverride(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt1() {
  do_check_eq(getResult(gRequestURL), gAppInfo.name);
  run_test_pt2();
}

// url constructed with %VERSION%
function run_test_pt2() {
  gCheckFunc = check_test_pt2;
  var url = URL_PREFIX + "%VERSION%/";
  logTestInfo("testing url constructed with %VERSION% - " + url);
  setUpdateURLOverride(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt2() {
  do_check_eq(getResult(gRequestURL), gAppInfo.version);
  run_test_pt3();
}

// url constructed with %BUILD_ID%
function run_test_pt3() {
  gCheckFunc = check_test_pt3;
  var url = URL_PREFIX + "%BUILD_ID%/";
  logTestInfo("testing url constructed with %BUILD_ID% - " + url);
  setUpdateURLOverride(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt3() {
  do_check_eq(getResult(gRequestURL), gAppInfo.appBuildID);
  run_test_pt4();
}

// url constructed with %BUILD_TARGET%
// XXX TODO - it might be nice if we tested the actual ABI
function run_test_pt4() {
  gCheckFunc = check_test_pt4;
  var url = URL_PREFIX + "%BUILD_TARGET%/";
  logTestInfo("testing url constructed with %BUILD_TARGET% - " + url);
  setUpdateURLOverride(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt4() {
  var abi;
  try {
    abi = gAppInfo.XPCOMABI;
  }
  catch (e) {
    do_throw("nsIXULAppInfo:XPCOMABI not defined\n");
  }

  if (IS_MACOSX) {
    // Mac universal build should report a different ABI than either macppc
    // or mactel. This is necessary since nsUpdateService.js will set the ABI to
    // Universal-gcc3 for Mac universal builds.
    var macutils = AUS_Cc["@mozilla.org/xpcom/mac-utils;1"].
                   getService(AUS_Ci.nsIMacUtils);

    if (macutils.isUniversalBinary)
      abi += "-u-" + macutils.architecturesInBinary;
    if (IS_SHARK) {
      // Disambiguate optimised and shark nightlies
      abi += "-shark"
    }

  }

  do_check_eq(getResult(gRequestURL), gAppInfo.OS + "_" + abi);
  run_test_pt5();
}

// url constructed with %LOCALE%
// Bug 488936 added the update.locale file that stores the update locale
function run_test_pt5() {
  gCheckFunc = check_test_pt5;
  var url = URL_PREFIX + "%LOCALE%/";
  logTestInfo("testing url constructed with %LOCALE% - " + url);
  setUpdateURLOverride(url);
  try {
    gUpdateChecker.checkForUpdates(updateCheckListener, true);
  }
  catch (e) {
    logTestInfo("The following error is most likely due to a missing " +
                "update.locale file");
    do_throw(e);
  }
}

function check_test_pt5() {
  do_check_eq(getResult(gRequestURL), INSTALL_LOCALE);
  run_test_pt6();
}

// url constructed with %CHANNEL%
function run_test_pt6() {
  gCheckFunc = check_test_pt6;
  var url = URL_PREFIX + "%CHANNEL%/";
  logTestInfo("testing url constructed with %CHANNEL% - " + url);
  setUpdateURLOverride(url);
  setUpdateChannel("test_channel");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt6() {
  do_check_eq(getResult(gRequestURL), "test_channel");
  run_test_pt7();
}

// url constructed with %CHANNEL% with distribution partners
function run_test_pt7() {
  gCheckFunc = check_test_pt7;
  var url = URL_PREFIX + "%CHANNEL%/";
  logTestInfo("testing url constructed with %CHANNEL% - " + url);
  setUpdateURLOverride(url);
  gDefaultPrefBranch.setCharPref(PREF_APP_PARTNER_BRANCH + "test_partner1", "test_partner1");
  gDefaultPrefBranch.setCharPref(PREF_APP_PARTNER_BRANCH + "test_partner2", "test_partner2");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt7() {
  do_check_eq(getResult(gRequestURL), "test_channel-cck-test_partner1-test_partner2");
  run_test_pt8();
}

// url constructed with %PLATFORM_VERSION%
function run_test_pt8() {
  gCheckFunc = check_test_pt8;
  var url = URL_PREFIX + "%PLATFORM_VERSION%/";
  logTestInfo("testing url constructed with %PLATFORM_VERSION% - " + url);
  setUpdateURLOverride(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt8() {
  do_check_eq(getResult(gRequestURL), gAppInfo.platformVersion);
  run_test_pt9();
}

// url constructed with %OS_VERSION%
function run_test_pt9() {
  gCheckFunc = check_test_pt9;
  var url = URL_PREFIX + "%OS_VERSION%/";
  logTestInfo("testing url constructed with %OS_VERSION% - " + url);
  setUpdateURLOverride(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function getServicePack() {
  // NOTE: This function is a helper function and not a test.  Thus,
  // it uses throw() instead of do_throw().  Any tests that use this function
  // should catch exceptions thrown in this function and deal with them
  // appropriately (usually by calling do_throw).
  const BYTE = ctypes.uint8_t;
  const WORD = ctypes.uint16_t;
  const DWORD = ctypes.uint32_t;
  const WCHAR = ctypes.jschar;
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

  let kernel32 = ctypes.open("kernel32");
  try {
    let GetVersionEx = kernel32.declare("GetVersionExW",
                                        ctypes.default_abi,
                                        BOOL,
                                        OSVERSIONINFOEXW.ptr);
    let winVer = OSVERSIONINFOEXW();
    winVer.dwOSVersionInfoSize = OSVERSIONINFOEXW.size;

    if(0 === GetVersionEx(winVer.address())) {
      // Using "throw" instead of "do_throw" (see NOTE above)
      throw("Failure in GetVersionEx (returned 0)");
    }

    return winVer.wServicePackMajor + "." + winVer.wServicePackMinor;
  } finally {
    kernel32.close();
  }
}

function getProcArchitecture() {
  // NOTE: This function is a helper function and not a test.  Thus,
  // it uses throw() instead of do_throw().  Any tests that use this function
  // should catch exceptions thrown in this function and deal with them
  // appropriately (usually by calling do_throw).
  const WORD = ctypes.uint16_t;
  const DWORD = ctypes.uint32_t;

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

  let kernel32 = ctypes.open("kernel32");
  try {
    let GetNativeSystemInfo = kernel32.declare("GetNativeSystemInfo",
                                               ctypes.default_abi,
                                               ctypes.void_t,
                                               SYSTEM_INFO.ptr);
    let sysInfo = SYSTEM_INFO();
    // Default to unknown
    sysInfo.wProcessorArchitecture = 0xffff;

    GetNativeSystemInfo(sysInfo.address());
    switch(sysInfo.wProcessorArchitecture) {
      case 9:
        return "x64";
      case 6:
        return "IA64";
      case 0:
        return "x86";
      default:
        // Using "throw" instead of "do_throw" (see NOTE above)
        throw("Unknown architecture returned from GetNativeSystemInfo: " + sysInfo.wProcessorArchitecture);
    }
  } finally {
    kernel32.close();
  }
}

function check_test_pt9() {
  var osVersion;
  var sysInfo = AUS_Cc["@mozilla.org/system-info;1"].
                getService(AUS_Ci.nsIPropertyBag2);
  osVersion = sysInfo.getProperty("name") + " " + sysInfo.getProperty("version");

  if(IS_WIN) {
    try {
      let servicePack = getServicePack();
      osVersion += "." + servicePack;
    } catch (e) {
      do_throw("Failure obtaining service pack: " + e);
    }

    if("5.0" === sysInfo.getProperty("version")) { // Win2K
      osVersion += " (unknown)";
    } else {
      try {
        osVersion += " (" + getProcArchitecture() + ")";
      } catch (e) {
        do_throw("Failed to obtain processor architecture: " + e);
      }
    }
  }

  if (osVersion) {
    try {
      osVersion += " (" + sysInfo.getProperty("secondaryLibrary") + ")";
    }
    catch (e) {
      // Not all platforms have a secondary widget library, so an error is
      // nothing to worry about.
    }
    osVersion = encodeURIComponent(osVersion);
  }

  do_check_eq(getResult(gRequestURL), osVersion);
  run_test_pt10();
}

// url constructed with %DISTRIBUTION%
function run_test_pt10() {
  gCheckFunc = check_test_pt10;
  var url = URL_PREFIX + "%DISTRIBUTION%/";
  logTestInfo("testing url constructed with %DISTRIBUTION% - " + url);
  setUpdateURLOverride(url);
  gDefaultPrefBranch.setCharPref(PREF_DISTRIBUTION_ID, "test_distro");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt10() {
  do_check_eq(getResult(gRequestURL), "test_distro");
  run_test_pt11();
}

// url constructed with %DISTRIBUTION_VERSION%
function run_test_pt11() {
  gCheckFunc = check_test_pt11;
  var url = URL_PREFIX + "%DISTRIBUTION_VERSION%/";
  logTestInfo("testing url constructed with %DISTRIBUTION_VERSION% - " + url);
  setUpdateURLOverride(url);
  gDefaultPrefBranch.setCharPref(PREF_DISTRIBUTION_VERSION, "test_distro_version");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt11() {
  do_check_eq(getResult(gRequestURL), "test_distro_version");
  run_test_pt12();
}

// url with force param that doesn't already have a param - bug 454357
function run_test_pt12() {
  gCheckFunc = check_test_pt12;
  var url = URL_PREFIX;
  logTestInfo("testing url with force param that doesn't already have a " +
              "param - " + url);
  setUpdateURLOverride(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt12() {
  do_check_eq(getResult(gRequestURL), "?force=1");
  run_test_pt13();
}

// url with force param that already has a param - bug 454357
function run_test_pt13() {
  gCheckFunc = check_test_pt13;
  var url = URL_PREFIX + "?extra=param";
  logTestInfo("testing url with force param that already has a param - " + url);
  logTestInfo("testing url constructed that has a parameter - " + url);
  setUpdateURLOverride(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt13() {
  do_check_eq(getResult(gRequestURL), "?extra=param&force=1");
  run_test_pt14();
}

function run_test_pt14() {
  Services.prefs.setCharPref("app.update.custom", "custom");
  gCheckFunc = check_test_pt14;
  var url = URL_PREFIX + "?custom=%CUSTOM%";
  logTestInfo("testing url constructed with %CUSTOM% - " + url);
  setUpdateURLOverride(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt14() {
  do_check_eq(getResult(gRequestURL), "?custom=custom&force=1");
  do_test_finished();
}
