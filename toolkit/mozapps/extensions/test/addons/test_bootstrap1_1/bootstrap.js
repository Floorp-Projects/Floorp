Components.utils.import("resource://gre/modules/Services.jsm");

function enable() {
  Services.prefs.setIntPref("bootstraptest.version", 1);
}

function disable() {
  Services.prefs.setIntPref("bootstraptest.version", 0);
}
