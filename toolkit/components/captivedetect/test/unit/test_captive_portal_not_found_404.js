/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const kInterfaceName = "wifi";

var step = 0;
var attempt = 0;

function xhr_handler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 404, "Page not Found");
  attempt++;
}

function fakeUIResponse() {
  Services.obs.addObserver(function observe(subject, topic, data) {
    if (topic === "captive-portal-login") {
      do_throw("should not receive captive-portal-login event");
    }
  }, "captive-portal-login");
}

function test_portal_not_found() {
  do_test_pending();

  let callback = {
    QueryInterface: ChromeUtils.generateQI(["nsICaptivePortalCallback"]),
    prepare: function prepare() {
      Assert.equal(++step, 1);
      gCaptivePortalDetector.finishPreparation(kInterfaceName);
    },
    complete: function complete(success) {
      Assert.equal(++step, 2);
      Assert.ok(!success);
      Assert.equal(attempt, 6);
      gServer.stop(do_test_finished);
    },
  };

  gCaptivePortalDetector.checkCaptivePortal(kInterfaceName, callback);
}

function run_test() {
  run_captivedetect_test(xhr_handler, fakeUIResponse, test_portal_not_found);
}
