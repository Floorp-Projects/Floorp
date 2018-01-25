/* exported startup, shutdown */
Components.utils.import("resource://gre/modules/Services.jsm");

function startup(data, reason) {
  Services.prefs.setIntPref("bootstraptest.active_version", 1);
}

function shutdown(data, reason) {
  Services.prefs.setIntPref("bootstraptest.active_version", 0);
}
