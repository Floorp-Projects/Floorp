/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [
  "Troubleshoot",
];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");

var Experiments;
try {
  Experiments = Cu.import("resource:///modules/experiments/Experiments.jsm").Experiments;
}
catch (e) {
}

// We use a preferences whitelist to make sure we only show preferences that
// are useful for support and won't compromise the user's privacy.  Note that
// entries are *prefixes*: for example, "accessibility." applies to all prefs
// under the "accessibility.*" branch.
const PREFS_WHITELIST = [
  "accessibility.",
  "apz.",
  "browser.cache.",
  "browser.display.",
  "browser.download.folderList",
  "browser.download.hide_plugins_without_extensions",
  "browser.download.importedFromSqlite",
  "browser.download.lastDir.savePerSite",
  "browser.download.manager.addToRecentDocs",
  "browser.download.manager.alertOnEXEOpen",
  "browser.download.manager.closeWhenDone",
  "browser.download.manager.displayedHistoryDays",
  "browser.download.manager.quitBehavior",
  "browser.download.manager.resumeOnWakeDelay",
  "browser.download.manager.retention",
  "browser.download.manager.scanWhenDone",
  "browser.download.manager.showAlertOnComplete",
  "browser.download.manager.showWhenStarting",
  "browser.download.preferred.",
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
  "browser.search.searchEnginesURL",
  "browser.search.suggest.enabled",
  "browser.search.update",
  "browser.search.useDBForOrder",
  "browser.sessionstore.",
  "browser.startup.homepage",
  "browser.tabs.",
  "browser.urlbar.",
  "browser.zoom.",
  "dom.",
  "extensions.checkCompatibility",
  "extensions.lastAppVersion",
  "font.",
  "general.autoScroll",
  "general.useragent.",
  "gfx.",
  "html5.",
  "image.",
  "javascript.",
  "keyword.",
  "layers.",
  "layout.css.dpi",
  "media.",
  "mousewheel.",
  "network.",
  "permissions.default.image",
  "places.",
  "plugin.",
  "plugins.",
  "print.",
  "privacy.",
  "security.",
  "services.sync.declinedEngines",
  "services.sync.lastPing",
  "services.sync.lastSync",
  "services.sync.numClients",
  "services.sync.engine.",
  "social.enabled",
  "storage.vacuum.last.",
  "svg.",
  "toolkit.startup.recent_crashes",
  "ui.osk.enabled",
  "ui.osk.detect_physical_keyboard",
  "ui.osk.require_tablet_mode",
  "ui.osk.debug.keyboardDisplayReason",
  "webgl.",
];

// The blacklist, unlike the whitelist, is a list of regular expressions.
const PREFS_BLACKLIST = [
  /^network[.]proxy[.]/,
  /[.]print_to_filename$/,
  /^print[.]macosx[.]pagesetup/,
];

// Table of getters for various preference types.
// It's important to use getComplexValue for strings: it returns Unicode (wchars), getCharPref returns UTF-8 encoded chars.
const PREFS_GETTERS = {};

PREFS_GETTERS[Ci.nsIPrefBranch.PREF_STRING] = (prefs, name) => prefs.getComplexValue(name, Ci.nsISupportsString).data;
PREFS_GETTERS[Ci.nsIPrefBranch.PREF_INT] = (prefs, name) => prefs.getIntPref(name);
PREFS_GETTERS[Ci.nsIPrefBranch.PREF_BOOL] = (prefs, name) => prefs.getBoolPref(name);

// Return the preferences filtered by PREFS_BLACKLIST and PREFS_WHITELIST lists
// and also by the custom 'filter'-ing function.
function getPrefList(filter) {
  filter = filter || (name => true);
  function getPref(name) {
    let type = Services.prefs.getPrefType(name);
    if (!(type in PREFS_GETTERS))
      throw new Error("Unknown preference type " + type + " for " + name);
    return PREFS_GETTERS[type](Services.prefs, name);
  }

  return PREFS_WHITELIST.reduce(function(prefs, branch) {
    Services.prefs.getChildList(branch).forEach(function(name) {
      if (filter(name) && !PREFS_BLACKLIST.some(re => re.test(name)))
        prefs[name] = getPref(name);
    });
    return prefs;
  }, {});
}

