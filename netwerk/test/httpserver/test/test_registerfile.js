/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// tests the registerFile API

ChromeUtils.defineLazyGetter(this, "BASE", function () {
  return "http://localhost:" + srv.identity.primaryPort;
});

var file = do_get_file("test_registerfile.js");

function onStart(ch) {
  Assert.equal(ch.responseStatus, 200);
}

function onStop(ch, status, data) {
  // not sufficient for equality, but not likely to be wrong!
  Assert.equal(data.length, file.fileSize);
}

ChromeUtils.defineLazyGetter(this, "test", function () {
  return new Test(BASE + "/foo", null, onStart, onStop);
});

var srv;

function run_test() {
  srv = createServer();

  try {
    srv.registerFile("/foo", do_get_profile());
    throw new Error("registerFile succeeded!");
  } catch (e) {
    isException(e, Cr.NS_ERROR_INVALID_ARG);
  }

  srv.registerFile("/foo", file);
  srv.start(-1);

  runHttpTests([test], testComplete(srv));
}
