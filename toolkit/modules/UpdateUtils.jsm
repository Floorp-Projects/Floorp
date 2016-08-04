/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["UpdateUtils"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/ctypes.jsm");

const FILE_UPDATE_LOCALE                  = "update.locale";
const PREF_APP_DISTRIBUTION               = "distribution.id";
const PREF_APP_DISTRIBUTION_VERSION       = "distribution.version";
const PREF_APP_B2G_VERSION                = "b2g.version";
const PREF_APP_UPDATE_CUSTOM              = "app.update.custom";
const PREF_APP_UPDATE_IMEI_HASH           = "app.update.imei_hash";


this.UpdateUtils = {
  /**
   * Read the update channel from defaults only.  We do this to ensure that
   * the channel is tightly coupled with the application and does not apply
   * to other instances of the application that may use the same profile.
   *
   * @param [optional] aIncludePartners
   *        Whether or not to include the partner bits. Default: true.
   */
  getUpdateChannel(aIncludePartners = true) {
    let channel = AppConstants.MOZ_UPDATE_CHANNEL;
    let defaults = Services.prefs.getDefaultBranch(null);
    try {
      channel = defaults.getCharPref("app.update.channel");
    } catch (e) {
      // use default value when pref not found
    }

    if (aIncludePartners) {
      try {
        let partners = Services.prefs.getChildList("app.partner.").sort();
        if (partners.length) {
          channel += "-cck";
          partners.forEach(function (prefName) {
            channel += "-" + Services.prefs.getCharPref(prefName);
          });
        }
      } catch (e) {
        Cu.reportError(e);
      }
    }

    return channel;
  },

  get UpdateChannel() {
    return this.getUpdateChannel();
  },

  /**
   * Formats a URL by replacing %...% values with OS, build and locale specific
   * values.
   *
   * @param  url
   *         The URL to format.
   * @return The formatted URL.
   */
  formatUpdateURL(url) {
    url = url.replace(/%PRODUCT%/g, Services.appinfo.name);
    url = url.replace(/%VERSION%/g, Services.appinfo.version);
    url = url.replace(/%BUILD_ID%/g, Services.appinfo.appBuildID);
    url = url.replace(/%BUILD_TARGET%/g, Services.appinfo.OS + "_" + this.ABI);
    url = url.replace(/%OS_VERSION%/g, this.OSVersion);
    url = url.replace(/%SYSTEM_CAPABILITIES%/g, gSystemCapabilities);
    if (/%LOCALE%/.test(url)) {
      url = url.replace(/%LOCALE%/g, this.Locale);
    }
    url = url.replace(/%CHANNEL%/g, this.UpdateChannel);
    url = url.replace(/%PLATFORM_VERSION%/g, Services.appinfo.platformVersion);
    url = url.replace(/%DISTRIBUTION%/g,
                      getDistributionPrefValue(PREF_APP_DISTRIBUTION));
    url = url.replace(/%DISTRIBUTION_VERSION%/g,
                      getDistributionPrefValue(PREF_APP_DISTRIBUTION_VERSION));
    url = url.replace(/%CUSTOM%/g, Preferences.get(PREF_APP_UPDATE_CUSTOM, ""));
    url = url.replace(/\+/g, "%2B");

    if (AppConstants.platform == "gonk") {
      let sysLibs = {};
      Cu.import("resource://gre/modules/systemlibs.js", sysLibs);
      let productDevice = sysLibs.libcutils.property_get("ro.product.device");
      let buildType = sysLibs.libcutils.property_get("ro.build.type");
      url = url.replace(/%PRODUCT_MODEL%/g,
                        sysLibs.libcutils.property_get("ro.product.model"));
      if (buildType == "user" || buildType == "userdebug") {
        url = url.replace(/%PRODUCT_DEVICE%/g, productDevice);
      } else {
        url = url.replace(/%PRODUCT_DEVICE%/g, productDevice + "-" + buildType);
      }
      url = url.replace(/%B2G_VERSION%/g,
                        Preferences.get(PREF_APP_B2G_VERSION, null));
      url = url.replace(/%IMEI%/g,
                        Preferences.get(PREF_APP_UPDATE_IMEI_HASH, "default"));
    }

    return url;
  }
};

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
 * Gets the locale from the update.locale file for replacing %LOCALE% in the
 * update url. The update.locale file can be located in the application
 * directory or the GRE directory with preference given to it being located in
 * the application directory.
 */
XPCOMUtils.defineLazyGetter(UpdateUtils, "Locale", function() {
  let channel;
  let locale;
  for (let res of ['app', 'gre']) {
    channel = NetUtil.newChannel({
      uri: "resource://" + res + "/" + FILE_UPDATE_LOCALE,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_INTERNAL_XMLHTTPREQUEST,
      loadUsingSystemPrincipal: true
    });
    try {
      let inputStream = channel.open2();
      locale = NetUtil.readInputStreamToString(inputStream, inputStream.available());
    } catch (e) {}
    if (locale)
      return locale.trim();
  }

  Cu.reportError(FILE_UPDATE_LOCALE + " file doesn't exist in either the " +
                 "application or GRE directories");

  return null;
});

/**
 * Provides adhoc system capability information for application update.
 */
XPCOMUtils.defineLazyGetter(this, "gSystemCapabilities", function aus_gSC() {
  if (AppConstants.platform == "win") {
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
      instructionSet = "error";
      Cu.reportError("Error getting processor instruction set. " +
                     "Exception: " + e);
    }

    lib.close();
    return instructionSet;
  }

  return "NA"
});

