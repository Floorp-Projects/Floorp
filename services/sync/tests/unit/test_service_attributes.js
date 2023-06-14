/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { Service } = ChromeUtils.importESModule(
  "resource://services-sync/service.sys.mjs"
);
const { FakeGUIDService } = ChromeUtils.importESModule(
  "resource://testing-common/services/sync/fakeservices.sys.mjs"
);

add_task(async function test_urls() {
  _("URL related Service properties correspond to preference settings.");
  try {
    Assert.equal(Service.clusterURL, "");
    Assert.ok(!Service.userBaseURL);
    Assert.equal(Service.infoURL, undefined);
    Assert.equal(Service.storageURL, undefined);
    Assert.equal(Service.metaURL, undefined);

    _("The 'clusterURL' attribute updates preferences and cached URLs.");

    // Since we don't have a cluster URL yet, these will still not be defined.
    Assert.equal(Service.infoURL, undefined);
    Assert.ok(!Service.userBaseURL);
    Assert.equal(Service.storageURL, undefined);
    Assert.equal(Service.metaURL, undefined);

    Service.clusterURL = "http://weave.cluster/1.1/johndoe/";

    Assert.equal(Service.userBaseURL, "http://weave.cluster/1.1/johndoe/");
    Assert.equal(
      Service.infoURL,
      "http://weave.cluster/1.1/johndoe/info/collections"
    );
    Assert.equal(
      Service.storageURL,
      "http://weave.cluster/1.1/johndoe/storage/"
    );
    Assert.equal(
      Service.metaURL,
      "http://weave.cluster/1.1/johndoe/storage/meta/global"
    );
  } finally {
    for (const pref of Svc.PrefBranch.getChildList("")) {
      Svc.PrefBranch.clearUserPref(pref);
    }
  }
});

add_test(function test_syncID() {
  _("Service.syncID is auto-generated, corresponds to preference.");
  new FakeGUIDService();

  try {
    // Ensure pristine environment
    Assert.equal(
      Svc.PrefBranch.getPrefType("client.syncID"),
      Ci.nsIPrefBranch.PREF_INVALID
    );

    // Performing the first get on the attribute will generate a new GUID.
    Assert.equal(Service.syncID, "fake-guid-00");
    Assert.equal(Svc.PrefBranch.getStringPref("client.syncID"), "fake-guid-00");

    Svc.PrefBranch.setStringPref("client.syncID", Utils.makeGUID());
    Assert.equal(Svc.PrefBranch.getStringPref("client.syncID"), "fake-guid-01");
    Assert.equal(Service.syncID, "fake-guid-01");
  } finally {
    for (const pref of Svc.PrefBranch.getChildList("")) {
      Svc.PrefBranch.clearUserPref(pref);
    }
    new FakeGUIDService();
    run_next_test();
  }
});

add_test(function test_locked() {
  _("The 'locked' attribute can be toggled with lock() and unlock()");

  // Defaults to false
  Assert.equal(Service.locked, false);

  Assert.equal(Service.lock(), true);
  Assert.equal(Service.locked, true);

  // Locking again will return false
  Assert.equal(Service.lock(), false);

  Service.unlock();
  Assert.equal(Service.locked, false);
  run_next_test();
});
