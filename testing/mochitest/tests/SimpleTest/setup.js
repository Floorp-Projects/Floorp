/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This file expects the following files to be loaded.
/* import-globals-from TestRunner.js */

// From the harness:
/* import-globals-from ../../chrome-harness.js */
/* import-globals-from ../../chunkifyTests.js */

// It appears we expect these from one of the MochiKit scripts.
/* global toggleElementClass, removeElementClass, addElementClass,
          hasElementClass */

TestRunner.logEnabled = true;
TestRunner.logger = LogController;

if (!("SpecialPowers" in window)) {
  dump("SimpleTest setup.js found SpecialPowers unavailable: reloading...\n");
  setTimeout(() => {
    window.location.reload();
  }, 1000);
}

/* Helper function */
function parseQueryString(encodedString, useArrays) {
  // strip a leading '?' from the encoded string
  var qstr =
    encodedString.length && encodedString[0] == "?"
      ? encodedString.substring(1)
      : encodedString;
  var pairs = qstr.replace(/\+/g, "%20").split(/(\&amp\;|\&\#38\;|\&#x26;|\&)/);
  var o = {};
  var decode;
  if (typeof decodeURIComponent != "undefined") {
    decode = decodeURIComponent;
  } else {
    decode = unescape;
  }
  if (useArrays) {
    for (var i = 0; i < pairs.length; i++) {
      var pair = pairs[i].split("=");
      if (pair.length !== 2) {
        continue;
      }
      var name = decode(pair[0]);
      var arr = o[name];
      if (!(arr instanceof Array)) {
        arr = [];
        o[name] = arr;
      }
      arr.push(decode(pair[1]));
    }
  } else {
    for (i = 0; i < pairs.length; i++) {
      pair = pairs[i].split("=");
      if (pair.length !== 2) {
        continue;
      }
      o[decode(pair[0])] = decode(pair[1]);
    }
  }
  return o;
}

/* helper function, specifically for prefs to ignore */
function loadFile(url, callback) {
  let req = new XMLHttpRequest();
  req.open("GET", url);
  req.onload = function () {
    if (req.readyState == 4) {
      if (req.status == 200) {
        try {
          let prefs = JSON.parse(req.responseText);
          callback(prefs);
        } catch (e) {
          dump(
            "TEST-UNEXPECTED-FAIL: setup.js | error parsing " +
              url +
              " (" +
              e +
              ")\n"
          );
          throw e;
        }
      } else {
        dump(
          "TEST-UNEXPECTED-FAIL: setup.js | error loading " +
            url +
            " (HTTP " +
            req.status +
            ")\n"
        );
        callback({});
      }
    }
  };
  req.send();
}

// Check the query string for arguments
var params = parseQueryString(location.search.substring(1), true);

var config = {};
if (window.readConfig) {
  config = readConfig();
}

if (config.testRoot == "chrome" || config.testRoot == "a11y") {
  for (var p in params) {
    // Compare with arrays to find boolean equivalents, since that's what
    // |parseQueryString| with useArrays returns.
    if (params[p] == [1]) {
      config[p] = true;
    } else if (params[p] == [0]) {
      config[p] = false;
    } else {
      config[p] = params[p];
    }
  }
  params = config;
  params.baseurl = "chrome://mochitests/content";
} else if (params.xOriginTests) {
  params.baseurl = "http://mochi.test:8888/tests/";
} else {
  params.baseurl = "";
}

if (params.testRoot == "browser") {
  params.testPrefix = "chrome://mochitests/content/browser/";
} else if (params.testRoot == "chrome") {
  params.testPrefix = "chrome://mochitests/content/chrome/";
} else if (params.testRoot == "a11y") {
  params.testPrefix = "chrome://mochitests/content/a11y/";
} else if (params.xOriginTests) {
  params.testPrefix = "http://mochi.test:8888/tests/";
  params.httpsBaseUrl = "https://example.org:443/tests/";
} else {
  params.testPrefix = "/tests/";
}

// set the per-test timeout if specified in the query string
if (params.timeout) {
  TestRunner.timeout = parseInt(params.timeout) * 1000;
}

// log levels for console and logfile
var fileLevel = params.fileLevel || null;
var consoleLevel = params.consoleLevel || null;

// repeat tells us how many times to repeat the tests
if (params.repeat) {
  TestRunner.repeat = params.repeat;
}

if (params.runUntilFailure) {
  TestRunner.runUntilFailure = true;
}

// closeWhenDone tells us to close the browser when complete
if (params.closeWhenDone) {
  TestRunner.onComplete = SpecialPowers.quit.bind(SpecialPowers);
}

if (params.failureFile) {
  TestRunner.setFailureFile(params.failureFile);
}

// Breaks execution and enters the JS debugger on a test failure
if (params.debugOnFailure) {
  TestRunner.debugOnFailure = true;
}

// logFile to write our results
if (params.logFile) {
  var mfl = new MozillaFileLogger(params.logFile);
  TestRunner.logger.addListener("mozLogger", fileLevel + "", mfl.logCallback);
}

// A temporary hack for android 4.0 where Fennec utilizes the pandaboard so much it reboots
if (params.runSlower) {
  TestRunner.runSlower = true;
}

if (params.dumpOutputDirectory) {
  TestRunner.dumpOutputDirectory = params.dumpOutputDirectory;
}

if (params.dumpAboutMemoryAfterTest) {
  TestRunner.dumpAboutMemoryAfterTest = true;
}

if (params.dumpDMDAfterTest) {
  TestRunner.dumpDMDAfterTest = true;
}

// We need to check several things here because mochitest-chrome passes
// `jsdebugger` and `debugger` directly, but in other tests we're reliant
// on the `interactiveDebugger` flag being passed along.
if (params.interactiveDebugger || params.jsdebugger || params.debugger) {
  TestRunner.interactiveDebugger = true;
}

if (params.jscovDirPrefix) {
  TestRunner.jscovDirPrefix = params.jscovDirPrefix;
}

if (params.maxTimeouts) {
  TestRunner.maxTimeouts = params.maxTimeouts;
}

if (params.cleanupCrashes) {
  TestRunner.cleanupCrashes = true;
}

if (params.xOriginTests) {
  TestRunner.xOriginTests = true;
  TestRunner.setXOriginEventHandler();
}

if (params.timeoutAsPass) {
  TestRunner.timeoutAsPass = true;
}

if (params.conditionedProfile) {
  TestRunner.conditionedProfile = true;
}

if (params.comparePrefs) {
  TestRunner.comparePrefs = true;
}

// Log things to the console if appropriate.
TestRunner.logger.addListener(
  "dumpListener",
  consoleLevel + "",
  function (msg) {
    dump(msg.info.join(" ") + "\n");
  }
);

var gTestList = [];
var RunSet = {};

RunSet.runall = function (e) {
  // Filter tests to include|exclude tests based on data in params.filter.
  // This allows for including or excluding tests from the gTestList
  // TODO Only used by ipc tests, remove once those are implemented sanely
  if (params.testManifest) {
    getTestManifest(
      getTestManifestURL(params.testManifest),
      params,
      function (filter) {
        gTestList = filterTests(filter, gTestList, params.runOnly);
        RunSet.runtests();
      }
    );
  } else {
    RunSet.runtests();
  }
};

RunSet.runtests = function (e) {
  // Which tests we're going to run
  var my_tests = gTestList;

  if (params.startAt || params.endAt) {
    my_tests = skipTests(my_tests, params.startAt, params.endAt);
  }

  if (params.shuffle) {
    for (var i = my_tests.length - 1; i > 0; --i) {
      var j = Math.floor(Math.random() * i);
      var tmp = my_tests[j];
      my_tests[j] = my_tests[i];
      my_tests[i] = tmp;
    }
  }
  TestRunner.setParameterInfo(params);
  TestRunner.runTests(my_tests);
};

RunSet.reloadAndRunAll = function (e) {
  e.preventDefault();
  //window.location.hash = "";
  if (params.autorun) {
    window.location.search += "";
    // eslint-disable-next-line no-self-assign
    window.location.href = window.location.href;
  } else if (window.location.search) {
    window.location.href += "&autorun=1";
  } else {
    window.location.href += "?autorun=1";
  }
};

// UI Stuff
function toggleVisible(elem) {
  toggleElementClass("invisible", elem);
}

function makeVisible(elem) {
  removeElementClass(elem, "invisible");
}

function makeInvisible(elem) {
  addElementClass(elem, "invisible");
}

function isVisible(elem) {
  // you may also want to check for
  // getElement(elem).style.display == "none"
  return !hasElementClass(elem, "invisible");
}

function toggleNonTests(e) {
  e.preventDefault();
  var elems = document.getElementsByClassName("non-test");
  for (var i = "0"; i < elems.length; i++) {
    toggleVisible(elems[i]);
  }
  if (isVisible(elems[0])) {
    $("toggleNonTests").innerHTML = "Hide Non-Tests";
  } else {
    $("toggleNonTests").innerHTML = "Show Non-Tests";
  }
}

// hook up our buttons
function hookup() {
  if (params.manifestFile) {
    getTestManifest(
      getTestManifestURL(params.manifestFile),
      params,
      hookupTests
    );
  } else {
    hookupTests(gTestList);
  }
}

function getPrefList() {
  if (params.ignorePrefsFile) {
    loadFile(getTestManifestURL(params.ignorePrefsFile), function (prefs) {
      TestRunner.ignorePrefs = prefs;
      RunSet.runall();
    });
  } else {
    RunSet.runall();
  }
}

function hookupTests(testList) {
  if (testList.length) {
    gTestList = testList;
  } else {
    gTestList = [];
    for (var obj in testList) {
      gTestList.push(testList[obj]);
    }
  }

  document.getElementById("runtests").onclick = RunSet.reloadAndRunAll;
  document.getElementById("toggleNonTests").onclick = toggleNonTests;
  // run automatically if autorun specified
  if (params.autorun) {
    getPrefList();
  }
}

function getTestManifestURL(path) {
  // The test manifest url scheme should be the same protocol as the containing
  // window... unless it's not http(s)
  if (
    window.location.protocol == "http:" ||
    window.location.protocol == "https:"
  ) {
    return window.location.protocol + "//" + window.location.host + "/" + path;
  }
  return "http://mochi.test:8888/" + path;
}
