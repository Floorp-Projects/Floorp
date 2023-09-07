const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);

const URI = Services.io.newURI("http://example.org/");

const { COOKIE_CHANGED, COOKIE_ADDED } = Ci.nsICookieNotification;

function run_test() {
  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  // Clear cookies.
  Services.cookies.removeAll();

  // Add a new cookie.
  setCookie("foo=bar", {
    type: COOKIE_ADDED,
    isSession: true,
    isSecure: false,
    isHttpOnly: false,
  });

  // Update cookie with isHttpOnly=true.
  setCookie("foo=bar; HttpOnly", {
    type: COOKIE_CHANGED,
    isSession: true,
    isSecure: false,
    isHttpOnly: true,
  });

  // Update cookie with isSecure=true.
  setCookie("foo=bar; Secure", {
    type: COOKIE_CHANGED,
    isSession: true,
    isSecure: true,
    isHttpOnly: false,
  });

  // Update cookie with isSession=false.
  let expiry = new Date();
  expiry.setUTCFullYear(expiry.getUTCFullYear() + 2);
  setCookie(`foo=bar; Expires=${expiry.toGMTString()}`, {
    type: COOKIE_CHANGED,
    isSession: false,
    isSecure: false,
    isHttpOnly: false,
  });

  // Reset cookie.
  setCookie("foo=bar", {
    type: COOKIE_CHANGED,
    isSession: true,
    isSecure: false,
    isHttpOnly: false,
  });
}

function setCookie(value, expected) {
  function setCookieInternal(valueInternal, expectedInternal = null) {
    function observer(subject) {
      if (!expectedInternal) {
        do_throw("no notification expected");
        return;
      }

      let notification = subject.QueryInterface(Ci.nsICookieNotification);

      // Check we saw the right notification.
      Assert.equal(notification.action, expectedInternal.type);

      // Check cookie details.
      let cookie = notification.cookie.QueryInterface(Ci.nsICookie);
      Assert.equal(cookie.isSession, expectedInternal.isSession);
      Assert.equal(cookie.isSecure, expectedInternal.isSecure);
      Assert.equal(cookie.isHttpOnly, expectedInternal.isHttpOnly);
    }

    Services.obs.addObserver(observer, "cookie-changed");

    let channel = NetUtil.newChannel({
      uri: URI,
      loadUsingSystemPrincipal: true,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    });

    Services.cookies.setCookieStringFromHttp(URI, valueInternal, channel);
    Services.obs.removeObserver(observer, "cookie-changed");
  }

  // Check that updating/inserting the cookie works.
  setCookieInternal(value, expected);

  // Check that we ignore identical cookies.
  setCookieInternal(value);
}
