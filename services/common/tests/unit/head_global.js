/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Where to bind test HTTP servers to.
const TEST_SERVER_URL = "http://localhost:8080/";

// This has the side-effect of populating Cc, Ci, Cu, Cr. It's best not to
// ask questions and just accept it.
do_load_httpd_js();
const Cm = Components.manager;

let gSyncProfile = do_get_profile();

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let XULAppInfo = {
  vendor: "Mozilla",
  name: "XPCShell",
  ID: "xpcshell@tests.mozilla.org",
  version: "1",
  appBuildID: "20100621",
  platformVersion: "",
  platformBuildID: "20100621",
  inSafeMode: false,
  logConsoleErrors: true,
  OS: "XPCShell",
  XPCOMABI: "noarch-spidermonkey",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIXULAppInfo, Ci.nsIXULRuntime]),
  invalidateCachesOnRestart: function invalidateCachesOnRestart() { }
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

function addResourceAlias() {
  Cu.import("resource://gre/modules/Services.jsm");
  const handler = Services.io.getProtocolHandler("resource")
                  .QueryInterface(Ci.nsIResProtocolHandler);

  let modules = ["common", "crypto"];
  for each (let module in modules) {
    let uri = Services.io.newURI("resource:///modules/services-" + module + "/",
                                 null, null);
    handler.setSubstitution("services-" + module, uri);
  }
}
addResourceAlias();