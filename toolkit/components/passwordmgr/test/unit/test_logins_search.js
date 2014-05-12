/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests methods that find specific logins in the store (findLogins,
 * searchLogins, and countLogins).
 *
 * The getAllLogins method is not tested explicitly here, because it is used by
 * all tests to verify additions, removals and modifications to the login store.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Globals

/**
 * Returns a list of new nsILoginInfo objects that are a subset of the test
 * data, built to match the specified query.
 *
 * @param aQuery
 *        Each property and value of this object restricts the search to those
 *        entries from the test data that match the property exactly.
 */
function buildExpectedLogins(aQuery)
{
  return TestData.loginList().filter(
    entry => Object.keys(aQuery).every(name => entry[name] === aQuery[name]));
}

/**
 * Tests the searchLogins function.
 *
 * @param aQuery
 *        Each property and value of this object is translated to an entry in
 *        the nsIPropertyBag parameter of searchLogins.
 * @param aExpectedCount
 *        Number of logins from the test data that should be found.  The actual
 *        list of logins is obtained using the buildExpectedLogins helper, and
 *        this value is just used to verify that modifications to the test data
 *        don't make the current test meaningless.
 */
function checkSearchLogins(aQuery, aExpectedCount)
{
  do_print("Testing searchLogins for " + JSON.stringify(aQuery));

  let expectedLogins = buildExpectedLogins(aQuery);
  do_check_eq(expectedLogins.length, aExpectedCount);

  let outCount = {};
  let logins = Services.logins.searchLogins(outCount, newPropertyBag(aQuery));
  do_check_eq(outCount.value, expectedLogins.length);
  LoginTest.assertLoginListsEqual(logins, expectedLogins);
}

/**
 * Tests findLogins, searchLogins, and countLogins with the same query.
 *
 * @param aQuery
 *        The "hostname", "formSubmitURL", and "httpRealm" properties of this
 *        object are passed as parameters to findLogins and countLogins.  The
 *        same object is then passed to the checkSearchLogins function.
 * @param aExpectedCount
 *        Number of logins from the test data that should be found.  The actual
 *        list of logins is obtained using the buildExpectedLogins helper, and
 *        this value is just used to verify that modifications to the test data
 *        don't make the current test meaningless.
 */
function checkAllSearches(aQuery, aExpectedCount)
{
  do_print("Testing all search functions for " + JSON.stringify(aQuery));

  let expectedLogins = buildExpectedLogins(aQuery);
  do_check_eq(expectedLogins.length, aExpectedCount);

  // The findLogins and countLogins functions support wildcard matches by
  // specifying empty strings as parameters, while searchLogins requires
  // omitting the property entirely.
  let hostname = ("hostname" in aQuery) ? aQuery.hostname : "";
  let formSubmitURL = ("formSubmitURL" in aQuery) ? aQuery.formSubmitURL : "";
  let httpRealm = ("httpRealm" in aQuery) ? aQuery.httpRealm : "";

  // Test findLogins.
  let outCount = {};
  let logins = Services.logins.findLogins(outCount, hostname, formSubmitURL,
                                          httpRealm);
  do_check_eq(outCount.value, expectedLogins.length);
  LoginTest.assertLoginListsEqual(logins, expectedLogins);

  // Test countLogins.
  let count = Services.logins.countLogins(hostname, formSubmitURL, httpRealm)
  do_check_eq(count, expectedLogins.length);

  // Test searchLogins.
  checkSearchLogins(aQuery, aExpectedCount);
}

////////////////////////////////////////////////////////////////////////////////
//// Tests

/**
 * Prepare data for the following tests.
 */
add_task(function test_initialize()
{
  for (let login of TestData.loginList()) {
    Services.logins.addLogin(login);
  }
});

/**
 * Tests findLogins, searchLogins, and countLogins with basic queries.
 */
