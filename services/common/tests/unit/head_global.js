/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var Cm = Components.manager;

// Required to avoid failures.
do_get_profile();
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.import("resource://testing-common/AppInfo.jsm", this);
updateAppInfo({
  name: "XPCShell",
  ID: "xpcshell@tests.mozilla.org",
  version: "1",
  platformVersion: "",
});

function addResourceAlias() {
  ChromeUtils.import("resource://gre/modules/Services.jsm");
  const handler = Services.io.getProtocolHandler("resource")
                  .QueryInterface(Ci.nsIResProtocolHandler);

  let modules = ["common", "crypto", "settings"];
  for (let module of modules) {
    let uri = Services.io.newURI("resource://gre/modules/services-" + module + "/");
    handler.setSubstitution("services-" + module, uri);
  }
}
addResourceAlias();
