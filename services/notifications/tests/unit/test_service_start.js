// Check that everything is getting hooked together properly.
function run_test() {
  _("When imported, Service.onStartup is called.");
  Cu.import("resource://services-notifications/service.js");

  do_check_eq(Service.serverURL, "https://notifications.mozilla.org/");
  do_check_eq(Service.ready, true);
}
