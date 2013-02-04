/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "getAppInfo",
  "updateAppInfo",
];


const {interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let APP_INFO = {
  vendor: "Mozilla",
  name: "xpcshell",
  ID: "xpcshell@tests.mozilla.org",
  version: "1",
  appBuildID: "20121107",
  platformVersion: "p-ver",
  platformBuildID: "20121106",
  inSafeMode: false,
  logConsoleErrors: true,
  OS: "XPCShell",
  XPCOMABI: "noarch-spidermonkey",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIXULAppInfo, Ci.nsIXULRuntime]),
  invalidateCachesOnRestart: function() {},
};


/**
 * Obtain a reference to the current object used to define XULAppInfo.
 */
this.getAppInfo = function () { return APP_INFO; }

/**
 * Update the current application info.
 *
 * If the argument is defined, it will be the object used. Else, APP_INFO is
 * used.
 *
 * To change the current XULAppInfo, simply call this function. If there was
 * a previously registered app info object, it will be unloaded and replaced.
 */
this.updateAppInfo = function (obj) {
  obj = obj || APP_INFO;
  APP_INFO = obj;

  let id = Components.ID("{fbfae60b-64a4-44ef-a911-08ceb70b9f31}");
  let cid = "@mozilla.org/xre/app-info;1";
  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

  // Unregister an existing factory if one exists.
  try {
    let existing = Components.manager.getClassObjectByContractID(cid, Ci.nsIFactory);
    registrar.unregisterFactory(id, existing);
  } catch (ex) {}

  let factory = {
    createInstance: function (outer, iid) {
      if (outer != null) {
        throw Cr.NS_ERROR_NO_AGGREGATION;
      }

      return obj.QueryInterface(iid);
    },
  };

  registrar.registerFactory(id, "XULAppInfo", cid, factory);
};

