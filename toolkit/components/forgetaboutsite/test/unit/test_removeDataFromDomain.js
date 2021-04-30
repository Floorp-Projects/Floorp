/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test added with bug 460086 to test the behavior of the new API that was added
 * to remove all traces of visiting a site.
 */

// Globals

const { ForgetAboutSite } = ChromeUtils.import(
  "resource://gre/modules/ForgetAboutSite.jsm"
);

const { PlacesUtils } = ChromeUtils.import(
  "resource://gre/modules/PlacesUtils.jsm"
);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm"
);

const COOKIE_EXPIRY = Math.round(Date.now() / 1000) + 60;
const COOKIE_NAME = "testcookie";
const COOKIE_PATH = "/";

const LOGIN_USERNAME = "username";
const LOGIN_PASSWORD = "password";
const LOGIN_USERNAME_FIELD = "username_field";
const LOGIN_PASSWORD_FIELD = "password_field";

const PERMISSION_TYPE = "test-perm";
const PERMISSION_VALUE = Ci.nsIPermissionManager.ALLOW_ACTION;

const PREFERENCE_NAME = "test-pref";

// Utility Functions

/**
 * Add a cookie to the cookie service.
 *
 * @param aDomain
 */
function add_cookie(aDomain) {
  check_cookie_exists(aDomain, false);
  Services.cookies.add(
    aDomain,
    COOKIE_PATH,
    COOKIE_NAME,
    "",
    false,
    false,
    false,
    COOKIE_EXPIRY,
    {},
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_HTTPS
  );
  check_cookie_exists(aDomain, true);
}

/**
 * Checks to ensure that a cookie exists or not for a domain.
 *
 * @param aDomain
 *        The domain to check for the cookie.
 * @param aExists
 *        True if the cookie should exist, false otherwise.
 */
function check_cookie_exists(aDomain, aExists) {
  Assert.equal(
    aExists,
    Services.cookies.cookieExists(aDomain, COOKIE_PATH, COOKIE_NAME, {})
  );
}

/**
 * Adds a disabled host to the login manager.
 *
 * @param aHost
 *        The host to add to the list of disabled hosts.
 */
function add_disabled_host(aHost) {
  check_disabled_host(aHost, false);
  Services.logins.setLoginSavingEnabled(aHost, false);
  check_disabled_host(aHost, true);
}

/**
 * Checks to see if a host is disabled for storing logins or not.
 *
 * @param aHost
 *        The host to check if it is disabled.
 * @param aIsDisabled
 *        True if the host should be disabled, false otherwise.
 */
function check_disabled_host(aHost, aIsDisabled) {
  Assert.equal(!aIsDisabled, Services.logins.getLoginSavingEnabled(aHost));
}

/**
 * Adds a login for the specified host to the login manager.
 *
 * @param aHost
 *        The host to add the login for.
 */
function add_login(aHost) {
  check_login_exists(aHost, false);
  let login = Cc["@mozilla.org/login-manager/loginInfo;1"].createInstance(
    Ci.nsILoginInfo
  );
  login.init(
    aHost,
    "",
    null,
    LOGIN_USERNAME,
    LOGIN_PASSWORD,
    LOGIN_USERNAME_FIELD,
    LOGIN_PASSWORD_FIELD
  );
  Services.logins.addLogin(login);
  check_login_exists(aHost, true);
}

/**
 * Checks to see if a login exists for a host.
 *
 * @param aHost
 *        The host to check for the test login.
 * @param aExists
 *        True if the login should exist, false otherwise.
 */
function check_login_exists(aHost, aExists) {
  let logins = Services.logins.findLogins(aHost, "", null);
  Assert.equal(logins.length, aExists ? 1 : 0);
}

/**
 * Adds a permission for the specified URI to the permission manager.
 *
 * @param aURI
 *        The URI to add the test permission for.
 */
function add_permission(aURI) {
  check_permission_exists(aURI, false);
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    aURI,
    {}
  );

  Services.perms.addFromPrincipal(principal, PERMISSION_TYPE, PERMISSION_VALUE);
  check_permission_exists(aURI, true);
}

