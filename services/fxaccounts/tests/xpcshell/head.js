/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

"use strict";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

(function initFxAccountsTestingInfrastructure() {
  do_get_profile();

  let ns = {};
  Cu.import("resource://testing-common/services/common/logging.js", ns);

  ns.initTestLogging("Trace");
}).call(this);

