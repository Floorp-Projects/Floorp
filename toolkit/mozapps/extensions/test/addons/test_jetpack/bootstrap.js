/* exported startup, shutdown, install, uninstall */
Components.utils.import("resource://gre/modules/Services.jsm");

function install(data, reason) {
  Services.prefs.setIntPref("jetpacktest.installed_version", 1);
}

function startup(data, reason) {
  Services.prefs.setIntPref("jetpacktest.active_version", 1);
}

function shutdown(data, reason) {
  Services.prefs.setIntPref("jetpacktest.active_version", 0);
}

function uninstall(data, reason) {
  Services.prefs.setIntPref("jetpacktest.installed_version", 0);
}
