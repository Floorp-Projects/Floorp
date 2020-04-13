/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["UpdateUtils"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { ctypes } = ChromeUtils.import("resource://gre/modules/ctypes.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "WindowsVersionInfo",
  "resource://gre/modules/components-utils/WindowsVersionInfo.jsm"
);
XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]); /* globals fetch */

ChromeUtils.defineModuleGetter(
  this,
  "WindowsRegistry",
  "resource://gre/modules/WindowsRegistry.jsm"
);

// The file that stores Application Update configuration settings. The file is
// located in the update directory which makes it a common setting across all
// application profiles and allows the Background Update Agent to read it.
const FILE_UPDATE_CONFIG_JSON = "update-config.json";
const FILE_UPDATE_LOCALE = "update.locale";
const PREF_APP_DISTRIBUTION = "distribution.id";
const PREF_APP_DISTRIBUTION_VERSION = "distribution.version";
// Do not use the PREF_APP_UPDATE_AUTO preference directly!
// Call getAppUpdateAutoEnabled or setAppUpdateAutoEnabled instead.
const PREF_APP_UPDATE_AUTO = "app.update.auto";
const PREF_APP_UPDATE_AUTO_MIGRATED = "app.update.auto.migrated";
// The setting name in the FILE_UPDATE_CONFIG_JSON file for whether the
// Application Update Service automatically downloads and installs updates.
const CONFIG_APP_UPDATE_AUTO = "app.update.auto";
// The default value for the CONFIG_APP_UPDATE_AUTO setting and the
// PREF_APP_UPDATE_AUTO preference.
const DEFAULT_APP_UPDATE_AUTO = true;

