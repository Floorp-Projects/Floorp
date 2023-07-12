/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

var httpserver = new HttpServer();
var earlyhintspath = "/earlyhints";
var multipleearlyhintspath = "/multipleearlyhintspath";
var otherearlyhintspath = "/otherearlyhintspath";
var noearlyhintspath = "/noearlyhints";
var httpbody = "0123456789";
var hint1 = "</style.css>; rel=preload; as=style";
var hint2 = "</img.png>; rel=preload; as=image";

function earlyHintsResponse(metadata, response) {
  response.setInformationalResponseStatusLine(
    metadata.httpVersion,
    103,
    "EarlyHints"
  );
  response.setInformationalResponseHeader("Link", hint1, false);

  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
}

function multipleEarlyHintsResponse(metadata, response) {
  response.setInformationalResponseStatusLine(
    metadata.httpVersion,
    103,
    "EarlyHints"
  );
  response.setInformationalResponseHeader("Link", hint1, false);
  response.setInformationalHeaderNoCheck("Link", hint2);

  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
}

function otherHeadersEarlyHintsResponse(metadata, response) {
  response.setInformationalResponseStatusLine(
    metadata.httpVersion,
    103,
    "EarlyHints"
  );
  response.setInformationalResponseHeader("Link", hint1, false);
  response.setInformationalResponseHeader("Something", "something", false);

  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
}

function noEarlyHintsResponse(metadata, response) {
  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
}

let EarlyHintsListener = function () {};

EarlyHintsListener.prototype = {
  _expected_hints: "",
  earlyHintsReceived: false,

  earlyHint: function testEarlyHint(header) {
    Assert.equal(header, this._expected_hints);
    this.earlyHintsReceived = true;
  },
};

function chanPromise(uri, listener) {
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
      .QueryInterface(Ci.nsIHttpChannelInternal)
      .setEarlyHintObserver(listener);
    chan.asyncOpen(new ChannelListener(resolve));
  });
}

add_task(async function setup() {
  httpserver.registerPathHandler(earlyhintspath, earlyHintsResponse);
  httpserver.registerPathHandler(
    multipleearlyhintspath,
    multipleEarlyHintsResponse
  );
  httpserver.registerPathHandler(
    otherearlyhintspath,
    otherHeadersEarlyHintsResponse
  );
  httpserver.registerPathHandler(noearlyhintspath, noEarlyHintsResponse);
  httpserver.start(-1);
});

add_task(async function early_hints() {
  let earlyHints = new EarlyHintsListener();
  earlyHints._expected_hints = hint1;

  await chanPromise(
    "http://localhost:" + httpserver.identity.primaryPort + earlyhintspath,
    earlyHints
  );
  Assert.ok(earlyHints.earlyHintsReceived);
});

add_task(async function no_early_hints() {
  let earlyHints = new EarlyHintsListener("");

  await chanPromise(
    "http://localhost:" + httpserver.identity.primaryPort + noearlyhintspath,
    earlyHints
  );
  Assert.ok(!earlyHints.earlyHintsReceived);
});

add_task(async function early_hints_multiple() {
  let earlyHints = new EarlyHintsListener();
  earlyHints._expected_hints = hint1 + ", " + hint2;

  await chanPromise(
    "http://localhost:" +
      httpserver.identity.primaryPort +
      multipleearlyhintspath,
    earlyHints
  );
  Assert.ok(earlyHints.earlyHintsReceived);
});

add_task(async function early_hints_other() {
  let earlyHints = new EarlyHintsListener();
  earlyHints._expected_hints = hint1;

  await chanPromise(
    "http://localhost:" + httpserver.identity.primaryPort + otherearlyhintspath,
    earlyHints
  );
  Assert.ok(earlyHints.earlyHintsReceived);
});

add_task(async function early_hints_only_secure_context() {
  Services.prefs.setBoolPref("network.proxy.allow_hijacking_localhost", true);
  let earlyHints2 = new EarlyHintsListener();
  earlyHints2._expected_hints = "";

  await chanPromise(
    "http://localhost:" + httpserver.identity.primaryPort + earlyhintspath,
    earlyHints2
  );
  Assert.ok(!earlyHints2.earlyHintsReceived);
});

add_task(async function clean_up() {
  Services.prefs.clearUserPref("network.proxy.allow_hijacking_localhost");
  await new Promise(resolve => {
    httpserver.stop(resolve);
  });
});
