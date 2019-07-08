/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the legacy case of a login store containing entries that have an empty
 * string in the formActionOrigin field.
 *
 * In normal conditions, for the purpose of login autocomplete, HTML forms are
 * identified using both the prePath of the URI on which they are located, and
 * the prePath of the URI where the data will be submitted.  This is represented
 * by the origin and formActionOrigin properties of the stored nsILoginInfo.
 *
 * When a new login for use in forms is saved (after the user replies to the
 * password prompt), it is always stored with both the origin and the
 * formActionOrigin (that will be equal to the origin when the form has no
 * "action" attribute).
 *
 * When the same form is displayed again, the password is autocompleted.  If
 * there is another form on the same site that submits to a different site, it
 * is considered a different form, so the password is not autocompleted, but a
 * new password can be stored for the other form.
 *
 * However, the login database might contain data for an nsILoginInfo that has a
 * valid origin, but an empty formActionOrigin.  This means that the login
 * applies to all forms on the site, regardless of where they submit data to.
 *
 * A site can have at most one such login, and in case it is present, then it is
 * not possible to store separate logins for forms on the same site that submit
 * data to different sites.
 *
 * The only way to have such condition is to be using logins that were initially
 * saved by a very old version of the browser, or because of data manually added
 * by an extension in an old version.
 */

"use strict";

// Tests

/**
 * Adds a login with an empty formActionOrigin, then it verifies that no other
 * form logins can be added for the same host.
 */
add_task(function test_addLogin_wildcard() {
  let loginInfo = TestData.formLogin({
    origin: "http://any.example.com",
    formActionOrigin: "",
  });
  Services.logins.addLogin(loginInfo);

  // Normal form logins cannot be added anymore.
  loginInfo = TestData.formLogin({ origin: "http://any.example.com" });
  Assert.throws(() => Services.logins.addLogin(loginInfo), /already exists/);

  // Authentication logins can still be added.
  loginInfo = TestData.authLogin({ origin: "http://any.example.com" });
  Services.logins.addLogin(loginInfo);

  // Form logins can be added for other hosts.
  loginInfo = TestData.formLogin({ origin: "http://other.example.com" });
  Services.logins.addLogin(loginInfo);
});

/**
 * Verifies that findLogins, searchLogins, and countLogins include all logins
 * that have an empty formActionOrigin in the store, even when a formActionOrigin is
 * specified.
 */
add_task(function test_search_all_wildcard() {
  // Search a given formActionOrigin on any host.
  let matchData = newPropertyBag({
    formActionOrigin: "http://www.example.com",
  });
  Assert.equal(Services.logins.searchLogins(matchData).length, 2);

  Assert.equal(
    Services.logins.findLogins("", "http://www.example.com", null).length,
    2
  );

  Assert.equal(
    Services.logins.countLogins("", "http://www.example.com", null),
    2
  );

  // Restrict the search to one host.
  matchData.setProperty("origin", "http://any.example.com");
  Assert.equal(Services.logins.searchLogins(matchData).length, 1);

  Assert.equal(
    Services.logins.findLogins(
      "http://any.example.com",
      "http://www.example.com",
      null
    ).length,
    1
  );

  Assert.equal(
    Services.logins.countLogins(
      "http://any.example.com",
      "http://www.example.com",
      null
    ),
    1
  );
});

/**
 * Verifies that specifying an empty string for formActionOrigin in searchLogins
 * includes only logins that have an empty formActionOrigin in the store.
 */
add_task(function test_searchLogins_wildcard() {
  let logins = Services.logins.searchLogins(
    newPropertyBag({ formActionOrigin: "" })
  );

  let loginInfo = TestData.formLogin({
    origin: "http://any.example.com",
    formActionOrigin: "",
  });
  LoginTestUtils.assertLoginListsEqual(logins, [loginInfo]);
});
