Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/UrlClassifierTestUtils.jsm");

function run_test() {
  do_get_profile();
  Services.prefs.setBoolPref("privacy.trackingprotection.annotate_channels", false);
  Services.prefs.setBoolPref("privacy.trackingprotection.lower_network_priority", false);
  do_test_pending();
  UrlClassifierTestUtils.addTestTrackers().then(() => {
    run_test_in_child("../unit/test_trackingProtection_annotateChannels.js", () => {
      UrlClassifierTestUtils.cleanupTestTrackers();
      do_test_finished();
    });
    do_test_finished();
  });
}
