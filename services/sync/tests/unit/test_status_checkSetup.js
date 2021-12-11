/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { Status } = ChromeUtils.import("resource://services-sync/status.js");

add_task(async function test_status_checkSetup() {
  try {
    _("Fresh setup, we're not configured.");
    Assert.equal(Status.checkSetup(), CLIENT_NOT_CONFIGURED);
    Assert.equal(Status.login, LOGIN_FAILED_NO_USERNAME);
    Status.resetSync();

    _("Let's provide the syncKeyBundle");
    await configureIdentity();

    _("checkSetup()");
    Assert.equal(Status.checkSetup(), STATUS_OK);
    Status.resetSync();
  } finally {
    Svc.Prefs.resetBranch("");
  }
});
