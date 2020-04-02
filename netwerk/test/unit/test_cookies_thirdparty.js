/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// test third party cookie blocking, for the cases:
// 1) with null channel
// 2) with channel, but with no docshell parent

function run_test() {
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  Services.prefs.setBoolPref(
    "network.cookie.rejectForeignWithExceptions.enabled",
    false
  );

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

    let channel1 = NetUtil.newChannel({
      uri: uri1,
      loadUsingSystemPrincipal: true,
    });
    let channel2 = NetUtil.newChannel({
      uri: uri2,
      loadUsingSystemPrincipal: true,
    });

    do_set_cookies(uri1, channel1, true, [1, 2, 3, 4]);
    Services.cookies.removeAll();
    do_set_cookies(uri1, channel2, true, [1, 2, 3, 4]);
    Services.cookies.removeAll();
  }

  // test with third party cookies blocked
  {
    Services.prefs.setIntPref(
      "network.cookie.cookieBehavior",
      Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN
    );

    let channel1 = NetUtil.newChannel({
      uri: uri1,
      loadUsingSystemPrincipal: true,
    });
    let channel2 = NetUtil.newChannel({
      uri: uri2,
      loadUsingSystemPrincipal: true,
    });

    do_set_cookies(uri1, channel1, true, [0, 0, 0, 0]);
    Services.cookies.removeAll();
    do_set_cookies(uri1, channel2, true, [0, 0, 0, 0]);
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

    let channel1 = NetUtil.newChannel({
      uri: uri1,
      loadUsingSystemPrincipal: true,
    });
    let httpchannel1 = channel1.QueryInterface(Ci.nsIHttpChannelInternal);
    httpchannel1.forceAllowThirdPartyCookie = true;

    let channel2 = NetUtil.newChannel({
      uri: uri2,
      loadUsingSystemPrincipal: true,
    });
    let httpchannel2 = channel2.QueryInterface(Ci.nsIHttpChannelInternal);
    httpchannel2.forceAllowThirdPartyCookie = true;

    do_set_cookies(uri1, channel1, true, [1, 2, 3, 4]);
    Services.cookies.removeAll();
    do_set_cookies(uri1, channel2, true, [1, 2, 3, 4]);
    Services.cookies.removeAll();
  }

  // test with third party cookies blocked
  {
    Services.prefs.setIntPref(
      "network.cookie.cookieBehavior",
      Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN
    );

    let channel1 = NetUtil.newChannel({
      uri: uri1,
      loadUsingSystemPrincipal: true,
    });
    let httpchannel1 = channel1.QueryInterface(Ci.nsIHttpChannelInternal);
    httpchannel1.forceAllowThirdPartyCookie = true;

    let channel2 = NetUtil.newChannel({
      uri: uri2,
      loadUsingSystemPrincipal: true,
    });
    let httpchannel2 = channel2.QueryInterface(Ci.nsIHttpChannelInternal);
    httpchannel2.forceAllowThirdPartyCookie = true;

    do_set_cookies(uri1, channel1, true, [0, 1, 1, 2]);
    Services.cookies.removeAll();
    do_set_cookies(uri1, channel2, true, [0, 0, 0, 0]);
    Services.cookies.removeAll();
  }

  // test with third party cookies limited
  {
    Services.prefs.setIntPref(
      "network.cookie.cookieBehavior",
      Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN
    );

    let channel1 = NetUtil.newChannel({
      uri: uri1,
      loadUsingSystemPrincipal: true,
    });
    let httpchannel1 = channel1.QueryInterface(Ci.nsIHttpChannelInternal);
    httpchannel1.forceAllowThirdPartyCookie = true;

    let channel2 = NetUtil.newChannel({
      uri: uri2,
      loadUsingSystemPrincipal: true,
    });
    let httpchannel2 = channel2.QueryInterface(Ci.nsIHttpChannelInternal);
    httpchannel2.forceAllowThirdPartyCookie = true;

    do_set_cookies(uri1, channel1, true, [0, 1, 2, 3]);
    Services.cookies.removeAll();
    do_set_cookies(uri1, channel2, true, [0, 0, 0, 0]);
    Services.cookies.removeAll();
    do_set_single_http_cookie(uri1, channel1, 1);
    do_set_cookies(uri1, channel2, true, [2, 3, 4, 5]);
    Services.cookies.removeAll();
  }
}