var UpdateUtils = {
  _locale: undefined,

  /**
   * Read the update channel from defaults only.  We do this to ensure that
   * the channel is tightly coupled with the application and does not apply
   * to other instances of the application that may use the same profile.
   *
   * @param [optional] aIncludePartners
   *        Whether or not to include the partner bits. Default: true.
   */
  getUpdateChannel(aIncludePartners = true) {
    let defaults = Services.prefs.getDefaultBranch(null);
    let channel = defaults.getCharPref(
      "app.update.channel",
      AppConstants.MOZ_UPDATE_CHANNEL
    );

    if (aIncludePartners) {
      try {
        let partners = Services.prefs.getChildList("app.partner.").sort();
        if (partners.length) {
          channel += "-cck";
          partners.forEach(function(prefName) {
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
  async formatUpdateURL(url) {
    const locale = await this.getLocale();

    return url
      .replace(/%(\w+)%/g, (match, name) => {
        switch (name) {
          case "PRODUCT":
            return Services.appinfo.name;
          case "VERSION":
            return Services.appinfo.version;
          case "BUILD_ID":
            return Services.appinfo.appBuildID;
          case "BUILD_TARGET":
            return Services.appinfo.OS + "_" + this.ABI;
          case "OS_VERSION":
            return this.OSVersion;
          case "LOCALE":
            return locale;
          case "CHANNEL":
            return this.UpdateChannel;
          case "PLATFORM_VERSION":
            return Services.appinfo.platformVersion;
          case "SYSTEM_CAPABILITIES":
            return getSystemCapabilities();
          case "DISTRIBUTION":
            return getDistributionPrefValue(PREF_APP_DISTRIBUTION);
          case "DISTRIBUTION_VERSION":
            return getDistributionPrefValue(PREF_APP_DISTRIBUTION_VERSION);
        }
        return match;
      })
      .replace(/\+/g, "%2B");
  },

  /**
   * Gets the locale from the update.locale file for replacing %LOCALE% in the
   * update url. The update.locale file can be located in the application
   * directory or the GRE directory with preference given to it being located in
   * the application directory.
   */
  async getLocale() {
    if (this._locale !== undefined) {
      return this._locale;
    }

    for (let res of ["app", "gre"]) {
      const url = "resource://" + res + "/" + FILE_UPDATE_LOCALE;
      let data;
      try {
        data = await fetch(url);
      } catch (e) {
        continue;
      }
      const locale = await data.text();
      if (locale) {
        return (this._locale = locale.trim());
      }
    }

    Cu.reportError(
      FILE_UPDATE_LOCALE +
        " file doesn't exist in either the " +
        "application or GRE directories"
    );

    return (this._locale = null);
  },

  /**
   * Determines whether or not the Application Update Service automatically
   * downloads and installs updates. This corresponds to whether or not the user
   * has selected "Automatically install updates" in about:preferences.
   *
   * On Windows, this setting is shared across all profiles for the installation
   * and is read asynchronously from the file. On other operating systems, this
   * setting is stored in a pref and is thus a per-profile setting.
   *
   * @return A Promise that resolves with a boolean.
   */
  getAppUpdateAutoEnabled() {
    if (Services.policies) {
      if (!Services.policies.isAllowed("app-auto-updates-off")) {
        // We aren't allowed to turn off auto-update - it is forced on.
        return Promise.resolve(true);
      }
      if (!Services.policies.isAllowed("app-auto-updates-on")) {
        // We aren't allowed to turn on auto-update - it is forced off.
        return Promise.resolve(false);
      }
    }
    if (AppConstants.platform != "win") {
      // On platforms other than Windows the setting is stored in a preference.
      let prefValue = Services.prefs.getBoolPref(
        PREF_APP_UPDATE_AUTO,
        DEFAULT_APP_UPDATE_AUTO
      );
      return Promise.resolve(prefValue);
    }
    // Justification for the empty catch statement below:
    // All promises returned by (get|set)AutoUpdateIsEnabled are part of a
    // single promise chain in order to serialize disk operations. We don't want
    // the entire promise chain to reject when one operation fails.
    //
    // There is only one situation when a promise in this chain should ever
    // reject, which is when writing fails and the error is logged and
    // re-thrown. All other possible exceptions are wrapped in try blocks, which
    // also log any exception that may occur.
    let readPromise = updateAutoIOPromise
      .catch(() => {})
      .then(async () => {
        try {
          let configValue = await readUpdateAutoConfig();
          // If we read a value out of this file, don't later perform migration.
          // If the file is deleted, we don't want some stale pref getting
          // written to it just because a different profile performed migration.
          Services.prefs.setBoolPref(PREF_APP_UPDATE_AUTO_MIGRATED, true);
          return configValue;
        } catch (e) {
          // Not being able to read from the app update configuration file is not
          // a serious issue so use logStringMessage to avoid concern from users.
          Services.console.logStringMessage(
            "UpdateUtils.getAppUpdateAutoEnabled - Unable to read app update " +
              "configuration file. Exception: " +
              e
          );
          let valueMigrated = Services.prefs.getBoolPref(
            PREF_APP_UPDATE_AUTO_MIGRATED,
            false
          );
          if (!valueMigrated) {
            Services.prefs.setBoolPref(PREF_APP_UPDATE_AUTO_MIGRATED, true);
            let prefValue = Services.prefs.getBoolPref(
              PREF_APP_UPDATE_AUTO,
              DEFAULT_APP_UPDATE_AUTO
            );
            try {
              let writtenValue = await writeUpdateAutoConfig(prefValue);
              Services.prefs.clearUserPref(PREF_APP_UPDATE_AUTO);
              return writtenValue;
            } catch (e) {
              Cu.reportError(
                "UpdateUtils.getAppUpdateAutoEnabled - Migration " +
                  "failed. Exception: " +
                  e
              );
            }
          }
        }
        // Fallthrough for if the value could not be read or migrated.
        return DEFAULT_APP_UPDATE_AUTO;
      })
      .then(maybeUpdateAutoConfigChanged);
    updateAutoIOPromise = readPromise;
    return readPromise;
  },

  /**
   * Toggles whether the Update Service automatically downloads and installs
   * updates. This effectively selects between the "Automatically install
   * updates" and "Check for updates but let you choose to install them" options
   * in about:preferences.
   *
   * On Windows, this setting is shared across all profiles for the installation
   * and is written asynchronously to the file. On other operating systems, this
   * setting is stored in a pref and is thus a per-profile setting.
   *
   * If this method is called when the setting is locked, the returned promise
   * will reject. The lock status can be determined with
   * UpdateUtils.appUpdateAutoSettingIsLocked()
   *
   * @param  enabled If set to true, automatic download and installation of
   *                 updates will be enabled. If set to false, this will be
   *                 disabled.
   * @return A Promise that, once the setting has been saved, resolves with the
   *         boolean value that was saved. If the setting could not be
   *         successfully saved, the Promise will reject.
   *         On Windows, where this setting is stored in a file, this Promise
   *         may reject with an I/O error.
   *         On other operating systems, this promise should not reject as
   *         this operation simply sets a pref.
   */
  setAppUpdateAutoEnabled(enabledValue) {
    if (this.appUpdateAutoSettingIsLocked()) {
      return Promise.reject(
        "setAppUpdateAutoEnabled: Unable to change value of setting because " +
          "it is locked by policy"
      );
    }
    if (AppConstants.platform != "win") {
      // Only in Windows do we store the update config in the update directory
      let prefValue = !!enabledValue;
      Services.prefs.setBoolPref(PREF_APP_UPDATE_AUTO, prefValue);
      // Rather than call maybeUpdateAutoConfigChanged, a pref observer has
      // been connected to PREF_APP_UPDATE_AUTO. This allows us to catch direct
      // changes to the pref (which Firefox shouldn't be doing, but the user
      // might do in about:config).
      return Promise.resolve(prefValue);
    }
    // Justification for the empty catch statement below:
    // All promises returned by (get|set)AutoUpdateIsEnabled are part of a
    // single promise chain in order to serialize disk operations. We don't want
    // the entire promise chain to reject when one operation fails.
    //
    // There is only one situation when a promise in this chain should ever
    // reject, which is when writing fails and the error is logged and
    // re-thrown. All other possible exceptions are wrapped in try blocks, which
    // also log any exception that may occur.
    let writePromise = updateAutoIOPromise
      .catch(() => {})
      .then(async () => {
        try {
          return await writeUpdateAutoConfig(enabledValue);
        } catch (e) {
          Cu.reportError(
            "UpdateUtils.setAppUpdateAutoEnabled - App update " +
              "configuration file write failed. Exception: " +
              e
          );
          // Rethrow the error so the caller knows that writing the value in the
          // app update config file failed.
          throw e;
        }
      })
      .then(maybeUpdateAutoConfigChanged);
    updateAutoIOPromise = writePromise;
    return writePromise;
  },

  /**
   * This function should be used to determine if the automatic application
   * update setting is locked by an enterprise policy
   *
   * @return true if the automatic update setting is currently locked.
   *         Otherwise, false.
   */
  appUpdateAutoSettingIsLocked() {
    return (
      Services.policies &&
      (!Services.policies.isAllowed("app-auto-updates-off") ||
        !Services.policies.isAllowed("app-auto-updates-on"))
    );
  },
};

// Used for serializing reads and writes of the app update json config file so
// the writes don't happen out of order and the last write is the one that
// the sets the value.
var updateAutoIOPromise = Promise.resolve();
var updateAutoSettingCachedVal = null;

async function readUpdateAutoConfig() {
  let configFile = FileUtils.getDir("UpdRootD", [], true);
  configFile.append(FILE_UPDATE_CONFIG_JSON);
  let binaryData = await OS.File.read(configFile.path);
  let jsonData = new TextDecoder().decode(binaryData);
  let configData = JSON.parse(jsonData);
  return !!configData[CONFIG_APP_UPDATE_AUTO];
}

async function writeUpdateAutoConfig(enabledValue) {
  let enabledBoolValue = !!enabledValue;
  let configFile = FileUtils.getDir("UpdRootD", [], true);
  configFile.append(FILE_UPDATE_CONFIG_JSON);
  let configObject = { [CONFIG_APP_UPDATE_AUTO]: enabledBoolValue };
  await OS.File.writeAtomic(configFile.path, JSON.stringify(configObject));
  return enabledBoolValue;
}

// Notifies observers if the value of app.update.auto has changed and returns
// the value for app.update.auto.
function maybeUpdateAutoConfigChanged(newValue) {
  if (newValue !== updateAutoSettingCachedVal) {
    updateAutoSettingCachedVal = newValue;
    Services.obs.notifyObservers(
      null,
      "auto-update-config-change",
      newValue.toString()
    );
  }
  return newValue;
}
// On non-Windows platforms, the Update Auto Config is still stored as a pref.
// On those platforms, the best way to notify observers of this setting is
// just to propagate it from a pref observer
if (AppConstants.platform != "win") {
  Services.prefs.addObserver(
    PREF_APP_UPDATE_AUTO,
    async (subject, topic, data) => {
      let value = await UpdateUtils.getAppUpdateAutoEnabled();
      maybeUpdateAutoConfigChanged(value);
    }
  );
}

/* Get the distribution pref values, from defaults only */
function getDistributionPrefValue(aPrefName) {
  let value = Services.prefs
    .getDefaultBranch(null)
    .getCharPref(aPrefName, "default");
  if (!value) {
    value = "default";
  }
  return value;
}

function getSystemCapabilities() {
  return "ISET:" + gInstructionSet + ",MEM:" + getMemoryMB();
}

/**
 * Gets the RAM size in megabytes. This will round the value because sysinfo
 * doesn't always provide RAM in multiples of 1024.
 */
function getMemoryMB() {
  let memoryMB = "unknown";
  try {
    memoryMB = Services.sysinfo.getProperty("memsize");
    if (memoryMB) {
      memoryMB = Math.round(memoryMB / 1024 / 1024);
    }
  } catch (e) {
    Cu.reportError(
      "Error getting system info memsize property. Exception: " + e
    );
  }
  return memoryMB;
}

/**
 * Gets the supported CPU instruction set.
 */
XPCOMUtils.defineLazyGetter(this, "gInstructionSet", function aus_gIS() {
  const CPU_EXTENSIONS = [
    "hasSSE4_2",
    "hasSSE4_1",
    "hasSSE4A",
    "hasSSSE3",
    "hasSSE3",
    "hasSSE2",
    "hasSSE",
    "hasMMX",
    "hasNEON",
    "hasARMv7",
    "hasARMv6",
  ];
  for (let ext of CPU_EXTENSIONS) {
    if (Services.sysinfo.getProperty(ext)) {
      return ext.substring(3);
    }
  }

  return "unknown";
});

/* Windows only getter that returns the processor architecture. */
XPCOMUtils.defineLazyGetter(this, "gWinCPUArch", function aus_gWinCPUArch() {
  // Get processor architecture
  let arch = "unknown";

  const WORD = ctypes.uint16_t;
  const DWORD = ctypes.uint32_t;

  // This structure is described at:
  // http://msdn.microsoft.com/en-us/library/ms724958%28v=vs.85%29.aspx
  const SYSTEM_INFO = new ctypes.StructType("SYSTEM_INFO", [
    { wProcessorArchitecture: WORD },
    { wReserved: WORD },
    { dwPageSize: DWORD },
    { lpMinimumApplicationAddress: ctypes.voidptr_t },
    { lpMaximumApplicationAddress: ctypes.voidptr_t },
    { dwActiveProcessorMask: DWORD.ptr },
    { dwNumberOfProcessors: DWORD },
    { dwProcessorType: DWORD },
    { dwAllocationGranularity: DWORD },
    { wProcessorLevel: WORD },
    { wProcessorRevision: WORD },
  ]);

  let kernel32 = false;
  try {
    kernel32 = ctypes.open("Kernel32");
  } catch (e) {
    Cu.reportError("Unable to open kernel32! Exception: " + e);
  }

  if (kernel32) {
    try {
      let GetNativeSystemInfo = kernel32.declare(
        "GetNativeSystemInfo",
        ctypes.winapi_abi,
        ctypes.void_t,
        SYSTEM_INFO.ptr
      );
      let winSystemInfo = SYSTEM_INFO();
      // Default to unknown
      winSystemInfo.wProcessorArchitecture = 0xffff;

      GetNativeSystemInfo(winSystemInfo.address());
      switch (winSystemInfo.wProcessorArchitecture) {
        case 12:
          arch = "aarch64";
          break;
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
      Cu.reportError("Error getting processor architecture. Exception: " + e);
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
  } catch (e) {
    Cu.reportError("XPCOM ABI unknown");
  }

  if (AppConstants.platform == "win") {
    // Windows build should report the CPU architecture that it's running on.
    abi += "-" + gWinCPUArch;
  }

  if (AppConstants.ASAN) {
    // Allow ASan builds to receive their own updates
    abi += "-asan";
  }

  return abi;
});

XPCOMUtils.defineLazyGetter(UpdateUtils, "OSVersion", function() {
  let osVersion;
  try {
    osVersion =
      Services.sysinfo.getProperty("name") +
      " " +
      Services.sysinfo.getProperty("version");
  } catch (e) {
    Cu.reportError("OS Version unknown.");
  }

  if (osVersion) {
    if (AppConstants.platform == "win") {
      // Add service pack and build number
      try {
        const {
          servicePackMajor,
          servicePackMinor,
          buildNumber,
        } = WindowsVersionInfo.get();
        osVersion += `.${servicePackMajor}.${servicePackMinor}.${buildNumber}`;
      } catch (err) {
        Cu.reportError(
          "Unable to retrieve windows version information: " + err
        );
        osVersion += ".unknown";
      }

      // add UBR if on Windows 10
      if (
        Services.vc.compare(Services.sysinfo.getProperty("version"), "10") >= 0
      ) {
        const WINDOWS_UBR_KEY_PATH =
          "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
        let ubr = WindowsRegistry.readRegKey(
          Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
          WINDOWS_UBR_KEY_PATH,
          "UBR",
          Ci.nsIWindowsRegKey.WOW64_64
        );
        if (ubr !== undefined) {
          osVersion += `.${ubr}`;
        } else {
          osVersion += ".unknown";
        }
      }

      // Add processor architecture
      osVersion += " (" + gWinCPUArch + ")";
    }

    try {
      osVersion +=
        " (" + Services.sysinfo.getProperty("secondaryLibrary") + ")";
    } catch (e) {
      // Not all platforms have a secondary widget library, so an error is nothing to worry about.
    }
    osVersion = encodeURIComponent(osVersion);
  }
  return osVersion;
});
