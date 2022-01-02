/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests getLoginSavingEnabled, setLoginSavingEnabled, and getAllDisabledHosts.
 */

"use strict";

// Tests

/**
 * Tests setLoginSavingEnabled and getAllDisabledHosts.
 */
add_task(function test_setLoginSavingEnabled_getAllDisabledHosts() {
  // Add some disabled hosts, and verify that different schemes for the same
  // domain are considered different hosts.
  let origin1 = "http://disabled1.example.com";
  let origin2 = "http://disabled2.example.com";
  let origin3 = "https://disabled2.example.com";
  Services.logins.setLoginSavingEnabled(origin1, false);
  Services.logins.setLoginSavingEnabled(origin2, false);
  Services.logins.setLoginSavingEnabled(origin3, false);

  LoginTestUtils.assertDisabledHostsEqual(
    Services.logins.getAllDisabledHosts(),
    [origin1, origin2, origin3]
  );

  // Adding the same host twice should not result in an error.
  Services.logins.setLoginSavingEnabled(origin2, false);
  LoginTestUtils.assertDisabledHostsEqual(
    Services.logins.getAllDisabledHosts(),
    [origin1, origin2, origin3]
  );

  // Removing a disabled host should work.
  Services.logins.setLoginSavingEnabled(origin2, true);
  LoginTestUtils.assertDisabledHostsEqual(
    Services.logins.getAllDisabledHosts(),
    [origin1, origin3]
  );

  // Removing the last disabled host should work.
  Services.logins.setLoginSavingEnabled(origin1, true);
  Services.logins.setLoginSavingEnabled(origin3, true);
  LoginTestUtils.assertDisabledHostsEqual(
    Services.logins.getAllDisabledHosts(),
    []
  );
});

/**
 * Tests setLoginSavingEnabled and getLoginSavingEnabled.
 */
add_task(function test_setLoginSavingEnabled_getLoginSavingEnabled() {
  let origin1 = "http://disabled.example.com";
  let origin2 = "https://disabled.example.com";

  // Hosts should not be disabled by default.
  Assert.ok(Services.logins.getLoginSavingEnabled(origin1));
  Assert.ok(Services.logins.getLoginSavingEnabled(origin2));

  // Test setting initial values.
  Services.logins.setLoginSavingEnabled(origin1, false);
  Services.logins.setLoginSavingEnabled(origin2, true);
  Assert.ok(!Services.logins.getLoginSavingEnabled(origin1));
  Assert.ok(Services.logins.getLoginSavingEnabled(origin2));

  // Test changing values.
  Services.logins.setLoginSavingEnabled(origin1, true);
  Services.logins.setLoginSavingEnabled(origin2, false);
  Assert.ok(Services.logins.getLoginSavingEnabled(origin1));
  Assert.ok(!Services.logins.getLoginSavingEnabled(origin2));

  // Clean up.
  Services.logins.setLoginSavingEnabled(origin2, true);
});

/**
 * Tests setLoginSavingEnabled with invalid NUL characters in the origin.
 */
add_task(function test_setLoginSavingEnabled_invalid_characters() {
  let origin = "http://null\0X.example.com";
  Assert.throws(
    () => Services.logins.setLoginSavingEnabled(origin, false),
    /Invalid origin/
  );

  // Verify that no data was stored by the previous call.
  LoginTestUtils.assertDisabledHostsEqual(
    Services.logins.getAllDisabledHosts(),
    []
  );
});

/**
 * Tests different values of the "signon.rememberSignons" property.
 */