/* Windows only getter that returns the processor architecture. */
XPCOMUtils.defineLazyGetter(this, "gWinCPUArch", function aus_gWinCPUArch() {
  // Get processor architecture
  let arch = "unknown";

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

  let kernel32 = false;
  try {
    kernel32 = ctypes.open("Kernel32");
  } catch (e) {
    Cu.reportError("Unable to open kernel32! Exception: " + e);
  }

  if (kernel32) {
    try {
      let GetNativeSystemInfo = kernel32.declare("GetNativeSystemInfo",
                                                 ctypes.default_abi,
                                                 ctypes.void_t,
                                                 SYSTEM_INFO.ptr);
      let winSystemInfo = SYSTEM_INFO();
      // Default to unknown
      winSystemInfo.wProcessorArchitecture = 0xffff;

      GetNativeSystemInfo(winSystemInfo.address());
      switch (winSystemInfo.wProcessorArchitecture) {
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
      Cu.reportError("Error getting processor architecture. " +
                     "Exception: " + e);
    } finally {
      kernel32.close();
    }
  }

  return arch;
});

XPCOMUtils.defineLazyGetter(UpdateUtils, "ABI", function() {
  let abi = null;
  try {
    abi = Services.appinfo.XPCOMABI;
  }
  catch (e) {
    Cu.reportError("XPCOM ABI unknown");
  }

  if (AppConstants.platform == "macosx") {
    // Mac universal build should report a different ABI than either macppc
    // or mactel.
    let macutils = Cc["@mozilla.org/xpcom/mac-utils;1"].
                   getService(Ci.nsIMacUtils);

    if (macutils.isUniversalBinary) {
      abi += "-u-" + macutils.architecturesInBinary;
    }
  } else if (AppConstants.platform == "win") {
    // Windows build should report the CPU architecture that it's running on.
    abi += "-" + gWinCPUArch;
  }
  return abi;
});

XPCOMUtils.defineLazyGetter(UpdateUtils, "OSVersion", function() {
  let osVersion;
  try {
    osVersion = Services.sysinfo.getProperty("name") + " " +
                Services.sysinfo.getProperty("version");
  }
  catch (e) {
    Cu.reportError("OS Version unknown.");
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
        Cu.reportError("Unable to open kernel32! " + e);
        osVersion += ".unknown (unknown)";
      }

      if (kernel32) {
        try {
          // Get Service pack info
          try {
            let GetVersionEx = kernel32.declare("GetVersionExW",
                                                ctypes.default_abi,
                                                BOOL,
                                                OSVERSIONINFOEXW.ptr);
            let winVer = OSVERSIONINFOEXW();
            winVer.dwOSVersionInfoSize = OSVERSIONINFOEXW.size;

            if (0 !== GetVersionEx(winVer.address())) {
              osVersion += "." + winVer.wServicePackMajor +
                           "." + winVer.wServicePackMinor;
            } else {
              Cu.reportError("Unknown failure in GetVersionEX (returned 0)");
              osVersion += ".unknown";
            }
          } catch (e) {
            Cu.reportError("Error getting service pack information. Exception: " + e);
            osVersion += ".unknown";
          }
        } finally {
          kernel32.close();
        }

        // Add processor architecture
        osVersion += " (" + gWinCPUArch + ")";
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
