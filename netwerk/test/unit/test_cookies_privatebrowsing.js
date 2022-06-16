/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test private browsing mode.

"use strict";

function make_channel(url) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
}

function getCookieStringFromPrivateDocument(uriSpec) {
  return CookieXPCShellUtils.getCookieStringFromDocument(uriSpec, {
    privateBrowsing: true,
  });
}

add_task(async () => {
  // Set up a profile.
  do_get_profile();

  // We don't want to have CookieJarSettings blocking this test.
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  // Test with cookies enabled.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  Services.prefs.setBoolPref("dom.security.https_first", false);

  // Test with https-first-mode disabled in PBM
  Services.prefs.setBoolPref("dom.security.https_first_pbm", false);

  CookieXPCShellUtils.createServer({ hosts: ["foo.com", "bar.com"] });

  // We need to keep a private-browsing window active, otherwise the
  // 'last-pb-context-exited' notification will be dispatched.
  const privateBrowsingHolder = await CookieXPCShellUtils.loadContentPage(
    "http://bar.com/",
    { privateBrowsing: true }
  );

  // Create URIs pointing to foo.com and bar.com.
  let uri1 = NetUtil.newURI("http://foo.com/foo.html");
  let uri2 = NetUtil.newURI("http://bar.com/bar.html");

  // Set a cookie for host 1.
  Services.cookiesvc.setCookieStringFromHttp(
    uri1,
    "oh=hai; max-age=1000",
    make_channel(uri1.spec)
  );
  Assert.equal(Services.cookies.countCookiesFromHost(uri1.host), 1);

  // Enter private browsing mode, set a cookie for host 2, and check the counts.
  var chan1 = make_channel(uri1.spec);
  chan1.QueryInterface(Ci.nsIPrivateBrowsingChannel);
  chan1.setPrivate(true);

  var chan2 = make_channel(uri2.spec);
  chan2.QueryInterface(Ci.nsIPrivateBrowsingChannel);
  chan2.setPrivate(true);

  Services.cookiesvc.setCookieStringFromHttp(
    uri2,
    "oh=hai; max-age=1000",
    chan2
  );
  Assert.equal(await getCookieStringFromPrivateDocument(uri1.spec), "");
  Assert.equal(await getCookieStringFromPrivateDocument(uri2.spec), "oh=hai");

  // Remove cookies and check counts.
  Services.obs.notifyObservers(null, "last-pb-context-exited");
  Assert.equal(await getCookieStringFromPrivateDocument(uri1.spec), "");
  Assert.equal(await getCookieStringFromPrivateDocument(uri2.spec), "");

  Services.cookiesvc.setCookieStringFromHttp(
    uri2,
    "oh=hai; max-age=1000",
    chan2
  );
  Assert.equal(await getCookieStringFromPrivateDocument(uri2.spec), "oh=hai");

  // Leave private browsing mode and check counts.
  Services.obs.notifyObservers(null, "last-pb-context-exited");
  Assert.equal(Services.cookies.countCookiesFromHost(uri1.host), 1);
  Assert.equal(Services.cookies.countCookiesFromHost(uri2.host), 0);

  // Fake a profile change.
  await promise_close_profile();
  do_load_profile();

  // Check that the right cookie persisted.
  Assert.equal(Services.cookies.countCookiesFromHost(uri1.host), 1);
  Assert.equal(Services.cookies.countCookiesFromHost(uri2.host), 0);

  // Enter private browsing mode, set a cookie for host 2, and check the counts.
  Assert.equal(await getCookieStringFromPrivateDocument(uri1.spec), "");
  Assert.equal(await getCookieStringFromPrivateDocument(uri2.spec), "");
  Services.cookiesvc.setCookieStringFromHttp(
    uri2,
    "oh=hai; max-age=1000",
    chan2
  );
  Assert.equal(await getCookieStringFromPrivateDocument(uri2.spec), "oh=hai");

  // Fake a profile change.
  await promise_close_profile();
  do_load_profile();

  // We're still in private browsing mode, but should have a new session.
  // Check counts.
  Assert.equal(await getCookieStringFromPrivateDocument(uri1.spec), "");
  Assert.equal(await getCookieStringFromPrivateDocument(uri2.spec), "");

  // Leave private browsing mode and check counts.
  Services.obs.notifyObservers(null, "last-pb-context-exited");
  Assert.equal(Services.cookies.countCookiesFromHost(uri1.host), 1);
  Assert.equal(Services.cookies.countCookiesFromHost(uri2.host), 0);

  // Enter private browsing mode.

  // Fake a profile change, but wait for async read completion.
  await promise_close_profile();
  await promise_load_profile();

  // We're still in private browsing mode, but should have a new session.
  // Check counts.
  Assert.equal(await getCookieStringFromPrivateDocument(uri1.spec), "");
  Assert.equal(await getCookieStringFromPrivateDocument(uri2.spec), "");

  // Leave private browsing mode and check counts.
  Services.obs.notifyObservers(null, "last-pb-context-exited");
  Assert.equal(Services.cookies.countCookiesFromHost(uri1.host), 1);
  Assert.equal(Services.cookies.countCookiesFromHost(uri2.host), 0);

  // Let's release the last PB window.
  privateBrowsingHolder.close();
  Services.prefs.clearUserPref("dom.security.https_first");
});
