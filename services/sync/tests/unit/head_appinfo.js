/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

let gSyncProfile;

gSyncProfile = do_get_profile();

// Init FormHistoryStartup and pretend we opened a profile.
let fhs = Cc["@mozilla.org/satchel/form-history-startup;1"]
            .getService(Ci.nsIObserver);
fhs.observe(null, "profile-after-change", null);


Cu.import("resource://gre/modules/XPCOMUtils.jsm");

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
  ID: "xpcshell@tests.mozilla.org",
  version: "1",
  appBuildID: "20100621",
  platformVersion: "",
  platformBuildID: "20100621",
  inSafeMode: false,
  logConsoleErrors: true,
  OS: OS,
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


// Register resource aliases. Normally done in SyncComponents.manifest.
function addResourceAlias() {
  Cu.import("resource://gre/modules/Services.jsm");
  const resProt = Services.io.getProtocolHandler("resource")
                          .QueryInterface(Ci.nsIResProtocolHandler);
  for each (let s in ["common", "sync", "crypto"]) {
    let uri = Services.io.newURI("resource://gre/modules/services-" + s + "/", null,
                                 null);
    resProt.setSubstitution("services-" + s, uri);
  }
}
addResourceAlias();
