/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../../common/tests/unit/head_helpers.js */
/* import-globals-from ../../../common/tests/unit/head_http.js */

"use strict";

var {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
var {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

(function initFxAccountsTestingInfrastructure() {
  do_get_profile();

  let ns = {};
  ChromeUtils.import("resource://testing-common/services/common/logging.js", ns);

  ns.initTestLogging("Trace");
}).call(this);

// ================================================
// Load mocking/stubbing library, sinon
// docs: http://sinonjs.org/releases/v2.3.2/
var {clearInterval, clearTimeout, setInterval, setIntervalWithTarget, setTimeout, setTimeoutWithTarget} = ChromeUtils.import("resource://gre/modules/Timer.jsm");
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js", this);
/* globals sinon */
// ================================================
