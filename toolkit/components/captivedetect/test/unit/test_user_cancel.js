/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const kInterfaceName = "wifi";

var server;
var step = 0;

function xhr_handler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/plain", false);
  response.write("false");
}

function fakeUIResponse() {
  Services.obs.addObserver(function observe(subject, topic, data) {
    if (topic === "captive-portal-login") {
      let xhr = new XMLHttpRequest();
      xhr.open("GET", gServerURL + kCanonicalSitePath, true);
      xhr.send();
      Assert.equal(++step, 2);
      let details = JSON.parse(data);
      gCaptivePortalDetector.cancelLogin(details.id);
    }
  }, "captive-portal-login");
}

function test_cancel() {
  do_test_pending();

  let callback = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsICaptivePortalCallback]),
    prepare: function prepare() {
      Assert.equal(++step, 1);
      gCaptivePortalDetector.finishPreparation(kInterfaceName);
    },
    complete: function complete(success) {
      Assert.equal(++step, 3);
      Assert.ok(!success);
      gServer.stop(do_test_finished);
    },
  };

  gCaptivePortalDetector.checkCaptivePortal(kInterfaceName, callback);
}

function run_test() {
  run_captivedetect_test(xhr_handler, fakeUIResponse, test_cancel);
}
