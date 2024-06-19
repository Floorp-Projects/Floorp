/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { getFxAccountsSingleton } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccounts.sys.mjs"
);
const fxAccounts = getFxAccountsSingleton();

const { CLIENT_IS_THUNDERBIRD, ON_NEW_DEVICE_ID, PREF_ACCOUNT_ROOT } =
  ChromeUtils.importESModule("resource://gre/modules/FxAccountsCommon.sys.mjs");
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

_("Misc tests for FxAccounts.device");

fxAccounts.device._checkRemoteCommandsUpdateNeeded = async () => false;

add_test(function test_default_device_name() {
  // Note that head_helpers overrides getDefaultLocalName - this test is
  // really just to ensure the actual implementation is sane - we can't
  // really check the value it uses is correct.
  // We are just hoping to avoid a repeat of bug 1369285.
  let def = fxAccounts.device.getDefaultLocalName(); // make sure it doesn't throw.
  _("default value is " + def);
  ok(!!def.length);

  // This is obviously tied to the implementation, but we want early warning
  // if any of these things fail.
  // We really want one of these 2 to provide a value.
  let hostname = Services.sysinfo.get("device") || Services.dns.myHostName;
  _("hostname is " + hostname);
  ok(!!hostname.length);
  // the hostname should be in the default.
  ok(def.includes(hostname));
  // We expect the following to work as a fallback to the above.
  let fallback = Cc["@mozilla.org/network/protocol;1?name=http"].getService(
    Ci.nsIHttpProtocolHandler
  ).oscpu;
  _("UA fallback is " + fallback);
  ok(!!fallback.length);
  // the fallback should not be in the default
  ok(!def.includes(fallback));

  run_next_test();
});

add_test(function test_migration() {
  Services.prefs.clearUserPref("identity.fxaccounts.account.device.name");
  Services.prefs.setStringPref("services.sync.client.name", "my client name");
  // calling getLocalName() should move the name to the new pref and reset the old.
  equal(fxAccounts.device.getLocalName(), "my client name");
  equal(
    Services.prefs.getStringPref("identity.fxaccounts.account.device.name"),
    "my client name"
  );
  ok(!Services.prefs.prefHasUserValue("services.sync.client.name"));
  run_next_test();
});

add_test(function test_migration_set_before_get() {
  Services.prefs.setStringPref("services.sync.client.name", "old client name");
  fxAccounts.device.setLocalName("new client name");
  equal(fxAccounts.device.getLocalName(), "new client name");
  run_next_test();
});

add_task(async function test_reset() {
  // We don't test the client name specifically here because the client name
  // is set as part of signing the user in via the attempt to register the
  // device.
  const testPref = PREF_ACCOUNT_ROOT + "test-pref";
  Services.prefs.setStringPref(testPref, "whatever");
  let credentials = {
    email: "foo@example.com",
    uid: "1234@lcip.org",
    sessionToken: "dead",
    verified: true,
    ...MOCK_ACCOUNT_KEYS,
  };
  // FxA will try to register its device record in the background after signin.
  const registerDevice = sinon
    .stub(fxAccounts._internal.fxAccountsClient, "registerDevice")
    .callsFake(async () => {
      return { id: "foo" };
    });
  const notifyPromise = TestUtils.topicObserved(ON_NEW_DEVICE_ID);
  await fxAccounts._internal.setSignedInUser(credentials);
  await notifyPromise;
  if (!CLIENT_IS_THUNDERBIRD) {
    // Firefox fires the notification twice - once during `setSignedInUser` and
    // once after it returns. It's the second notification we need to wait for.
    // Thunderbird, OTOH, fires only once during `setSignedInUser` and that's
    // what we need to wait for.
    await TestUtils.topicObserved(ON_NEW_DEVICE_ID);
  }
  ok(!Services.prefs.prefHasUserValue(testPref));
  // signing the user out should reset the name pref.
  const namePref = PREF_ACCOUNT_ROOT + "device.name";
  ok(Services.prefs.prefHasUserValue(namePref));
  await fxAccounts.signOut(/* localOnly = */ true);
  ok(!Services.prefs.prefHasUserValue(namePref));
  registerDevice.restore();
});

add_task(async function test_name_sanitization() {
  fxAccounts.device.setLocalName("emoji is valid \u2665");
  Assert.equal(fxAccounts.device.getLocalName(), "emoji is valid \u2665");

  let invalid = "x\uFFFD\n\r\t" + "x".repeat(255);
  let sanitized = "x\uFFFD\uFFFD\uFFFD\uFFFD" + "x".repeat(250); // 255 total.

  // If the pref already has the invalid value we still get the valid one back.
  Services.prefs.setStringPref(
    "identity.fxaccounts.account.device.name",
    invalid
  );
  Assert.equal(fxAccounts.device.getLocalName(), sanitized);

  // But if we explicitly set it to an invalid name, the sanitized value ends
  // up in the pref.
  fxAccounts.device.setLocalName(invalid);
  Assert.equal(fxAccounts.device.getLocalName(), sanitized);
  Assert.equal(
    Services.prefs.getStringPref("identity.fxaccounts.account.device.name"),
    sanitized
  );
});
