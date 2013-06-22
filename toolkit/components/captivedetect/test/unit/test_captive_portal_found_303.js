/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const kInterfaceName = 'wifi';

var server;
var step = 0;
var loginFinished = false;

function xhr_handler(metadata, response) {
  if (loginFinished) {
    response.setStatusLine(metadata.httpVersion, 200, 'OK');
    response.setHeader('Cache-Control', 'no-cache', false);
    response.setHeader('Content-Type', 'text/plain', false);
    response.write('true');
  } else {
    response.setStatusLine(metadata.httpVersion, 303, "See Other");
    response.setHeader("Location", "http://example.org/", false);
    response.setHeader("Content-Type", "text/html", false);
  }
}

function fakeUIResponse() {
  Services.obs.addObserver(function observe(subject, topic, data) {
    if (topic === 'captive-portal-login') {
      let xhr = Cc['@mozilla.org/xmlextras/xmlhttprequest;1']
                  .createInstance(Ci.nsIXMLHttpRequest);
      xhr.open('GET', gServerURL + kCanonicalSitePath, true);
      xhr.send();
      loginFinished = true;
      do_check_eq(++step, 2);
    }
  }, 'captive-portal-login', false);
}

function test_portal_found() {
  do_test_pending();

  let callback = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsICaptivePortalCallback]),
    prepare: function prepare() {
      do_check_eq(++step, 1);
      gCaptivePortalDetector.finishPreparation(kInterfaceName);
    },
    complete: function complete(success) {
      do_check_eq(++step, 3);
      do_check_true(success);
      gServer.stop(do_test_finished);
    },
  };

  gCaptivePortalDetector.checkCaptivePortal(kInterfaceName, callback);
}

function run_test() {
  run_captivedetect_test(xhr_handler, fakeUIResponse, test_portal_found);

  server = new HttpServer();
  server.registerPathHandler(kCanonicalSitePath, xhr_handler);
  server.start(4444);

  fakeUIResponse();

  test_portal_found();
}
