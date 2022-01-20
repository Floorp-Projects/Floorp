/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["Troubleshoot"];

const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { E10SUtils } = ChromeUtils.import(
  "resource://gre/modules/E10SUtils.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { FeatureGate } = ChromeUtils.import(
  "resource://featuregates/FeatureGate.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["DOMParser"]);

// We use a list of prefs for display to make sure we only show prefs that
// are useful for support and won't compromise the user's privacy.  Note that
// entries are *prefixes*: for example, "accessibility." applies to all prefs
// under the "accessibility.*" branch.
const PREFS_FOR_DISPLAY = [
  "accessibility.",
  "apz.",
  "browser.cache.",
  "browser.contentblocking.category",
  "browser.display.",
  "browser.download.folderList",
  "browser.download.lastDir.savePerSite",
  "browser.download.manager.addToRecentDocs",
  "browser.download.manager.resumeOnWakeDelay",
  "browser.download.preferred.",
  "browser.download.skipConfirmLaunchExecutable",
  "browser.download.useDownloadDir",
  "browser.fixup.",
  "browser.history_expire_",
  "browser.link.open_newwindow",
  "browser.places.",
  "browser.privatebrowsing.",
  "browser.search.context.loadInBackground",
  "browser.search.log",
  "browser.search.openintab",
  "browser.search.param",
  "browser.search.region",
  "browser.search.searchEnginesURL",
  "browser.search.suggest.enabled",
  "browser.search.update",
  "browser.sessionstore.",
  "browser.startup.homepage",
  "browser.startup.page",
  "browser.tabs.",
  "browser.urlbar.",
  "browser.zoom.",
  "doh-rollout.",
  "dom.",
  "extensions.checkCompatibility",
  "extensions.formautofill.",
  "extensions.lastAppVersion",
  "fission.autostart",
  "font.",
  "general.autoScroll",
  "general.useragent.",
  "gfx.",
  "html5.",
  "identity.fxaccounts.enabled",
  "idle.",
  "image.",
  "javascript.",
  "keyword.",
  "layers.",
  "layout.css.dpi",
  "layout.display-list.",
  "layout.frame_rate",
  "media.",
  "mousewheel.",
  "network.",
  "permissions.default.image",
  "places.",
  "plugin.",
  "plugins.",
  "privacy.",
  "security.",
  "services.sync.declinedEngines",
  "services.sync.lastPing",
  "services.sync.lastSync",
  "services.sync.numClients",
  "services.sync.engine.",
  "signon.",
  "storage.vacuum.last.",
  "svg.",
  "toolkit.startup.recent_crashes",
  "ui.osk.enabled",
  "ui.osk.detect_physical_keyboard",
  "ui.osk.require_tablet_mode",
  "ui.osk.debug.keyboardDisplayReason",
  "webgl.",
  "widget.dmabuf",
  "widget.use-xdg-desktop-portal",
  "widget.wayland",
];

// The list of prefs we don't display, unlike the list of prefs for display,
// is a list of regular expressions.
const PREF_REGEXES_NOT_TO_DISPLAY = [
  /^browser[.]fixup[.]domainwhitelist[.]/,
  /^dom[.]push[.]userAgentID/,
  /^media[.]webrtc[.]debug[.]aec_log_dir/,
  /^media[.]webrtc[.]debug[.]log_file/,
  /^print[.].*print_to_filename$/,
  /^network[.]proxy[.]/,
];

// Table of getters for various preference types.
const PREFS_GETTERS = {};

PREFS_GETTERS[Ci.nsIPrefBranch.PREF_STRING] = (prefs, name) =>
  prefs.getStringPref(name);
PREFS_GETTERS[Ci.nsIPrefBranch.PREF_INT] = (prefs, name) =>
  prefs.getIntPref(name);
PREFS_GETTERS[Ci.nsIPrefBranch.PREF_BOOL] = (prefs, name) =>
  prefs.getBoolPref(name);

// List of unimportant locked prefs (won't be shown on the troubleshooting
// session)
const PREFS_UNIMPORTANT_LOCKED = [
  "dom.postMessage.sharedArrayBuffer.bypassCOOP_COEP.insecure.enabled",
  "privacy.restrict3rdpartystorage.url_decorations",
];

function getPref(name) {
  let type = Services.prefs.getPrefType(name);
  if (!(type in PREFS_GETTERS)) {
    throw new Error("Unknown preference type " + type + " for " + name);
  }
  return PREFS_GETTERS[type](Services.prefs, name);
}

// Return the preferences filtered by PREF_REGEXES_NOT_TO_DISPLAY and PREFS_FOR_DISPLAY
// and also by the custom 'filter'-ing function.
function getPrefList(filter, allowlist = PREFS_FOR_DISPLAY) {
  return allowlist.reduce(function(prefs, branch) {
    Services.prefs.getChildList(branch).forEach(function(name) {
      if (
        filter(name) &&
        !PREF_REGEXES_NOT_TO_DISPLAY.some(re => re.test(name))
      ) {
        prefs[name] = getPref(name);
      }
    });
    return prefs;
  }, {});
}

var Troubleshoot = {
  /**
   * Captures a snapshot of data that may help troubleshooters troubleshoot
   * trouble.
   *
   * @param done A function that will be asynchronously called when the
   *             snapshot completes.  It will be passed the snapshot object.
   */
  snapshot: function snapshot(done) {
    let snapshot = {};
    let numPending = Object.keys(dataProviders).length;
    function providerDone(providerName, providerData) {
      snapshot[providerName] = providerData;
      if (--numPending == 0) {
        // Ensure that done is always and truly called asynchronously.
        Services.tm.dispatchToMainThread(done.bind(null, snapshot));
      }
    }
    for (let name in dataProviders) {
      try {
        dataProviders[name](providerDone.bind(null, name));
      } catch (err) {
        let msg = "Troubleshoot data provider failed: " + name + "\n" + err;
        Cu.reportError(msg);
        providerDone(name, msg);
      }
    }
  },

  kMaxCrashAge: 3 * 24 * 60 * 60 * 1000, // 3 days
};

// Each data provider is a name => function mapping.  When a snapshot is
// captured, each provider's function is called, and it's the function's job to
// generate the provider's data.  The function is passed a "done" callback, and
// when done, it must pass its data to the callback.  The resulting snapshot
// object will contain a name => data entry for each provider.
var dataProviders = {
  application: function application(done) {
    let data = {
      name: Services.appinfo.name,
      osVersion:
        Services.sysinfo.getProperty("name") +
        " " +
        Services.sysinfo.getProperty("version") +
        " " +
        Services.sysinfo.getProperty("build"),
      version: AppConstants.MOZ_APP_VERSION_DISPLAY,
      buildID: Services.appinfo.appBuildID,
      distributionID: Services.prefs
        .getDefaultBranch("")
        .getCharPref("distribution.id", ""),
      userAgent: Cc["@mozilla.org/network/protocol;1?name=http"].getService(
        Ci.nsIHttpProtocolHandler
      ).userAgent,
      safeMode: Services.appinfo.inSafeMode,
    };

    if (Services.sysinfo.getProperty("name") == "Windows_NT") {
      if (Services.sysinfo.processInfo.isWindowsSMode) {
        data.osVersion += " S";
      }
    }

    if (AppConstants.MOZ_UPDATER) {
      data.updateChannel = ChromeUtils.import(
        "resource://gre/modules/UpdateUtils.jsm",
        {}
      ).UpdateUtils.UpdateChannel;
    }

    // eslint-disable-next-line mozilla/use-default-preference-values
    try {
      data.vendor = Services.prefs.getCharPref("app.support.vendor");
    } catch (e) {}
    try {
      data.supportURL = Services.urlFormatter.formatURLPref(
        "app.support.baseURL"
      );
    } catch (e) {}

    data.osTheme = Services.sysinfo.getProperty("osThemeInfo");

    try {
      // MacOSX: Check for rosetta status, if it exists
      data.rosetta = Services.sysinfo.getProperty("rosettaStatus");
    } catch (e) {}

    data.numTotalWindows = 0;
    data.numFissionWindows = 0;
    data.numRemoteWindows = 0;
    for (let { docShell } of Services.wm.getEnumerator("navigator:browser")) {
      docShell.QueryInterface(Ci.nsILoadContext);
      data.numTotalWindows++;
      if (docShell.useRemoteSubframes) {
        data.numFissionWindows++;
      }
      if (docShell.useRemoteTabs) {
        data.numRemoteWindows++;
      }
    }

    try {
      data.launcherProcessState = Services.appinfo.launcherProcessState;
    } catch (e) {}

    data.fissionAutoStart = Services.appinfo.fissionAutostart;
    data.fissionDecisionStatus = Services.appinfo.fissionDecisionStatusString;

    data.remoteAutoStart = Services.appinfo.browserTabsRemoteAutostart;

    if (Services.policies) {
      data.policiesStatus = Services.policies.status;
    }

    const keyLocationServiceGoogle = Services.urlFormatter
      .formatURL("%GOOGLE_LOCATION_SERVICE_API_KEY%")
      .trim();
    data.keyLocationServiceGoogleFound =
      keyLocationServiceGoogle != "no-google-location-service-api-key" &&
      !!keyLocationServiceGoogle.length;

    const keySafebrowsingGoogle = Services.urlFormatter
      .formatURL("%GOOGLE_SAFEBROWSING_API_KEY%")
      .trim();
    data.keySafebrowsingGoogleFound =
      keySafebrowsingGoogle != "no-google-safebrowsing-api-key" &&
      !!keySafebrowsingGoogle.length;

    const keyMozilla = Services.urlFormatter
      .formatURL("%MOZILLA_API_KEY%")
      .trim();
    data.keyMozillaFound =
      keyMozilla != "no-mozilla-api-key" && !!keyMozilla.length;

    done(data);
  },

  addons: async function addons(done) {
    let addons = await AddonManager.getAddonsByTypes([
      "extension",
      "locale",
      "dictionary",
    ]);
    addons = addons.filter(e => !e.isSystem);
    addons.sort(function(a, b) {
      if (a.isActive != b.isActive) {
        return b.isActive ? 1 : -1;
      }

      if (a.type != b.type) {
        return a.type.localeCompare(b.type);
      }

      // In some unfortunate cases add-on names can be null.
      let aname = a.name || "";
      let bname = b.name || "";
      let lc = aname.localeCompare(bname);
      if (lc != 0) {
        return lc;
      }
      if (a.version != b.version) {
        return a.version > b.version ? 1 : -1;
      }
      return 0;
    });
    let props = ["name", "type", "version", "isActive", "id"];
    done(
      addons.map(function(ext) {
        return props.reduce(function(extData, prop) {
          extData[prop] = ext[prop];
          return extData;
        }, {});
      })
    );
  },

  securitySoftware: function securitySoftware(done) {
    let data = {};

    const keys = [
      "registeredAntiVirus",
      "registeredAntiSpyware",
      "registeredFirewall",
    ];
    for (let key of keys) {
      let prop = "";
      try {
        prop = Services.sysinfo.getProperty(key);
      } catch (e) {}

      data[key] = prop;
    }

    done(data);
  },

  features: async function features(done) {
    let features = await AddonManager.getAddonsByTypes(["extension"]);
    features = features.filter(f => f.isSystem);
    features.sort(function(a, b) {
      // In some unfortunate cases addon names can be null.
      let aname = a.name || null;
      let bname = b.name || null;
      let lc = aname.localeCompare(bname);
      if (lc != 0) {
        return lc;
      }
      if (a.version != b.version) {
        return a.version > b.version ? 1 : -1;
      }
      return 0;
    });
    let props = ["name", "version", "id"];
    done(
      features.map(function(f) {
        return props.reduce(function(fData, prop) {
          fData[prop] = f[prop];
          return fData;
        }, {});
      })
    );
  },

  processes: function processes(done) {
    let remoteTypes = {};

    for (let i = 0; i < Services.ppmm.childCount; i++) {
      let remoteType;
      try {
        remoteType = Services.ppmm.getChildAt(i).remoteType;
      } catch (e) {}

      // The parent process is also managed by the ppmm (because
      // of non-remote tabs), but it doesn't have a remoteType.
      if (!remoteType) {
        continue;
      }

      remoteType = E10SUtils.remoteTypePrefix(remoteType);

      if (remoteTypes[remoteType]) {
        remoteTypes[remoteType]++;
      } else {
        remoteTypes[remoteType] = 1;
      }
    }

    try {
      let winUtils = Services.wm.getMostRecentWindow("").windowUtils;
      if (winUtils.gpuProcessPid != -1) {
        remoteTypes.gpu = 1;
      }
    } catch (e) {}

    if (Services.io.socketProcessLaunched) {
      remoteTypes.socket = 1;
    }

    let data = {
      remoteTypes,
      maxWebContentProcesses: Services.appinfo.maxWebProcessCount,
    };

    done(data);
  },

  async experimentalFeatures(done) {
    if (AppConstants.platform == "android") {
      done();
      return;
    }
    let gates = await FeatureGate.all();
    done(
      gates.map(gate => {
        return [
          gate.title,
          gate.preference,
          Services.prefs.getBoolPref(gate.preference),
        ];
      })
    );
  },

  async environmentVariables(done) {
    let Subprocess;
    try {
      // Subprocess is not available in all builds
      Subprocess = ChromeUtils.import("resource://gre/modules/Subprocess.jsm")
        .Subprocess;
    } catch (ex) {
      done({});
      return;
    }

    let environment = Subprocess.getEnvironment();
    let filteredEnvironment = {};
    // Limit the environment variables to those that we
    // know may affect Firefox to reduce leaking PII.
    let filteredEnvironmentKeys = ["xre_", "moz_", "gdk", "display"];
    for (let key of Object.keys(environment)) {
      if (filteredEnvironmentKeys.some(k => key.toLowerCase().startsWith(k))) {
        filteredEnvironment[key] = environment[key];
      }
    }
    done(filteredEnvironment);
  },

  modifiedPreferences: function modifiedPreferences(done) {
    done(getPrefList(name => Services.prefs.prefHasUserValue(name)));
  },

  lockedPreferences: function lockedPreferences(done) {
    done(
      getPrefList(
        name =>
          !PREFS_UNIMPORTANT_LOCKED.includes(name) &&
          Services.prefs.prefIsLocked(name)
      )
    );
  },

  printingPreferences: function printingPreferences(done) {
    let filter = name => Services.prefs.prefHasUserValue(name);
    let prefs = getPrefList(filter, ["print."]);

    // print_printer is special and is the only pref that is outside of the
    // "print." branch... Maybe we should change it to print.printer or
    // something...
    if (filter("print_printer")) {
      prefs.print_printer = getPref("print_printer");
    }

    done(prefs);
  },

  graphics: function graphics(done) {
    function statusMsgForFeature(feature) {
      // We return an object because in the try-newer-driver case we need to
      // include the suggested version, which the consumer likely needs to plug
      // into a format string from a localization file. Rather than returning
      // a string in some cases and an object in others, return an object always.
      let msg = { key: "" };
      try {
        var status = gfxInfo.getFeatureStatus(feature);
      } catch (e) {}
      switch (status) {
        case Ci.nsIGfxInfo.FEATURE_BLOCKED_DEVICE:
        case Ci.nsIGfxInfo.FEATURE_DISCOURAGED:
          msg = { key: "blocked-gfx-card" };
          break;
        case Ci.nsIGfxInfo.FEATURE_BLOCKED_OS_VERSION:
          msg = { key: "blocked-os-version" };
          break;
        case Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION:
          try {
            var driverVersion = gfxInfo.getFeatureSuggestedDriverVersion(
              feature
            );
          } catch (e) {}
          msg = driverVersion
            ? { key: "try-newer-driver", args: { driverVersion } }
            : { key: "blocked-driver" };
          break;
        case Ci.nsIGfxInfo.FEATURE_BLOCKED_MISMATCHED_VERSION:
          msg = { key: "blocked-mismatched-version" };
          break;
      }
      return msg;
    }

    let data = {};

    try {
      // nsIGfxInfo may not be implemented on some platforms.
      var gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
    } catch (e) {}

    let promises = [];
    // done will be called upon all pending promises being resolved.
    // add your pending promise to promises when adding new ones.
    function completed() {
      Promise.all(promises).then(() => done(data));
    }

    data.numTotalWindows = 0;
    data.numAcceleratedWindows = 0;
    for (let win of Services.ww.getWindowEnumerator()) {
      let winUtils = win.windowUtils;
      try {
        // NOTE: windowless browser's windows should not be reported in the graphics troubleshoot report
        if (
          winUtils.layerManagerType == "None" ||
          !winUtils.layerManagerRemote
        ) {
          continue;
        }
        data.numTotalWindows++;
        data.windowLayerManagerType = winUtils.layerManagerType;
        data.windowLayerManagerRemote = winUtils.layerManagerRemote;
      } catch (e) {
        continue;
      }
      if (data.windowLayerManagerType != "Basic") {
        data.numAcceleratedWindows++;
      }
    }

    // If we had no OMTC windows, report back Basic Layers.
    if (!data.windowLayerManagerType) {
      data.windowLayerManagerType = "Basic";
      data.windowLayerManagerRemote = false;
    }

    if (!data.numAcceleratedWindows && gfxInfo) {
      let win = AppConstants.platform == "win";
      let feature = win
        ? gfxInfo.FEATURE_DIRECT3D_9_LAYERS
        : gfxInfo.FEATURE_OPENGL_LAYERS;
      data.numAcceleratedWindowsMessage = statusMsgForFeature(feature);
    }

    if (!gfxInfo) {
      completed();
      return;
    }

    // keys are the names of attributes on nsIGfxInfo, values become the names
    // of the corresponding properties in our data object.  A null value means
    // no change.  This is needed so that the names of properties in the data
    // object are the same as the names of keys in aboutSupport.properties.
    let gfxInfoProps = {
      adapterDescription: null,
      adapterVendorID: null,
      adapterDeviceID: null,
      adapterSubsysID: null,
      adapterRAM: null,
      adapterDriver: "adapterDrivers",
      adapterDriverVendor: "driverVendor",
      adapterDriverVersion: "driverVersion",
      adapterDriverDate: "driverDate",

      adapterDescription2: null,
      adapterVendorID2: null,
      adapterDeviceID2: null,
      adapterSubsysID2: null,
      adapterRAM2: null,
      adapterDriver2: "adapterDrivers2",
      adapterDriverVendor2: "driverVendor2",
      adapterDriverVersion2: "driverVersion2",
      adapterDriverDate2: "driverDate2",
      isGPU2Active: null,

      D2DEnabled: "direct2DEnabled",
      DWriteEnabled: "directWriteEnabled",
      DWriteVersion: "directWriteVersion",
      cleartypeParameters: "clearTypeParameters",
      TargetFrameRate: "targetFrameRate",
      windowProtocol: null,
      desktopEnvironment: null,
    };

    for (let prop in gfxInfoProps) {
      try {
        data[gfxInfoProps[prop] || prop] = gfxInfo[prop];
      } catch (e) {}
    }

    if ("direct2DEnabled" in data && !data.direct2DEnabled) {
      data.direct2DEnabledMessage = statusMsgForFeature(
        Ci.nsIGfxInfo.FEATURE_DIRECT2D
      );
    }

    let doc = new DOMParser().parseFromString("<html/>", "text/html");

    function GetWebGLInfo(data, keyPrefix, contextType) {
      data[keyPrefix + "Renderer"] = "-";
      data[keyPrefix + "Version"] = "-";
      data[keyPrefix + "DriverExtensions"] = "-";
      data[keyPrefix + "Extensions"] = "-";
      data[keyPrefix + "WSIInfo"] = "-";

      // //

      let canvas = doc.createElement("canvas");
      canvas.width = 1;
      canvas.height = 1;

      // //

      let creationError = null;

      canvas.addEventListener(
        "webglcontextcreationerror",

        function(e) {
          creationError = e.statusMessage;
        }
      );

      let gl = null;
      try {
        gl = canvas.getContext(contextType);
      } catch (e) {
        if (!creationError) {
          creationError = e.toString();
        }
      }
      if (!gl) {
        data[keyPrefix + "Renderer"] =
          creationError || "(no creation error info)";
        return;
      }

      // //

      data[keyPrefix + "Extensions"] = gl.getSupportedExtensions().join(" ");

      // //

      let ext = gl.getExtension("MOZ_debug");
      // This extension is unconditionally available to chrome. No need to check.
      let vendor = ext.getParameter(gl.VENDOR);
      let renderer = ext.getParameter(gl.RENDERER);

      data[keyPrefix + "Renderer"] = vendor + " -- " + renderer;
      data[keyPrefix + "Version"] = ext.getParameter(gl.VERSION);
      data[keyPrefix + "DriverExtensions"] = ext.getParameter(ext.EXTENSIONS);
      data[keyPrefix + "WSIInfo"] = ext.getParameter(ext.WSI_INFO);

      // //

      // Eagerly free resources.
      let loseExt = gl.getExtension("WEBGL_lose_context");
      if (loseExt) {
        loseExt.loseContext();
      }
    }

    GetWebGLInfo(data, "webgl1", "webgl");
    GetWebGLInfo(data, "webgl2", "webgl2");

    let infoInfo = gfxInfo.getInfo();
    if (infoInfo) {
      data.info = infoInfo;
    }

    let failureIndices = {};

    let failures = gfxInfo.getFailures(failureIndices);
    if (failures.length) {
      data.failures = failures;
      if (failureIndices.value.length == failures.length) {
        data.indices = failureIndices.value;
      }
    }

    data.featureLog = gfxInfo.getFeatureLog();
    data.crashGuards = gfxInfo.getActiveCrashGuards();

    completed();
  },

  media: function media(done) {
    function convertDevices(devices) {
      if (!devices) {
        return undefined;
      }
      let infos = [];
      for (let i = 0; i < devices.length; ++i) {
        let device = devices.queryElementAt(i, Ci.nsIAudioDeviceInfo);
        infos.push({
          name: device.name,
          groupId: device.groupId,
          vendor: device.vendor,
          type: device.type,
          state: device.state,
          preferred: device.preferred,
          supportedFormat: device.supportedFormat,
          defaultFormat: device.defaultFormat,
          maxChannels: device.maxChannels,
          defaultRate: device.defaultRate,
          maxRate: device.maxRate,
          minRate: device.minRate,
          maxLatency: device.maxLatency,
          minLatency: device.minLatency,
        });
      }
      return infos;
    }

    let data = {};
    let winUtils = Services.wm.getMostRecentWindow("").windowUtils;
    data.currentAudioBackend = winUtils.currentAudioBackend;
    data.currentMaxAudioChannels = winUtils.currentMaxAudioChannels;
    data.currentPreferredSampleRate = winUtils.currentPreferredSampleRate;
    data.audioOutputDevices = convertDevices(
      winUtils
        .audioDevices(Ci.nsIDOMWindowUtils.AUDIO_OUTPUT)
        .QueryInterface(Ci.nsIArray)
    );
    data.audioInputDevices = convertDevices(
      winUtils
        .audioDevices(Ci.nsIDOMWindowUtils.AUDIO_INPUT)
        .QueryInterface(Ci.nsIArray)
    );
    done(data);
  },

  accessibility: function accessibility(done) {
    let data = {};
    data.isActive = Services.appinfo.accessibilityEnabled;
    // eslint-disable-next-line mozilla/use-default-preference-values
    try {
      data.forceDisabled = Services.prefs.getIntPref(
        "accessibility.force_disabled"
      );
    } catch (e) {}
    data.handlerUsed = Services.appinfo.accessibleHandlerUsed;
    data.instantiator = Services.appinfo.accessibilityInstantiator;
    done(data);
  },

  startupCache: function startupCache(done) {
    const startupInfo = Cc["@mozilla.org/startupcacheinfo;1"].getService(
      Ci.nsIStartupCacheInfo
    );
    done({
      DiskCachePath: startupInfo.DiskCachePath,
      IgnoreDiskCache: startupInfo.IgnoreDiskCache,
      FoundDiskCacheOnInit: startupInfo.FoundDiskCacheOnInit,
      WroteToDiskCache: startupInfo.WroteToDiskCache,
    });
  },

  libraryVersions: function libraryVersions(done) {
    let data = {};
    let verInfo = Cc["@mozilla.org/security/nssversion;1"].getService(
      Ci.nsINSSVersion
    );
    for (let prop in verInfo) {
      let match = /^([^_]+)_((Min)?Version)$/.exec(prop);
      if (match) {
        let verProp = match[2][0].toLowerCase() + match[2].substr(1);
        data[match[1]] = data[match[1]] || {};
        data[match[1]][verProp] = verInfo[prop];
      }
    }
    done(data);
  },

  userJS: function userJS(done) {
    let userJSFile = Services.dirsvc.get("PrefD", Ci.nsIFile);
    userJSFile.append("user.js");
    done({
      exists: userJSFile.exists() && userJSFile.fileSize > 0,
    });
  },

  intl: function intl(done) {
    const osPrefs = Cc["@mozilla.org/intl/ospreferences;1"].getService(
      Ci.mozIOSPreferences
    );
    done({
      localeService: {
        requested: Services.locale.requestedLocales,
        available: Services.locale.availableLocales,
        supported: Services.locale.appLocalesAsBCP47,
        regionalPrefs: Services.locale.regionalPrefsLocales,
        defaultLocale: Services.locale.defaultLocale,
      },
      osPrefs: {
        systemLocales: osPrefs.systemLocales,
        regionalPrefsLocales: osPrefs.regionalPrefsLocales,
      },
    });
  },

  async normandy(done) {
    if (!AppConstants.MOZ_NORMANDY) {
      done();
      return;
    }

    const {
      PreferenceExperiments: NormandyPreferenceStudies,
    } = ChromeUtils.import("resource://normandy/lib/PreferenceExperiments.jsm");
    const { AddonStudies: NormandyAddonStudies } = ChromeUtils.import(
      "resource://normandy/lib/AddonStudies.jsm"
    );
    const {
      PreferenceRollouts: NormandyPreferenceRollouts,
    } = ChromeUtils.import("resource://normandy/lib/PreferenceRollouts.jsm");
    const { ExperimentManager } = ChromeUtils.import(
      "resource://nimbus/lib/ExperimentManager.jsm"
    );

    // Get Normandy data in parallel, and sort each group by slug.
    const [
      addonStudies,
      prefRollouts,
      prefStudies,
      nimbusExperiments,
      remoteConfigs,
    ] = await Promise.all(
      [
        NormandyAddonStudies.getAllActive(),
        NormandyPreferenceRollouts.getAllActive(),
        NormandyPreferenceStudies.getAllActive(),
        ExperimentManager.store
          .ready()
          .then(() => ExperimentManager.store.getAllActive()),
        ExperimentManager.store
          .ready()
          .then(() => ExperimentManager.store.getAllRemoteConfigs()),
      ].map(promise =>
        promise
          .catch(error => {
            Cu.reportError(error);
            return [];
          })
          .then(items => items.sort((a, b) => a.slug.localeCompare(b.slug)))
      )
    );

    done({
      addonStudies,
      prefRollouts,
      prefStudies,
      nimbusExperiments,
      remoteConfigs,
    });
  },
};

if (AppConstants.MOZ_CRASHREPORTER) {
  dataProviders.crashes = function crashes(done) {
    const { CrashReports } = ChromeUtils.import(
      "resource://gre/modules/CrashReports.jsm"
    );
    let reports = CrashReports.getReports();
    let now = new Date();
    let reportsNew = reports.filter(
      report => now - report.date < Troubleshoot.kMaxCrashAge
    );
    let reportsSubmitted = reportsNew.filter(report => !report.pending);
    let reportsPendingCount = reportsNew.length - reportsSubmitted.length;
    let data = { submitted: reportsSubmitted, pending: reportsPendingCount };
    done(data);
  };
}

if (AppConstants.MOZ_SANDBOX) {
  dataProviders.sandbox = function sandbox(done) {
    let data = {};
    if (AppConstants.unixstyle == "linux") {
      const keys = [
        "hasSeccompBPF",
        "hasSeccompTSync",
        "hasPrivilegedUserNamespaces",
        "hasUserNamespaces",
        "canSandboxContent",
        "canSandboxMedia",
      ];

      for (let key of keys) {
        if (Services.sysinfo.hasKey(key)) {
          data[key] = Services.sysinfo.getPropertyAsBool(key);
        }
      }

      let reporter = Cc["@mozilla.org/sandbox/syscall-reporter;1"].getService(
        Ci.mozISandboxReporter
      );
      const snapshot = reporter.snapshot();
      let syscalls = [];
      for (let index = snapshot.begin; index < snapshot.end; ++index) {
        let report = snapshot.getElement(index);
        let { msecAgo, pid, tid, procType, syscall } = report;
        let args = [];
        for (let i = 0; i < report.numArgs; ++i) {
          args.push(report.getArg(i));
        }
        syscalls.push({ index, msecAgo, pid, tid, procType, syscall, args });
      }
      data.syscallLog = syscalls;
    }

    if (AppConstants.MOZ_SANDBOX) {
      let sandboxSettings = Cc[
        "@mozilla.org/sandbox/sandbox-settings;1"
      ].getService(Ci.mozISandboxSettings);
      data.contentSandboxLevel = Services.prefs.getIntPref(
        "security.sandbox.content.level"
      );
      data.effectiveContentSandboxLevel =
        sandboxSettings.effectiveContentSandboxLevel;
      data.contentWin32kLockdownState =
        sandboxSettings.contentWin32kLockdownStateString;
    }

    done(data);
  };
}

if (AppConstants.ENABLE_WEBDRIVER) {
  dataProviders.remoteAgent = function remoteAgent(done) {
    const { RemoteAgent } = ChromeUtils.import(
      "chrome://remote/content/components/RemoteAgent.jsm"
    );
    const { listening, scheme, host, port } = RemoteAgent;
    let url = "";
    if (listening) {
      url = `${scheme}://${host}:${port}/`;
    }
    done({ listening, url });
  };
}