/**
 * Checks to see if a permission exists for the given URI.
 *
 * @param aURI
 *        The URI to check if a permission exists.
 * @param aExists
 *        True if the permission should exist, false otherwise.
 */
function check_permission_exists(aURI, aExists) {
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    aURI,
    {}
  );

  let perm = Services.perms.testExactPermissionFromPrincipal(
    principal,
    PERMISSION_TYPE
  );
  let checker = aExists ? "equal" : "notEqual";
  Assert[checker](perm, PERMISSION_VALUE);
}

/**
 * Adds a content preference for the specified URI.
 *
 * @param aURI
 *        The URI to add a preference for.
 */
function add_preference(aURI) {
  return new Promise(resolve => {
    let cp = Cc["@mozilla.org/content-pref/service;1"].getService(
      Ci.nsIContentPrefService2
    );
    cp.set(aURI.spec, PREFERENCE_NAME, "foo", null, {
      handleCompletion: () => resolve(),
    });
  });
}

/**
 * Checks to see if a content preference exists for the given URI.
 *
 * @param aURI
 *        The URI to check if a preference exists.
 */
function preference_exists(aURI) {
  return new Promise(resolve => {
    let cp = Cc["@mozilla.org/content-pref/service;1"].getService(
      Ci.nsIContentPrefService2
    );
    let exists = false;
    cp.getByDomainAndName(aURI.spec, PREFERENCE_NAME, null, {
      handleResult: () => (exists = true),
      handleCompletion: () => resolve(exists),
    });
  });
}

// Test Functions

// History
async function test_history_cleared_with_direct_match() {
  const TEST_URI = Services.io.newURI("http://mozilla.org/foo");
  Assert.equal(false, await PlacesUtils.history.hasVisits(TEST_URI));
  await PlacesTestUtils.addVisits(TEST_URI);
  Assert.ok(await PlacesUtils.history.hasVisits(TEST_URI));
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  Assert.equal(false, await PlacesUtils.history.hasVisits(TEST_URI));
}

async function test_history_cleared_with_subdomain() {
  const TEST_URI = Services.io.newURI("http://www.mozilla.org/foo");
  Assert.equal(false, await PlacesUtils.history.hasVisits(TEST_URI));
  await PlacesTestUtils.addVisits(TEST_URI);
  Assert.ok(await PlacesUtils.history.hasVisits(TEST_URI));
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  Assert.equal(false, await PlacesUtils.history.hasVisits(TEST_URI));
}

async function test_history_not_cleared_with_uri_contains_domain() {
  const TEST_URI = Services.io.newURI("http://ilovemozilla.org/foo");
  Assert.equal(false, await PlacesUtils.history.hasVisits(TEST_URI));
  await PlacesTestUtils.addVisits(TEST_URI);
  Assert.ok(await PlacesUtils.history.hasVisits(TEST_URI));
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  Assert.ok(await PlacesUtils.history.hasVisits(TEST_URI));

  // Clear history since we left something there from this test.
  await PlacesUtils.history.clear();
}

// Cookie Service
async function test_cookie_cleared_with_direct_match() {
  const TEST_DOMAIN = "mozilla.org";
  add_cookie(TEST_DOMAIN);
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  check_cookie_exists(TEST_DOMAIN, false);
}

async function test_cookie_cleared_with_subdomain() {
  const TEST_DOMAIN = "www.mozilla.org";
  add_cookie(TEST_DOMAIN);
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  check_cookie_exists(TEST_DOMAIN, false);
}

async function test_cookie_not_cleared_with_uri_contains_domain() {
  const TEST_DOMAIN = "ilovemozilla.org";
  add_cookie(TEST_DOMAIN);
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  check_cookie_exists(TEST_DOMAIN, true);
}

// Login Manager
async function test_login_manager_disabled_hosts_cleared_with_direct_match() {
  const TEST_HOST = "http://mozilla.org";
  add_disabled_host(TEST_HOST);
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  check_disabled_host(TEST_HOST, false);
}

async function test_login_manager_disabled_hosts_cleared_with_subdomain() {
  const TEST_HOST = "http://www.mozilla.org";
  add_disabled_host(TEST_HOST);
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  check_disabled_host(TEST_HOST, false);
}