this.Troubleshoot = {

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
      if (--numPending == 0)
        // Ensure that done is always and truly called asynchronously.
        Services.tm.mainThread.dispatch(done.bind(null, snapshot),
                                        Ci.nsIThread.DISPATCH_NORMAL);
    }
    for (let name in dataProviders) {
      try {
        dataProviders[name](providerDone.bind(null, name));
      }
      catch (err) {
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

    let sysInfo = Cc["@mozilla.org/system-info;1"].
                  getService(Ci.nsIPropertyBag2);

    let data = {
      name: Services.appinfo.name,
      osVersion: sysInfo.getProperty("name") + " " + sysInfo.getProperty("version"),
      version: AppConstants.MOZ_APP_VERSION_DISPLAY,
      buildID: Services.appinfo.appBuildID,
      userAgent: Cc["@mozilla.org/network/protocol;1?name=http"].
                 getService(Ci.nsIHttpProtocolHandler).
                 userAgent,
      safeMode: Services.appinfo.inSafeMode,
    };

    if (AppConstants.MOZ_UPDATER)
      data.updateChannel = Cu.import("resource://gre/modules/UpdateUtils.jsm", {}).UpdateUtils.UpdateChannel;

    try {
      data.vendor = Services.prefs.getCharPref("app.support.vendor");
    }
    catch (e) {}
    let urlFormatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].
                       getService(Ci.nsIURLFormatter);
    try {
      data.supportURL = urlFormatter.formatURLPref("app.support.baseURL");
    }
    catch (e) {}

    data.numTotalWindows = 0;
    data.numRemoteWindows = 0;
    let winEnumer = Services.wm.getEnumerator("navigator:browser");
    while (winEnumer.hasMoreElements()) {
      data.numTotalWindows++;
      let remote = winEnumer.getNext().
                   QueryInterface(Ci.nsIInterfaceRequestor).
                   getInterface(Ci.nsIWebNavigation).
                   QueryInterface(Ci.nsILoadContext).
                   useRemoteTabs;
      if (remote) {
        data.numRemoteWindows++;
      }
    }

    data.remoteAutoStart = Services.appinfo.browserTabsRemoteAutostart;

    try {
      let e10sStatus = Cc["@mozilla.org/supports-PRUint64;1"]
                         .createInstance(Ci.nsISupportsPRUint64);
      let appinfo = Services.appinfo.QueryInterface(Ci.nsIObserver);
      appinfo.observe(e10sStatus, "getE10SBlocked", "");
      data.autoStartStatus = e10sStatus.data;
    } catch (e) {
      data.autoStartStatus = -1;
    }

    done(data);
  },

  extensions: function extensions(done) {
    AddonManager.getAddonsByTypes(["extension"], function (extensions) {
      extensions.sort(function (a, b) {
        if (a.isActive != b.isActive)
          return b.isActive ? 1 : -1;

        // In some unfortunate cases addon names can be null.
        let aname = a.name || null;
        let bname = b.name || null;
        let lc = aname.localeCompare(bname);
        if (lc != 0)
          return lc;
        if (a.version != b.version)
          return a.version > b.version ? 1 : -1;
        return 0;
      });
      let props = ["name", "version", "isActive", "id"];
      done(extensions.map(function (ext) {
        return props.reduce(function (extData, prop) {
          extData[prop] = ext[prop];
          return extData;
        }, {});
      }));
    });
  },

  experiments: function experiments(done) {
    if (Experiments === undefined) {
      done([]);
      return;
    }

    // getExperiments promises experiment history
    Experiments.instance().getExperiments().then(
      experiments => done(experiments)
    );
  },

  modifiedPreferences: function modifiedPreferences(done) {
    done(getPrefList(name => Services.prefs.prefHasUserValue(name)));
  },

  lockedPreferences: function lockedPreferences(done) {
    done(getPrefList(name => Services.prefs.prefIsLocked(name)));
  },

  graphics: function graphics(done) {
    function statusMsgForFeature(feature) {
      // We return an array because in the tryNewerDriver case we need to
      // include the suggested version, which the consumer likely needs to plug
      // into a format string from a localization file.  Rather than returning
      // a string in some cases and an array in others, return an array always.
      let msg = [""];
      try {
        var status = gfxInfo.getFeatureStatus(feature);
      }
      catch (e) {}
      switch (status) {
      case Ci.nsIGfxInfo.FEATURE_BLOCKED_DEVICE:
      case Ci.nsIGfxInfo.FEATURE_DISCOURAGED:
        msg = ["blockedGfxCard"];
        break;
      case Ci.nsIGfxInfo.FEATURE_BLOCKED_OS_VERSION:
        msg = ["blockedOSVersion"];
        break;
      case Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION:
        try {
          var suggestedDriverVersion =
            gfxInfo.getFeatureSuggestedDriverVersion(feature);
        }
        catch (e) {}
        msg = suggestedDriverVersion ?
              ["tryNewerDriver", suggestedDriverVersion] :
              ["blockedDriver"];
        break;
      case Ci.nsIGfxInfo.FEATURE_BLOCKED_MISMATCHED_VERSION:
        msg = ["blockedMismatchedVersion"];
        break;
      }
      return msg;
    }

    let data = {};

    try {
      // nsIGfxInfo may not be implemented on some platforms.
      var gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
    }
    catch (e) {}

    let promises = [];
    // done will be called upon all pending promises being resolved.
    // add your pending promise to promises when adding new ones.
    function completed() {
      Promise.all(promises).then(() => done(data));
    }

    data.numTotalWindows = 0;
    data.numAcceleratedWindows = 0;
    let winEnumer = Services.ww.getWindowEnumerator();
    while (winEnumer.hasMoreElements()) {
      let winUtils = winEnumer.getNext().
                     QueryInterface(Ci.nsIInterfaceRequestor).
                     getInterface(Ci.nsIDOMWindowUtils);
      try {
        // NOTE: windowless browser's windows should not be reported in the graphics troubleshoot report
        if (winUtils.layerManagerType == "None") {
          continue;
        }
        data.numTotalWindows++;
        data.windowLayerManagerType = winUtils.layerManagerType;
        data.windowLayerManagerRemote = winUtils.layerManagerRemote;
      }
      catch (e) {
        continue;
      }
      if (data.windowLayerManagerType != "Basic")
        data.numAcceleratedWindows++;
    }

    let winUtils = Services.wm.getMostRecentWindow("").
                   QueryInterface(Ci.nsIInterfaceRequestor).
                   getInterface(Ci.nsIDOMWindowUtils)
    data.supportsHardwareH264 = "Unknown";
    let promise = winUtils.supportsHardwareH264Decoding;
    promise.then(function(v) {
      data.supportsHardwareH264 = v;
    });
    promises.push(promise);

    data.currentAudioBackend = winUtils.currentAudioBackend;

    if (!data.numAcceleratedWindows && gfxInfo) {
      let win = AppConstants.platform == "win";
      let feature = win ? gfxInfo.FEATURE_DIRECT3D_9_LAYERS :
                          gfxInfo.FEATURE_OPENGL_LAYERS;
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
      adapterDriverVersion: "driverVersion",
      adapterDriverDate: "driverDate",

      adapterDescription2: null,
      adapterVendorID2: null,
      adapterDeviceID2: null,
      adapterSubsysID2: null,
      adapterRAM2: null,
      adapterDriver2: "adapterDrivers2",
      adapterDriverVersion2: "driverVersion2",
      adapterDriverDate2: "driverDate2",
      isGPU2Active: null,

      D2DEnabled: "direct2DEnabled",
      DWriteEnabled: "directWriteEnabled",
      DWriteVersion: "directWriteVersion",
      cleartypeParameters: "clearTypeParameters",
    };

    for (let prop in gfxInfoProps) {
      try {
        data[gfxInfoProps[prop] || prop] = gfxInfo[prop];
      }
      catch (e) {}
    }

    if (("direct2DEnabled" in data) && !data.direct2DEnabled)
      data.direct2DEnabledMessage =
        statusMsgForFeature(Ci.nsIGfxInfo.FEATURE_DIRECT2D);


    let doc =
      Cc["@mozilla.org/xmlextras/domparser;1"]
      .createInstance(Ci.nsIDOMParser)
      .parseFromString("<html/>", "text/html");

    function GetWebGLInfo(contextType) {
        let canvas = doc.createElement("canvas");
        canvas.width = 1;
        canvas.height = 1;


        let creationError = "(no info)";

        canvas.addEventListener(
            "webglcontextcreationerror",

            function(e) {
                creationError = e.statusMessage;
            },

            false
        );

        let gl = canvas.getContext(contextType);
        if (!gl)
            return creationError;


        let infoExt = gl.getExtension("WEBGL_debug_renderer_info");
        // This extension is unconditionally available to chrome. No need to check.
        let vendor = gl.getParameter(infoExt.UNMASKED_VENDOR_WEBGL);
        let renderer = gl.getParameter(infoExt.UNMASKED_RENDERER_WEBGL);

        let contextInfo = vendor + " -- " + renderer;


        // Eagerly free resources.
        let loseExt = gl.getExtension("WEBGL_lose_context");
        loseExt.loseContext();


        return contextInfo;
    }


    data.webglRenderer = GetWebGLInfo("webgl");
    data.webgl2Renderer = GetWebGLInfo("webgl2");


    let infoInfo = gfxInfo.getInfo();
    if (infoInfo)
      data.info = infoInfo;

    let failureCount = {};
    let failureIndices = {};

    let failures = gfxInfo.getFailures(failureCount, failureIndices);
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

  javaScript: function javaScript(done) {
    let data = {};
    let winEnumer = Services.ww.getWindowEnumerator();
    if (winEnumer.hasMoreElements())
      data.incrementalGCEnabled = winEnumer.getNext().
                                  QueryInterface(Ci.nsIInterfaceRequestor).
                                  getInterface(Ci.nsIDOMWindowUtils).
                                  isIncrementalGCEnabled();
    done(data);
  },

  accessibility: function accessibility(done) {
    let data = {};
    try {
      data.isActive = Components.manager.QueryInterface(Ci.nsIServiceManager).
                      isServiceInstantiatedByContractID(
                        "@mozilla.org/accessibilityService;1",
                        Ci.nsISupports);
    }
    catch (e) {
      data.isActive = false;
    }
    try {
      data.forceDisabled =
        Services.prefs.getIntPref("accessibility.force_disabled");
    }
    catch (e) {}
    done(data);
  },

  libraryVersions: function libraryVersions(done) {
    let data = {};
    let verInfo = Cc["@mozilla.org/security/nssversion;1"].
                  getService(Ci.nsINSSVersion);
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
  }
};

