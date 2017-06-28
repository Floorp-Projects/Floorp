/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This utility script is for instrumenting your Talos test for
 * performance profiles while running within the parent process.
 * Almost all of the functions that this script exposes to the
 * Gecko Profiler are synchronous, except for finishTest, since that
 * involves requesting the profiles from any content processes and
 * then writing to disk.
 *
 * If your test is running in the content process, you should use
 * TalosContentProfiler.js instead.
 */

var TalosParentProfiler;

(function() {

  // Whether or not this TalosContentProfiler object has had initFromObject
  // or initFromURLQueryParams called on it. Any functions that change the
  // state of the Gecko Profiler should only be called after calling either
  // initFromObject or initFromURLQueryParams.
  let initted = false;

  // The subtest name that beginTest() was called with.
  let currentTest = "unknown";

  // Profiler settings.
  let interval, entries, threadsArray, profileDir;

  Components.utils.import("resource://gre/modules/Services.jsm");
  Components.utils.import("resource://gre/modules/Console.jsm");

  // Use a bit of XPCOM hackery to get at the Talos Powers service
  // implementation...
  let TalosPowers =
    Components.classes["@mozilla.org/talos/talos-powers-service;1"]
              .getService(Components.interfaces.nsISupports)
              .wrappedJSObject;

  /**
   * Parses an url query string into a JS object.
   *
   * @param locationSearch (string)
   *        The location string to parse.
   * @returns Object
   *        The GET params from the location string as
   *        key-value pairs in the Object.
   */
  function searchToObject(locationSearch) {
    let pairs = locationSearch.substring(1).split("&");
    let result = {};

    for (let i in pairs) {
      if (pairs[i] !== "") {
        let pair = pairs[i].split("=");
        result[decodeURIComponent(pair[0])] = decodeURIComponent(pair[1] || "");
      }
    }

    return result;
  }

  TalosParentProfiler = {
    /**
     * Initialize the profiler using profiler settings supplied in a JS object.
     *
     * @param obj (object)
     *   The following properties on the object are respected:
     *     gecko_profile_interval (int)
     *     gecko_profile_entries (int)
     *     gecko_profile_threads (string, comma separated list of threads to filter with)
     *     gecko_profile_dir (string)
     */
    initFromObject(obj = {}) {
      if (!initted) {
        if (("gecko_profile_dir" in obj) && typeof obj.gecko_profile_dir == "string" &&
            ("gecko_profile_interval" in obj) && Number.isFinite(obj.gecko_profile_interval * 1) &&
            ("gecko_profile_entries" in obj) && Number.isFinite(obj.gecko_profile_entries * 1) &&
            ("gecko_profile_threads" in obj) && typeof obj.gecko_profile_threads == "string") {
          interval = obj.gecko_profile_interval;
          entries = obj.gecko_profile_entries;
          threadsArray = obj.gecko_profile_threads.split(",");
          profileDir = obj.gecko_profile_dir;
          initted = true;
        } else {
          console.error("Profiler could not init with object: " + JSON.stringify(obj));
        }
      }
    },

    /**
     * Initialize the profiler using a string from a location string.
     *
     * @param locationSearch (string)
     *        The location string to initialize with.
     */
    initFromURLQueryParams(locationSearch) {
      this.initFromObject(searchToObject(locationSearch));
    },

    /**
     * A Talos test is about to start. Note that the Gecko Profiler will be
     * paused immediately after starting and that resume() should be called
     * in order to collect samples.
     *
     * @param testName (string)
     *        The name of the test to use in Profiler markers.
     */
    beginTest(testName) {
      if (initted) {
        currentTest = testName;
        TalosPowers.profilerBegin({ entries, interval, threadsArray });
      } else {
        let msg = "You should not call beginTest without having first " +
                  "initted the Profiler"
        console.error(msg);
      }
    },

    /**
     * A Talos test has finished. This will stop the Gecko Profiler from
     * sampling, and return a Promise that resolves once the Profiler has
     * finished dumping the multi-process profile to disk.
     *
     * @returns Promise
     *          Resolves once the profile has been dumped to disk. The test should
     *          not try to quit the browser until this has resolved.
     */
    finishTest() {
      if (initted) {
        let profileFile = profileDir + "/" + currentTest + ".profile";
        return TalosPowers.profilerFinish(profileFile);
      }
        let msg = "You should not call finishTest without having first " +
                  "initted the Profiler";
        console.error(msg);
        return Promise.reject(msg);

    },

    /**
     * A start-up test has finished. Callers don't need to run beginTest or
     * finishTest, but should pause the sampler as soon as possible, and call
     * this function to dump the profile.
     *
     * @returns Promise
     *          Resolves once the profile has been dumped to disk. The test should
     *          not try to quit the browser until this has resolved.
     */
    finishStartupProfiling() {
      if (initted) {
        let profileFile = profileDir + "/startup.profile";
        return TalosPowers.profilerFinish(profileFile);
      }
      return Promise.resolve();
    },

    /**
     * Resumes the Gecko Profiler sampler. Can also simultaneously set a marker.
     *
     * @returns Promise
     *          Resolves once the Gecko Profiler has resumed.
     */
    resume(marker = "") {
      if (initted) {
        TalosPowers.profilerResume(marker);
      }
    },

    /**
     * Pauses the Gecko Profiler sampler. Can also simultaneously set a marker.
     *
     * @returns Promise
     *          Resolves once the Gecko Profiler has paused.
     */
    pause(marker = "") {
      if (initted) {
        TalosPowers.profilerPause(marker);
      }
    },

    /**
     * Adds a marker to the profile.
     *
     * @returns Promise
     *          Resolves once the marker has been set.
     */
    mark(marker) {
      if (initted) {
        // If marker is omitted, just use the test name
        if (!marker) {
          marker = currentTest;
        }

        TalosPowers.profilerMarker(marker);
      }
    },
  };
})();