async function test_login_manager_disabled_hosts_not_cleared_with_uri_contains_domain() {
  const TEST_HOST = "http://ilovemozilla.org";
  add_disabled_host(TEST_HOST);
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  check_disabled_host(TEST_HOST, true);

  // Reset state
  Services.logins.setLoginSavingEnabled(TEST_HOST, true);
  check_disabled_host(TEST_HOST, false);
}

async function test_login_manager_logins_cleared_with_direct_match() {
  const TEST_HOST = "http://mozilla.org";
  add_login(TEST_HOST);
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  check_login_exists(TEST_HOST, false);
}

async function test_login_manager_logins_cleared_with_subdomain() {
  const TEST_HOST = "http://www.mozilla.org";
  add_login(TEST_HOST);
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  check_login_exists(TEST_HOST, false);
}

async function test_login_manager_logins_not_cleared_with_uri_contains_domain() {
  const TEST_HOST = "http://ilovemozilla.org";
  add_login(TEST_HOST);
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  check_login_exists(TEST_HOST, true);

  Services.logins.removeAllUserFacingLogins();
  check_login_exists(TEST_HOST, false);
}

// Permission Manager
async function test_permission_manager_cleared_with_direct_match() {
  const TEST_URI = Services.io.newURI("http://mozilla.org");
  add_permission(TEST_URI);
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  check_permission_exists(TEST_URI, false);
}

async function test_permission_manager_cleared_with_subdomain() {
  const TEST_URI = Services.io.newURI("http://www.mozilla.org");
  add_permission(TEST_URI);
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  check_permission_exists(TEST_URI, false);
}

async function test_permission_manager_not_cleared_with_uri_contains_domain() {
  const TEST_URI = Services.io.newURI("http://ilovemozilla.org");
  add_permission(TEST_URI);
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  check_permission_exists(TEST_URI, true);

  // Reset state
  Services.perms.removeAll();
  check_permission_exists(TEST_URI, false);
}

// Content Preferences
async function test_content_preferences_cleared_with_direct_match() {
  const TEST_URI = Services.io.newURI("http://mozilla.org");
  Assert.equal(false, await preference_exists(TEST_URI));
  await add_preference(TEST_URI);
  Assert.ok(await preference_exists(TEST_URI));
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  Assert.equal(false, await preference_exists(TEST_URI));
}

async function test_content_preferences_cleared_with_subdomain() {
  const TEST_URI = Services.io.newURI("http://www.mozilla.org");
  Assert.equal(false, await preference_exists(TEST_URI));
  await add_preference(TEST_URI);
  Assert.ok(await preference_exists(TEST_URI));
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  Assert.equal(false, await preference_exists(TEST_URI));
}

async function test_content_preferences_not_cleared_with_uri_contains_domain() {
  const TEST_URI = Services.io.newURI("http://ilovemozilla.org");
  Assert.equal(false, await preference_exists(TEST_URI));
  await add_preference(TEST_URI);
  Assert.ok(await preference_exists(TEST_URI));
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  Assert.ok(await preference_exists(TEST_URI));

  // Reset state
  await ForgetAboutSite.removeDataFromDomain("ilovemozilla.org");
  Assert.equal(false, await preference_exists(TEST_URI));
}

// Push
async function test_push_cleared() {
  let ps;
  try {
    ps = Cc["@mozilla.org/push/Service;1"].getService(Ci.nsIPushService);
  } catch (e) {
    // No push service, skip test.
    return;
  }

  // Enable the push service, but disable the connection, so that accessing
  // `pushImpl.service` doesn't try to connect.
  Services.prefs.setBoolPref("dom.push.connection.enabled", false);
  Services.prefs.setBoolPref("dom.push.enabled", true);
  let pushImpl = ps.wrappedJSObject;
  if (typeof pushImpl != "object" || !pushImpl || !("service" in pushImpl)) {
    // GeckoView, for example, uses a different implementation; bail if it's
    // not something we can replace.
    info("Can't test with this push service implementation");
    return;
  }
  // Otherwise, tear down the old one and replace it with our mock backend,
  // so that we don't have to initialize an entire mock WebSocket only to
  // test clearing data.
  await pushImpl.service.uninit();
  let wasCleared = false;
  pushImpl.service = {
    async clear({ domain } = {}) {
      Assert.equal(
        domain,
        "mozilla.org",
        "Should pass domain to clear to push service"
      );
      wasCleared = true;
    },
  };
  Services.prefs.setBoolPref("dom.push.enabled", true);

  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  Assert.ok(wasCleared, "Should have cleared push data");
}

