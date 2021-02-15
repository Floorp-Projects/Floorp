/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains utilities that can be shared between xpcshell tests and mochitests.
 */

// The marker phases.
const INSTANT = 0;
const INTERVAL = 1;
const INTERVAL_START = 2;
const INTERVAL_END = 3;

// This Services declaration may shadow another from head.js, so define it as
// a var rather than a const.
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const defaultSettings = {
  entries: 8 * 1024 * 1024, // 8M entries = 64MB
  interval: 1, // ms
  features: ["threads"],
  threads: ["GeckoMain"],
};

function startProfiler(callersSettings) {
  if (Services.profiler.IsActive()) {
    throw new Error(
      "The profiler must not be active before starting it in a test."
    );
  }
  const settings = Object.assign({}, defaultSettings, callersSettings);
  Services.profiler.StartProfiler(
    settings.entries,
    settings.interval,
    settings.features,
    settings.threads,
    0,
    settings.duration
  );
}

/**
 * This is a helper function be able to run `await wait(500)`. Unfortunately
 * this is needed as the act of collecting functions relies on the periodic
 * sampling of the threads. See:
 * https://bugzilla.mozilla.org/show_bug.cgi?id=1529053
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

/**
 * Get the payloads of a type recursively, including from all subprocesses.
 *
 * @param {Object} profile The gecko profile.
 * @param {string} type The marker payload type, e.g. "DiskIO".
 * @param {Array} payloadTarget The recursive list of payloads.
 * @return {Array} The final payloads.
 */
function getPayloadsOfTypeFromAllThreads(profile, type, payloadTarget = []) {
  for (const { markers } of profile.threads) {
    for (const markerTuple of markers.data) {
      const payload = markerTuple[markers.schema.data];
      if (payload && payload.type === type) {
        payloadTarget.push(payload);
      }
    }
  }

  for (const subProcess of profile.processes) {
    getPayloadsOfTypeFromAllThreads(subProcess, type, payloadTarget);
  }

  return payloadTarget;
}

/**
 * Get the payloads of a type from a single thread.
 *
 * @param {Object} thread The thread from a profile.
 * @param {string} type The marker payload type, e.g. "DiskIO".
 * @return {Array} The payloads.
 */
function getPayloadsOfType(thread, type) {
  const { markers } = thread;
  const results = [];
  for (const markerTuple of markers.data) {
    const payload = markerTuple[markers.schema.data];
    if (payload && payload.type === type) {
      results.push(payload);
    }
  }
  return results;
}

/**
 * Applies the marker schema to create individual objects for each marker
 *
 * @param {Object} thread The thread from a profile.
 * @return {InflatedMarker[]} The markers.
 */
function getInflatedMarkerData(thread) {
  const { markers, stringTable } = thread;
  return markers.data.map(markerTuple => {
    const marker = {};
    for (const [key, tupleIndex] of Object.entries(markers.schema)) {
      marker[key] = markerTuple[tupleIndex];
      if (key === "name") {
        // Use the string from the string table.
        marker[key] = stringTable[marker[key]];
      }
    }
    return marker;
  });
}

/**
 * It can be helpful to force the profiler to collect a JavaScript sample. This
 * function spins on a while loop until at least one more sample is collected.
 *
 * @return {number} The index of the collected sample.
 */
function captureAtLeastOneJsSample() {
  function getProfileSampleCount() {
    const profile = Services.profiler.getProfileData();
    return profile.threads[0].samples.data.length;
  }

  const sampleCount = getProfileSampleCount();
  // Create an infinite loop until a sample has been collected.
  while (true) {
    if (sampleCount < getProfileSampleCount()) {
      return sampleCount;
    }
  }
}

/**
 * This function pauses the profiler before getting the profile. Then after the
 * getting the data, the profiler is stopped, and all profiler data is removed.
 * @returns {Promise<Profile>}
 */
async function stopAndGetProfile() {
  Services.profiler.Pause();
  const profile = await Services.profiler.getProfileDataAsync();
  Services.profiler.StopProfiler();
  return profile;
}

/**
 * Verifies that a marker is an interval marker.
 *
 * @param {InflatedMarker} marker
 * @returns {boolean}
 */
function isIntervalMarker(inflatedMarker) {
  return (
    inflatedMarker.phase === 1 &&
    typeof inflatedMarker.startTime === "number" &&
    typeof inflatedMarker.endTime === "number"
  );
}

/**
 * @param {Profile} profile
 * @returns {Thread[]}
 */
function getThreads(profile) {
  const threads = [];

  function getThreadsRecursive(process) {
    for (const thread of process.threads) {
      threads.push(thread);
    }
    for (const subprocess of process.processes) {
      getThreadsRecursive(subprocess);
    }
  }

  getThreadsRecursive(profile);
  return threads;
}

/**
 * Find a specific marker schema from any process of a profile.
 *
 * @param {Profile} profile
 * @param {string} name
 * @returns {MarkerSchema}
 */
function getSchema(profile, name) {
  {
    const schema = profile.meta.markerSchema.find(s => s.name === name);
    if (schema) {
      return schema;
    }
  }
  for (const subprocess of profile.processes) {
    const schema = subprocess.meta.markerSchema.find(s => s.name === name);
    if (schema) {
      return schema;
    }
  }
  console.error("Parent process schema", profile.meta.markerSchema);
  for (const subprocess of profile.processes) {
    console.error("Child process schema", subprocess.meta.markerSchema);
  }
  throw new Error(`Could not find a schema for "${name}".`);
}