if (AppConstants.MOZ_CRASHREPORTER) {
  dataProviders.crashes = function crashes(done) {
    let CrashReports = Cu.import("resource://gre/modules/CrashReports.jsm").CrashReports;
    let reports = CrashReports.getReports();
    let now = new Date();
    let reportsNew = reports.filter(report => (now - report.date < Troubleshoot.kMaxCrashAge));
    let reportsSubmitted = reportsNew.filter(report => (!report.pending));
    let reportsPendingCount = reportsNew.length - reportsSubmitted.length;
    let data = {submitted : reportsSubmitted, pending : reportsPendingCount};
    done(data);
  }
}

if (AppConstants.MOZ_SANDBOX) {
  dataProviders.sandbox = function sandbox(done) {
    let data = {};
    if (AppConstants.platform == "linux") {
      const keys = ["hasSeccompBPF", "hasSeccompTSync",
                    "hasPrivilegedUserNamespaces", "hasUserNamespaces",
                    "canSandboxContent", "canSandboxMedia"];

      let sysInfo = Cc["@mozilla.org/system-info;1"].
                    getService(Ci.nsIPropertyBag2);
      for (let key of keys) {
        if (sysInfo.hasKey(key)) {
          data[key] = sysInfo.getPropertyAsBool(key);
        }
      }
    }

    if (AppConstants.MOZ_CONTENT_SANDBOX) {
      data.contentSandboxLevel =
        Services.prefs.getIntPref("security.sandbox.content.level");
    }

    done(data);
  }
}
