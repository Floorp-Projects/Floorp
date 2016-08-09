/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests getLoginSavingEnabled, setLoginSavingEnabled, and getAllDisabledHosts.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Tests

/**
 * Tests setLoginSavingEnabled and getAllDisabledHosts.
 */
add_task(function test_setLoginSavingEnabled_getAllDisabledHosts()
{
  // Add some disabled hosts, and verify that different schemes for the same
  // domain are considered different hosts.
  let hostname1 = "http://disabled1.example.com";
  let hostname2 = "http://disabled2.example.com";
  let hostname3 = "https://disabled2.example.com";
  Services.logins.setLoginSavingEnabled(hostname1, false);
  Services.logins.setLoginSavingEnabled(hostname2, false);
  Services.logins.setLoginSavingEnabled(hostname3, false);

  LoginTestUtils.assertDisabledHostsEqual(Services.logins.getAllDisabledHosts(),
                                          [hostname1, hostname2, hostname3]);

  // Adding the same host twice should not result in an error.
  Services.logins.setLoginSavingEnabled(hostname2, false);
  LoginTestUtils.assertDisabledHostsEqual(Services.logins.getAllDisabledHosts(),
                                          [hostname1, hostname2, hostname3]);

  // Removing a disabled host should work.
  Services.logins.setLoginSavingEnabled(hostname2, true);
  LoginTestUtils.assertDisabledHostsEqual(Services.logins.getAllDisabledHosts(),
                                          [hostname1, hostname3]);

  // Removing the last disabled host should work.
  Services.logins.setLoginSavingEnabled(hostname1, true);
  Services.logins.setLoginSavingEnabled(hostname3, true);
  LoginTestUtils.assertDisabledHostsEqual(Services.logins.getAllDisabledHosts(),
                                          []);
});

/**
 * Tests setLoginSavingEnabled and getLoginSavingEnabled.
 */
add_task(function test_setLoginSavingEnabled_getLoginSavingEnabled()
{
  let hostname1 = "http://disabled.example.com";
  let hostname2 = "https://disabled.example.com";

  // Hosts should not be disabled by default.
  do_check_true(Services.logins.getLoginSavingEnabled(hostname1));
  do_check_true(Services.logins.getLoginSavingEnabled(hostname2));

  // Test setting initial values.
  Services.logins.setLoginSavingEnabled(hostname1, false);
  Services.logins.setLoginSavingEnabled(hostname2, true);
  do_check_false(Services.logins.getLoginSavingEnabled(hostname1));
  do_check_true(Services.logins.getLoginSavingEnabled(hostname2));

  // Test changing values.
  Services.logins.setLoginSavingEnabled(hostname1, true);
  Services.logins.setLoginSavingEnabled(hostname2, false);
  do_check_true(Services.logins.getLoginSavingEnabled(hostname1));
  do_check_false(Services.logins.getLoginSavingEnabled(hostname2));

  // Clean up.
  Services.logins.setLoginSavingEnabled(hostname2, true);
});

/**
 * Tests setLoginSavingEnabled with invalid NUL characters in the hostname.
 */
add_task(function test_setLoginSavingEnabled_invalid_characters()
{
  let hostname = "http://null\0X.example.com";
  Assert.throws(() => Services.logins.setLoginSavingEnabled(hostname, false),
                /Invalid hostname/);

  // Verify that no data was stored by the previous call.
  LoginTestUtils.assertDisabledHostsEqual(Services.logins.getAllDisabledHosts(),
                                          []);
});

/**
 * Tests different values of the "signon.rememberSignons" property.
 */
