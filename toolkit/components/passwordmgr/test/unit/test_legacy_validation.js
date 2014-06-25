/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the legacy validation made when storing nsILoginInfo or disabled hosts.
 *
 * These rules exist because of limitations of the "signons.txt" storage file,
 * that is not used anymore.  They are still enforced by the Login Manager
 * service, despite these values can now be safely stored in the back-end.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Tests

/**
 * Tests legacy validation with addLogin.
 */
add_task(function test_addLogin_invalid_characters_legacy()
{
  // Test newlines and carriage returns in properties that contain URLs.
  for (let testValue of ["http://newline\n.example.com",
                         "http://carriagereturn.example.com\r"]) {
    let loginInfo = TestData.formLogin({ hostname: testValue });
    Assert.throws(() => Services.logins.addLogin(loginInfo),
                  /login values can't contain newlines/);

    loginInfo = TestData.formLogin({ formSubmitURL: testValue });
    Assert.throws(() => Services.logins.addLogin(loginInfo),
                  /login values can't contain newlines/);

    loginInfo = TestData.authLogin({ httpRealm: testValue });
    Assert.throws(() => Services.logins.addLogin(loginInfo),
                  /login values can't contain newlines/);
  }

  // Test newlines and carriage returns in form field names.
  for (let testValue of ["newline_field\n", "carriagereturn\r_field"]) {
    let loginInfo = TestData.formLogin({ usernameField: testValue });
    Assert.throws(() => Services.logins.addLogin(loginInfo),
                  /login values can't contain newlines/);

    loginInfo = TestData.formLogin({ passwordField: testValue });
    Assert.throws(() => Services.logins.addLogin(loginInfo),
                  /login values can't contain newlines/);
  }

  // Test a single dot as the value of usernameField and formSubmitURL.
  let loginInfo = TestData.formLogin({ usernameField: "." });
  Assert.throws(() => Services.logins.addLogin(loginInfo),
                /login values can't be periods/);

  loginInfo = TestData.formLogin({ formSubmitURL: "." });
  Assert.throws(() => Services.logins.addLogin(loginInfo),
                /login values can't be periods/);

  // Test the sequence " (" inside the value of the "hostname" property.
  loginInfo = TestData.formLogin({ hostname: "http://parens (.example.com" });
  Assert.throws(() => Services.logins.addLogin(loginInfo),
                /bad parens in hostname/);
});

/**
 * Tests legacy validation with setLoginSavingEnabled.
 */
add_task(function test_setLoginSavingEnabled_invalid_characters_legacy()
{
  for (let hostname of ["http://newline\n.example.com",
                        "http://carriagereturn.example.com\r",
                        "."]) {
    Assert.throws(() => Services.logins.setLoginSavingEnabled(hostname, false),
                  /Invalid hostname/);
  }
});
