/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

const { CookieXPCShellUtils } = ChromeUtils.import(
  "resource://testing-common/CookieXPCShellUtils.jsm"
);

CookieXPCShellUtils.init(this);

function Requestor() {}
Requestor.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIInterfaceRequestor",
    "nsIAuthPrompt2",
  ]),

  getInterface(iid) {
    if (iid.equals(Ci.nsIAuthPrompt2)) {
      return this;
    }

    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  },

  promptAuth(channel, level, authInfo) {
    Assert.equal("secret", authInfo.realm);
    // No passwords in the URL -> nothing should be prefilled
    Assert.equal(authInfo.username, "");
    Assert.equal(authInfo.password, "");
    Assert.equal(authInfo.domain, "");

    authInfo.username = "guest";
    authInfo.password = "guest";

    return true;
  },

  asyncPromptAuth(chan, cb, ctx, lvl, info) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },
};

let observer = channel => {
  if (
    !(channel instanceof Ci.nsIHttpChannel && channel.URI.host === "localhost")
  ) {
    return;
  }
  channel.notificationCallbacks = new Requestor();
};
Services.obs.addObserver(observer, "http-on-modify-request");

function makeChan(url, loadingUrl) {
  var principal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI(loadingUrl),
    {}
  );
  return NetUtil.newChannel({
    uri: url,
    loadingPrincipal: principal,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
  });
}

add_task(async () => {
  do_get_profile();

  Services.prefs.setBoolPref("network.predictor.enabled", false);
  Services.prefs.setBoolPref("network.predictor.enable-prefetch", false);
  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  Services.prefs.setIntPref("network.auth.subresource-http-auth-allow", 2);

  for (let test of [true, false]) {
    Cc["@mozilla.org/network/http-auth-manager;1"]
      .getService(Ci.nsIHttpAuthManager)
      .clearAll();

    await new Promise(resolve =>
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve)
    );

    info("Enabling network state partitioning");
    Services.prefs.setBoolPref("privacy.partition.network_state", test);

    const httpserv = new HttpServer();
    httpserv.registerPathHandler("/auth", (metadata, response) => {
      // btoa("guest:guest"), but that function is not available here
      const expectedHeader = "Basic Z3Vlc3Q6Z3Vlc3Q=";

      let body;
      if (
        metadata.hasHeader("Authorization") &&
        metadata.getHeader("Authorization") == expectedHeader
      ) {
        response.setStatusLine(metadata.httpVersion, 200, "OK, authorized");
        response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);

        body = "success";
      } else {
        // didn't know guest:guest, failure
        response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
        response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);

        body = "failed";
      }

      response.bodyOutputStream.write(body, body.length);
    });

    httpserv.start(-1);
    const URL = "http://localhost:" + httpserv.identity.primaryPort;

    const httpHandler = Cc[
      "@mozilla.org/network/protocol;1?name=http"
    ].getService(Ci.nsIHttpProtocolHandler);

    const contentPage = await CookieXPCShellUtils.loadContentPage(
      URL + "/auth?r=" + Math.random()
    );
    await contentPage.close();

    let key;
    if (test) {
      key = `^partitionKey=%28http%2Clocalhost%2C${httpserv.identity.primaryPort}%29:http://localhost:${httpserv.identity.primaryPort}`;
    } else {
      key = `:http://localhost:${httpserv.identity.primaryPort}`;
    }

    Assert.equal(httpHandler.authCacheKeys.includes(key), true, "Key found!");

    await new Promise(resolve => httpserv.stop(resolve));
  }
});
