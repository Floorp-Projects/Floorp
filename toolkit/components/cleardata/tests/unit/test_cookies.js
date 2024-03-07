/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for cookies.
 */

"use strict";

add_task(async function test_all_cookies() {
  const expiry = Date.now() + 24 * 60 * 60;
  Services.cookies.add(
    "example.net",
    "path",
    "name",
    "value",
    true /* secure */,
    true /* http only */,
    false /* session */,
    expiry,
    {},
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_HTTPS
  );
  Assert.equal(Services.cookies.countCookiesFromHost("example.net"), 1);

  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_COOKIES,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  Assert.equal(Services.cookies.countCookiesFromHost("example.net"), 0);
});

add_task(async function test_range_cookies() {
  const expiry = Date.now() + 24 * 60 * 60;
  Services.cookies.add(
    "example.net",
    "path",
    "name",
    "value",
    true /* secure */,
    true /* http only */,
    false /* session */,
    expiry,
    {},
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_HTTPS
  );
  Assert.equal(Services.cookies.countCookiesFromHost("example.net"), 1);

  // The cookie is out of time range here.
  let from = Date.now() + 60 * 60;
  await new Promise(aResolve => {
    Services.clearData.deleteDataInTimeRange(
      from * 1000,
      expiry * 2000,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_COOKIES,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  Assert.equal(Services.cookies.countCookiesFromHost("example.net"), 1);

  // Now we delete all.
  from = Date.now() - 60 * 60;
  await new Promise(aResolve => {
    Services.clearData.deleteDataInTimeRange(
      from * 1000,
      expiry * 2000,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_COOKIES,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  Assert.equal(Services.cookies.countCookiesFromHost("example.net"), 0);
});

add_task(async function test_principal_cookies() {
  const expiry = Date.now() + 24 * 60 * 60;
  Services.cookies.add(
    "example.net",
    "path",
    "name",
    "value",
    true /* secure */,
    true /* http only */,
    false /* session */,
    expiry,
    {},
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_HTTPS
  );
  Assert.equal(Services.cookies.countCookiesFromHost("example.net"), 1);

  let uri = Services.io.newURI("http://example.com");
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(
      principal,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_COOKIES,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  Assert.equal(Services.cookies.countCookiesFromHost("example.net"), 1);

  // Now we delete all.
  uri = Services.io.newURI("http://example.net");
  principal = Services.scriptSecurityManager.createContentPrincipal(uri, {});
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(
      principal,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_COOKIES,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  Assert.equal(Services.cookies.countCookiesFromHost("example.net"), 0);
});

add_task(async function test_localfile_cookies() {
  const expiry = Date.now() + 24 * 60 * 60;
  Services.cookies.add(
    "", // local file
    "path",
    "name",
    "value",
    false /* secure */,
    false /* http only */,
    false /* session */,
    expiry,
    {},
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_HTTP
  );

  Assert.notEqual(Services.cookies.countCookiesFromHost(""), 0);

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromLocalFiles(
      true,
      Ci.nsIClearDataService.CLEAR_COOKIES,
      aResolve
    );
  });
  Assert.equal(Services.cookies.countCookiesFromHost(""), 0);
});

// The following tests ensure we properly clear (partitioned/unpartitioned)
// cookies when using deleteDataFromBaseDomain and deleteDataFromHost.

function getTestCookieName(host, topLevelBaseDomain) {
  if (!topLevelBaseDomain) {
    return host;
  }
  return `${host}_${topLevelBaseDomain}`;
}

function setTestCookie({
  host,
  topLevelBaseDomain = null,
  originAttributes = {},
}) {
  SiteDataTestUtils.addToCookies({
    host,
    name: getTestCookieName(host, topLevelBaseDomain),
    originAttributes: getOAWithPartitionKey(
      { topLevelBaseDomain },
      originAttributes
    ),
  });
}

function setTestCookies() {
  // First party cookies
  setTestCookie({ host: "example.net" });
  setTestCookie({ host: "test.example.net" });
  setTestCookie({ host: "example.org" });

  // Third-party partitioned cookies.
  setTestCookie({ host: "example.com", topLevelBaseDomain: "example.net" });
  setTestCookie({
    host: "example.com",
    topLevelBaseDomain: "example.net",
    originAttributes: { userContextId: 1 },
  });
  setTestCookie({ host: "example.net", topLevelBaseDomain: "example.org" });
  setTestCookie({
    host: "test.example.net",
    topLevelBaseDomain: "example.org",
  });

  // Ensure we have the correct cookie test state.
  // Not using countCookiesFromHost because it doesn't see partitioned cookies.
  testCookieExists({ host: "example.net" });
  testCookieExists({ host: "test.example.net" });
  testCookieExists({ host: "example.org" });

  testCookieExists({ host: "example.com", topLevelBaseDomain: "example.net" });
  testCookieExists({
    host: "example.com",
    topLevelBaseDomain: "example.net",
    originAttributes: { userContextId: 1 },
  });
  testCookieExists({ host: "example.net", topLevelBaseDomain: "example.org" });
  testCookieExists({
    host: "test.example.net",
    topLevelBaseDomain: "example.org",
  });
}

function testCookieExists({
  host,
  topLevelBaseDomain = null,
  expected = true,
  originAttributes = {},
}) {
  let exists = Services.cookies.cookieExists(
    host,
    "path",
    getTestCookieName(host, topLevelBaseDomain),
    getOAWithPartitionKey({ topLevelBaseDomain }, originAttributes)
  );
  let message = `Cookie ${expected ? "is set" : "is not set"} for ${host}`;
  if (topLevelBaseDomain) {
    message += ` partitioned under ${topLevelBaseDomain}`;
  }
  Assert.equal(exists, expected, message);
  return exists;
}

/**
 * Tests deleting (partitioned) cookies by base domain.
 */
add_task(async function test_baseDomain_cookies() {
  Services.cookies.removeAll();
  setTestCookies();

  // Clear cookies of example.net including partitions.
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromBaseDomain(
      "example.net",
      false,
      Ci.nsIClearDataService.CLEAR_COOKIES,
      aResolve
    );
  });

  testCookieExists({ host: "example.net", expected: false });
  testCookieExists({ host: "test.example.net", expected: false });
  testCookieExists({ host: "example.org" });

  testCookieExists({
    host: "example.com",
    topLevelBaseDomain: "example.net",
    expected: false,
  });
  testCookieExists({
    host: "example.com",
    topLevelBaseDomain: "example.net",
    originAttributes: { userContextId: 1 },
    expected: false,
  });
  testCookieExists({
    host: "example.net",
    topLevelBaseDomain: "example.org",
    expected: false,
  });
  testCookieExists({
    host: "test.example.net",
    topLevelBaseDomain: "example.org",
    expected: false,
  });

  // Cleanup
  Services.cookies.removeAll();
});

/**
 * Tests deleting (non-partitioned) cookies by host.
 */
add_task(async function test_host_cookies() {
  Services.cookies.removeAll();
  setTestCookies();

  // Clear cookies of example.net without partitions.
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromHost(
      "example.net",
      false,
      Ci.nsIClearDataService.CLEAR_COOKIES,
      aResolve
    );
  });

  testCookieExists({ host: "example.net", expected: false });
  testCookieExists({ host: "test.example.net" });
  testCookieExists({ host: "example.org" });
  // Third-party partitioned cookies under example.net should not be cleared.
  testCookieExists({ host: "example.com", topLevelBaseDomain: "example.net" });
  setTestCookie({
    host: "example.com",
    topLevelBaseDomain: "example.net",
    originAttributes: { userContextId: 1 },
  });
  // Third-party partitioned cookies of example.net should be removed, because
  // CookieCleaner matches with host, but any partition key (oa = {}) via
  // removeCookiesFromExactHost.
  testCookieExists({
    host: "example.net",
    topLevelBaseDomain: "example.org",
    expected: false,
  });
  testCookieExists({
    host: "test.example.net",
    topLevelBaseDomain: "example.org",
  });

  // Cleanup
  Services.cookies.removeAll();
});

/**
 * Tests that we correctly clear data when given a subdomain.
 */
add_task(async function test_baseDomain_cookies_subdomain() {
  Services.cookies.removeAll();
  setTestCookies();

  // Clear cookies of test.example.net including partitions.
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromBaseDomain(
      "test.example.net",
      false,
      Ci.nsIClearDataService.CLEAR_COOKIES,
      aResolve
    );
  });

  testCookieExists({ host: "example.net", expected: false });
  testCookieExists({ host: "test.example.net", expected: false });
  testCookieExists({ host: "example.org" });

  testCookieExists({
    host: "example.com",
    topLevelBaseDomain: "example.net",
    expected: false,
  });
  setTestCookie({
    host: "example.com",
    topLevelBaseDomain: "example.net",
    originAttributes: { userContextId: 1 },
    expected: false,
  });
  testCookieExists({
    host: "example.net",
    topLevelBaseDomain: "example.org",
    expected: false,
  });
  testCookieExists({
    host: "test.example.net",
    topLevelBaseDomain: "example.org",
    expected: false,
  });

  // Cleanup
  Services.cookies.removeAll();
});

