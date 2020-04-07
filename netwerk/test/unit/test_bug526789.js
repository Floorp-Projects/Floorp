/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function run_test() {
  var cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);
  var expiry = (Date.now() + 1000) * 1000;

  cm.removeAll();

  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  // test that variants of 'baz.com' get normalized appropriately, but that
  // malformed hosts are rejected
  cm.add(
    "baz.com",
    "/",
    "foo",
    "bar",
    false,
    false,
    true,
    expiry,
    {},
    Ci.nsICookie.SAMESITE_NONE
  );
  Assert.equal(cm.countCookiesFromHost("baz.com"), 1);
  Assert.equal(cm.countCookiesFromHost("BAZ.com"), 1);
  Assert.equal(cm.countCookiesFromHost(".baz.com"), 1);
  Assert.equal(cm.countCookiesFromHost("baz.com."), 0);
  Assert.equal(cm.countCookiesFromHost(".baz.com."), 0);
  do_check_throws(function() {
    cm.countCookiesFromHost("baz.com..");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.countCookiesFromHost("baz..com");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.countCookiesFromHost("..baz.com");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  cm.remove("BAZ.com.", "foo", "/", {});
  Assert.equal(cm.countCookiesFromHost("baz.com"), 1);
  cm.remove("baz.com", "foo", "/", {});
  Assert.equal(cm.countCookiesFromHost("baz.com"), 0);

  // Test that 'baz.com' and 'baz.com.' are treated differently
  cm.add(
    "baz.com.",
    "/",
    "foo",
    "bar",
    false,
    false,
    true,
    expiry,
    {},
    Ci.nsICookie.SAMESITE_NONE
  );
  Assert.equal(cm.countCookiesFromHost("baz.com"), 0);
  Assert.equal(cm.countCookiesFromHost("BAZ.com"), 0);
  Assert.equal(cm.countCookiesFromHost(".baz.com"), 0);
  Assert.equal(cm.countCookiesFromHost("baz.com."), 1);
  Assert.equal(cm.countCookiesFromHost(".baz.com."), 1);
  cm.remove("baz.com", "foo", "/", {});
  Assert.equal(cm.countCookiesFromHost("baz.com."), 1);
  cm.remove("baz.com.", "foo", "/", {});
  Assert.equal(cm.countCookiesFromHost("baz.com."), 0);

  // test that domain cookies are illegal for IP addresses, aliases such as
  // 'localhost', and eTLD's such as 'co.uk'
  cm.add(
    "192.168.0.1",
    "/",
    "foo",
    "bar",
    false,
    false,
    true,
    expiry,
    {},
    Ci.nsICookie.SAMESITE_NONE
  );
  Assert.equal(cm.countCookiesFromHost("192.168.0.1"), 1);
  Assert.equal(cm.countCookiesFromHost("192.168.0.1."), 0);
  do_check_throws(function() {
    cm.countCookiesFromHost(".192.168.0.1");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.countCookiesFromHost(".192.168.0.1.");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  cm.add(
    "localhost",
    "/",
    "foo",
    "bar",
    false,
    false,
    true,
    expiry,
    {},
    Ci.nsICookie.SAMESITE_NONE
  );
  Assert.equal(cm.countCookiesFromHost("localhost"), 1);
  Assert.equal(cm.countCookiesFromHost("localhost."), 0);
  do_check_throws(function() {
    cm.countCookiesFromHost(".localhost");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.countCookiesFromHost(".localhost.");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  cm.add(
    "co.uk",
    "/",
    "foo",
    "bar",
    false,
    false,
    true,
    expiry,
    {},
    Ci.nsICookie.SAMESITE_NONE
  );
  Assert.equal(cm.countCookiesFromHost("co.uk"), 1);
  Assert.equal(cm.countCookiesFromHost("co.uk."), 0);
  do_check_throws(function() {
    cm.countCookiesFromHost(".co.uk");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.countCookiesFromHost(".co.uk.");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  cm.removeAll();

  var uri = NetUtil.newURI("http://baz.com/");
  Assert.equal(uri.asciiHost, "baz.com");
  cs.setCookieString(uri, "foo=bar", null);
  Assert.equal(cs.getCookieString(uri, null), "foo=bar");

  Assert.equal(cm.countCookiesFromHost(""), 0);
  do_check_throws(function() {
    cm.countCookiesFromHost(".");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.countCookiesFromHost("..");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  var cookies = cm.getCookiesFromHost("", {});
  Assert.ok(!cookies.length);
  do_check_throws(function() {
    cm.getCookiesFromHost(".", {});
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.getCookiesFromHost("..", {});
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  cookies = cm.getCookiesFromHost("baz.com", {});
  Assert.equal(cookies.length, 1);
  Assert.equal(cookies[0].name, "foo");
  cookies = cm.getCookiesFromHost("", {});
  Assert.ok(!cookies.length);
  do_check_throws(function() {
    cm.getCookiesFromHost(".", {});
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.getCookiesFromHost("..", {});
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  cm.removeAll();

  // test that an empty file:// host works
  let emptyuri = NetUtil.newURI("file:///");
  Assert.equal(emptyuri.asciiHost, "");
  Assert.equal(NetUtil.newURI("file://./").asciiHost, "");
  Assert.equal(NetUtil.newURI("file://foo.bar/").asciiHost, "");
  cs.setCookieString(emptyuri, "foo2=bar", null);
  Assert.equal(getCookieCount(), 1);
  cs.setCookieString(emptyuri, "foo3=bar; domain=", null);
  Assert.equal(getCookieCount(), 2);
  cs.setCookieString(emptyuri, "foo4=bar; domain=.", null);
  Assert.equal(getCookieCount(), 2);
  cs.setCookieString(emptyuri, "foo5=bar; domain=bar.com", null);
  Assert.equal(getCookieCount(), 2);

  Assert.equal(cs.getCookieString(emptyuri, null), "foo2=bar; foo3=bar");

  Assert.equal(cm.countCookiesFromHost("baz.com"), 0);
  Assert.equal(cm.countCookiesFromHost(""), 2);
  do_check_throws(function() {
    cm.countCookiesFromHost(".");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  cookies = cm.getCookiesFromHost("baz.com", {});
  Assert.ok(!cookies.length);
  cookies = cm.getCookiesFromHost("", {});
  Assert.equal(cookies.length, 2);
  do_check_throws(function() {
    cm.getCookiesFromHost(".", {});
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  cm.removeAll();

  // test that an empty host to add() or remove() works,
  // but a host of '.' doesn't
  cm.add(
    "",
    "/",
    "foo2",
    "bar",
    false,
    false,
    true,
    expiry,
    {},
    Ci.nsICookie.SAMESITE_NONE
  );
  Assert.equal(getCookieCount(), 1);
  do_check_throws(function() {
    cm.add(
      ".",
      "/",
      "foo3",
      "bar",
      false,
      false,
      true,
      expiry,
      {},
      Ci.nsICookie.SAMESITE_NONE
    );
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  Assert.equal(getCookieCount(), 1);

  cm.remove("", "foo2", "/", {});
  Assert.equal(getCookieCount(), 0);
  do_check_throws(function() {
    cm.remove(".", "foo3", "/", {});
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  // test that the 'domain' attribute accepts a leading dot for IP addresses,
  // aliases such as 'localhost', and eTLD's such as 'co.uk'; but that the
  // resulting cookie is for the exact host only.
  testDomainCookie("http://192.168.0.1/", "192.168.0.1");
  testDomainCookie("http://localhost/", "localhost");
  testDomainCookie("http://co.uk/", "co.uk");

  // Test that trailing dots are treated differently for purposes of the
  // 'domain' attribute when using setCookieString.
  testTrailingDotCookie("http://localhost", "localhost");
  testTrailingDotCookie("http://foo.com", "foo.com");

  cm.removeAll();
}

function getCookieCount() {
  var count = 0;
  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);
  for (let cookie of cm.cookies) {
    ++count;
  }
  return count;
}

function testDomainCookie(uriString, domain) {
  var cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);

  cm.removeAll();

  var uri = NetUtil.newURI(uriString);
  cs.setCookieString(uri, "foo=bar; domain=" + domain, null);
  var cookies = cm.getCookiesFromHost(domain, {});
  Assert.ok(cookies.length);
  Assert.equal(cookies[0].host, domain);
  cm.removeAll();

  cs.setCookieString(uri, "foo=bar; domain=." + domain, null);
  cookies = cm.getCookiesFromHost(domain, {});
  Assert.ok(cookies.length);
  Assert.equal(cookies[0].host, domain);
  cm.removeAll();
}

function testTrailingDotCookie(uriString, domain) {
  var cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);

  cm.removeAll();

  var uri = NetUtil.newURI(uriString);
  cs.setCookieString(uri, "foo=bar; domain=" + domain + ".", null);
  Assert.equal(cm.countCookiesFromHost(domain), 0);
  Assert.equal(cm.countCookiesFromHost(domain + "."), 0);
  cm.removeAll();

  uri = NetUtil.newURI(uriString + ".");
  cs.setCookieString(uri, "foo=bar; domain=" + domain, null);
  Assert.equal(cm.countCookiesFromHost(domain), 0);
  Assert.equal(cm.countCookiesFromHost(domain + "."), 0);
  cm.removeAll();
}
