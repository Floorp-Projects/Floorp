/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The server will always respond with a 103 EarlyHint followed by a
// 200 response.
// 103 response contains:
//  1) a non-link header
//  2) a link header if a request has a "link-to-set" header. If the
//     request header is not set, the response will not have Link headers.
//     A "link-to-set" header may contain multiple link headers
//     separated with a comma.

var earlyhintspath = "/103_response";
var hint1 = "</style.css>; rel=preload; as=style";
var hint2 = "</img.png>; rel=preload; as=image";

let EarlyHintsListener = function () {};

EarlyHintsListener.prototype = {
  _expected_hints: "",
  earlyHintsReceived: false,

  QueryInterface: ChromeUtils.generateQI(["nsIEarlyHintObserver"]),

  earlyHint: function testEarlyHint(header) {
    Assert.equal(header, this._expected_hints);
    this.earlyHintsReceived = true;
  },
};

function chanPromise(uri, listener, headers) {
  return new Promise(resolve => {
    var principal = Services.scriptSecurityManager.createContentPrincipal(
      NetUtil.newURI(uri),
      {}
    );
    var chan = NetUtil.newChannel({
      uri,
      loadingPrincipal: principal,
      securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    });

    chan
      .QueryInterface(Ci.nsIHttpChannel)
      .setRequestHeader("link-to-set", headers, false);
    chan
      .QueryInterface(Ci.nsIHttpChannelInternal)
      .setEarlyHintObserver(listener);
    chan.asyncOpen(new ChannelListener(resolve));
  });
}

let http2Port;

add_task(async function setup() {
  Services.prefs.setCharPref("network.dns.localDomains", "foo.example.com");
  http2Port = Services.env.get("MOZHTTP2_PORT");
  Assert.notEqual(http2Port, null);
  Assert.notEqual(http2Port, "");

  do_get_profile();
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");
});

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("network.dns.localDomains");
});

add_task(async function early_hints() {
  let earlyHints = new EarlyHintsListener();
  earlyHints._expected_hints = hint1;

  await chanPromise(
    `https://foo.example.com:${http2Port}${earlyhintspath}`,
    earlyHints,
    hint1
  );
  Assert.ok(earlyHints.earlyHintsReceived);
});

// Test when there is no Link header in a 103 response.
// 103 response will contain non-link headers.
add_task(async function no_early_hints() {
  let earlyHints = new EarlyHintsListener("");

  await chanPromise(
    `https://foo.example.com:${http2Port}${earlyhintspath}`,
    earlyHints,
    ""
  );
  Assert.ok(!earlyHints.earlyHintsReceived);
});

add_task(async function early_hints_multiple() {
  let earlyHints = new EarlyHintsListener();
  earlyHints._expected_hints = hint1 + ", " + hint2;

  await chanPromise(
    `https://foo.example.com:${http2Port}${earlyhintspath}`,
    earlyHints,
    hint1 + ", " + hint2
  );
  Assert.ok(earlyHints.earlyHintsReceived);
});
