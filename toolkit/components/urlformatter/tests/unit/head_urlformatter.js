/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var Cc = Components.classes;
var Ci = Components.interfaces;
var Cr = Components.results;

var XULAppInfo = {
  vendor: "Mozilla",
  name: "Url Formatter Test",
  ID: "urlformattertest@test.mozilla.org",
  version: "1",
  appBuildID: "2007122405",
  platformVersion: "2.0",
  platformBuildID: "2007122406",
  inSafeMode: false,
  logConsoleErrors: true,
  OS: "XPCShell",
  XPCOMABI: "noarch-spidermonkey",

  QueryInterface: function QueryInterface(iid) {
    if (iid.equals(Ci.nsIXULAppInfo) ||
        iid.equals(Ci.nsIXULRuntime) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

var XULAppInfoFactory = {
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return XULAppInfo.QueryInterface(iid);
  }
};

var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{ecff8849-cee8-40a7-bd4a-3f4fdfeddb5c}"),
                          "XULAppInfo", "@mozilla.org/xre/app-info;1",
                          XULAppInfoFactory);
