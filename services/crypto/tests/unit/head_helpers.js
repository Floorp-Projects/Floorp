const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

try {
  // In the context of xpcshell tests, there won't be a default AppInfo
  Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo);
}
catch(ex) {

do_get_profile();

// Make sure to provide the right OS so crypto loads the right binaries
let OS = "XPCShell";
if ("@mozilla.org/windows-registry-key;1" in Cc)
  OS = "WINNT";
else if ("nsILocalFileMac" in Ci)
  OS = "Darwin";
else
  OS = "Linux";

let XULAppInfo = {
  vendor: "Mozilla",
  name: "XPCShell",
  ID: "{3e3ba16c-1675-4e88-b9c8-afef81b3d2ef}",
  version: "1",
  appBuildID: "20100621",
  platformVersion: "",
  platformBuildID: "20100621",
  inSafeMode: false,
  logConsoleErrors: true,
  OS: OS,
  XPCOMABI: "noarch-spidermonkey",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIXULAppInfo, Ci.nsIXULRuntime])
};

let XULAppInfoFactory = {
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return XULAppInfo.QueryInterface(iid);
  }
};

let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{fbfae60b-64a4-44ef-a911-08ceb70b9f31}"),
                          "XULAppInfo", "@mozilla.org/xre/app-info;1",
                          XULAppInfoFactory);

}

// Provide resource://services-crypto if it isn't already available
let weaveService = Cc["@mozilla.org/weave/service;1"].getService();
weaveService.wrappedJSObject.addResourceAlias();

/**
 * Print some debug message to the console. All arguments will be printed,
 * separated by spaces.
 *
 * @param [arg0, arg1, arg2, ...]
 *        Any number of arguments to print out
 * @usage _("Hello World") -> prints "Hello World"
 * @usage _(1, 2, 3) -> prints "1 2 3"
 */
let _ = function(some, debug, text, to) print(Array.slice(arguments).join(" "));
