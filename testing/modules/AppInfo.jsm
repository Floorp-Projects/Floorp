/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "newAppInfo",
  "getAppInfo",
  "updateAppInfo",
];


const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let origPlatformInfo = Cc["@mozilla.org/xre/app-info;1"]
    .getService(Ci.nsIPlatformInfo);

let origRuntime = Cc["@mozilla.org/xre/app-info;1"]
    .getService(Ci.nsIXULRuntime);

/**
 * Create new XULAppInfo instance with specified options.
 *
 * options is a object with following keys:
 *   ID:              nsIXULAppInfo.ID
 *   name:            nsIXULAppInfo.name
 *   version:         nsIXULAppInfo.version
 *   platformVersion: nsIXULAppInfo.platformVersion
 *   OS:              nsIXULRuntime.OS
 *
 *   crashReporter:   nsICrashReporter interface is implemented if true
 *   extraProps:      extra properties added to XULAppInfo
 */
this.newAppInfo = function(options = {}) {
  let ID = ("ID" in options) ? options.ID : "xpcshell@tests.mozilla.org";
  let name = ("name" in options) ? options.name : "xpcshell";
  let version = ("version" in options) ? options.version : "1";
  let platformVersion
      = ("platformVersion" in options) ? options.platformVersion : "p-ver";
  let OS = ("OS" in options) ? options.OS : "XPCShell";
  let extraProps = ("extraProps" in options) ? options.extraProps : {};

  let appInfo = {
    // nsIXULAppInfo
    vendor: "Mozilla",
    name,
    ID,
    version,
    appBuildID: "20160315",

    // nsIPlatformInfo
    platformVersion,
    platformBuildID: origPlatformInfo.platformBuildID,

    // nsIXULRuntime
    inSafeMode: false,
    logConsoleErrors: true,
    OS,
    XPCOMABI: "noarch-spidermonkey",
    invalidateCachesOnRestart() {},
    shouldBlockIncompatJaws: false,
    processType: origRuntime.processType,
    uniqueProcessID: origRuntime.uniqueProcessID,

    // nsIWinAppHelper
    get userCanElevate() {
      return false;
    },
  };

  let interfaces = [Ci.nsIXULAppInfo,
                    Ci.nsIPlatformInfo,
                    Ci.nsIXULRuntime];
  if ("nsIWinAppHelper" in Ci) {
    interfaces.push(Ci.nsIWinAppHelper);
  }

  if ("crashReporter" in options && options.crashReporter) {
    // nsICrashReporter
    appInfo.annotations = {};
    appInfo.annotateCrashReport = function(key, data) {
      this.annotations[key] = data;
    };
    interfaces.push(Ci.nsICrashReporter);
  }

  for (let key of Object.keys(extraProps)) {
    appInfo.browserTabsRemoteAutostart = extraProps[key];
  }

  appInfo.QueryInterface = XPCOMUtils.generateQI(interfaces);

  return appInfo;
};

var currentAppInfo = newAppInfo();

/**
 * Obtain a reference to the current object used to define XULAppInfo.
 */
this.getAppInfo = function() { return currentAppInfo; };

/**
 * Update the current application info.
 *
 * See newAppInfo for options.
 *
 * To change the current XULAppInfo, simply call this function. If there was
 * a previously registered app info object, it will be unloaded and replaced.
 */
this.updateAppInfo = function(options) {
  currentAppInfo = newAppInfo(options);

  let id = Components.ID("{fbfae60b-64a4-44ef-a911-08ceb70b9f31}");
  let cid = "@mozilla.org/xre/app-info;1";
  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

  // Unregister an existing factory if one exists.
  try {
    let existing = Components.manager.getClassObjectByContractID(cid, Ci.nsIFactory);
    registrar.unregisterFactory(id, existing);
  } catch (ex) {}

  let factory = {
    createInstance(outer, iid) {
      if (outer != null) {
        throw Cr.NS_ERROR_NO_AGGREGATION;
      }

      return currentAppInfo.QueryInterface(iid);
    },
  };

  registrar.registerFactory(id, "XULAppInfo", cid, factory);

  // Ensure that Cc actually maps cid to the new shim AppInfo. This is
  // needed when JSM global sharing is enabled, because some prior
  // code may already have looked up |Cc[cid]|.
  Cc.initialize(Cc[cid], cid);
};

