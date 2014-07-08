/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// We need to initialize the profile or OS.File may not work. See bug 810543.
do_get_profile();

(function initTestingInfrastructure() {
  let ns = {};
  Components.utils.import("resource://testing-common/services/common/logging.js",
                          ns);

  ns.initTestLogging();
}).call(this);

