/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/fakeservices.js");
Cu.import("resource://testing-common/services/sync/utils.js");

add_task(async function test_urls() {
  _("URL related Service properties correspond to preference settings.");
  try {
    do_check_eq(Service.clusterURL, "");
    do_check_false(Service.userBaseURL);
    do_check_eq(Service.infoURL, undefined);
    do_check_eq(Service.storageURL, undefined);
    do_check_eq(Service.metaURL, undefined);

    _("The 'clusterURL' attribute updates preferences and cached URLs.");

    // Since we don't have a cluster URL yet, these will still not be defined.
    do_check_eq(Service.infoURL, undefined);
    do_check_false(Service.userBaseURL);
    do_check_eq(Service.storageURL, undefined);
    do_check_eq(Service.metaURL, undefined);

    Service.clusterURL = "http://weave.cluster/1.1/johndoe/";

    do_check_eq(Service.userBaseURL, "http://weave.cluster/1.1/johndoe/");
    do_check_eq(Service.infoURL,
                "http://weave.cluster/1.1/johndoe/info/collections");
    do_check_eq(Service.storageURL,
                "http://weave.cluster/1.1/johndoe/storage/");
    do_check_eq(Service.metaURL,
                "http://weave.cluster/1.1/johndoe/storage/meta/global");

  } finally {
    Svc.Prefs.resetBranch("");
  }
});


add_test(function test_syncID() {
  _("Service.syncID is auto-generated, corresponds to preference.");
  new FakeGUIDService();

  try {
    // Ensure pristine environment
    do_check_eq(Svc.Prefs.get("client.syncID"), undefined);

    // Performing the first get on the attribute will generate a new GUID.
    do_check_eq(Service.syncID, "fake-guid-00");
    do_check_eq(Svc.Prefs.get("client.syncID"), "fake-guid-00");

    Svc.Prefs.set("client.syncID", Utils.makeGUID());
    do_check_eq(Svc.Prefs.get("client.syncID"), "fake-guid-01");
    do_check_eq(Service.syncID, "fake-guid-01");
  } finally {
    Svc.Prefs.resetBranch("");
    new FakeGUIDService();
    run_next_test();
  }
});

add_test(function test_locked() {
  _("The 'locked' attribute can be toggled with lock() and unlock()");

  // Defaults to false
  do_check_eq(Service.locked, false);

  do_check_eq(Service.lock(), true);
  do_check_eq(Service.locked, true);

  // Locking again will return false
  do_check_eq(Service.lock(), false);

  Service.unlock();
  do_check_eq(Service.locked, false);
  run_next_test();
});
