/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { fxAccounts } = ChromeUtils.import(
  "resource://gre/modules/FxAccounts.jsm"
);

const { PREF_ACCOUNT_ROOT } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsCommon.js"
);

_("Misc tests for FxAccounts.device");

add_test(function test_default_device_name() {
  // Note that head_helpers overrides getDefaultLocalName - this test is
  // really just to ensure the actual implementation is sane - we can't
  // really check the value it uses is correct.
  // We are just hoping to avoid a repeat of bug 1369285.
  let def = fxAccounts.device.getDefaultLocalName(); // make sure it doesn't throw.
  _("default value is " + def);
  ok(def.length > 0);

  // This is obviously tied to the implementation, but we want early warning
  // if any of these things fail.
  // We really want one of these 2 to provide a value.
  let hostname =
    Services.sysinfo.get("device") ||
    Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService)
      .myHostName;
  _("hostname is " + hostname);
  ok(hostname.length > 0);
  // the hostname should be in the default.
  ok(def.includes(hostname));
  // We expect the following to work as a fallback to the above.
  let fallback = Cc["@mozilla.org/network/protocol;1?name=http"].getService(
    Ci.nsIHttpProtocolHandler
  ).oscpu;
  _("UA fallback is " + fallback);
  ok(fallback.length > 0);
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
    assertion: "foobar",
    sessionToken: "dead",
    kSync: "beef",
    kXCS: "cafe",
    kExtSync: "bacon",
    kExtKbHash: "cheese",
    verified: true,
  };
  await fxAccounts._internal.setSignedInUser(credentials);
  ok(!Services.prefs.prefHasUserValue(testPref));
  // signing the user out should reset the name pref.
  const namePref = PREF_ACCOUNT_ROOT + "device.name";
  ok(Services.prefs.prefHasUserValue(namePref));
  await fxAccounts.signOut(/* localOnly = */ true);
  ok(!Services.prefs.prefHasUserValue(namePref));
});
