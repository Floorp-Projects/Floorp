/*
 * Test Services.logins.searchLogins with the `schemeUpgrades` property.
 */

const HTTP3_ORIGIN = "http://www3.example.com";
const HTTPS_ORIGIN = "https://www.example.com";
const HTTP_ORIGIN = "http://www.example.com";

/**
 * Returns a list of new nsILoginInfo objects that are a subset of the test
 * data, built to match the specified query.
 *
 * @param {Object} aQuery
 *        Each property and value of this object restricts the search to those
 *        entries from the test data that match the property exactly.
 */
function buildExpectedLogins(aQuery) {
  return TestData.loginList().filter(
    entry => Object.keys(aQuery).every(name => {
      if (name == "schemeUpgrades") {
        return true;
      }
      if (["hostname", "formSubmitURL"].includes(name)) {
        return LoginHelper.isOriginMatching(entry[name], aQuery[name], {
          schemeUpgrades: aQuery.schemeUpgrades,
        });
      }
      return entry[name] === aQuery[name];
    }));
}

/**
 * Tests the searchLogins function.
 *
 * @param {Object} aQuery
 *        Each property and value of this object is translated to an entry in
 *        the nsIPropertyBag parameter of searchLogins.
 * @param {Number} aExpectedCount
 *        Number of logins from the test data that should be found.  The actual
 *        list of logins is obtained using the buildExpectedLogins helper, and
 *        this value is just used to verify that modifications to the test data
 *        don't make the current test meaningless.
 */
function checkSearch(aQuery, aExpectedCount) {
  do_print("Testing searchLogins for " + JSON.stringify(aQuery));

  let expectedLogins = buildExpectedLogins(aQuery);
  do_check_eq(expectedLogins.length, aExpectedCount);

  let outCount = {};
  let logins = Services.logins.searchLogins(outCount, newPropertyBag(aQuery));
  do_check_eq(outCount.value, expectedLogins.length);
  LoginTestUtils.assertLoginListsEqual(logins, expectedLogins);
}

/**
 * Prepare data for the following tests.
 */
add_task(function test_initialize() {
  for (let login of TestData.loginList()) {
    Services.logins.addLogin(login);
  }
});

/**
 * Tests searchLogins with the `schemeUpgrades` property
 */
add_task(function test_search_schemeUpgrades_hostname() {
  // Hostname-only
  checkSearch({
    hostname: HTTPS_ORIGIN,
  }, 1);
  checkSearch({
    hostname: HTTPS_ORIGIN,
    schemeUpgrades: false,
  }, 1);
  checkSearch({
    hostname: HTTPS_ORIGIN,
    schemeUpgrades: undefined,
  }, 1);
  checkSearch({
    hostname: HTTPS_ORIGIN,
    schemeUpgrades: true,
  }, 2);
});

/**
 * Same as above but replacing hostname with formSubmitURL.
 */
add_task(function test_search_schemeUpgrades_formSubmitURL() {
  checkSearch({
    formSubmitURL: HTTPS_ORIGIN,
  }, 2);
  checkSearch({
    formSubmitURL: HTTPS_ORIGIN,
    schemeUpgrades: false,
  }, 2);
  checkSearch({
    formSubmitURL: HTTPS_ORIGIN,
    schemeUpgrades: undefined,
  }, 2);
  checkSearch({
    formSubmitURL: HTTPS_ORIGIN,
    schemeUpgrades: true,
  }, 4);
});


add_task(function test_search_schemeUpgrades_hostname_formSubmitURL() {
  checkSearch({
    formSubmitURL: HTTPS_ORIGIN,
    hostname: HTTPS_ORIGIN,
  }, 1);
  checkSearch({
    formSubmitURL: HTTPS_ORIGIN,
    hostname: HTTPS_ORIGIN,
    schemeUpgrades: false,
  }, 1);
  checkSearch({
    formSubmitURL: HTTPS_ORIGIN,
    hostname: HTTPS_ORIGIN,
    schemeUpgrades: undefined,
  }, 1);
  checkSearch({
    formSubmitURL: HTTPS_ORIGIN,
    hostname: HTTPS_ORIGIN,
    schemeUpgrades: true,
  }, 2);
  checkSearch({
    formSubmitURL: HTTPS_ORIGIN,
    hostname: HTTPS_ORIGIN,
    schemeUpgrades: true,
    usernameField: "form_field_username",
  }, 2);
  checkSearch({
    formSubmitURL: HTTPS_ORIGIN,
    hostname: HTTPS_ORIGIN,
    passwordField: "form_field_password",
    schemeUpgrades: true,
    usernameField: "form_field_username",
  }, 2);
  checkSearch({
    formSubmitURL: HTTPS_ORIGIN,
    hostname: HTTPS_ORIGIN,
    httpRealm: null,
    passwordField: "form_field_password",
    schemeUpgrades: true,
    usernameField: "form_field_username",
  }, 2);
});

/**
 * HTTP submitting to HTTPS
 */
add_task(function test_http_to_https() {
  checkSearch({
    formSubmitURL: HTTPS_ORIGIN,
    hostname: HTTP3_ORIGIN,
    httpRealm: null,
    schemeUpgrades: false,
  }, 1);
  checkSearch({
    formSubmitURL: HTTPS_ORIGIN,
    hostname: HTTP3_ORIGIN,
    httpRealm: null,
    schemeUpgrades: true,
  }, 2);
});

/**
 * schemeUpgrades shouldn't cause downgrades
 */
add_task(function test_search_schemeUpgrades_downgrade() {
  checkSearch({
    formSubmitURL: HTTP_ORIGIN,
    hostname: HTTP_ORIGIN,
  }, 1);
  do_print("The same number should be found with schemeUpgrades since we're searching for HTTP");
  checkSearch({
    formSubmitURL: HTTP_ORIGIN,
    hostname: HTTP_ORIGIN,
    schemeUpgrades: true,
  }, 1);
});
