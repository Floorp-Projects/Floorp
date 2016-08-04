/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General URL Construction Tests */

const URL_PREFIX = URL_HOST + "/";

var gAppInfo;

function run_test() {
  // This test needs access to omni.ja to read the update.locale file so don't
  // use a custom directory for the application directory.
  gUseTestAppDir = false;
  setupTestCommon();

  // The mock XMLHttpRequest is MUCH faster
  overrideXHR(callHandleEvent);
  standardInit();
  gAppInfo = Cc["@mozilla.org/xre/app-info;1"].
             getService(Ci.nsIXULAppInfo).
             QueryInterface(Ci.nsIXULRuntime);
  do_execute_soon(run_test_pt1);
}

// Callback function used by the custom XMLHttpRequest implementation to
// call the nsIDOMEventListener's handleEvent method for onload.
function callHandleEvent(aXHR) {
  // The mock xmlhttprequest needs a status code to return to the consumer and
  // the value is not important for this test.
  aXHR.status = 404;
  let e = { target: aXHR };
  aXHR.onload(e);
}

// Helper function for parsing the result from the contructed url
function getResult(url) {
  return url.substr(URL_PREFIX.length).split("/")[0];
}

// url constructed with %PRODUCT%
function run_test_pt1() {
  gCheckFunc = check_test_pt1;
  let url = URL_PREFIX + "%PRODUCT%/";
  debugDump("testing url constructed with %PRODUCT% - " + url);
  setUpdateURLOverride(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt1() {
  Assert.equal(getResult(gRequestURL), gAppInfo.name,
               "the url param for %PRODUCT%" + MSG_SHOULD_EQUAL);
  run_test_pt2();
}

// url constructed with %VERSION%
function run_test_pt2() {
  gCheckFunc = check_test_pt2;
  let url = URL_PREFIX + "%VERSION%/";
  debugDump("testing url constructed with %VERSION% - " + url);
  setUpdateURLOverride(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt2() {
  Assert.equal(getResult(gRequestURL), gAppInfo.version,
               "the url param for %VERSION%" + MSG_SHOULD_EQUAL);
  run_test_pt3();
}

// url constructed with %BUILD_ID%
function run_test_pt3() {
  gCheckFunc = check_test_pt3;
  let url = URL_PREFIX + "%BUILD_ID%/";
  debugDump("testing url constructed with %BUILD_ID% - " + url);
  setUpdateURLOverride(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt3() {
  Assert.equal(getResult(gRequestURL), gAppInfo.appBuildID,
               "the url param for %BUILD_ID%" + MSG_SHOULD_EQUAL);
  run_test_pt4();
}

// url constructed with %BUILD_TARGET%
// XXX TODO - it might be nice if we tested the actual ABI
function run_test_pt4() {
  gCheckFunc = check_test_pt4;
  let url = URL_PREFIX + "%BUILD_TARGET%/";
  debugDump("testing url constructed with %BUILD_TARGET% - " + url);
  setUpdateURLOverride(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt4() {
  let abi;
  try {
    abi = gAppInfo.XPCOMABI;
  } catch (e) {
    do_throw("nsIXULAppInfo:XPCOMABI not defined\n");
  }

  if (IS_MACOSX) {
    // Mac universal build should report a different ABI than either macppc
    // or mactel. This is necessary since nsUpdateService.js will set the ABI to
    // Universal-gcc3 for Mac universal builds.
    let macutils = Cc["@mozilla.org/xpcom/mac-utils;1"].
                   getService(Ci.nsIMacUtils);

    if (macutils.isUniversalBinary) {
      abi += "-u-" + macutils.architecturesInBinary;
    }
  } else if (IS_WIN) {
    // Windows build should report the CPU architecture that it's running on.
    abi += "-" + getProcArchitecture();
  }

  Assert.equal(getResult(gRequestURL), gAppInfo.OS + "_" + abi,
               "the url param for %BUILD_TARGET%" + MSG_SHOULD_EQUAL);
  run_test_pt5();
}

// url constructed with %LOCALE%
// Bug 488936 added the update.locale file that stores the update locale
function run_test_pt5() {
  // The code that gets the locale accesses the profile which is only available
  // after calling do_get_profile in xpcshell tests. This prevents an error from
  // being logged.
  do_get_profile();

  gCheckFunc = check_test_pt5;
  let url = URL_PREFIX + "%LOCALE%/";
  debugDump("testing url constructed with %LOCALE% - " + url);
  setUpdateURLOverride(url);
  try {
    gUpdateChecker.checkForUpdates(updateCheckListener, true);
  } catch (e) {
    debugDump("The following error is most likely due to a missing " +
              "update.locale file");
    do_throw(e);
  }
}

function check_test_pt5() {
  Assert.equal(getResult(gRequestURL), INSTALL_LOCALE,
               "the url param for %LOCALE%" + MSG_SHOULD_EQUAL);
  run_test_pt6();
}

// url constructed with %CHANNEL%
function run_test_pt6() {
  gCheckFunc = check_test_pt6;
  let url = URL_PREFIX + "%CHANNEL%/";
  debugDump("testing url constructed with %CHANNEL% - " + url);
  setUpdateURLOverride(url);
  setUpdateChannel("test_channel");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt6() {
  Assert.equal(getResult(gRequestURL), "test_channel",
               "the url param for %CHANNEL%" + MSG_SHOULD_EQUAL);
  run_test_pt7();
}

// url constructed with %CHANNEL% with distribution partners
function run_test_pt7() {
  gCheckFunc = check_test_pt7;
  let url = URL_PREFIX + "%CHANNEL%/";
  debugDump("testing url constructed with %CHANNEL% - " + url);
  setUpdateURLOverride(url);
  gDefaultPrefBranch.setCharPref(PREFBRANCH_APP_PARTNER + "test_partner1",
                                 "test_partner1");
  gDefaultPrefBranch.setCharPref(PREFBRANCH_APP_PARTNER + "test_partner2",
                                 "test_partner2");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt7() {
  Assert.equal(getResult(gRequestURL),
               "test_channel-cck-test_partner1-test_partner2",
               "the url param for %CHANNEL%" + MSG_SHOULD_EQUAL);
  run_test_pt8();
}

// url constructed with %PLATFORM_VERSION%
function run_test_pt8() {
  gCheckFunc = check_test_pt8;
  let url = URL_PREFIX + "%PLATFORM_VERSION%/";
  debugDump("testing url constructed with %PLATFORM_VERSION% - " + url);
  setUpdateURLOverride(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt8() {
  Assert.equal(getResult(gRequestURL), gAppInfo.platformVersion,
               "the url param for %PLATFORM_VERSION%" + MSG_SHOULD_EQUAL);
  run_test_pt9();
}

// url constructed with %OS_VERSION%
function run_test_pt9() {
  gCheckFunc = check_test_pt9;
  let url = URL_PREFIX + "%OS_VERSION%/";
  debugDump("testing url constructed with %OS_VERSION% - " + url);
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

  let kernel32 = ctypes.open("kernel32");
  try {
    let GetVersionEx = kernel32.declare("GetVersionExW",
                                        ctypes.default_abi,
                                        BOOL,
                                        OSVERSIONINFOEXW.ptr);
    let winVer = OSVERSIONINFOEXW();
    winVer.dwOSVersionInfoSize = OSVERSIONINFOEXW.size;

    if (0 === GetVersionEx(winVer.address())) {
      // Using "throw" instead of "do_throw" (see NOTE above)
      throw ("Failure in GetVersionEx (returned 0)");
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
    switch (sysInfo.wProcessorArchitecture) {
      case 9:
        return "x64";
      case 6:
        return "IA64";
      case 0:
        return "x86";
      default:
        // Using "throw" instead of "do_throw" (see NOTE above)
        throw ("Unknown architecture returned from GetNativeSystemInfo: " + sysInfo.wProcessorArchitecture);
    }
  } finally {
    kernel32.close();
  }
}

function check_test_pt9() {
  let osVersion;
  let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
  osVersion = sysInfo.getProperty("name") + " " + sysInfo.getProperty("version");

  if (IS_WIN) {
    try {
      let servicePack = getServicePack();
      osVersion += "." + servicePack;
    } catch (e) {
      do_throw("Failure obtaining service pack: " + e);
    }

    if ("5.0" === sysInfo.getProperty("version")) { // Win2K
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
    } catch (e) {
      // Not all platforms have a secondary widget library, so an error is
      // nothing to worry about.
    }
    osVersion = encodeURIComponent(osVersion);
  }

  Assert.equal(getResult(gRequestURL), osVersion,
               "the url param for %OS_VERSION%" + MSG_SHOULD_EQUAL);
  run_test_pt10();
}

// url constructed with %SYSTEM_CAPABILITIES%
function run_test_pt10() {
  gCheckFunc = check_test_pt10;
  let url = URL_PREFIX + "%SYSTEM_CAPABILITIES%/";
  debugDump("testing url constructed with %SYSTEM_CAPABILITIES% - " + url);
  setUpdateURLOverride(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

/**
 * Provides system capability information for application update though it may
 * be used by other consumers.
 */
function getSystemCapabilities() {
  if (IS_WIN) {
    const PF_MMX_INSTRUCTIONS_AVAILABLE = 3; // MMX
    const PF_XMMI_INSTRUCTIONS_AVAILABLE = 6; // SSE
    const PF_XMMI64_INSTRUCTIONS_AVAILABLE = 10; // SSE2
    const PF_SSE3_INSTRUCTIONS_AVAILABLE = 13; // SSE3

    let lib = ctypes.open("kernel32.dll");
    let IsProcessorFeaturePresent = lib.declare("IsProcessorFeaturePresent",
                                                ctypes.winapi_abi,
                                                ctypes.int32_t, /* success */
                                                ctypes.uint32_t); /* DWORD */
    let instructionSet = "unknown";
    try {
      if (IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE)) {
        instructionSet = "SSE3";
      } else if (IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE)) {
        instructionSet = "SSE2";
      } else if (IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE)) {
        instructionSet = "SSE";
      } else if (IsProcessorFeaturePresent(PF_MMX_INSTRUCTIONS_AVAILABLE)) {
        instructionSet = "MMX";
      }
    } catch (e) {
        Cu.reportError("Error getting processor instruction set. " +
                       "Exception: " + e);
    }

    lib.close();
    return instructionSet;
  }

  return "NA"
}

function check_test_pt10() {
  let systemCapabilities = "NA";
  if (IS_WIN) {
    systemCapabilities = getSystemCapabilities();
  }

  Assert.equal(getResult(gRequestURL), systemCapabilities,
               "the url param for %SYSTEM_CAPABILITIES%" + MSG_SHOULD_EQUAL);
  run_test_pt11();
}

// url constructed with %DISTRIBUTION%
function run_test_pt11() {
  gCheckFunc = check_test_pt11;
  let url = URL_PREFIX + "%DISTRIBUTION%/";
  debugDump("testing url constructed with %DISTRIBUTION% - " + url);
  setUpdateURLOverride(url);
  gDefaultPrefBranch.setCharPref(PREF_DISTRIBUTION_ID, "test_distro");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt11() {
  Assert.equal(getResult(gRequestURL), "test_distro",
               "the url param for %DISTRIBUTION%" + MSG_SHOULD_EQUAL);
  run_test_pt12();
}

// url constructed with %DISTRIBUTION_VERSION%
function run_test_pt12() {
  gCheckFunc = check_test_pt12;
  let url = URL_PREFIX + "%DISTRIBUTION_VERSION%/";
  debugDump("testing url constructed with %DISTRIBUTION_VERSION% - " + url);
  setUpdateURLOverride(url);
  gDefaultPrefBranch.setCharPref(PREF_DISTRIBUTION_VERSION, "test_distro_version");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt12() {
  Assert.equal(getResult(gRequestURL), "test_distro_version",
               "the url param for %DISTRIBUTION_VERSION%" + MSG_SHOULD_EQUAL);
  run_test_pt13();
}

// url with force param that doesn't already have a param - bug 454357
function run_test_pt13() {
  gCheckFunc = check_test_pt13;
  let url = URL_PREFIX;
  debugDump("testing url with force param that doesn't already have a " +
            "param - " + url);
  setUpdateURLOverride(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt13() {
  Assert.equal(getResult(gRequestURL), "?force=1",
               "the url query string for force" + MSG_SHOULD_EQUAL);
  run_test_pt14();
}

// url with force param that already has a param - bug 454357
function run_test_pt14() {
  gCheckFunc = check_test_pt14;
  let url = URL_PREFIX + "?extra=param";
  debugDump("testing url with force param that already has a param - " + url);
  setUpdateURLOverride(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt14() {
  Assert.equal(getResult(gRequestURL), "?extra=param&force=1",
               "the url query string for force with an extra string" +
               MSG_SHOULD_EQUAL);
  run_test_pt15();
}

function run_test_pt15() {
  Services.prefs.setCharPref("app.update.custom", "custom");
  gCheckFunc = check_test_pt15;
  let url = URL_PREFIX + "?custom=%CUSTOM%";
  debugDump("testing url constructed with %CUSTOM% - " + url);
  setUpdateURLOverride(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt15() {
  Assert.equal(getResult(gRequestURL), "?custom=custom&force=1",
               "the url query string for force with a custom string" +
               MSG_SHOULD_EQUAL);
  doTestFinish();
}
