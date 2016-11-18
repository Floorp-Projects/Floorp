/* exported startup, shutdown, install, uninstall */
Components.utils.import("resource://gre/modules/Services.jsm");

function startup(data, reason) {
  Services.prefs.setIntPref("webapitest.active_version", 1);
}

function shutdown(data, reason) {
  Services.prefs.setIntPref("webapitest.active_version", 0);
}
