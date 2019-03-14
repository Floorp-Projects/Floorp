/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../../common/tests/unit/head_helpers.js */
/* import-globals-from ../../../common/tests/unit/head_http.js */

"use strict";

var {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
var {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
var {sinon} = ChromeUtils.import("resource://testing-common/Sinon.jsm");

(function initFxAccountsTestingInfrastructure() {
  do_get_profile();

  let ns = {};
  ChromeUtils.import("resource://testing-common/services/common/logging.js", ns);

  ns.initTestLogging("Trace");
}).call(this);