add_task(function test_rememberSignons()
{
  let hostname1 = "http://example.com";
  let hostname2 = "http://localhost";

  // The default value for the preference should be true.
  do_check_true(Services.prefs.getBoolPref("signon.rememberSignons"));

  // Hosts should not be disabled by default.
  Services.logins.setLoginSavingEnabled(hostname1, false);
  do_check_false(Services.logins.getLoginSavingEnabled(hostname1));
  do_check_true(Services.logins.getLoginSavingEnabled(hostname2));

  // Disable storage of saved passwords globally.
  Services.prefs.setBoolPref("signon.rememberSignons", false);
  do_register_cleanup(
    () => Services.prefs.clearUserPref("signon.rememberSignons"));

  // All hosts should now appear disabled.
  do_check_false(Services.logins.getLoginSavingEnabled(hostname1));
  do_check_false(Services.logins.getLoginSavingEnabled(hostname2));

  // The list of disabled hosts should be unaltered.
  LoginTestUtils.assertDisabledHostsEqual(Services.logins.getAllDisabledHosts(),
                                          [hostname1]);

  // Changing values with the preference set should work.
  Services.logins.setLoginSavingEnabled(hostname1, true);
  Services.logins.setLoginSavingEnabled(hostname2, false);

  // All hosts should still appear disabled.
  do_check_false(Services.logins.getLoginSavingEnabled(hostname1));
  do_check_false(Services.logins.getLoginSavingEnabled(hostname2));

  // The list of disabled hosts should have been changed.
  LoginTestUtils.assertDisabledHostsEqual(Services.logins.getAllDisabledHosts(),
                                          [hostname2]);

  // Enable storage of saved passwords again.
  Services.prefs.setBoolPref("signon.rememberSignons", true);

  // Hosts should now appear enabled as requested.
  do_check_true(Services.logins.getLoginSavingEnabled(hostname1));
  do_check_false(Services.logins.getLoginSavingEnabled(hostname2));

  // Clean up.
  Services.logins.setLoginSavingEnabled(hostname2, true);
  LoginTestUtils.assertDisabledHostsEqual(Services.logins.getAllDisabledHosts(),
                                          []);
});

/**
 * Tests storing disabled hosts with non-ASCII characters where IDN is supported.
 */
add_task(function* test_storage_setLoginSavingEnabled_nonascii_IDN_is_supported()
{
  let hostname = "http://大.net";
  let encoding = "http://xn--pss.net";

  // Test adding disabled host with nonascii URL (http://大.net).
  Services.logins.setLoginSavingEnabled(hostname, false);
  yield* LoginTestUtils.reloadData();
  Assert.equal(Services.logins.getLoginSavingEnabled(hostname), false);
  Assert.equal(Services.logins.getLoginSavingEnabled(encoding), false);
  LoginTestUtils.assertDisabledHostsEqual(Services.logins.getAllDisabledHosts(), [hostname]);

  LoginTestUtils.clearData();

  // Test adding disabled host with IDN ("http://xn--pss.net").
  Services.logins.setLoginSavingEnabled(encoding, false);
  yield* LoginTestUtils.reloadData();
  Assert.equal(Services.logins.getLoginSavingEnabled(hostname), false);
  Assert.equal(Services.logins.getLoginSavingEnabled(encoding), false);
  LoginTestUtils.assertDisabledHostsEqual(Services.logins.getAllDisabledHosts(), [hostname]);

  LoginTestUtils.clearData();
});

/**
 * Tests storing disabled hosts with non-ASCII characters where IDN is not supported.
 */
add_task(function* test_storage_setLoginSavingEnabled_nonascii_IDN_not_supported()
{
  let hostname = "http://√.com";
  let encoding = "http://xn--19g.com";

  // Test adding disabled host with nonascii URL (http://√.com).
  Services.logins.setLoginSavingEnabled(hostname, false);
  yield* LoginTestUtils.reloadData();
  Assert.equal(Services.logins.getLoginSavingEnabled(hostname), false);
  Assert.equal(Services.logins.getLoginSavingEnabled(encoding), false);
  LoginTestUtils.assertDisabledHostsEqual(Services.logins.getAllDisabledHosts(), [encoding]);

  LoginTestUtils.clearData();

  // Test adding disabled host with IDN ("http://xn--19g.com").
  Services.logins.setLoginSavingEnabled(encoding, false);
  yield* LoginTestUtils.reloadData();
  Assert.equal(Services.logins.getLoginSavingEnabled(hostname), false);
  Assert.equal(Services.logins.getLoginSavingEnabled(encoding), false);
  LoginTestUtils.assertDisabledHostsEqual(Services.logins.getAllDisabledHosts(), [encoding]);

  LoginTestUtils.clearData();
});
