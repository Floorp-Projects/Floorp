Components.utils.import("resource://gre/modules/Services.jsm");

function install(data, reason) {
  Services.prefs.setIntPref("bootstraptest2.installed_version", 1);
}

function startup(data, reason) {
  Services.prefs.setIntPref("bootstraptest2.active_version", 1);
}

function shutdown(data, reason) {
  Services.prefs.setIntPref("bootstraptest2.active_version", 0);
}

function uninstall(data, reason) {
  Services.prefs.setIntPref("bootstraptest2.installed_version", 0);
}