// Cache
async function test_cache_cleared() {
  // Because this test is asynchronous, it should be the last test
  Assert.ok(tests[tests.length - 1] == test_cache_cleared);

  // NOTE: We could be more extensive with this test and actually add an entry
  //       to the cache, and then make sure it is gone.  However, we trust that
  //       the API is well tested, and that when we get the observer
  //       notification, we have actually cleared the cache.
  // This seems to happen asynchronously...
  let observer = {
    observe(aSubject, aTopic, aData) {
      Services.obs.removeObserver(observer, "cacheservice:empty-cache");
      // Shutdown the download manager.
      Services.obs.notifyObservers(null, "quit-application");
      do_test_finished();
    },
  };
  Services.obs.addObserver(observer, "cacheservice:empty-cache");
  await ForgetAboutSite.removeDataFromDomain("mozilla.org");
  do_test_pending();
}

async function test_storage_cleared() {
  function getStorageForURI(aURI) {
    let principal = Services.scriptSecurityManager.createContentPrincipal(
      aURI,
      {}
    );

    return Services.domStorageManager.createStorage(
      null,
      principal,
      principal,
      ""
    );
  }

  Services.prefs.setBoolPref("dom.storage.client_validation", false);

  let s = [
    getStorageForURI(Services.io.newURI("http://mozilla.org")),
    getStorageForURI(Services.io.newURI("http://my.mozilla.org")),
    getStorageForURI(Services.io.newURI("http://ilovemozilla.org")),
  ];

  for (let i = 0; i < s.length; ++i) {
    let storage = s[i];
    storage.setItem("test", "value" + i);
    Assert.equal(storage.length, 1);
    Assert.equal(storage.key(0), "test");
    Assert.equal(storage.getItem("test"), "value" + i);
  }

  await ForgetAboutSite.removeDataFromDomain("mozilla.org");

  Assert.equal(s[0].getItem("test"), null);
  Assert.equal(s[0].length, 0);
  Assert.equal(s[1].getItem("test"), null);
  Assert.equal(s[1].length, 0);
  Assert.equal(s[2].getItem("test"), "value2");
  Assert.equal(s[2].length, 1);
}

var tests = [
  // History
  test_history_cleared_with_direct_match,
  test_history_cleared_with_subdomain,
  test_history_not_cleared_with_uri_contains_domain,

  // Cookie Service
  test_cookie_cleared_with_direct_match,
  test_cookie_cleared_with_subdomain,
  test_cookie_not_cleared_with_uri_contains_domain,

  // Login Manager
  test_login_manager_disabled_hosts_cleared_with_direct_match,
  test_login_manager_disabled_hosts_cleared_with_subdomain,
  test_login_manager_disabled_hosts_not_cleared_with_uri_contains_domain,
  test_login_manager_logins_cleared_with_direct_match,
  test_login_manager_logins_cleared_with_subdomain,
  test_login_manager_logins_not_cleared_with_uri_contains_domain,

  // Permission Manager
  test_permission_manager_cleared_with_direct_match,
  test_permission_manager_cleared_with_subdomain,
  test_permission_manager_not_cleared_with_uri_contains_domain,

  // Content Preferences
  test_content_preferences_cleared_with_direct_match,
  test_content_preferences_cleared_with_subdomain,
  test_content_preferences_not_cleared_with_uri_contains_domain,

  // Push
  test_push_cleared,

  // Storage
  test_storage_cleared,
];

// Cache
//
// Due to these prefs being static, setting them doesn't make a difference in time for the test
// As we are removing AppCache in Bug 1584984 this will just be removed soon.
if (Services.prefs.getBoolPref("browser.cache.offline.enable")) {
  tests.push(test_cache_cleared);
}

function run_test() {
  for (let i = 0; i < tests.length; i++) {
    add_task(tests[i]);
  }

  run_next_test();
}
