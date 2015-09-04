Components.utils.import("resource://gre/modules/Services.jsm");

const ID = "system1@tests.mozilla.org";
const VERSION = "1.0";

function install(data, reason) {
}

function startup(data, reason) {
  Services.prefs.setCharPref("bootstraptest." + ID + ".active_version", VERSION);
}

function shutdown(data, reason) {
  Services.prefs.clearUserPref("bootstraptest." + ID + ".active_version");
}

function uninstall(data, reason) {
}
