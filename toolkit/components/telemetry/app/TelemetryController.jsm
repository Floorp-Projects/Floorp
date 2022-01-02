/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module chooses the correct telemetry controller module to load
 * based on the process:
 *
 * - TelemetryControllerParent is loaded only in the parent process, and
 *   contains code specific to the parent.
 * - TelemetryControllerContent is loaded only in content processes, and
 *   contains code specific to them.
 *
 * Both the parent and the content modules load TelemetryControllerBase,
 * which contains code which is common to all processes.
 *
 * This division is important for content process memory usage and
 * startup time. The parent-specific code occupies tens of KB of memory
 * which, multiplied by the number of content processes we have, adds up
 * fast.
 */

var EXPORTED_SYMBOLS = ["TelemetryController"];

// We can't use Services.appinfo here because tests stub out the appinfo
// service, and if we touch Services.appinfo now, the built-in version
// will be cached in place of the stub.
const isParentProcess =
  // eslint-disable-next-line mozilla/use-services
  Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).processType ===
  Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

var TelemetryController;
if (isParentProcess) {
  ({ TelemetryController } = ChromeUtils.import(
    "resource://gre/modules/TelemetryControllerParent.jsm"
  ));
} else {
  ({ TelemetryController } = ChromeUtils.import(
    "resource://gre/modules/TelemetryControllerContent.jsm"
  ));
}
