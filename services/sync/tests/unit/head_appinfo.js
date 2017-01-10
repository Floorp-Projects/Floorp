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

// Set the validation prefs to attempt validation every time to avoid non-determinism.
Services.prefs.setIntPref("services.sync.validation.interval", 0);
Services.prefs.setIntPref("services.sync.validation.percentageChance", 100);
Services.prefs.setIntPref("services.sync.validation.maxRecords", -1);
Services.prefs.setBoolPref("services.sync.validation.enabled", true);

// Make sure to provide the right OS so crypto loads the right binaries
function getOS() {
  switch (mozinfo.os) {
    case "win":
      return "WINNT";
    case "mac":
      return "Darwin";
    default:
      return "Linux";
  }
}

Cu.import("resource://testing-common/AppInfo.jsm", this);
updateAppInfo({
  name: "XPCShell",
  ID: "xpcshell@tests.mozilla.org",
  version: "1",
  platformVersion: "",
  OS: getOS(),
});

// Register resource aliases. Normally done in SyncComponents.manifest.
function addResourceAlias() {
  const resProt = Services.io.getProtocolHandler("resource")
                          .QueryInterface(Ci.nsIResProtocolHandler);
  for (let s of ["common", "sync", "crypto"]) {
    let uri = Services.io.newURI("resource://gre/modules/services-" + s + "/");
    resProt.setSubstitution("services-" + s, uri);
  }
}
addResourceAlias();
