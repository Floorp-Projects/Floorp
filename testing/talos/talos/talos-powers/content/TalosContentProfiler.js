/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This utility script is for instrumenting your Talos test for
 * performance profiles while running within content. If your test
 * is running in the parent process, you should use
 * TalosParentProfiler.js instead to avoid the messaging overhead.
 */

var TalosContentProfiler;

(function() {

  // Whether or not this TalosContentProfiler object has had initFromObject
  // or initFromURLQueryParams called on it. Any functions that will send
  // events to the parent to change the behaviour of the Gecko Profiler
  // should only be called after calling either initFromObject or
  // initFromURLQueryParams.
  var initted = false;

  // The subtest name that beginTest() was called with.
  var currentTest = "unknown";

  // Profiler settings.
  var interval, entries, threadsArray, profileDir;

  try {
    // Outside of talos, this throws a security exception which no-op this file.
    // (It's not required nor allowed for addons since Firefox 17)
    // It's used inside talos from non-privileged pages (like during tscroll),
    // and it works because talos disables all/most security measures.
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  } catch (e) {}

  Components.utils.import("resource://gre/modules/Services.jsm");

  /**
   * Emits a TalosContentProfiler prefixed event and then returns a Promise
   * that resolves once a corresponding acknowledgement event is
   * dispatched on our document.
   *
   * @param name
   *        The name of the event that will be TalosContentProfiler prefixed and
   *        eventually sent to the parent.
   * @param data (optional)
   *        The data that will be sent to the parent.
   * @returns Promise
   *        Resolves when a corresponding acknowledgement event is dispatched
   *        on this document.
   */
  function sendEventAndWait(name, data = {}) {
    return new Promise((resolve) => {
      var event = new CustomEvent("TalosContentProfilerCommand", {
        bubbles: true,
        detail: {
          name,
          data,
        }
      });
      document.dispatchEvent(event);

      addEventListener("TalosContentProfilerResponse", function onResponse(event) {
        if (event.detail.name != name) {
          return;
        }

        removeEventListener("TalosContentProfilerResponse", onResponse);

        resolve(event.detail.data);
      });
    });
  }

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
    var pairs = locationSearch.substring(1).split("&");
    var result = {};

    for (var i in pairs) {
      if (pairs[i] !== "") {
        var pair = pairs[i].split("=");
        result[decodeURIComponent(pair[0])] = decodeURIComponent(pair[1] || "");
      }
    }

    return result;
  }

  TalosContentProfiler = {
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
     * A Talos test is about to start. This will return a Promise that
     * resolves once the Profiler has been initialized. Note that the
     * Gecko Profiler will be paused immediately after starting and that
     * resume() should be called in order to collect samples.
     *
     * @param testName (string)
     *        The name of the test to use in Profiler markers.
     * @returns Promise
     *        Resolves once the Gecko Profiler has been initialized and paused.
     *        If the TalosContentProfiler is not initialized, then this resolves
     *        without doing anything.
     */
    beginTest(testName) {
      if (initted) {
        currentTest = testName;
        return sendEventAndWait("Profiler:Begin", {
          interval,
          entries,
          threadsArray,
        });
      }
      return Promise.resolve();
    },

    /**
     * A Talos test has finished. This will stop the Gecko Profiler from
     * sampling, and return a Promise that resolves once the Profiler has
     * finished dumping the multi-process profile to disk.
     *
     * @returns Promise
     *          Resolves once the profile has been dumped to disk. The test should
     *          not try to quit the browser until this has resolved.
     *          If the TalosContentProfiler is not initialized, then this resolves
     *          without doing anything.
     */
    finishTest() {
      if (initted) {
        let profileFile = profileDir + "/" + currentTest + ".profile";
        return sendEventAndWait("Profiler:Finish", { profileFile });
      }
      return Promise.resolve();
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
        return sendEventAndWait("Profiler:Finish", { profileFile });
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
        return sendEventAndWait("Profiler:Resume", { marker });
      }
      return Promise.resolve();
    },

    /**
     * Pauses the Gecko Profiler sampler. Can also simultaneously set a marker.
     *
     * @returns Promise
     *          Resolves once the Gecko Profiler has paused.
     */
    pause(marker = "") {
      if (initted) {
        return sendEventAndWait("Profiler:Pause", { marker });
      }

      return Promise.resolve();
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

        return sendEventAndWait("Profiler:Marker", { marker });
      }

      return Promise.resolve();
    },

    /**
     * Add a marker to the profile on the content process samples.
     * This occurs synchronously.
     */
    contentMarker(marker) {
      Services.profiler.AddMarker(marker);
    },
  };
})();
