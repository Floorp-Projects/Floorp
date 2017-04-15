Components.utils.import("resource://gre/modules/Services.jsm");

do_register_cleanup(function() {
  Services.obs.notifyObservers(null, "quit-application");
});
