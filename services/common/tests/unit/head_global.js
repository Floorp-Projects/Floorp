/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var {classes: Cc, interfaces: Ci, results: Cr, utils: Cu, manager: Cm} = Components;

var gSyncProfile = do_get_profile();
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://testing-common/AppInfo.jsm", this);
updateAppInfo({
  name: "XPCShell",
  ID: "xpcshell@tests.mozilla.org",
  version: "1",
  platformVersion: "",
});

function addResourceAlias() {
  Cu.import("resource://gre/modules/Services.jsm");
  const handler = Services.io.getProtocolHandler("resource")
                  .QueryInterface(Ci.nsIResProtocolHandler);

  let modules = ["common", "crypto"];
  for (let module of modules) {
    let uri = Services.io.newURI("resource://gre/modules/services-" + module + "/");
    handler.setSubstitution("services-" + module, uri);
  }
}
addResourceAlias();
