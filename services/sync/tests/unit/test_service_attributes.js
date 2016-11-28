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
    do_check_true(!!Service.serverURL); // actual value may change
    do_check_eq(Service.clusterURL, "");
    do_check_false(Service.userBaseURL);
    do_check_eq(Service.infoURL, undefined);
    do_check_eq(Service.storageURL, undefined);
    do_check_eq(Service.metaURL, undefined);

    _("The 'clusterURL' attribute updates preferences and cached URLs.");
    Service.identity.username = "johndoe";

    // Since we don't have a cluster URL yet, these will still not be defined.
    do_check_eq(Service.infoURL, undefined);
    do_check_false(Service.userBaseURL);
    do_check_eq(Service.storageURL, undefined);
    do_check_eq(Service.metaURL, undefined);

    Service.serverURL = "http://weave.server/";
    Service.clusterURL = "http://weave.cluster/1.1/johndoe/";

    do_check_eq(Service.userBaseURL, "http://weave.cluster/1.1/johndoe/");
    do_check_eq(Service.infoURL,
                "http://weave.cluster/1.1/johndoe/info/collections");
    do_check_eq(Service.storageURL,
                "http://weave.cluster/1.1/johndoe/storage/");
    do_check_eq(Service.metaURL,
                "http://weave.cluster/1.1/johndoe/storage/meta/global");

    _("The 'miscURL' and 'userURL' attributes can be relative to 'serverURL' or absolute.");
    Svc.Prefs.set("miscURL", "relative/misc/");
    Svc.Prefs.set("userURL", "relative/user/");
    do_check_eq(Service.miscAPI,
                "http://weave.server/relative/misc/1.0/");
    do_check_eq(Service.userAPIURI,
                "http://weave.server/relative/user/1.0/");

    Svc.Prefs.set("miscURL", "http://weave.misc.services/");
    Svc.Prefs.set("userURL", "http://weave.user.services/");
    do_check_eq(Service.miscAPI, "http://weave.misc.services/1.0/");
    do_check_eq(Service.userAPIURI, "http://weave.user.services/1.0/");

    do_check_eq(Service.pwResetURL,
                "http://weave.server/weave-password-reset");

    _("Empty/false value for 'username' resets preference.");
    Service.identity.username = "";
    do_check_eq(Svc.Prefs.get("username"), undefined);
    do_check_eq(Service.identity.username, null);

    _("The 'serverURL' attributes updates/resets preferences.");

    Service.serverURL = "http://different.auth.node/";
    do_check_eq(Svc.Prefs.get("serverURL"), "http://different.auth.node/");
    do_check_eq(Service.clusterURL, "");

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
