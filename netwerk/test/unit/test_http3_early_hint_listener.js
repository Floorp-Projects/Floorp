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

const earlyhintspath = "/103_response";
const hint1 = "</style.css>; rel=preload; as=style";
const hint2 = "</img.png>; rel=preload; as=image";

let EarlyHintsListener = function () {};

EarlyHintsListener.prototype = {
  _expected_hints: [],
  earlyHintsReceived: 0,

  QueryInterface: ChromeUtils.generateQI(["nsIEarlyHintObserver"]),

  earlyHint(header) {
    Assert.ok(this._expected_hints.includes(header));
    this.earlyHintsReceived += 1;
  },
};

function chanPromise(uri, listener, headers) {
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
  chan.QueryInterface(Ci.nsIHttpChannelInternal).setEarlyHintObserver(listener);

  return promiseAsyncOpen(chan);
}

registerCleanupFunction(async () => {
  http3_clear_prefs();
});

add_task(async function setup() {
  await http3_setup_tests("h3-29");
});

add_task(async function early_hints() {
  let earlyHints = new EarlyHintsListener();
  earlyHints._expected_hints = [hint1];

  await chanPromise(
    `https://foo.example.com${earlyhintspath}`,
    earlyHints,
    hint1
  );
  Assert.equal(earlyHints.earlyHintsReceived, 1);
});

// Test when there is no Link header in a 103 response.
// 103 response will contain non-link headers.
add_task(async function no_early_hints() {
  let earlyHints = new EarlyHintsListener();

  await chanPromise(`https://foo.example.com${earlyhintspath}`, earlyHints, "");
  Assert.equal(earlyHints.earlyHintsReceived, 0);
});

add_task(async function early_hints_multiple() {
  let earlyHints = new EarlyHintsListener();
  earlyHints._expected_hints = [hint1, hint2];

  await chanPromise(
    `https://foo.example.com${earlyhintspath}`,
    earlyHints,
    hint1 + ", " + hint2
  );
  Assert.equal(earlyHints.earlyHintsReceived, 2);
});
