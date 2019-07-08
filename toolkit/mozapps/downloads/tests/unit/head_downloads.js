var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

registerCleanupFunction(function() {
  Services.obs.notifyObservers(null, "quit-application");
});
