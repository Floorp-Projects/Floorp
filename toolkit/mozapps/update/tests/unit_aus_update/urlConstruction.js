/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General URL Construction Tests */

const URL_PREFIX = URL_HOST + "/";

var gAppInfo;

// Since gUpdateChecker.checkForUpdates uses XMLHttpRequest and XMLHttpRequest
// can be slow it combines the checks whenever possible.
function run_test() {
  // This test needs access to omni.ja to read the update.locale file so don't
  // use a custom directory for the application directory.
  gUseTestAppDir = false;
  setupTestCommon();

  standardInit();
  gAppInfo = Cc["@mozilla.org/xre/app-info;1"].
             getService(Ci.nsIXULAppInfo).
             QueryInterface(Ci.nsIXULRuntime);
  do_execute_soon(run_test_pt1);
}


// url constructed with:
// %PRODUCT%
// %VERSION%
// %BUILD_ID%
// %BUILD_TARGET%
// %LOCALE%
// %CHANNEL%
// %PLATFORM_VERSION%
// %OS_VERSION%
// %SYSTEM_CAPABILITIES%
// %DISTRIBUTION%
// %DISTRIBUTION_VERSION%
function run_test_pt1() {
  gCheckFunc = check_test_pt1;
  // The code that gets the locale accesses the profile which is only available
  // after calling do_get_profile in xpcshell tests. This prevents an error from
  // being logged.
  do_get_profile();

  setUpdateChannel("test_channel");
  gDefaultPrefBranch.setCharPref(PREF_DISTRIBUTION_ID, "test_distro");
  gDefaultPrefBranch.setCharPref(PREF_DISTRIBUTION_VERSION, "test_distro_version");

  let url = URL_PREFIX + "%PRODUCT%/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/" +
            "%LOCALE%/%CHANNEL%/%PLATFORM_VERSION%/%OS_VERSION%/" +
            "%SYSTEM_CAPABILITIES%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/" +
            "updates.xml";
  debugDump("testing url construction - url: " + url);
  setUpdateURLOverride(url);
  try {
    gUpdateChecker.checkForUpdates(updateCheckListener, true);
  } catch (e) {
    debugDump("The following error is most likely due to a missing " +
              "update.locale file");
    do_throw(e);
  }
}

function check_test_pt1() {
  let url = URL_PREFIX + gAppInfo.name + "/" + gAppInfo.version + "/" +
            gAppInfo.appBuildID + "/" + gAppInfo.OS + "_" + getABI() + "/" +
            INSTALL_LOCALE + "/test_channel/" + gAppInfo.platformVersion + "/" +
            getOSVersion() + "/" + getSystemCapabilities() +
            "/test_distro/test_distro_version/updates.xml?force=1"
  // Log the urls since Assert.equal won't print the entire urls to the log.
  if (gRequestURL != url) {
    logTestInfo("expected url: " + url);
    logTestInfo("returned url: " + gRequestURL);
  }
  Assert.equal(gRequestURL, url,
               "the url" + MSG_SHOULD_EQUAL);
  run_test_pt2();
}

// url constructed with:
// %CHANNEL% with distribution partners
// %CUSTOM% parameter
// force param when there already is a param - bug 454357
function run_test_pt2() {
  gCheckFunc = check_test_pt2;
  let url = URL_PREFIX + "%CHANNEL%/updates.xml?custom=%CUSTOM%";
  debugDump("testing url constructed with %CHANNEL% - " + url);
  setUpdateURLOverride(url);
  gDefaultPrefBranch.setCharPref(PREFBRANCH_APP_PARTNER + "test_partner1",
                                 "test_partner1");
  gDefaultPrefBranch.setCharPref(PREFBRANCH_APP_PARTNER + "test_partner2",
                                 "test_partner2");
  Services.prefs.setCharPref("app.update.custom", "custom");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt2() {
  let url = URL_PREFIX + "test_channel-cck-test_partner1-test_partner2/" +
            "updates.xml?custom=custom&force=1";
  Assert.equal(gRequestURL, url,
               "the url" + MSG_SHOULD_EQUAL);
  doTestFinish();
}

function getABI() {
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
  return abi;
}

function getOSVersion() {
  let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
  let osVersion = sysInfo.getProperty("name") + " " + sysInfo.getProperty("version");

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
  return osVersion
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
