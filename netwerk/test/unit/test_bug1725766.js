/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);
let hserv = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
  Ci.nsIHandlerService
);
let handlerInfo;
const testScheme = "x-moz-test";

function setup() {
  var handler = Cc["@mozilla.org/uriloader/web-handler-app;1"].createInstance(
    Ci.nsIWebHandlerApp
  );
  handler.name = testScheme;
  handler.uriTemplate = "http://test.mozilla.org/%s";

  var extps = Cc[
    "@mozilla.org/uriloader/external-protocol-service;1"
  ].getService(Ci.nsIExternalProtocolService);
  handlerInfo = extps.getProtocolHandlerInfo(testScheme);
  handlerInfo.possibleApplicationHandlers.appendElement(handler);

  hserv.store(handlerInfo);
  Assert.ok(extps.externalProtocolHandlerExists(testScheme));
}

setup();
registerCleanupFunction(() => {
  hserv.remove(handlerInfo);
});

function makeChan(url) {
  let chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

function channelOpenPromise(chan, flags) {
  return new Promise(resolve => {
    function finish(req, buffer) {
      resolve([req, buffer]);
    }
    let internal = chan.QueryInterface(Ci.nsIHttpChannelInternal);
    internal.setWaitForHTTPSSVCRecord();
    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

add_task(async function viewsourceExternalProtocol() {
  Assert.throws(
    () => makeChan(`view-source:${testScheme}:foo.example.com`),
    /NS_ERROR_MALFORMED_URI/
  );
});

add_task(async function viewsourceExternalProtocolRedirect() {
  let httpserv = new HttpServer();
  httpserv.registerPathHandler("/", function handler(metadata, response) {
    response.setStatusLine(metadata.httpVersion, 301, "Moved Permanently");
    response.setHeader("Location", `${testScheme}:foo@bar.com`, false);

    var body = "Moved\n";
    response.bodyOutputStream.write(body, body.length);
  });
  httpserv.start(-1);

  let chan = makeChan(
    `view-source:http://127.0.0.1:${httpserv.identity.primaryPort}/`
  );
  let [req] = await channelOpenPromise(chan, CL_EXPECT_FAILURE);
  Assert.equal(req.status, Cr.NS_ERROR_MALFORMED_URI);
  await httpserv.stop();
});
