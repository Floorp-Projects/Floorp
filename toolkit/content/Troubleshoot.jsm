/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [
  "Troubleshoot",
];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");

// We use a preferences whitelist to make sure we only show preferences that
// are useful for support and won't compromise the user's privacy.  Note that
// entries are *prefixes*: for example, "accessibility." applies to all prefs
// under the "accessibility.*" branch.
const PREFS_WHITELIST = [
  "accessibility.",
  "browser.cache.",
  "browser.display.",
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
  "image.mem.",
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
  "social.enabled",
  "svg.",
  "toolkit.startup.recent_crashes",
  "webgl.",
];

// The blacklist, unlike the whitelist, is a list of regular expressions.
const PREFS_BLACKLIST = [
  /^network[.]proxy[.]/,
  /[.]print_to_filename$/,
];

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
};

// Each data provider is a name => function mapping.  When a snapshot is
// captured, each provider's function is called, and it's the function's job to
// generate the provider's data.  The function is passed a "done" callback, and
// when done, it must pass its data to the callback.  The resulting snapshot
// object will contain a name => data entry for each provider.
let dataProviders = {

  application: function application(done) {
    let data = {
      name: Services.appinfo.name,
      version: Services.appinfo.version,
      userAgent: Cc["@mozilla.org/network/protocol;1?name=http"].
                 getService(Ci.nsIHttpProtocolHandler).
                 userAgent,
    };
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
    done(data);
  },

  extensions: function extensions(done) {
    AddonManager.getAddonsByTypes(["extension"], function (extensions) {
      extensions.sort(function (a, b) {
        if (a.isActive != b.isActive)
          return b.isActive ? 1 : -1;
        let lc = a.name.localeCompare(b.name);
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

  modifiedPreferences: function modifiedPreferences(done) {
    function getPref(name) {
      let table = {};
      table[Ci.nsIPrefBranch.PREF_STRING] = "getCharPref";
      table[Ci.nsIPrefBranch.PREF_INT] = "getIntPref";
      table[Ci.nsIPrefBranch.PREF_BOOL] = "getBoolPref";
      let type = Services.prefs.getPrefType(name);
      if (!(type in table))
        throw new Error("Unknown preference type " + type + " for " + name);
      return Services.prefs[table[type]](name);
    }
    done(PREFS_WHITELIST.reduce(function (prefs, branch) {
      Services.prefs.getChildList(branch).forEach(function (name) {
        if (Services.prefs.prefHasUserValue(name) &&
            !PREFS_BLACKLIST.some(function (re) re.test(name)))
          prefs[name] = getPref(name);
      });
      return prefs;
    }, {}));
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
      }
      return msg;
    }

    let data = {};

    try {
      // nsIGfxInfo may not be implemented on some platforms.
      var gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
    }
    catch (e) {}

    data.numTotalWindows = 0;
    data.numAcceleratedWindows = 0;
    let winEnumer = Services.ww.getWindowEnumerator();
    while (winEnumer.hasMoreElements()) {
      data.numTotalWindows++;
      let winUtils = winEnumer.getNext().
                     QueryInterface(Ci.nsIInterfaceRequestor).
                     getInterface(Ci.nsIDOMWindowUtils);
      try {
        data.windowLayerManagerType = winUtils.layerManagerType;
      }
      catch (e) {
        continue;
      }
      if (data.windowLayerManagerType != "Basic")
        data.numAcceleratedWindows++;
    }

    if (!data.numAcceleratedWindows && gfxInfo) {
      let feature =
#ifdef XP_WIN
        gfxInfo.FEATURE_DIRECT3D_9_LAYERS;
#else
        gfxInfo.FEATURE_OPENGL_LAYERS;
#endif
      data.numAcceleratedWindowsMessage = statusMsgForFeature(feature);
    }

    if (!gfxInfo) {
      done(data);
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
      adapterRAM: null,
      adapterDriver: "adapterDrivers",
      adapterDriverVersion: "driverVersion",
      adapterDriverDate: "driverDate",

      adapterDescription2: null,
      adapterVendorID2: null,
      adapterDeviceID2: null,
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

    let canvas = doc.createElement("canvas");
    canvas.width = 1;
    canvas.height = 1;

    let gl;
    try {
      gl = canvas.getContext("experimental-webgl");
    } catch(e) {}

    if (gl) {
      let ext = gl.getExtension("WEBGL_debug_renderer_info");
      // this extension is unconditionally available to chrome. No need to check.
      data.webglRenderer = gl.getParameter(ext.UNMASKED_VENDOR_WEBGL)
                           + " -- "
                           + gl.getParameter(ext.UNMASKED_RENDERER_WEBGL);
    } else {
      let feature =
#ifdef XP_WIN
        // If ANGLE is not available but OpenGL is, we want to report on the
        // OpenGL feature, because that's what's going to get used.  In all
        // other cases we want to report on the ANGLE feature.
        gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_WEBGL_ANGLE) !=
          Ci.nsIGfxInfo.FEATURE_NO_INFO &&
        gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_WEBGL_OPENGL) ==
          Ci.nsIGfxInfo.FEATURE_NO_INFO ?
        Ci.nsIGfxInfo.FEATURE_WEBGL_OPENGL :
        Ci.nsIGfxInfo.FEATURE_WEBGL_ANGLE;
#else
        Ci.nsIGfxInfo.FEATURE_WEBGL_OPENGL;
#endif
      data.webglRendererMessage = statusMsgForFeature(feature);
    }

    let infoInfo = gfxInfo.getInfo();
    if (infoInfo)
      data.info = infoInfo;

    let failures = gfxInfo.getFailures();
    if (failures.length)
      data.failures = failures;

    done(data);
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
  },
};
