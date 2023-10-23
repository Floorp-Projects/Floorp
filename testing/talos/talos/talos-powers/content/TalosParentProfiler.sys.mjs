/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module is for instrumenting your Talos test for
 * performance profiles while running within the parent process.
 * Almost all of the functions that this script exposes to the
 * Gecko Profiler are synchronous, except for finishTest, since that
 * involves requesting the profiles from any content processes and
 * then writing to disk.
 *
 * If your test is running in the content process, you should use the
 * TalosContentProfiler.js utility script instead.
 */

export const TalosParentProfiler = {
  // Whether or not this TalosContentProfiler object has had initFromObject
  // or initFromURLQueryParams called on it. Any functions that change the
  // state of the Gecko Profiler should only be called after calling either
  // initFromObject or initFromURLQueryParams.
  initted: false,
  // The subtest name that beginTest() was called with.
  currentTest: "unknown",
  // Profiler settings.
  interval: undefined,
  entries: undefined,
  featuresArray: undefined,
  threadsArray: undefined,
  profileDir: undefined,

  get TalosPowers() {
    // Use a bit of XPCOM hackery to get at the Talos Powers service
    // implementation...
    return Cc["@mozilla.org/talos/talos-powers-service;1"].getService(
      Ci.nsISupports
    ).wrappedJSObject;
  },

  /**
   * Initialize the profiler using profiler settings supplied in a JS object.
   *
   * @param obj (object)
   *   The following properties on the object are respected:
   *     gecko_profile_interval (int)
   *     gecko_profile_entries (int)
   *     gecko_profile_features (string, comma separated list of features to enable)
   *     gecko_profile_threads (string, comma separated list of threads to filter with)
   *     gecko_profile_dir (string)
   */
  initFromObject(obj = {}) {
    if (!this.initted) {
      if (
        "gecko_profile_dir" in obj &&
        typeof obj.gecko_profile_dir == "string" &&
        "gecko_profile_interval" in obj &&
        Number.isFinite(obj.gecko_profile_interval * 1) &&
        "gecko_profile_entries" in obj &&
        Number.isFinite(obj.gecko_profile_entries * 1) &&
        "gecko_profile_features" in obj &&
        typeof obj.gecko_profile_features == "string" &&
        "gecko_profile_threads" in obj &&
        typeof obj.gecko_profile_threads == "string"
      ) {
        this.interval = obj.gecko_profile_interval;
        this.entries = obj.gecko_profile_entries;
        this.featuresArray = obj.gecko_profile_features.split(",");
        this.threadsArray = obj.gecko_profile_threads.split(",");
        this.profileDir = obj.gecko_profile_dir;
        this.initted = true;
      } else {
        console.error(
          "Profiler could not init with object: " + JSON.stringify(obj)
        );
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
    this.initFromObject(this.searchToObject(locationSearch));
  },

  /**
   * Parses an url query string into a JS object.
   *
   * @param locationSearch (string)
   *        The location string to parse.
   * @returns Object
   *        The GET params from the location string as
   *        key-value pairs in the Object.
   */
  searchToObject(locationSearch) {
    let pairs = locationSearch.substring(1).split("&");
    let result = {};

    for (let i in pairs) {
      if (pairs[i] !== "") {
        let pair = pairs[i].split("=");
        result[decodeURIComponent(pair[0])] = decodeURIComponent(pair[1] || "");
      }
    }

    return result;
  },

  /**
   * A Talos test is about to start.
   *
   * @param testName (string)
   *        The name of the test to use in Profiler markers.
   */
  beginTest(testName) {
    if (this.initted) {
      this.currentTest = testName;
      this.TalosPowers.profilerBegin({
        entries: this.entries,
        interval: this.interval,
        featuresArray: this.featuresArray,
        threadsArray: this.threadsArray,
      });
    } else {
      let msg =
        "You should not call beginTest without having first " +
        "initted the Profiler";
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
    if (this.initted) {
      let profileFile = `${this.currentTest}.profile`;
      return this.TalosPowers.profilerFinish(this.profileDir, profileFile);
    }
    let msg =
      "You should not call finishTest without having first " +
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
    if (this.initted) {
      let profileFile = "startup.profile";
      return this.TalosPowers.profilerFinish(this.profileDir, profileFile);
    }
    return Promise.resolve();
  },

  /**
   * Set a marker indicating the start of the subtest.
   *
   */
  subtestStart(marker = "") {
    if (this.initted) {
      this.TalosPowers.profilerSubtestStart(marker);
    }
  },

  /**
   * Set a marker indicating the duration of the subtest.
   *
   * @param marker (string, optional)
   *        If non-empty, will set a marker immediately.
   * @param startTime (number, optional)
   *        Start time, used to create an interval profile marker. If
   *        undefined, a single instance marker will be placed.
   */
  subtestEnd(marker = "", startTime = undefined) {
    if (this.initted) {
      this.TalosPowers.profilerSubtestEnd(marker, startTime);
    }
  },

  /**
   * Adds a marker to the profile.
   *
   * @param marker (string, optional)
   *        If non-empty, will set a marker immediately.
   * @param startTime (number, optional)
   *        Start time, used to create an interval profile marker. If
   *        undefined, a single instance marker will be placed.
   * @returns Promise
   *          Resolves once the marker has been set.
   */
  mark(marker, startTime = undefined) {
    if (this.initted) {
      // If marker is omitted, just use the test name
      if (!marker) {
        marker = this.currentTest;
      }

      this.TalosPowers.addIntervalMarker(marker, startTime);
    }
  },

  afterProfileGathered() {
    if (!this.initted) {
      return Promise.resolve();
    }

    return new Promise(resolve => {
      Services.obs.addObserver(function onGathered() {
        Services.obs.removeObserver(onGathered, "talos-profile-gathered");
        resolve();
      }, "talos-profile-gathered");
    });
  },
};
