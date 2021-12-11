// Test bug 1683176
//
// Summary:
//   Test the case when a channel is cancelled when "negotiate" authentication
//   is processing.
//

"use strict";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

let prefs;
let httpserv;

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserv.identity.primaryPort;
});

XPCOMUtils.defineLazyGetter(this, "PORT", function() {
  return httpserv.identity.primaryPort;
});

function makeChan(url, loadingUrl) {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var ssm = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(
    Ci.nsIScriptSecurityManager
  );
  var principal = ssm.createContentPrincipal(ios.newURI(loadingUrl), {});
  return NetUtil.newChannel({
    uri: url,
    loadingPrincipal: principal,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
  });
}

function authHandler(metadata, response) {
  var body = "blablabla";

  response.seizePower();
  response.write("HTTP/1.1 401 Unauthorized\r\n");
  response.write("WWW-Authenticate: Negotiate\r\n");
  response.write("WWW-Authenticate: Basic realm=test\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function setup() {
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  prefs.setIntPref("network.auth.subresource-http-auth-allow", 2);
  prefs.setStringPref("network.negotiate-auth.trusted-uris", "localhost");

  httpserv = new HttpServer();
  httpserv.registerPathHandler("/auth", authHandler);
  httpserv.start(-1);
}

setup();
registerCleanupFunction(async () => {
  prefs.clearUserPref("network.auth.subresource-http-auth-allow");
  prefs.clearUserPref("network.negotiate-auth.trusted-uris");
  await httpserv.stop();
});

function makeChan(url) {
  let chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  return chan;
}

function channelOpenPromise(chan) {
  return new Promise(resolve => {
    let topic = "http-on-transaction-suspended-authentication";
    let observer = {
      QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
      observe(aSubject, aTopic, aData) {
        if (aTopic == topic) {
          Services.obs.removeObserver(observer, topic);
          let channel = aSubject.QueryInterface(Ci.nsIChannel);
          channel.cancel(Cr.NS_BINDING_ABORTED);
          resolve();
        }
      },
    };
    Services.obs.addObserver(observer, topic);

    chan.asyncOpen(new ChannelListener(finish, null, CL_EXPECT_FAILURE));
    function finish() {
      resolve();
    }
  });
}

add_task(async function testCancelAuthentication() {
  let chan = makeChan(URL + "/auth", URL);
  await channelOpenPromise(chan);
  Assert.equal(chan.status, Cr.NS_BINDING_ABORTED);
});