add_task(function test_search_all_basic()
{
  // Find all logins, using no filters in the search functions.
  checkAllSearches({}, 23);

  // Find all form logins, then all authentication logins.
  checkAllSearches({ httpRealm: null }, 14);
  checkAllSearches({ formSubmitURL: null }, 9);

  // Find all form logins on one host, then all authentication logins.
  checkAllSearches({ hostname: "http://www4.example.com",
                     httpRealm: null }, 3);
  checkAllSearches({ hostname: "http://www2.example.org",
                     formSubmitURL: null }, 2);

  // Verify that scheme and subdomain are distinct in the hostname.
  checkAllSearches({ hostname: "http://www.example.com" }, 1);
  checkAllSearches({ hostname: "https://www.example.com" }, 1);
  checkAllSearches({ hostname: "https://example.com" }, 1);
  checkAllSearches({ hostname: "http://www3.example.com" }, 3);

  // Verify that scheme and subdomain are distinct in formSubmitURL.
  checkAllSearches({ formSubmitURL: "http://www.example.com" }, 2);
  checkAllSearches({ formSubmitURL: "https://www.example.com" }, 2);
  checkAllSearches({ formSubmitURL: "http://example.com" }, 1);

  // Find by formSubmitURL on a single host.
  checkAllSearches({ hostname: "http://www3.example.com",
                     formSubmitURL: "http://www.example.com" }, 1);
  checkAllSearches({ hostname: "http://www3.example.com",
                     formSubmitURL: "https://www.example.com" }, 1);
  checkAllSearches({ hostname: "http://www3.example.com",
                     formSubmitURL: "http://example.com" }, 1);

  // Find by httpRealm on all hosts.
  checkAllSearches({ httpRealm: "The HTTP Realm" }, 3);
  checkAllSearches({ httpRealm: "ftp://ftp.example.org" }, 1);
  checkAllSearches({ httpRealm: "The HTTP Realm Other" }, 2);

  // Find by httpRealm on a single host.
  checkAllSearches({ hostname: "http://example.net",
                     httpRealm: "The HTTP Realm" }, 1);
  checkAllSearches({ hostname: "http://example.net",
                     httpRealm: "The HTTP Realm Other" }, 1);
  checkAllSearches({ hostname: "ftp://example.net",
                     httpRealm: "ftp://example.net" }, 1);
});

/**
 * Tests searchLogins with advanced queries.
 */
add_task(function test_searchLogins()
{
  checkSearchLogins({ usernameField: "form_field_username" }, 12);
  checkSearchLogins({ passwordField: "form_field_password" }, 13);

  // Find all logins with an empty usernameField, including for authentication.
  checkSearchLogins({ usernameField: "" }, 11);

  // Find form logins with an empty usernameField.
  checkSearchLogins({ httpRealm: null,
                      usernameField: "" }, 2);

  // Find logins with an empty usernameField on one host.
  checkSearchLogins({ hostname: "http://www6.example.com",
                      usernameField: "" }, 1);
});

/**
 * Tests searchLogins with invalid arguments.
 */
add_task(function test_searchLogins_invalid()
{
  Assert.throws(() => Services.logins.searchLogins({},
                                      newPropertyBag({ username: "value" })),
                /Unexpected field/);
});

/**
 * Tests that matches are case-sensitive, compare the full field value, and are
 * strict when interpreting the prePath of URIs.
 */
add_task(function test_search_all_full_case_sensitive()
{
  checkAllSearches({ hostname: "http://www.example.com" }, 1);
  checkAllSearches({ hostname: "http://www.example.com/" }, 0);
  checkAllSearches({ hostname: "http://" }, 0);
  checkAllSearches({ hostname: "example.com" }, 0);

  checkAllSearches({ formSubmitURL: "http://www.example.com" }, 2);
  checkAllSearches({ formSubmitURL: "http://www.example.com/" }, 0);
  checkAllSearches({ formSubmitURL: "http://" }, 0);
  checkAllSearches({ formSubmitURL: "example.com" }, 0);

  checkAllSearches({ httpRealm: "The HTTP Realm" }, 3);
  checkAllSearches({ httpRealm: "The http Realm" }, 0);
  checkAllSearches({ httpRealm: "The HTTP" }, 0);
  checkAllSearches({ httpRealm: "Realm" }, 0);
});

/**
 * Tests findLogins, searchLogins, and countLogins with queries that should
 * return no values.
 */
add_task(function test_search_all_empty()
{
  checkAllSearches({ hostname: "http://nonexistent.example.com" }, 0);
  checkAllSearches({ formSubmitURL: "http://www.example.com",
                     httpRealm: "The HTTP Realm" }, 0);

  checkSearchLogins({ hostname: "" }, 0);
  checkSearchLogins({ id: "1000" }, 0);
});
