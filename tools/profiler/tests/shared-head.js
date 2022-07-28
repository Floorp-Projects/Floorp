/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals Assert */

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

const defaultSettings = {
  entries: 8 * 1024 * 1024, // 8M entries = 64MB
  interval: 1, // ms
  features: [],
  threads: ["GeckoMain"],
};

// Effectively `async`: Start the profiler and return the `startProfiler`
// promise that will get resolved when all child process have started their own
// profiler.
function startProfiler(callersSettings) {
  if (Services.profiler.IsActive()) {
    throw new Error(
      "The profiler must not be active before starting it in a test."
    );
  }
  const settings = Object.assign({}, defaultSettings, callersSettings);
  return Services.profiler.StartProfiler(
    settings.entries,
    settings.interval,
    settings.features,
    settings.threads,
    0,
    settings.duration
  );
}

function startProfilerForMarkerTests() {
  return startProfiler({
    features: ["nostacksampling", "js"],
    threads: ["GeckoMain", "DOM Worker"],
  });
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
 * Applies the marker schema to create individual objects for each marker, then
 * keeps only the network markers that match the profiler tests.
 *
 * @param {Object} thread The thread from a profile.
 * @return {InflatedMarker[]} The filtered network markers.
 */
function getInflatedNetworkMarkers(thread) {
  const markers = getInflatedMarkerData(thread);
  return markers.filter(
    m =>
      m.data &&
      m.data.type === "Network" &&
      // We filter out network markers that aren't related to the test, to
      // avoid intermittents.
      m.data.URI.includes("/tools/profiler/")
  );
}

/**
 * From a list of network markers, this returns pairs of start/stop markers.
 * If a stop marker can't be found for a start marker, this will return an array
 * of only 1 element.
 *
 * @param {InflatedMarker[]} networkMarkers Network markers
 * @return {InflatedMarker[][]} Pairs of network markers
 */
function getPairsOfNetworkMarkers(allNetworkMarkers) {
  // For each 'start' marker we want to find the next 'stop' or 'redirect'
  // marker with the same id.
  const result = [];
  const mapOfStartMarkers = new Map(); // marker id -> id in result array
  for (const marker of allNetworkMarkers) {
    const { data } = marker;
    if (data.status === "STATUS_START") {
      if (mapOfStartMarkers.has(data.id)) {
        const previousMarker = result[mapOfStartMarkers.get(data.id)][0];
        Assert.ok(
          false,
          `We found 2 start markers with the same id ${data.id}, without end marker in-between.` +
            `The first marker has URI ${previousMarker.data.URI}, the second marker has URI ${data.URI}.` +
            ` This should not happen.`
        );
        continue;
      }

      mapOfStartMarkers.set(data.id, result.length);
      result.push([marker]);
    } else {
      // STOP or REDIRECT
      if (!mapOfStartMarkers.has(data.id)) {
        Assert.ok(
          false,
          `We found an end marker without a start marker (id: ${data.id}, URI: ${data.URI}). This should not happen.`
        );
        continue;
      }
      result[mapOfStartMarkers.get(data.id)].push(marker);
      mapOfStartMarkers.delete(data.id);
    }
  }

  return result;
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

function isJSONWhitespace(c) {
  return ["\n", "\r", " ", "\t"].includes(c);
}

function verifyJSONStringIsCompact(s) {
  const stateData = 0;
  const stateString = 1;
  const stateEscapedChar = 2;
  let state = stateData;
  for (let i = 0; i < s.length; ++i) {
    let c = s[i];
    switch (state) {
      case stateData:
        if (isJSONWhitespace(c)) {
          Assert.ok(
            false,
            `"Unexpected JSON whitespace at index ${i} in profile: <<<${s}>>>"`
          );
          return;
        }
        if (c == '"') {
          state = stateString;
        }
        break;
      case stateString:
        if (c == '"') {
          state = stateData;
        } else if (c == "\\") {
          state = stateEscapedChar;
        }
        break;
      case stateEscapedChar:
        state = stateString;
        break;
    }
  }
}

/**
 * This function pauses the profiler before getting the profile. Then after
 * getting the data, the profiler is stopped, and all profiler data is removed.
 * @returns {Promise<Profile>}
 */
async function stopNowAndGetProfile() {
  // Don't await the pause, because each process will handle it before it
  // receives the following `getProfileDataAsArrayBuffer()`.
  Services.profiler.Pause();

  const profileArrayBuffer = await Services.profiler.getProfileDataAsArrayBuffer();
  await Services.profiler.StopProfiler();

  const profileUint8Array = new Uint8Array(profileArrayBuffer);
  const textDecoder = new TextDecoder("utf-8", { fatal: true });
  const profileString = textDecoder.decode(profileUint8Array);
  verifyJSONStringIsCompact(profileString);

  return JSON.parse(profileString);
}

/**
 * This function ensures there's at least one sample, then pauses the profiler
 * before getting the profile. Then after getting the data, the profiler is
 * stopped, and all profiler data is removed.
 * @returns {Promise<Profile>}
 */
async function waitSamplingAndStopAndGetProfile() {
  await Services.profiler.waitOnePeriodicSampling();
  return stopNowAndGetProfile();
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

/**
 * This escapes all characters that have a special meaning in RegExps.
 * This was stolen from https://github.com/sindresorhus/escape-string-regexp and
 * so it is licence MIT and:
 * Copyright (c) Sindre Sorhus <sindresorhus@gmail.com> (https://sindresorhus.com).
 * See the full license in https://raw.githubusercontent.com/sindresorhus/escape-string-regexp/main/license.
 * @param {string} string The string to be escaped
 * @returns {string} The result
 */
function escapeStringRegexp(string) {
  if (typeof string !== "string") {
    throw new TypeError("Expected a string");
  }

  // Escape characters with special meaning either inside or outside character
  // sets.  Use a simple backslash escape when it’s always valid, and a `\xnn`
  // escape when the simpler form would be disallowed by Unicode patterns’
  // stricter grammar.
  return string.replace(/[|\\{}()[\]^$+*?.]/g, "\\$&").replace(/-/g, "\\x2d");
}

/** ------ Assertions helper ------ */
/**
 * This assert helper function makes it easy to check a lot of properties in an
 * object. We augment Assert.jsm to make it easier to use.
 */
Object.assign(Assert, {
  /*
   * It checks if the properties on the right are all present in the object on
   * the left. Note that the object might still have other properties (see
   * objectContainsOnly below if you want the stricter form).
   *
   * The basic form does basic equality on each expected property:
   *
   * Assert.objectContains(fixture, {
   *   foo: "foo",
   *   bar: 1,
   *   baz: true,
   * });
   *
   * But it also has a more powerful form with expectations. The available
   * expectations are:
   * - any(): this only checks for the existence of the property, not its value
   * - number(), string(), boolean(), bigint(), function(), symbol(), object():
   *   this checks if the value is of this type
   * - objectContains(expected): this applies Assert.objectContains()
   *   recursively on this property.
   * - stringContains(needle): this checks if the expected value is included in
   *   the property value.
   * - stringMatches(regexp): this checks if the property value matches this
   *   regexp. The regexp can be passed as a string, to be dynamically built.
   *
   * example:
   *
   * Assert.objectContains(fixture, {
   *   name: Expect.stringMatches(`Load \\d+:.*${url}`),
   *   data: Expect.objectContains({
   *     status: "STATUS_STOP",
   *     URI: Expect.stringContains("https://"),
   *     requestMethod: "GET",
   *     contentType: Expect.string(),
   *     startTime: Expect.number(),
   *     cached: Expect.boolean(),
   *   }),
   * });
   *
   * Each expectation will translate into one or more Assert call. Therefore if
   * one expectation fails, this will be clearly visible in the test output.
   *
   * Expectations can also be normal functions, for example:
   *
   * Assert.objectContains(fixture, {
   *   number: value => Assert.greater(value, 5)
   * });
   *
   * Note that you'll need to use Assert inside this function.
   */
  objectContains(object, expectedProperties) {
    // Basic tests: we don't want to run other assertions if these tests fail.
    if (typeof object !== "object") {
      this.ok(
        false,
        `The first parameter should be an object, but found: ${object}.`
      );
      return;
    }

    if (typeof expectedProperties !== "object") {
      this.ok(
        false,
        `The second parameter should be an object, but found: ${expectedProperties}.`
      );
      return;
    }

    for (const key of Object.keys(expectedProperties)) {
      const expected = expectedProperties[key];
      if (!(key in object)) {
        this.report(
          true,
          object,
          expectedProperties,
          `The object should contain the property "${key}", but it's missing.`
        );
        continue;
      }

      if (typeof expected === "function") {
        // This is a function, so let's call it.
        expected(
          object[key],
          `The object should contain the property "${key}" with an expected value and type.`
        );
      } else {
        // Otherwise, we check for equality.
        this.equal(
          object[key],
          expectedProperties[key],
          `The object should contain the property "${key}" with an expected value.`
        );
      }
    }
  },

  /**
   * This is very similar to the previous `objectContains`, but this also looks
   * at the number of the objects' properties. Thus this will fail if the
   * objects don't have the same properties exactly.
   */
  objectContainsOnly(object, expectedProperties) {
    // Basic tests: we don't want to run other assertions if these tests fail.
    if (typeof object !== "object") {
      this.ok(
        false,
        `The first parameter should be an object but found: ${object}.`
      );
      return;
    }

    if (typeof expectedProperties !== "object") {
      this.ok(
        false,
        `The second parameter should be an object but found: ${expectedProperties}.`
      );
      return;
    }

    // In objectContainsOnly, we specifically want to check if all properties
    // from the fixture object are expected.
    // We'll be failing a test only for the specific properties that weren't
    // expected, and only fail with one message, so that the test outputs aren't
    // spammed.
    const extraProperties = [];
    for (const fixtureKey of Object.keys(object)) {
      if (!(fixtureKey in expectedProperties)) {
        extraProperties.push(fixtureKey);
      }
    }

    if (extraProperties.length) {
      // Some extra properties have been found.
      this.report(
        true,
        object,
        expectedProperties,
        `These properties are present, but shouldn't: "${extraProperties.join(
          '", "'
        )}".`
      );
    }

    // Now, let's carry on the rest of our work.
    this.objectContains(object, expectedProperties);
  },
});

const Expect = {
  any: () => actual => {} /* We don't check anything more than the presence of this property. */,
};

/* These functions are part of the Assert object, and we want to reuse them. */
[
  "stringContains",
  "stringMatches",
  "objectContains",
  "objectContainsOnly",
].forEach(
  assertChecker =>
    (Expect[assertChecker] = expected => (actual, ...moreArgs) =>
      Assert[assertChecker](actual, expected, ...moreArgs))
);

/* These functions will only check for the type. */
[
  "number",
  "string",
  "boolean",
  "bigint",
  "symbol",
  "object",
  "function",
].forEach(type => (Expect[type] = makeTypeChecker(type)));

function makeTypeChecker(type) {
  return (...unexpectedArgs) => {
    if (unexpectedArgs.length) {
      throw new Error(
        "Type checkers expectations aren't expecting any argument."
      );
    }
    return (actual, message) => {
      const isCorrect = typeof actual === type;
      Assert.report(!isCorrect, actual, type, message, "has type");
    };
  };
}
/* ------ End of assertion helper ------ */