add_task(function test_rememberSignons() {
  let origin1 = "http://example.com";
  let origin2 = "http://localhost";

  // The default value for the preference should be true.
  Assert.ok(Services.prefs.getBoolPref("signon.rememberSignons"));

  // Hosts should not be disabled by default.
  Services.logins.setLoginSavingEnabled(origin1, false);
  Assert.ok(!Services.logins.getLoginSavingEnabled(origin1));
  Assert.ok(Services.logins.getLoginSavingEnabled(origin2));

  // Disable storage of saved passwords globally.
  Services.prefs.setBoolPref("signon.rememberSignons", false);
  registerCleanupFunction(() =>
    Services.prefs.clearUserPref("signon.rememberSignons")
  );

  // All hosts should now appear disabled.
  Assert.ok(!Services.logins.getLoginSavingEnabled(origin1));
  Assert.ok(!Services.logins.getLoginSavingEnabled(origin2));

  // The list of disabled hosts should be unaltered.
  LoginTestUtils.assertDisabledHostsEqual(
    Services.logins.getAllDisabledHosts(),
    [origin1]
  );

  // Changing values with the preference set should work.
  Services.logins.setLoginSavingEnabled(origin1, true);
  Services.logins.setLoginSavingEnabled(origin2, false);

  // All hosts should still appear disabled.
  Assert.ok(!Services.logins.getLoginSavingEnabled(origin1));
  Assert.ok(!Services.logins.getLoginSavingEnabled(origin2));

  // The list of disabled hosts should have been changed.
  LoginTestUtils.assertDisabledHostsEqual(
    Services.logins.getAllDisabledHosts(),
    [origin2]
  );

  // Enable storage of saved passwords again.
  Services.prefs.setBoolPref("signon.rememberSignons", true);

  // Hosts should now appear enabled as requested.
  Assert.ok(Services.logins.getLoginSavingEnabled(origin1));
  Assert.ok(!Services.logins.getLoginSavingEnabled(origin2));

  // Clean up.
  Services.logins.setLoginSavingEnabled(origin2, true);
  LoginTestUtils.assertDisabledHostsEqual(
    Services.logins.getAllDisabledHosts(),
    []
  );
});

/**
 * Tests storing disabled hosts with non-ASCII characters where IDN is supported.
 */
add_task(
  async function test_storage_setLoginSavingEnabled_nonascii_IDN_is_supported() {
    let origin = "http://大.net";
    let encoding = "http://xn--pss.net";

    // Test adding disabled host with nonascii URL (http://大.net).
    Services.logins.setLoginSavingEnabled(origin, false);
    await LoginTestUtils.reloadData();
    Assert.equal(Services.logins.getLoginSavingEnabled(origin), false);
    Assert.equal(Services.logins.getLoginSavingEnabled(encoding), false);
    LoginTestUtils.assertDisabledHostsEqual(
      Services.logins.getAllDisabledHosts(),
      [origin]
    );

    LoginTestUtils.clearData();

    // Test adding disabled host with IDN ("http://xn--pss.net").
    Services.logins.setLoginSavingEnabled(encoding, false);
    await LoginTestUtils.reloadData();
    Assert.equal(Services.logins.getLoginSavingEnabled(origin), false);
    Assert.equal(Services.logins.getLoginSavingEnabled(encoding), false);
    LoginTestUtils.assertDisabledHostsEqual(
      Services.logins.getAllDisabledHosts(),
      [origin]
    );

    LoginTestUtils.clearData();
  }
);

/**
 * Tests storing disabled hosts with non-ASCII characters where IDN is not supported.
 */
add_task(
  async function test_storage_setLoginSavingEnabled_nonascii_IDN_not_supported() {
    let origin = "http://√.com";
    let encoding = "http://xn--19g.com";

    // Test adding disabled host with nonascii URL (http://√.com).
    Services.logins.setLoginSavingEnabled(origin, false);
    await LoginTestUtils.reloadData();
    Assert.equal(Services.logins.getLoginSavingEnabled(origin), false);
    Assert.equal(Services.logins.getLoginSavingEnabled(encoding), false);
    LoginTestUtils.assertDisabledHostsEqual(
      Services.logins.getAllDisabledHosts(),
      [encoding]
    );

    LoginTestUtils.clearData();

    // Test adding disabled host with IDN ("http://xn--19g.com").
    Services.logins.setLoginSavingEnabled(encoding, false);
    await LoginTestUtils.reloadData();
    Assert.equal(Services.logins.getLoginSavingEnabled(origin), false);
    Assert.equal(Services.logins.getLoginSavingEnabled(encoding), false);
    LoginTestUtils.assertDisabledHostsEqual(
      Services.logins.getAllDisabledHosts(),
      [encoding]
    );

    LoginTestUtils.clearData();
  }
);
