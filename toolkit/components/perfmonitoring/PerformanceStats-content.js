/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A proxy implementing communication between the PerformanceStats.jsm modules
 * of the parent and children processes.
 *
 * This script is loaded in all processes but is essentially a NOOP in the
 * parent process.
 */

"use strict";

const { utils: Cu, classes: Cc, interfaces: Ci } = Components;
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "PerformanceStats",
  "resource://gre/modules/PerformanceStats.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");

/**
 * A global performance monitor used by this process.
 *
 * For the sake of simplicity, rather than attempting to map each PerformanceMonitor
 * of the parent to a PerformanceMonitor in each child process, we maintain a single
 * PerformanceMonitor in each child process. Probes activation/deactivation for this
 * monitor is controlled by the activation/deactivation of probes marked as "-content"
 * in the parent.
 *
 * In the parent, this is always an empty monitor.
 */
let gMonitor = PerformanceStats.getMonitor([]);

/**
 * `true` if this is a content process, `false` otherwise.
 */
let isContent = Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;

/**
 * Handle message `performance-stats-service-acquire`: ensure that the global
 * monitor has a given probe. This message must be sent by the parent process
 * whenever a probe is activated application-wide.
 *
 * Note that we may miss acquire messages if they are sent before this process is
 * launched. For this reason, `performance-stats-service-collect` automatically
 * re-acquires probes if it realizes that they are missing.
 *
 * This operation is a NOOP on the parent process.
 *
 * @param {{payload: Array<string>}} msg.data The message received. `payload`
 * must be an array of probe names.
 */
Services.cpmm.addMessageListener("performance-stats-service-acquire", function(msg) {
  if (!isContent) {
    return;
  }
  let name = msg.data.payload;
  ensureAcquired(name);
});

/**
 * Handle message `performance-stats-service-release`: release a given probe
 * from the global monitor. This message must be sent by the parent process
 * whenever a probe is deactivated application-wide.
 *
 * Note that we may miss release messages if they are sent before this process is
 * launched. This is ok, as probes are inactive by default: if we miss the release
 * message, we have already missed the acquire message, and the effect of both
 * messages together is to reset to the default state.
 *
 * This operation is a NOOP on the parent process.
 *
 * @param {{payload: Array<string>}} msg.data The message received. `payload`
 * must be an array of probe names.
 */
Services.cpmm.addMessageListener("performance-stats-service-release", function(msg) {
  if (!isContent) {
    return;
  }
  // Keep only the probes that do not appear in the payload
  let probes = gMonitor.getProbeNames
    .filter(x => msg.data.payload.indexOf(x) == -1);
  gMonitor = PerformanceStats.getMonitor(probes);
});

/**
 * Ensure that this process has all the probes it needs.
 *
 * @param {Array<string>} probeNames The name of all probes needed by the
 * process.
 */
function ensureAcquired(probeNames) {
  let alreadyAcquired = gMonitor.probeNames;

  // Algorithm is O(n^2) because we expect that n ≤ 3.
  let shouldAcquire = [];
  for (let probeName of probeNames) {
    if (alreadyAcquired.indexOf(probeName) == -1) {
      shouldAcquire.push(probeName)
    }
  }

  if (shouldAcquire.length == 0) {
    return;
  }
  gMonitor = PerformanceStats.getMonitor([...alreadyAcquired, ...shouldAcquire]);
}

/**
 * Handle message `performance-stats-service-collected`: collect the data
 * obtained by the monitor. This message must be sent by the parent process
 * whenever we grab a performance snapshot of the application.
 *
 * This operation provides `null` on the parent process.
 *
 * @param {{data: {payload: Array<string>}}} msg The message received. `payload`
 * must be an array of probe names.
 */
Services.cpmm.addMessageListener("performance-stats-service-collect", Task.async(function*(msg) {
  let {id, payload: {probeNames}} = msg.data;
  if (!isContent) {
    // This message was sent by the parent process to itself.
    // As per protocol, respond `null`.
    Services.cpmm.sendAsyncMessage("performance-stats-service-collect", {
      id,
      data: null
    });
    return;
  }

  // We may have missed acquire messages if the process was loaded too late.
  // Catch up now.
  ensureAcquired(probeNames);

  // Collect and return data.
  let data = yield gMonitor.promiseSnapshot({probeNames});
  Services.cpmm.sendAsyncMessage("performance-stats-service-collect", {
    id,
    data
  });
}));
