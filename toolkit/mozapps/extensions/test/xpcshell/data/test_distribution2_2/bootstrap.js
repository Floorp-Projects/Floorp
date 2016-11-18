/* exported startup, shutdown, install, uninstall */
Components.utils.import("resource://gre/modules/Services.jsm");

function install(data, reason) {
  Services.prefs.setIntPref("bootstraptest.installed_version", 2);
  Services.prefs.setIntPref("bootstraptest.install_reason", reason);
}

function startup(data, reason) {
  Services.prefs.setIntPref("bootstraptest.active_version", 2);
  Services.prefs.setIntPref("bootstraptest.startup_reason", reason);
}

function shutdown(data, reason) {
  Services.prefs.setIntPref("bootstraptest.active_version", 0);
  Services.prefs.setIntPref("bootstraptest.shutdown_reason", reason);
}

function uninstall(data, reason) {
  Services.prefs.setIntPref("bootstraptest.installed_version", 0);
  Services.prefs.setIntPref("bootstraptest.uninstall_reason", reason);
}
