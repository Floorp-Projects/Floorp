/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function parseTestManifest(testManifest, params, callback) {
  let links = {};
  let paths = [];

  // Support --test-manifest format for mobile
  if ("runtests" in testManifest || "excludetests" in testManifest) {
    callback(testManifest);
    return;
  }

  // For mochitest-chrome and mochitest-browser-chrome harnesses, we
  // define tests as links[testname] = true.
  // For mochitest-plain, we define lists as an array of testnames.
  for (let obj of testManifest.tests) {
    let path = obj.path;
    // Note that obj.disabled may be "". We still want to skip in that case.
    if ("disabled" in obj) {
      dump("TEST-SKIPPED | " + path + " | " + obj.disabled + "\n");
      continue;
    }
    if (params.testRoot != "tests" && params.testRoot !== undefined) {
      let name = params.baseurl + "/" + params.testRoot + "/" + path;
      links[name] = {
        test: {
          url: name,
          expected: obj.expected,
          https_first_disabled: obj.https_first_disabled,
          allow_xul_xbl: obj.allow_xul_xbl,
        },
      };
    } else {
      let name = params.testPrefix + path;
      if (params.xOriginTests && obj.scheme == "https") {
        name = params.httpsBaseUrl + path;
      }
      paths.push({
        test: {
          url: name,
          expected: obj.expected,
          https_first_disabled: obj.https_first_disabled,
          allow_xul_xbl: obj.allow_xul_xbl,
        },
      });
    }
  }
  if (paths.length) {
    callback(paths);
  } else {
    callback(links);
  }
}

function getTestManifest(url, params, callback) {
  let req = new XMLHttpRequest();
  req.open("GET", url);
  req.onload = function() {
    if (req.readyState == 4) {
      if (req.status == 200) {
        try {
          parseTestManifest(JSON.parse(req.responseText), params, callback);
        } catch (e) {
          dump(
            "TEST-UNEXPECTED-FAIL: manifestLibrary.js | error parsing " +
              url +
              " (" +
              e +
              ")\n"
          );
          throw e;
        }
      } else {
        dump(
          "TEST-UNEXPECTED-FAIL: manifestLibrary.js | error loading " +
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

// Test Filtering Code
// TODO Only used by ipc tests, remove once those are implemented sanely

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
  let filteredTests = [];
  let runtests = {};
  let excludetests = {};

  if (filter == null) {
    return testList;
  }

  if ("runtests" in filter) {
    runtests = filter.runtests;
  }
  if ("excludetests" in filter) {
    excludetests = filter.excludetests;
  }
  if (!("runtests" in filter) && !("excludetests" in filter)) {
    if (runOnly == "true") {
      runtests = filter;
    } else {
      excludetests = filter;
    }
  }

  // eslint-disable-next-line no-undef
  let testRoot = config.testRoot || "tests";
  // Start with testList, and put everything that's in 'runtests' in
  // filteredTests.
  if (Object.keys(runtests).length) {
    for (let i = 0; i < testList.length; i++) {
      let testpath;
      if (testList[i] instanceof Object && "test" in testList[i]) {
        testpath = testList[i].test.url;
      } else {
        testpath = testList[i];
      }
      let tmppath = testpath.replace(/^\//, "");
      for (let f in runtests) {
        // Remove leading /tests/ if exists
        let file = f.replace(/^\//, "");
        file = file.replace(/^tests\//, "");

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

  let refilteredTests = [];
  for (let i = 0; i < filteredTests.length; i++) {
    let found = false;
    let testpath;
    if (filteredTests[i] instanceof Object && "test" in filteredTests[i]) {
      testpath = filteredTests[i].test.url;
    } else {
      testpath = filteredTests[i];
    }
    let tmppath = testpath.replace(/^\//, "");
    for (let f in excludetests) {
      // Remove leading /tests/ if exists
      let file = f.replace(/^\//, "");
      file = file.replace(/^tests\//, "");

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
