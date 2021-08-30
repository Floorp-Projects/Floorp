/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var {
  HTTP_400,
  HTTP_401,
  HTTP_402,
  HTTP_403,
  HTTP_404,
  HTTP_405,
  HTTP_406,
  HTTP_407,
  HTTP_408,
  HTTP_409,
  HTTP_410,
  HTTP_411,
  HTTP_412,
  HTTP_413,
  HTTP_414,
  HTTP_415,
  HTTP_417,
  HTTP_500,
  HTTP_501,
  HTTP_502,
  HTTP_503,
  HTTP_504,
  HTTP_505,
  HttpError,
  HttpServer,
} = ChromeUtils.import("resource://testing-common/httpd.js");

const fakeTelemetryService = {
  recordEvent: sinon.spy(),
};

const kCanonicalSitePath = "/canonicalSite.html";
const kCanonicalSiteContent = "true";
const kPrefsCanonicalURL = "captivedetect.canonicalURL";
const kPrefsCanonicalContent = "captivedetect.canonicalContent";
const kPrefsMaxWaitingTime = "captivedetect.maxWaitingTime";
const kPrefsPollingTime = "captivedetect.pollingTime";

var gServer;
var gServerURL;

function setupPrefs() {
  Services.prefs.setCharPref(
    kPrefsCanonicalURL,
    gServerURL + kCanonicalSitePath
  );
  Services.prefs.setCharPref(kPrefsCanonicalContent, kCanonicalSiteContent);
  Services.prefs.setIntPref(kPrefsMaxWaitingTime, 0);
  Services.prefs.setIntPref(kPrefsPollingTime, 1);
}

let gCaptivePortalDetector;
function run_captivedetect_test(xhr_handler, fakeUIResponse, testfun) {
  gServer = new HttpServer();
  gServer.registerPathHandler(kCanonicalSitePath, xhr_handler);
  gServer.start(-1);
  gServerURL = "http://localhost:" + gServer.identity.primaryPort;

  setupPrefs();

  // Instead of getting the XPCOM service, we need the real JS object, so that
  // we can give it a stubbed out telemetry service.
  const { CaptivePortalDetector } = ChromeUtils.import(
    "resource:///modules/CaptiveDetect.jsm"
  );
  gCaptivePortalDetector = new CaptivePortalDetector();
  gCaptivePortalDetector._telemetryService = fakeTelemetryService;

  fakeUIResponse();

  testfun();
}
