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
const kOtherInterfaceName = 'ril';

XPCOMUtils.defineLazyServiceGetter(this, 'gCaptivePortalDetector',
                                   '@mozilla.org/toolkit/captive-detector;1',
                                   'nsICaptivePortalDetector');
var server;
var step = 0;
var loginFinished = false;

function xhr_handler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, 'OK');
  response.setHeader('Cache-Control', 'no-cache', false);
  response.setHeader('Content-Type', 'text/plain', false);
  if (loginFinished) {
    response.write('true');
  } else {
    response.write('false');
  }
}

function fakeUIResponse() {
  Services.obs.addObserver(function observe(subject, topic, data) {
    if (topic === 'captive-portal-login') {
      let xhr = Cc['@mozilla.org/xmlextras/xmlhttprequest;1']
                  .createInstance(Ci.nsIXMLHttpRequest);
      xhr.open('GET', kServerURL + kCanonicalSitePath, true);
      xhr.send();
      loginFinished = true;
      do_check_eq(++step, 2);
    }
  }, 'captive-portal-login', false);
}

function test_multiple_requests() {
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
    },
  };

  let otherCallback = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsICaptivePortalCallback]),
    prepare: function prepare() {
      do_check_eq(++step, 4);
      gCaptivePortalDetector.finishPreparation(kOtherInterfaceName);
    },
    complete: function complete(success) {
      do_check_eq(++step, 5);
      do_check_true(success);
      server.stop(do_test_finished);
    }
  };

  gCaptivePortalDetector.checkCaptivePortal(kInterfaceName, callback);
  gCaptivePortalDetector.checkCaptivePortal(kOtherInterfaceName, otherCallback);
}

function run_test() {
  server = new HttpServer();
  server.registerPathHandler(kCanonicalSitePath, xhr_handler);
  server.start(4444);

  fakeUIResponse();

  test_multiple_requests();
}
