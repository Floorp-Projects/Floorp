/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/UpdateUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://testing-common/AppInfo.jsm");
Cu.import("resource://gre/modules/ctypes.jsm");

const PREF_APP_UPDATE_CHANNEL     = "app.update.channel";
const PREF_APP_PARTNER_BRANCH     = "app.partner.";
const PREF_DISTRIBUTION_ID        = "distribution.id";
const PREF_DISTRIBUTION_VERSION   = "distribution.version";

const URL_PREFIX = "http://localhost/";

const MSG_SHOULD_EQUAL = " should equal the expected value";

updateAppInfo();
const gAppInfo = getAppInfo();
const gDefaultPrefBranch = Services.prefs.getDefaultBranch(null);

function setUpdateChannel(aChannel) {
  gDefaultPrefBranch.setCharPref(PREF_APP_UPDATE_CHANNEL, aChannel);
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

// Helper function for formatting a url and getting the result we're
// interested in
function getResult(url) {
  url = UpdateUtils.formatUpdateURL(url);
  return url.substr(URL_PREFIX.length).split("/")[0];
}

// url constructed with %PRODUCT%
add_task(function* test_product() {
  let url = URL_PREFIX + "%PRODUCT%/";
  Assert.equal(getResult(url), gAppInfo.name,
               "the url param for %PRODUCT%" + MSG_SHOULD_EQUAL);
});

// url constructed with %VERSION%
add_task(function* test_version() {
  let url = URL_PREFIX + "%VERSION%/";
  Assert.equal(getResult(url), gAppInfo.version,
               "the url param for %VERSION%" + MSG_SHOULD_EQUAL);
});

// url constructed with %BUILD_ID%
add_task(function* test_build_id() {
  let url = URL_PREFIX + "%BUILD_ID%/";
  Assert.equal(getResult(url), gAppInfo.appBuildID,
               "the url param for %BUILD_ID%" + MSG_SHOULD_EQUAL);
});

// url constructed with %BUILD_TARGET%
// XXX TODO - it might be nice if we tested the actual ABI
add_task(function* test_build_target() {
  let url = URL_PREFIX + "%BUILD_TARGET%/";

  let abi;
  try {
    abi = gAppInfo.XPCOMABI;
  } catch (e) {
    do_throw("nsIXULAppInfo:XPCOMABI not defined\n");
  }

  if (AppConstants.platform == "macosx") {
    // Mac universal build should report a different ABI than either macppc
    // or mactel. This is necessary since nsUpdateService.js will set the ABI to
    // Universal-gcc3 for Mac universal builds.
    let macutils = Cc["@mozilla.org/xpcom/mac-utils;1"].
                   getService(Ci.nsIMacUtils);

    if (macutils.isUniversalBinary) {
      abi += "-u-" + macutils.architecturesInBinary;
    }
  } else if (AppConstants.platform == "win") {
    // Windows build should report the CPU architecture that it's running on.
    abi += "-" + getProcArchitecture();
  }

  Assert.equal(getResult(url), gAppInfo.OS + "_" + abi,
               "the url param for %BUILD_TARGET%" + MSG_SHOULD_EQUAL);
});

// url constructed with %LOCALE%
// Bug 488936 added the update.locale file that stores the update locale
add_task(function* test_locale() {
  // The code that gets the locale accesses the profile which is only available
  // after calling do_get_profile in xpcshell tests. This prevents an error from
  // being logged.
  do_get_profile();

  let url = URL_PREFIX + "%LOCALE%/";
  Assert.equal(getResult(url), AppConstants.INSTALL_LOCALE,
               "the url param for %LOCALE%" + MSG_SHOULD_EQUAL);
});

// url constructed with %CHANNEL%
add_task(function* test_channel() {
  let url = URL_PREFIX + "%CHANNEL%/";
  setUpdateChannel("test_channel");
  Assert.equal(getResult(url), "test_channel",
               "the url param for %CHANNEL%" + MSG_SHOULD_EQUAL);
});

// url constructed with %CHANNEL% with distribution partners
add_task(function* test_channel_distribution() {
  let url = URL_PREFIX + "%CHANNEL%/";
  gDefaultPrefBranch.setCharPref(PREF_APP_PARTNER_BRANCH + "test_partner1",
                                 "test_partner1");
  gDefaultPrefBranch.setCharPref(PREF_APP_PARTNER_BRANCH + "test_partner2",
                                 "test_partner2");
  Assert.equal(getResult(url),
               "test_channel-cck-test_partner1-test_partner2",
               "the url param for %CHANNEL%" + MSG_SHOULD_EQUAL);
});

// url constructed with %PLATFORM_VERSION%
add_task(function* test_platform_version() {
  let url = URL_PREFIX + "%PLATFORM_VERSION%/";
  Assert.equal(getResult(url), gAppInfo.platformVersion,
               "the url param for %PLATFORM_VERSION%" + MSG_SHOULD_EQUAL);
});

// url constructed with %OS_VERSION%
add_task(function* test_os_version() {
  let url = URL_PREFIX + "%OS_VERSION%/";
  let osVersion;
  let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
  osVersion = sysInfo.getProperty("name") + " " + sysInfo.getProperty("version");

  if (AppConstants.platform == "win") {
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

  Assert.equal(getResult(url), osVersion,
               "the url param for %OS_VERSION%" + MSG_SHOULD_EQUAL);
});

// url constructed with %DISTRIBUTION%
add_task(function* test_distribution() {
  let url = URL_PREFIX + "%DISTRIBUTION%/";
  gDefaultPrefBranch.setCharPref(PREF_DISTRIBUTION_ID, "test_distro");
  Assert.equal(getResult(url), "test_distro",
               "the url param for %DISTRIBUTION%" + MSG_SHOULD_EQUAL);
});

// url constructed with %DISTRIBUTION_VERSION%
add_task(function* test_distribution_version() {
  let url = URL_PREFIX + "%DISTRIBUTION_VERSION%/";
  gDefaultPrefBranch.setCharPref(PREF_DISTRIBUTION_VERSION, "test_distro_version");
  Assert.equal(getResult(url), "test_distro_version",
               "the url param for %DISTRIBUTION_VERSION%" + MSG_SHOULD_EQUAL);
});

add_task(function* test_custom() {
  Services.prefs.setCharPref("app.update.custom", "custom");
  let url = URL_PREFIX + "%CUSTOM%/";
  Assert.equal(getResult(url), "custom",
               "the url query string for %CUSTOM%" + MSG_SHOULD_EQUAL);
});
