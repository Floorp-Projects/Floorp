/**
 * Tests methods that find specific logins in the store (searchLogins and countLogins).
 *
 * The getAllLogins method is not tested explicitly here, because it is used by
 * all tests to verify additions, removals and modifications to the login store.
 */

"use strict";

// Globals

/**
 * Returns a list of new nsILoginInfo objects that are a subset of the test
 * data, built to match the specified query.
 *
 * @param aQuery
 *        Each property and value of this object restricts the search to those
 *        entries from the test data that match the property exactly.
 */
function buildExpectedLogins(aQuery) {
  return TestData.loginList().filter(entry =>
    Object.keys(aQuery).every(name => entry[name] === aQuery[name])
  );
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
function checkSearchLogins(aQuery, aExpectedCount) {
  info("Testing searchLogins for " + JSON.stringify(aQuery));

  let expectedLogins = buildExpectedLogins(aQuery);
  Assert.equal(expectedLogins.length, aExpectedCount);

  let logins = Services.logins.searchLogins(newPropertyBag(aQuery));
  LoginTestUtils.assertLoginListsEqual(logins, expectedLogins);
}

/**
 * Tests searchLogins, and countLogins with the same query.
 *
 * @param aQuery
 *        The "origin", "formActionOrigin", and "httpRealm" properties of this
 *        object are passed as parameters to countLogins.  The
 *        same object is then passed to the checkSearchLogins function.
 * @param aExpectedCount
 *        Number of logins from the test data that should be found.  The actual
 *        list of logins is obtained using the buildExpectedLogins helper, and
 *        this value is just used to verify that modifications to the test data
 *        don't make the current test meaningless.
 */
function checkAllSearches(aQuery, aExpectedCount) {
  info("Testing all search functions for " + JSON.stringify(aQuery));

  let expectedLogins = buildExpectedLogins(aQuery);
  Assert.equal(expectedLogins.length, aExpectedCount);

  // The countLogins function support wildcard matches by
  // specifying empty strings as parameters, while searchLogins requires
  // omitting the property entirely.
  let origin = "origin" in aQuery ? aQuery.origin : "";
  let formActionOrigin =
    "formActionOrigin" in aQuery ? aQuery.formActionOrigin : "";
  let httpRealm = "httpRealm" in aQuery ? aQuery.httpRealm : "";

  // Test countLogins.
  let count = Services.logins.countLogins(origin, formActionOrigin, httpRealm);
  Assert.equal(count, expectedLogins.length);

  // Test searchLogins.
  checkSearchLogins(aQuery, aExpectedCount);
}

// Tests

/**
 * Prepare data for the following tests.
 */
add_setup(async () => {
  await Services.logins.addLogins(TestData.loginList());
});

/**
 * Tests searchLogins, and countLogins with basic queries.
 */
add_task(function test_search_all_basic() {
  // Find all logins, using no filters in the search functions.
  checkAllSearches({}, 28);

  // Find all form logins, then all authentication logins.
  checkAllSearches({ httpRealm: null }, 17);
  checkAllSearches({ formActionOrigin: null }, 11);

  // Find all form logins on one host, then all authentication logins.
  checkAllSearches({ origin: "http://www4.example.com", httpRealm: null }, 3);
  checkAllSearches(
    { origin: "http://www2.example.org", formActionOrigin: null },
    2
  );

  // Verify that scheme and subdomain are distinct in the origin.
  checkAllSearches({ origin: "http://www.example.com" }, 1);
  checkAllSearches({ origin: "https://www.example.com" }, 1);
  checkAllSearches({ origin: "https://example.com" }, 1);
  checkAllSearches({ origin: "http://www3.example.com" }, 3);

  // Verify that scheme and subdomain are distinct in formActionOrigin.
  checkAllSearches({ formActionOrigin: "http://www.example.com" }, 2);
  checkAllSearches({ formActionOrigin: "https://www.example.com" }, 2);
  checkAllSearches({ formActionOrigin: "http://example.com" }, 1);

  // Find by formActionOrigin on a single host.
  checkAllSearches(
    {
      origin: "http://www3.example.com",
      formActionOrigin: "http://www.example.com",
    },
    1
  );
  checkAllSearches(
    {
      origin: "http://www3.example.com",
      formActionOrigin: "https://www.example.com",
    },
    1
  );
  checkAllSearches(
    {
      origin: "http://www3.example.com",
      formActionOrigin: "http://example.com",
    },
    1
  );

  // Find by httpRealm on all hosts.
  checkAllSearches({ httpRealm: "The HTTP Realm" }, 3);
  checkAllSearches({ httpRealm: "ftp://ftp.example.org" }, 1);
  checkAllSearches({ httpRealm: "The HTTP Realm Other" }, 2);

  // Find by httpRealm on a single host.
  checkAllSearches(
    { origin: "http://example.net", httpRealm: "The HTTP Realm" },
    1
  );
  checkAllSearches(
    { origin: "http://example.net", httpRealm: "The HTTP Realm Other" },
    1
  );
  checkAllSearches(
    { origin: "ftp://example.net", httpRealm: "ftp://example.net" },
    1
  );
});

/**
 * Tests searchLogins with advanced queries.
 */
add_task(function test_searchLogins() {
  checkSearchLogins({ usernameField: "form_field_username" }, 12);
  checkSearchLogins({ passwordField: "form_field_password" }, 13);

  // Find all logins with an empty usernameField, including for authentication.
  checkSearchLogins({ usernameField: "" }, 16);

  // Find form logins with an empty usernameField.
  checkSearchLogins({ httpRealm: null, usernameField: "" }, 5);

  // Find logins with an empty usernameField on one host.
  checkSearchLogins(
    { origin: "http://www6.example.com", usernameField: "" },
    1
  );
});

/**
 * Tests searchLogins with invalid arguments.
 */
add_task(function test_searchLogins_invalid() {
  Assert.throws(
    () => Services.logins.searchLogins(newPropertyBag({ username: "value" })),
    /Unexpected field/
  );
});

/**
 * Tests that matches are case-sensitive, compare the full field value, and are
 * strict when interpreting the prePath of URIs.
 */
add_task(function test_search_all_full_case_sensitive() {
  checkAllSearches({ origin: "http://www.example.com" }, 1);
  checkAllSearches({ origin: "http://www.example.com/" }, 0);
  checkAllSearches({ origin: "example.com" }, 0);

  checkAllSearches({ formActionOrigin: "http://www.example.com" }, 2);
  checkAllSearches({ formActionOrigin: "http://www.example.com/" }, 0);
  checkAllSearches({ formActionOrigin: "http://" }, 0);
  checkAllSearches({ formActionOrigin: "example.com" }, 0);

  checkAllSearches({ httpRealm: "The HTTP Realm" }, 3);
  checkAllSearches({ httpRealm: "The http Realm" }, 0);
  checkAllSearches({ httpRealm: "The HTTP" }, 0);
  checkAllSearches({ httpRealm: "Realm" }, 0);
});

/**
 * Tests searchLogins, and countLogins with queries that should
 * return no values.
 */
add_task(function test_search_all_empty() {
  checkAllSearches({ origin: "http://nonexistent.example.com" }, 0);
  checkAllSearches(
    { formActionOrigin: "http://www.example.com", httpRealm: "The HTTP Realm" },
    0
  );

  checkSearchLogins({ origin: "" }, 0);
  checkSearchLogins({ id: "1000" }, 0);
});
