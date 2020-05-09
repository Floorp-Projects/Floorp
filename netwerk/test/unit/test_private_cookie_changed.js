/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

function makeChan(uri, isPrivate) {
  var chan = NetUtil.newChannel({
    uri: uri.spec,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);

  chan.QueryInterface(Ci.nsIPrivateBrowsingChannel).setPrivate(isPrivate);
  return chan;
}

function run_test() {
  // We don't want to have CookieJarSettings blocking this test.
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  let publicNotifications = 0;
  let privateNotifications = 0;
  Services.obs.addObserver(function() {
    publicNotifications++;
  }, "cookie-changed");
  Services.obs.addObserver(function() {
    privateNotifications++;
  }, "private-cookie-changed");

  let uri = NetUtil.newURI("http://foo.com/");
  let publicChan = makeChan(uri, false);
  let svc = Services.cookies.QueryInterface(Ci.nsICookieService);
  svc.setCookieStringFromHttp(uri, "oh=hai", publicChan);
  let privateChan = makeChan(uri, true);
  svc.setCookieStringFromHttp(uri, "oh=hai", privateChan);
  Assert.equal(publicNotifications, 1);
  Assert.equal(privateNotifications, 1);
}
