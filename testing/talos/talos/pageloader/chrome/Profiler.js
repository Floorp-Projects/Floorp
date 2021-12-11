/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// - NOTE: This file is duplicated verbatim at:
//         - talos/pageloader/chrome/Profiler.js
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

(function() {
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
  var profiler_interval,
    profiler_entries,
    profiler_threadsArray,
    profiler_featuresArray,
    profiler_dir;

  /* eslint-disable mozilla/use-chromeutils-import */
  try {
    // eslint-disable-next-line mozilla/use-services
    _profiler = Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
  } catch (ex) {
    (typeof dumpLog == "undefined" ? dump : dumpLog)(ex + "\n");
  }

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
     *  - gecko_profile_interval
     *  - gecko_profile_entries
     *  - gecko_profile_features
     *  - gecko_profile_threads
     *  - gecko_profile_dir
     */
    initFromObject: function Profiler__initFromObject(obj) {
      if (
        obj &&
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
        profiler_interval = obj.gecko_profile_interval;
        profiler_entries = obj.gecko_profile_entries;
        profiler_featuresArray = obj.gecko_profile_features.split(",");
        profiler_threadsArray = obj.gecko_profile_threads.split(",");
        profiler_dir = obj.gecko_profile_dir;
        enabled = true;
      }
    },
    initFromURLQueryParams: function Profiler__initFromURLQueryParams(
      locationSearch
    ) {
      this.initFromObject(searchToObject(locationSearch));
    },
    beginTest: function Profiler__beginTest(testName) {
      currentTest = testName;
      if (_profiler && enabled) {
        _profiler.StartProfiler(
          profiler_entries,
          profiler_interval,
          profiler_featuresArray,
          profiler_threadsArray
        );
        _profiler.PauseSampling();
      }
    },
    finishTest: function Profiler__finishTest() {
      if (_profiler && enabled) {
        _profiler.Pause();
        _profiler.dumpProfileToFile(
          profiler_dir + "/" + currentTest + ".profile"
        );
        _profiler.StopProfiler();
      }
    },
    finishTestAsync: function Profiler__finishTest() {
      if (!(_profiler && enabled)) {
        return undefined;
      }
      return new Promise((resolve, reject) => {
        Services.profiler.getProfileDataAsync().then(
          profile => {
            let profileFile = profiler_dir + "/" + currentTest + ".profile";

            const { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm");
            const { FileUtils } = Cu.import(
              "resource://gre/modules/FileUtils.jsm"
            );

            var file = Cc["@mozilla.org/file/local;1"].createInstance(
              Ci.nsIFile
            );
            file.initWithPath(profileFile);

            var ostream = FileUtils.openSafeFileOutputStream(file);

            var converter = Cc[
              "@mozilla.org/intl/scriptableunicodeconverter"
            ].createInstance(Ci.nsIScriptableUnicodeConverter);
            converter.charset = "UTF-8";
            var istream = converter.convertToInputStream(
              JSON.stringify(profile)
            );

            // The last argument (the callback) is optional.
            NetUtil.asyncCopy(istream, ostream, function(status) {
              if (!Components.isSuccessCode(status)) {
                reject();
                return;
              }

              resolve();
            });
          },
          error => {
            Cu.reportError("Failed to gather profile: " + error);
            reject();
          }
        );
      });
    },
    finishStartupProfiling: function Profiler__finishStartupProfiling() {
      if (_profiler && enabled) {
        _profiler.Pause();
        _profiler.dumpProfileToFile(profiler_dir + "/startup.profile");
        _profiler.StopProfiler();
      }
    },
    resume: function Profiler__resume(name, explicit) {
      if (_profiler) {
        if (_profiler.ResumeSampling) {
          _profiler.ResumeSampling();
        }
        ChromeUtils.addProfilerMarker(
          explicit ? name : 'Start of test "' + (name || test_name) + '"'
        );
      }
    },
    pause: function Profiler__pause(name, explicit) {
      if (_profiler) {
        ChromeUtils.addProfilerMarker(
          explicit ? name : 'End of test "' + (name || test_name) + '"'
        );
        _profiler.PauseSampling();
      }
    },
    mark: function Profiler__mark(marker, explicit) {
      if (_profiler) {
        ChromeUtils.addProfilerMarker(
          explicit ? marker : 'Profiler: "' + (marker || test_name) + '"'
        );
      }
    },
  };
})();
