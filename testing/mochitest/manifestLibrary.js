/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function parseTestManifest(testManifest, params, callback) {
  var links = {};
  var paths = [];

  // Support --test-manifest format for mobile/b2g
  if ("runtests" in testManifest || "excludetests" in testManifest) {
    callback(testManifest);
    return;
  }

  // For mochitest-chrome and mochitest-browser-chrome harnesses, we 
  // define tests as links[testname] = true.
  // For mochitest-plain, we define lists as an array of testnames.
  for (var obj of testManifest['tests']) {
    var path = obj['path'];
    // Note that obj.disabled may be "". We still want to skip in that case.
    if ("disabled" in obj) {
      dump("TEST-SKIPPED | " + path + " | " + obj.disabled + "\n");
      continue;
    }
    if (params.testRoot != 'tests' && params.testRoot !== undefined) {
      links[params.baseurl + '/' + params.testRoot + '/' + path] = true
    } else {
      paths.push(params.testPrefix + path);
    }
  }
  if (paths.length > 0) {
    callback(paths);
  } else {
    callback(links);
  }
}

function getTestManifest(url, params, callback) {
  var req = new XMLHttpRequest();
  req.open("GET", url);
  req.onload = function() {
    if (req.readyState == 4) {
      if (req.status == 200) {
        try {
          parseTestManifest(JSON.parse(req.responseText), params, callback);
        } catch (e) {
          dump("TEST-UNEXPECTED-FAIL: setup.js | error parsing " + url + " (" + e + ")\n");
          throw e;
        }
      } else {
        dump("TEST-UNEXPECTED-FAIL: setup.js | error loading " + url + "\n");
        callback({});
      }
    }
  }
  req.send();
}

// Test Filtering Code

/*
 Open the file referenced by runOnly|exclude and use that to compare against
 testList
 parameters:
   filter = json object of runtests | excludetests
   testList = array of test names to run
   runOnly = use runtests vs excludetests in case both are defined
 returns:
   filtered version of testList
*/
function filterTests(filter, testList, runOnly) {

  var filteredTests = [];
  var removedTests = [];
  var runtests = {};
  var excludetests = {};

  if (filter == null) {
    return testList;
  }

  if ('runtests' in filter) {
    runtests = filter.runtests;
  }
  if ('excludetests' in filter) {
    excludetests = filter.excludetests;
  }
  if (!('runtests' in filter) && !('excludetests' in filter)) {
    if (runOnly == 'true') {
      runtests = filter;
    } else {
      excludetests = filter;
    }
  }

  var testRoot = config.testRoot || "tests";
  // Start with testList, and put everything that's in 'runtests' in
  // filteredTests.
  if (Object.keys(runtests).length) {
    for (var i = 0; i < testList.length; i++) {
      var testpath = testList[i];
      var tmppath = testpath.replace(/^\//, '');
      for (var f in runtests) {
        // Remove leading /tests/ if exists
        file = f.replace(/^\//, '')
        file = file.replace(/^tests\//, '')

        // Match directory or filename, testList has <testroot>/<path>
        if (tmppath.match(testRoot + "/" + file) != null) {
          filteredTests.push(testpath);
          break;
        }
      }
    }
  } else {
    filteredTests = testList.slice(0);
  }

  // Continue with filteredTests, and deselect everything that's in
  // excludedtests.
  if (!Object.keys(excludetests).length) {
    return filteredTests;
  }

  var refilteredTests = [];
  for (var i = 0; i < filteredTests.length; i++) {
    var found = false;
    var testpath = filteredTests[i];
    var tmppath = testpath.replace(/^\//, '');
    for (var f in excludetests) {
      // Remove leading /tests/ if exists
      file = f.replace(/^\//, '')
      file = file.replace(/^tests\//, '')

      // Match directory or filename, testList has <testroot>/<path>
      if (tmppath.match(testRoot + "/" + file) != null) {
        found = true;
        break;
      }
    }
    if (!found) {
      refilteredTests.push(testpath);
    }
  }
  return refilteredTests;
}
