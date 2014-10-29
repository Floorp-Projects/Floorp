/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function chunkifyTests(tests, totalChunks, thisChunk, chunkByDir, logger) {
  var total_chunks = parseInt(totalChunks);
  // this_chunk is in the range [1,total_chunks]
  var this_chunk = parseInt(thisChunk);
  var returnTests;

  // We want to split the tests up into chunks according to which directory
  // they're in
  if (chunkByDir) {
    chunkByDir = parseInt(chunkByDir);
    var tests_by_dir = {};
    var test_dirs = []
    for (var i = 0; i < tests.length; ++i) {
      if ('test' in tests[i]) {
        var test_path = tests[i]['test']['url'];
      } else {
        var test_path = tests[i]['url'];
      }
      if (test_path[0] == '/') {
        test_path = test_path.substr(1);
      }
      // mochitest-chrome and mochitest-browser-chrome pass an array of chrome://
      // URIs
      var protocolRegexp = /^[a-zA-Z]+:\/\//;
      if (protocolRegexp.test(test_path)) {
        test_path = test_path.replace(protocolRegexp, "");
      }
      var dir = test_path.split("/");
      // We want the first chunkByDir+1 components, or everything but the
      // last component, whichever is less.
      // we add 1 to chunkByDir since 'tests' is always part of the path, and
      // want to ignore the last component since it's the test filename.
      dir = dir.slice(0, Math.min(chunkByDir+1, dir.length-1));
      // reconstruct a directory name
      dir = dir.join("/");
      if (!(dir in tests_by_dir)) {
        tests_by_dir[dir] = [tests[i]];
        test_dirs.push(dir);
      } else {
        tests_by_dir[dir].push(tests[i]);
      }
    }
    var tests_per_chunk = test_dirs.length / total_chunks;
    var start = Math.round((this_chunk-1) * tests_per_chunk);
    var end = Math.round(this_chunk * tests_per_chunk);
    returnTests = [];
    var dirs = []
    for (var i = start; i < end; ++i) {
      var dir = test_dirs[i];
      dirs.push(dir);
      returnTests = returnTests.concat(tests_by_dir[dir]);
    }
    if (logger)
      logger.log("Running tests in " + dirs.join(", "));
  } else {
    var tests_per_chunk = tests.length / total_chunks;
    var start = Math.round((this_chunk-1) * tests_per_chunk);
    var end = Math.round(this_chunk * tests_per_chunk);
    returnTests = tests.slice(start, end);
    if (logger)
      logger.log("Running tests " + (start+1) + "-" + end + "/" + tests.length);
  }

  return returnTests;
}

function skipTests(tests, startTestPattern, endTestPattern) {
  var startIndex = 0, endIndex = tests.length - 1;
  for (var i = 0; i < tests.length; ++i) {
    var test_path;
    if ((tests[i] instanceof Object) && ('test' in tests[i])) {
      test_path = tests[i]['test']['url'];
    } else {
      test_path = tests[i];
    }
    if (startTestPattern && test_path.endsWith(startTestPattern)) {
      startIndex = i;
    }

    if (endTestPattern && test_path.endsWith(endTestPattern)) {
      endIndex = i;
    }
  }

  return tests.slice(startIndex, endIndex + 1);
}
