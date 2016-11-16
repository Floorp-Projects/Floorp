/* exported install */
Components.utils.import("resource://gre/modules/Services.jsm");

const install = function() {
  Services.obs.notifyObservers(null, "addon-install", "");
}
