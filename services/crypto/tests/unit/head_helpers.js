var Cc = Components.classes;
var Ci = Components.interfaces;
var Cr = Components.results;
var Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

try {
  // In the context of xpcshell tests, there won't be a default AppInfo
  Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo);
} catch (ex) {

// Make sure to provide the right OS so crypto loads the right binaries
var OS = "XPCShell";
if (mozinfo.os == "win")
  OS = "WINNT";
else if (mozinfo.os == "mac")
  OS = "Darwin";
else
  OS = "Linux";

Cu.import("resource://testing-common/AppInfo.jsm", this);
updateAppInfo({
  name: "XPCShell",
  ID: "{3e3ba16c-1675-4e88-b9c8-afef81b3d2ef}",
  version: "1",
  platformVersion: "",
  OS,
});
}

// Register resource alias. Normally done in SyncComponents.manifest.
function addResourceAlias() {
  Cu.import("resource://gre/modules/Services.jsm");
  const resProt = Services.io.getProtocolHandler("resource")
                          .QueryInterface(Ci.nsIResProtocolHandler);
  let uri = Services.io.newURI("resource://gre/modules/services-crypto/");
  resProt.setSubstitution("services-crypto", uri);
}
addResourceAlias();

/**
 * Print some debug message to the console. All arguments will be printed,
 * separated by spaces.
 *
 * @param [arg0, arg1, arg2, ...]
 *        Any number of arguments to print out
 * @usage _("Hello World") -> prints "Hello World"
 * @usage _(1, 2, 3) -> prints "1 2 3"
 */
var _ = function(some, debug, text, to) {
  print(Array.slice(arguments).join(" "));
};
