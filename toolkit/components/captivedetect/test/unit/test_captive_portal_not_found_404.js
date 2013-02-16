/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://testing-common/httpd.js');

const kInterfaceName = 'wifi';

XPCOMUtils.defineLazyServiceGetter(this, 'gCaptivePortalDetector',
                                   '@mozilla.org/toolkit/captive-detector;1',
                                   'nsICaptivePortalDetector');
var server;
var step = 0;
var loginFinished = false;
var attempt = 0;

function xhr_handler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 404, "Page not Found");
  attempt++;
}

function fakeUIResponse() {
  Services.obs.addObserver(function observe(subject, topic, data) {
    if (topic === 'captive-portal-login') {
      do_throw('should not receive captive-portal-login event');
    }
  }, 'captive-portal-login', false);
}

function test_portal_not_found() {
  do_test_pending();

  let callback = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsICaptivePortalCallback]),
    prepare: function prepare() {
      do_check_eq(++step, 1);
      gCaptivePortalDetector.finishPreparation(kInterfaceName);
    },
    complete: function complete(success) {
      do_check_eq(++step, 2);
      do_check_true(success);
      do_check_eq(attempt, 6);
      server.stop(do_test_finished);
    },
  };

  gCaptivePortalDetector.checkCaptivePortal(kInterfaceName, callback);
}

function run_test() {
  server = new HttpServer();
  server.registerPathHandler(kCanonicalSitePath, xhr_handler);
  server.start(4444);

  fakeUIResponse();

  test_portal_not_found();
}
