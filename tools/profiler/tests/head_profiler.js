/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
var {AppConstants} = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
var {setTimeout} = ChromeUtils.import("resource://gre/modules/Timer.jsm");

/**
 * Get the payloads of a type recursively, including from all subprocesses.
 *
 * @param {Object} profile The gecko profile.
 * @param {string} type The marker payload type, e.g. "DiskIO".
 * @param {Array} payloadTarget The recursive list of payloads.
 * @return {Array} The final payloads.
 */
function getAllPayloadsOfType(profile, type, payloadTarget = []) {
  for (const {markers} of profile.threads) {
    for (const markerTuple of markers.data) {
      const payload = markerTuple[markers.schema.data];
      if (payload && payload.type === type) {
        payloadTarget.push(payload);
      }
    }
  }

  for (const subProcess of profile.processes) {
    getAllPayloadsOfType(subProcess, type, payloadTarget);
  }

  return payloadTarget;
}


/**
 * This is a helper function be able to run `await wait(500)`. Unfortunately this
 * is needed as the act of collecting functions relies on the periodic sampling of
 * the threads. See: https://bugzilla.mozilla.org/show_bug.cgi?id=1529053
 *
 * @param {number} time
 * @returns {Promise}
 */
function wait(time) {
  return new Promise(resolve => {
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(resolve, time);
  });
}

function getInflatedStackLocations(thread, sample) {
  let stackTable = thread.stackTable;
  let frameTable = thread.frameTable;
  let stringTable = thread.stringTable;
  let SAMPLE_STACK_SLOT = thread.samples.schema.stack;
  let STACK_PREFIX_SLOT = stackTable.schema.prefix;
  let STACK_FRAME_SLOT = stackTable.schema.frame;
  let FRAME_LOCATION_SLOT = frameTable.schema.location;

  // Build the stack from the raw data and accumulate the locations in
  // an array.
  let stackIndex = sample[SAMPLE_STACK_SLOT];
  let locations = [];
  while (stackIndex !== null) {
    let stackEntry = stackTable.data[stackIndex];
    let frame = frameTable.data[stackEntry[STACK_FRAME_SLOT]];
    locations.push(stringTable[frame[FRAME_LOCATION_SLOT]]);
    stackIndex = stackEntry[STACK_PREFIX_SLOT];
  }

  // The profiler tree is inverted, so reverse the array.
  return locations.reverse();
}
