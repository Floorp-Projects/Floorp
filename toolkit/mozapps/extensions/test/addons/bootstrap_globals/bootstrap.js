/* exported startup, shutdown, install, uninstall */
Components.utils.import("resource://gre/modules/Services.jsm");

var seenGlobals = new Set();
var scope = this;
function checkGlobal(name, type) {
  if (scope[name] && typeof(scope[name]) == type)
    seenGlobals.add(name);
}

var wrapped = {};
Services.obs.notifyObservers({ wrappedJSObject: wrapped }, "bootstrap-request-globals");
for (let [name, type] of wrapped.expectedGlobals) {
  checkGlobal(name, type);
}

function install(data, reason) {
}

function startup(data, reason) {
  Services.obs.notifyObservers({
    wrappedJSObject: seenGlobals
  }, "bootstrap-seen-globals");
}

function shutdown(data, reason) {
}

function uninstall(data, reason) {
}
