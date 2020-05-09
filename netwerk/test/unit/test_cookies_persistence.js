/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// test for cookie persistence across sessions, for the cases:
// 1) network.cookie.lifetimePolicy = 0 (expire naturally)
// 2) network.cookie.lifetimePolicy = 2 (expire at end of session)

"use strict";

add_task(async () => {
  // Set up a profile.
  let profile = do_get_profile();

  CookieXPCShellUtils.createServer({
    hosts: ["foo.com", "bar.com", "third.com"],
  });

  // Create URIs and channels pointing to foo.com and bar.com.
  // We will use these to put foo.com into first and third party contexts.
  var spec1 = "http://foo.com/foo.html";
  var spec2 = "http://bar.com/bar.html";
  var uri1 = NetUtil.newURI(spec1);
  var uri2 = NetUtil.newURI(spec2);
  var channel1 = NetUtil.newChannel({
    uri: uri1,
    loadUsingSystemPrincipal: true,
  });
  var channel2 = NetUtil.newChannel({
    uri: uri2,
    loadUsingSystemPrincipal: true,
  });

  // Force the channel URI to be used when determining the originating URI of
  // the channel.
  var httpchannel1 = channel1.QueryInterface(Ci.nsIHttpChannelInternal);
  var httpchannel2 = channel1.QueryInterface(Ci.nsIHttpChannelInternal);
  httpchannel1.forceAllowThirdPartyCookie = true;
  httpchannel2.forceAllowThirdPartyCookie = true;

  // test with cookies enabled, and third party cookies persistent.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  Services.prefs.setBoolPref("network.cookie.thirdparty.sessionOnly", false);
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  await do_set_cookies(uri1, channel1, false, [1, 2]);
  await do_set_cookies(uri2, channel2, true, [1, 2]);

  // fake a profile change
  await promise_close_profile();
  do_load_profile();
  Assert.equal(Services.cookies.countCookiesFromHost(uri1.host), 2);
  Assert.equal(Services.cookies.countCookiesFromHost(uri2.host), 0);

  // Again, but don't wait for the async close to complete. This should always
  // work, since we blocked on close above and haven't kicked off any writes
  // since then.
  await promise_close_profile();
  do_load_profile();
  Assert.equal(Services.cookies.countCookiesFromHost(uri1.host), 2);
  Assert.equal(Services.cookies.countCookiesFromHost(uri2.host), 0);

  // test with cookies set to session-only
  Services.prefs.setIntPref("network.cookie.lifetimePolicy", 2);
  Services.cookies.removeAll();
  await do_set_cookies(uri1, channel1, false, [1, 2]);
  await do_set_cookies(uri2, channel2, true, [1, 2]);

  // fake a profile change
  await promise_close_profile();
  do_load_profile();
  Assert.equal(Services.cookies.countCookiesFromHost(uri1.host), 0);
  Assert.equal(Services.cookies.countCookiesFromHost(uri2.host), 0);
});
