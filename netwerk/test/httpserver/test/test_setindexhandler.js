/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Make sure setIndexHandler works as expected

var srv, serverBasePath;

function run_test() {
  srv = createServer();
  serverBasePath = do_get_profile();
  srv.registerDirectory("/", serverBasePath);
  srv.setIndexHandler(myIndexHandler);
  srv.start(-1);

  runHttpTests(tests, testComplete(srv));
}

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + srv.identity.primaryPort + "/";
});

XPCOMUtils.defineLazyGetter(this, "tests", function() {
  return [
    new Test(URL, init, startCustomIndexHandler, stopCustomIndexHandler),
    new Test(URL, init, startDefaultIndexHandler, stopDefaultIndexHandler),
  ];
});

function init(ch) {
  ch.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE; // important!
}
function startCustomIndexHandler(ch) {
  Assert.equal(ch.getResponseHeader("Content-Length"), "10");
  srv.setIndexHandler(null);
}
function stopCustomIndexHandler(ch, status, data) {
  Assert.ok(Components.isSuccessCode(status));
  Assert.equal(String.fromCharCode.apply(null, data), "directory!");
}

function startDefaultIndexHandler(ch) {
  Assert.equal(ch.responseStatus, 200);
}
function stopDefaultIndexHandler(ch, status, data) {
  Assert.ok(Components.isSuccessCode(status));
}

// PATH HANDLERS

function myIndexHandler(metadata, response) {
  var dir = metadata.getProperty("directory");
  Assert.ok(dir != null);
  Assert.ok(dir instanceof Ci.nsIFile);
  Assert.ok(dir.equals(serverBasePath));

  response.write("directory!");
}
