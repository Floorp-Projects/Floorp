/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// test third party cookie blocking, for the cases:
// 1) with null channel
// 2) with channel, but with no docshell parent

"use strict";

add_task(async () => {
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  Services.prefs.setBoolPref("dom.security.https_first", false);

  // Bug 1617611 - Fix all the tests broken by "cookies SameSite=Lax by default"
  Services.prefs.setBoolPref("network.cookie.sameSite.laxByDefault", false);

  CookieXPCShellUtils.createServer({
    hosts: ["foo.com", "bar.com", "third.com"],
  });

  function createChannel(uri, principal = null) {
    const channel = NetUtil.newChannel({
      uri,
      loadingPrincipal:
        principal ||
        Services.scriptSecurityManager.createContentPrincipal(uri, {}),
      securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
    });

    return channel.QueryInterface(Ci.nsIHttpChannelInternal);
  }

  // Create URIs and channels pointing to foo.com and bar.com.
  // We will use these to put foo.com into first and third party contexts.
  let spec1 = "http://foo.com/foo.html";
  let spec2 = "http://bar.com/bar.html";
  let uri1 = NetUtil.newURI(spec1);
  let uri2 = NetUtil.newURI(spec2);

  // test with cookies enabled
  {
    Services.prefs.setIntPref(
      "network.cookie.cookieBehavior",
      Ci.nsICookieService.BEHAVIOR_ACCEPT
    );

    let channel1 = createChannel(uri1);
    let channel2 = createChannel(uri2);

    await do_set_cookies(uri1, channel1, true, [1, 2]);
    Services.cookies.removeAll();
    await do_set_cookies(uri1, channel2, true, [1, 2]);
    Services.cookies.removeAll();
  }

  // test with third party cookies blocked
  {
    Services.prefs.setIntPref(
      "network.cookie.cookieBehavior",
      Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN
    );

    let channel1 = createChannel(uri1);
    let channel2 = createChannel(uri2);

    await do_set_cookies(uri1, channel1, true, [0, 1]);
    Services.cookies.removeAll();
    await do_set_cookies(uri1, channel2, true, [0, 0]);
    Services.cookies.removeAll();
  }

  // test with third party cookies blocked using system principal
  {
    Services.prefs.setIntPref(
      "network.cookie.cookieBehavior",
      Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN
    );

    let channel1 = createChannel(
      uri1,
      Services.scriptSecurityManager.getSystemPrincipal()
    );
    let channel2 = createChannel(
      uri2,
      Services.scriptSecurityManager.getSystemPrincipal()
    );

    await do_set_cookies(uri1, channel1, true, [0, 1]);
    Services.cookies.removeAll();
    await do_set_cookies(uri1, channel2, true, [0, 0]);
    Services.cookies.removeAll();
  }

  // Force the channel URI to be used when determining the originating URI of
  // the channel.
  // test with third party cookies blocked

  // test with cookies enabled
  {
    Services.prefs.setIntPref(
      "network.cookie.cookieBehavior",
      Ci.nsICookieService.BEHAVIOR_ACCEPT
    );

    let channel1 = createChannel(uri1);
    channel1.forceAllowThirdPartyCookie = true;

    let channel2 = createChannel(uri2);
    channel2.forceAllowThirdPartyCookie = true;

    await do_set_cookies(uri1, channel1, true, [1, 2]);
    Services.cookies.removeAll();
    await do_set_cookies(uri1, channel2, true, [1, 2]);
    Services.cookies.removeAll();
  }

  // test with third party cookies blocked
  {
    Services.prefs.setIntPref(
      "network.cookie.cookieBehavior",
      Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN
    );

    let channel1 = createChannel(uri1);
    channel1.forceAllowThirdPartyCookie = true;

    let channel2 = createChannel(uri2);
    channel2.forceAllowThirdPartyCookie = true;

    await do_set_cookies(uri1, channel1, true, [0, 1]);
    Services.cookies.removeAll();
    await do_set_cookies(uri1, channel2, true, [0, 0]);
    Services.cookies.removeAll();
  }

  // test with third party cookies limited
  {
    Services.prefs.setIntPref(
      "network.cookie.cookieBehavior",
      Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN
    );

    let channel1 = createChannel(uri1);
    channel1.forceAllowThirdPartyCookie = true;

    let channel2 = createChannel(uri2);
    channel2.forceAllowThirdPartyCookie = true;

    await do_set_cookies(uri1, channel1, true, [0, 1]);
    Services.cookies.removeAll();
    await do_set_cookies(uri1, channel2, true, [0, 0]);
    Services.cookies.removeAll();
    do_set_single_http_cookie(uri1, channel1, 1);
    await do_set_cookies(uri1, channel2, true, [1, 2]);
    Services.cookies.removeAll();
  }
  Services.prefs.clearUserPref("dom.security.https_first");
  Services.prefs.clearUserPref("network.cookie.sameSite.laxByDefault");
});