function addCookiesForHost(host) {
  const expiry = Date.now() + 24 * 60 * 60;
  Services.cookies.add(
    host,
    "path",
    "name",
    "value",
    true /* secure */,
    true /* http only */,
    false /* session */,
    expiry,
    {},
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_HTTPS
  );
}

function addIpv6Cookies() {
  addCookiesForHost("[A:B:C:D:E:0:0:1]");
  addCookiesForHost("[a:b:c:d:e:0:0:1]");
  addCookiesForHost("[A:B:C:D:E::1]");
  addCookiesForHost("[000A:000B:000C:000D:000E:0000:0000:0001]");
  addCookiesForHost("A:B:C:D:E:0:0:1");
  addCookiesForHost("a:b:c:d:e:0:0:1");
  addCookiesForHost("A:B:C:D:E::1");

  Assert.equal(Services.cookies.cookies.length, 7);
}

// This tests the intermediate fix for Bug 1860033.
// When Bug 1882259 is resolved multiple cookies with same IPv6 host in
// different representation will not be stored anymore and this test needs to
// be removed.
add_task(async function test_ipv6_cookies() {
  // Add multiple cookies of same IPv6 address in different representations.
  addIpv6Cookies();

  // Delete cookies using cookie service
  Services.cookies.removeCookiesFromExactHost(
    "A:B:C:D:E::1",
    JSON.stringify({})
  );

  // Assert that all cookies were removed.
  Assert.equal(Services.cookies.cookies.length, 0);

  // Add multiple cookies of same IPv6 address in different representations.
  addIpv6Cookies();

  // Delete cookies by principal from URI
  let uri = Services.io.newURI("http://[A:B:C:D:E::1]");
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(
      principal,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_COOKIES,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  // Assert that all cookies were removed.
  Assert.equal(Services.cookies.cookies.length, 0);
});
