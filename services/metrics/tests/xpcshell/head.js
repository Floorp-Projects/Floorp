/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

(function initMetricsTestingInfrastructure() {
  do_get_profile();

  let ns = {};
  Components.utils.import("resource://testing-common/services/common/logging.js",
                          ns);

  ns.initTestLogging("Trace");
}).call(this);

