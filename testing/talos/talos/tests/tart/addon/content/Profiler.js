/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// - NOTE: This file is duplicated verbatim at:
//         - talos/scripts/Profiler.js
//         - talos/pageloader/chrome/Profiler.js
//         - talos/tests/devtools/addon/content/Profiler.js
//         - talos/tests/tart/addon/content/Profiler.js
//         - talos/startup_test/tresize/addon/content/Profiler.js
//
//  - Please keep these copies in sync.
//  - Please make sure your changes apply cleanly to all use cases.

// Finer grained profiler control
//
// Use this object to pause and resume the profiler so that it only profiles the
// relevant parts of our tests.
var Profiler;

(function(){
  var _profiler;

  // If this script is loaded in a framescript context, there won't be a
  // document object, so just use a fallback value in that case.
  var test_name = this.document ? this.document.location.pathname : "unknown";

  // Whether Profiler has been initialized. Until that happens, most calls
  // will be ignored.
  var enabled = false;

  // The subtest name that beginTest() was called with.
  var currentTest = "";

  // Profiling settings.
  var profiler_interval, profiler_entries, profiler_threadsArray, profiler_dir;

  try {
    // Outside of talos, this throws a security exception which no-op this file.
    // (It's not required nor allowed for addons since Firefox 17)
    // It's used inside talos from non-privileged pages (like during tscroll),
    // and it works because talos disables all/most security measures.
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
  } catch (e) {}

  try {
    _profiler = Components.classes["@mozilla.org/tools/profiler;1"].getService(Components.interfaces.nsIProfiler);
  } catch (ex) { (typeof(dumpLog) == "undefined" ? dump : dumpLog)(ex + "\n"); }

  // Parses an url query string into a JS object.
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

  Profiler = {
    /**
     * Initialize the profiler using profiler settings supplied in a JS object.
     * The following properties on the object are respected:
     *  - sps_profile_interval
     *  - sps_profile_entries
     *  - sps_profile_threads
     *  - sps_profile_dir
     */
    initFromObject: function Profiler__initFromObject (obj) {
      if (obj &&
          ("sps_profile_dir" in obj) && typeof obj.sps_profile_dir == "string" &&
          ("sps_profile_interval" in obj) && Number.isFinite(obj.sps_profile_interval * 1) &&
          ("sps_profile_entries" in obj) && Number.isFinite(obj.sps_profile_entries * 1) &&
          ("sps_profile_threads" in obj) && typeof obj.sps_profile_threads == "string") {
        profiler_interval = obj.sps_profile_interval;
        profiler_entries = obj.sps_profile_entries;
        profiler_threadsArray = obj.sps_profile_threads.split(",");
        profiler_dir = obj.sps_profile_dir;
        enabled = true;
      }
    },
    initFromURLQueryParams: function Profiler__initFromURLQueryParams (locationSearch) {
      this.initFromObject(searchToObject(locationSearch));
    },
    beginTest: function Profiler__beginTest (testName) {
      currentTest = testName;
      if (_profiler && enabled) {
        _profiler.StartProfiler(profiler_entries, profiler_interval,
                                ["js", "leaf", "stackwalk", "threads"], 4,
                                profiler_threadsArray, profiler_threadsArray.length);
        if (_profiler.PauseSampling) {
          _profiler.PauseSampling();
        }
      }
    },
    finishTest: function Profiler__finishTest () {
      if (_profiler && enabled) {
        _profiler.dumpProfileToFile(profiler_dir + "/" + currentTest + ".sps");
        _profiler.StopProfiler();
      }
    },
    finishStartupProfiling: function Profiler__finishStartupProfiling () {
      if (_profiler && enabled) {
        _profiler.dumpProfileToFile(profiler_dir + "/startup.sps");
        _profiler.StopProfiler();
      }
    },
    resume: function Profiler__resume (name, explicit) {
      if (_profiler) {
        if (_profiler.ResumeSampling) {
          _profiler.ResumeSampling();
        }
        _profiler.AddMarker(explicit ? name : 'Start of test "' + (name || test_name) + '"');
      }
    },
    pause: function Profiler__pause (name, explicit) {
      if (_profiler) {
        if (_profiler.PauseSampling) {
          _profiler.PauseSampling();
        }
        _profiler.AddMarker(explicit ? name : 'End of test "' + (name || test_name) + '"');
      }
    },
    mark: function Profiler__mark (marker, explicit) {
      if (_profiler) {
        _profiler.AddMarker(explicit ? marker : 'Profiler: "' + (marker || test_name) + '"');
      }
    }
  };
})();
