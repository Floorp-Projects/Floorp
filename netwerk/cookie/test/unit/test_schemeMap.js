const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

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
});
