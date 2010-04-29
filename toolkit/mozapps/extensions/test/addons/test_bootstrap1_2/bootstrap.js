Components.utils.import("resource://gre/modules/Services.jsm");

function enable() {
  Services.prefs.setIntPref("bootstraptest.version", 2);
}

function disable() {
  Services.prefs.setIntPref("bootstraptest.version", 0);
}
