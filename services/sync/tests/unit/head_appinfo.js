/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var gSyncProfile;

gSyncProfile = do_get_profile();

// Init FormHistoryStartup and pretend we opened a profile.
var fhs = Cc["@mozilla.org/satchel/form-history-startup;1"]
            .getService(Ci.nsIObserver);
fhs.observe(null, "profile-after-change", null);

// An app is going to have some prefs set which xpcshell tests don't.
Services.prefs.setCharPref("identity.sync.tokenserver.uri", "http://token-server");

// Make sure to provide the right OS so crypto loads the right binaries
var OS = "XPCShell";
if (mozinfo.os == "win")
  OS = "WINNT";
else if (mozinfo.os == "mac")
  OS = "Darwin";
else
  OS = "Linux";

var XULAppInfo = {
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

var XULAppInfoFactory = {
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return XULAppInfo.QueryInterface(iid);
  }
};

var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{fbfae60b-64a4-44ef-a911-08ceb70b9f31}"),
                          "XULAppInfo", "@mozilla.org/xre/app-info;1",
                          XULAppInfoFactory);


// Register resource aliases. Normally done in SyncComponents.manifest.
function addResourceAlias() {
  const resProt = Services.io.getProtocolHandler("resource")
                          .QueryInterface(Ci.nsIResProtocolHandler);
  for each (let s in ["common", "sync", "crypto"]) {
    let uri = Services.io.newURI("resource://gre/modules/services-" + s + "/", null,
                                 null);
    resProt.setSubstitution("services-" + s, uri);
  }
}
addResourceAlias();
