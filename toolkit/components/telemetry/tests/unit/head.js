/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/TelemetryPing.jsm", this);
Components.utils.import("resource://gre/modules/Services.jsm", this);

const gIsWindows = ("@mozilla.org/windows-registry-key;1" in Components.classes);
const gIsMac = ("@mozilla.org/xpcom/mac-utils;1" in Components.classes);
const gIsAndroid =  ("@mozilla.org/android/bridge;1" in Components.classes);
const gIsGonk = ("@mozilla.org/cellbroadcast/gonkservice;1" in Components.classes);

const MILLISECONDS_PER_MINUTE = 60 * 1000;
const MILLISECONDS_PER_HOUR = 60 * MILLISECONDS_PER_MINUTE;
const MILLISECONDS_PER_DAY = 24 * MILLISECONDS_PER_HOUR;

const HAS_DATAREPORTINGSERVICE = "@mozilla.org/datareporting/service;1" in Components.classes;

let gOldAppInfo = null;
let gGlobalScope = this;

/**
 * Decode the payload of an HTTP request into a ping.
 * @param {Object} request The data representing an HTTP request (nsIHttpRequest).
 * @return {Object} The decoded ping payload.
 */
function decodeRequestPayload(request) {
  let s = request.bodyInputStream;
  let payload = null;
  let decoder = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON)

  if (request.getHeader("content-encoding") == "gzip") {
    let observer = {
      buffer: "",
      onStreamComplete: function(loader, context, status, length, result) {
        this.buffer = String.fromCharCode.apply(this, result);
      }
    };

    let scs = Cc["@mozilla.org/streamConverters;1"]
              .getService(Ci.nsIStreamConverterService);
    let listener = Cc["@mozilla.org/network/stream-loader;1"]
                  .createInstance(Ci.nsIStreamLoader);
    listener.init(observer);
    let converter = scs.asyncConvertData("gzip", "uncompressed",
                                         listener, null);
    converter.onStartRequest(null, null);
    converter.onDataAvailable(null, null, s, 0, s.available());
    converter.onStopRequest(null, null, null);
    let unicodeConverter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
    unicodeConverter.charset = "UTF-8";
    let utf8string = unicodeConverter.ConvertToUnicode(observer.buffer);
    utf8string += unicodeConverter.Finish();
    payload = decoder.decode(utf8string);
  } else {
    payload = decoder.decodeFromStream(s, s.available());
  }

  return payload;
}

function loadAddonManager(id, name, version, platformVersion) {
  let ns = {};
  Cu.import("resource://gre/modules/Services.jsm", ns);
  let head = "../../../../mozapps/extensions/test/xpcshell/head_addons.js";
  let file = do_get_file(head);
  let uri = ns.Services.io.newFileURI(file);
  ns.Services.scriptloader.loadSubScript(uri.spec, gGlobalScope);
  createAppInfo(id, name, version, platformVersion);
  startupManager();
}

function createAppInfo(id, name, version, platformVersion) {
  const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
  const XULAPPINFO_CID = Components.ID("{c763b610-9d49-455a-bbd2-ede71682a1ac}");
  let gAppInfo;
  if (!gOldAppInfo) {
    gOldAppInfo = Components.classes[XULAPPINFO_CONTRACTID]
                            .getService(Components.interfaces.nsIXULRuntime);
  }

  gAppInfo = {
    // nsIXULAppInfo
    vendor: "Mozilla",
    name: name,
    ID: id,
    version: version,
    appBuildID: "2007010101",
    platformVersion: platformVersion,
    platformBuildID: "2007010101",

    // nsIXULRuntime
    inSafeMode: false,
    logConsoleErrors: true,
    OS: "XPCShell",
    XPCOMABI: "noarch-spidermonkey",
    invalidateCachesOnRestart: function invalidateCachesOnRestart() {
      // Do nothing
    },

    // nsICrashReporter
    annotations: {},

    annotateCrashReport: function(key, data) {
      this.annotations[key] = data;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIXULAppInfo,
                                           Ci.nsIXULRuntime,
                                           Ci.nsICrashReporter,
                                           Ci.nsISupports])
  };

  Object.setPrototypeOf(gAppInfo, gOldAppInfo);

  var XULAppInfoFactory = {
    createInstance: function (outer, iid) {
      if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;
      return gAppInfo.QueryInterface(iid);
    }
  };
  var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(XULAPPINFO_CID, "XULAppInfo",
                            XULAPPINFO_CONTRACTID, XULAppInfoFactory);
}

// Fake the timeout functions for the TelemetryScheduler.
function fakeSchedulerTimer(set, clear) {
  let session = Components.utils.import("resource://gre/modules/TelemetrySession.jsm");
  session.Policy.setSchedulerTickTimeout = set;
  session.Policy.clearSchedulerTickTimeout = clear;
}

// Fake the current date.
function fakeNow(date) {
  let session = Cu.import("resource://gre/modules/TelemetrySession.jsm");
  session.Policy.now = () => date;
  let environment = Cu.import("resource://gre/modules/TelemetryEnvironment.jsm");
  environment.Policy.now = () => date;
}

// Return a date that is |offset| ms in the future from |date|.
function futureDate(date, offset) {
  return new Date(date.getTime() + offset);
}

function truncateToDays(aMsec) {
  return Math.floor(aMsec / MILLISECONDS_PER_DAY);
}

// Set logging preferences for all the tests.
Services.prefs.setCharPref("toolkit.telemetry.log.level", "Trace");
Services.prefs.setBoolPref("toolkit.telemetry.log.dump", true);
TelemetryPing.initLogging();

// Avoid timers interrupting test behavior.
fakeSchedulerTimer(() => {}, () => {});
