const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  Services,
  "cookiemgr",
  "@mozilla.org/cookiemanager;1",
  "nsICookieManager"
);

function inChildProcess() {
  return Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

add_task(async _ => {
  do_get_profile();

  // Allow all cookies if the pref service is available in this process.
  if (!inChildProcess()) {
    Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
    Services.prefs.setBoolPref(
      "network.cookieJarSettings.unblocked_for_testing",
      true
    );
  }

  let cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);

  info("Let's set a cookie from HTTP example.org");

  let uri = NetUtil.newURI("http://example.org/");
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );
  let channel = NetUtil.newChannel({
    uri,
    loadingPrincipal: principal,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
  });

  cs.setCookieStringFromHttp(uri, "a=b; sameSite=lax", channel);

  let cookies = Services.cookies.getCookiesFromHost("example.org", {});
  Assert.equal(cookies.length, 1, "We expect 1 cookie only");

  Assert.equal(cookies[0].schemeMap, Ci.nsICookie.SCHEME_HTTP, "HTTP Scheme");

  info("Let's set a cookie from HTTPS example.org");

  uri = NetUtil.newURI("https://example.org/");
  principal = Services.scriptSecurityManager.createContentPrincipal(uri, {});
  channel = NetUtil.newChannel({
    uri,
    loadingPrincipal: principal,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
  });

  cs.setCookieStringFromHttp(uri, "a=b; sameSite=lax", channel);

  cookies = Services.cookies.getCookiesFromHost("example.org", {});
  Assert.equal(cookies.length, 1, "We expect 1 cookie only");

  Assert.equal(
    cookies[0].schemeMap,
    Ci.nsICookie.SCHEME_HTTP | Ci.nsICookie.SCHEME_HTTPS,
    "HTTP + HTTPS Schemes"
  );

  Services.cookies.removeAll();
});

[true, false].forEach(schemefulComparison => {
  add_task(async () => {
    do_get_profile();

    // Allow all cookies if the pref service is available in this process.
    if (!inChildProcess()) {
      Services.prefs.setBoolPref(
        "network.cookie.sameSite.schemeful",
        schemefulComparison
      );
      Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
      Services.prefs.setBoolPref(
        "network.cookieJarSettings.unblocked_for_testing",
        true
      );
    }

    let cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);

    info("Let's set a cookie from HTTP example.org");

    let uri = NetUtil.newURI("http://example.org/");
    let principal = Services.scriptSecurityManager.createContentPrincipal(
      uri,
      {}
    );
    let channel = NetUtil.newChannel({
      uri,
      loadingPrincipal: principal,
      securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
    });

    cs.setCookieStringFromHttp(uri, "a=b; sameSite=lax", channel);

    let cookies = Services.cookies.getCookieStringFromHttp(uri, channel);
    Assert.equal(cookies, "a=b", "Cookies match");

    uri = NetUtil.newURI("https://example.org/");
    principal = Services.scriptSecurityManager.createContentPrincipal(uri, {});
    channel = NetUtil.newChannel({
      uri,
      loadingPrincipal: principal,
      securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
    });

    cookies = Services.cookies.getCookieStringFromHttp(uri, channel);
    if (schemefulComparison) {
      Assert.equal(cookies, "", "No cookie for different scheme!");
    } else {
      Assert.equal(cookies, "a=b", "Cookie even for different scheme!");
    }

    cookies = Services.cookies.getCookieStringForPrincipal(principal);
    if (schemefulComparison) {
      Assert.equal(cookies, "", "No cookie for different scheme!");
    } else {
      Assert.equal(cookies, "a=b", "Cookie even for different scheme!");
    }

    Services.cookies.removeAll();
  });
});

add_task(async _ => {
  do_get_profile();

  // Allow all cookies if the pref service is available in this process.
  if (!inChildProcess()) {
    Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
    Services.prefs.setBoolPref(
      "network.cookieJarSettings.unblocked_for_testing",
      true
    );
  }

  info("Let's set a cookie without scheme");
  Services.cookiemgr.add(
    "example.org",
    "/",
    "a",
    "b",
    false,
    false,
    false,
    Math.floor(Date.now() / 1000 + 1000),
    {},
    Ci.nsICookie.SAMESITE_LAX,
    Ci.nsICookie.SCHEME_UNSET
  );

  let cookies = Services.cookies.getCookiesFromHost("example.org", {});
  Assert.equal(cookies.length, 1, "We expect 1 cookie only");
  Assert.equal(cookies[0].schemeMap, Ci.nsICookie.SCHEME_UNSET, "Unset scheme");

  ["https", "http"].forEach(scheme => {
    let uri = NetUtil.newURI(scheme + "://example.org/");
    let principal = Services.scriptSecurityManager.createContentPrincipal(
      uri,
      {}
    );
    let channel = NetUtil.newChannel({
      uri,
      loadingPrincipal: principal,
      securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
    });

    cookies = Services.cookies.getCookieStringFromHttp(uri, channel);
    Assert.equal(cookies, "a=b", "Cookie for unset scheme");
  });

  Services.cookies.removeAll();
});
