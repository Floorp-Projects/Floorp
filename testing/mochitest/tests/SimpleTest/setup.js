/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

TestRunner.logEnabled = true;
TestRunner.logger = LogController;

/* Helper function */
parseQueryString = function(encodedString, useArrays) {
  // strip a leading '?' from the encoded string
  var qstr = (encodedString[0] == "?") ? encodedString.substring(1) : 
                                         encodedString;
  var pairs = qstr.replace(/\+/g, "%20").split(/(\&amp\;|\&\#38\;|\&#x26;|\&)/);
  var o = {};
  var decode;
  if (typeof(decodeURIComponent) != "undefined") {
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
};

// Check the query string for arguments
var params = parseQueryString(location.search.substring(1), true);

var config = {};
if (window.readConfig) {
  config = readConfig();
}

if (config.testRoot == "chrome" || config.testRoot == "a11y") {
  for (p in params) {
    if (params[p] == 1) {
      config[p] = true;
    } else if (params[p] == 0) {
      config[p] = false;
    } else {
      config[p] = params[p];
    }
  }
  params = config;
}

// set the per-test timeout if specified in the query string
if (params.timeout) {
  TestRunner.timeout = parseInt(params.timeout) * 1000;
}

// log levels for console and logfile
var fileLevel =  params.fileLevel || null;
var consoleLevel = params.consoleLevel || null;

// loop tells us how many times to run the tests
if (params.repeat) {
  TestRunner.repeat = params.repeat;
} 

// closeWhenDone tells us to close the browser when complete
if (params.closeWhenDone) {
  TestRunner.onComplete = SpecialPowers.quit;
}

if (params.failureFile) {
  TestRunner.setFailureFile(params.failureFile);
}

// logFile to write our results
if (params.logFile) {
  var spl = new SpecialPowersLogger(params.logFile);
  TestRunner.logger.addListener("mozLogger", fileLevel + "", spl.getLogCallback());
}

// if we get a quiet param, don't log to the console
if (!params.quiet) {
  function dumpListener(msg) {
    dump(msg.num + " " + msg.level + " " + msg.info.join(' ') + "\n");
  }
  TestRunner.logger.addListener("dumpListener", consoleLevel + "", dumpListener);
}

var gTestList = [];
var RunSet = {}
RunSet.runall = function(e) {
  // Filter tests to include|exclude tests based on data in params.filter.
  // This allows for including or excluding tests from the gTestList
  gTestList = filterTests(params.runOnlyTests, params.excludeTests);

  // Which tests we're going to run
  var my_tests = gTestList;

  if (params.totalChunks && params.thisChunk) {
    var total_chunks = parseInt(params.totalChunks);
    // this_chunk is in the range [1,total_chunks]
    var this_chunk = parseInt(params.thisChunk);

    // We want to split the tests up into chunks according to which directory
    // they're in
    if (params.chunkByDir) {
      var chunkByDir = parseInt(params.chunkByDir);
      var tests_by_dir = {};
      var test_dirs = []
      for (var i = 0; i < gTestList.length; ++i) {
        var test_path = gTestList[i];
        if (test_path[0] == '/') {
          test_path = test_path.substr(1);
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
          tests_by_dir[dir] = [gTestList[i]];
          test_dirs.push(dir);
        } else {
          tests_by_dir[dir].push(gTestList[i]);
        }
      }
      var tests_per_chunk = test_dirs.length / total_chunks;
      var start = Math.round((this_chunk-1) * tests_per_chunk);
      var end = Math.round(this_chunk * tests_per_chunk);
      my_tests = [];
      var dirs = []
      for (var i = start; i < end; ++i) {
        var dir = test_dirs[i];
        dirs.push(dir);
        my_tests = my_tests.concat(tests_by_dir[dir]);
      }
      TestRunner.logger.log("Running tests in " + dirs.join(", "));
    } else {
      var tests_per_chunk = gTestList.length / total_chunks;
      var start = Math.round((this_chunk-1) * tests_per_chunk);
      var end = Math.round(this_chunk * tests_per_chunk);
      my_tests = gTestList.slice(start, end);
      TestRunner.logger.log("Running tests " + (start+1) + "-" + end + "/" + gTestList.length);
    }
  }
  if (params.shuffle) {
    for (var i = my_tests.length-1; i > 0; --i) {
      var j = Math.floor(Math.random() * i);
      var tmp = my_tests[j];
      my_tests[j] = my_tests[i];
      my_tests[i] = tmp;
    }
  }
  TestRunner.runTests(my_tests);
}

RunSet.reloadAndRunAll = function(e) {
  e.preventDefault();
  //window.location.hash = "";
  var addParam = "";
  if (params.autorun) {
    window.location.search += "";
    window.location.href = window.location.href;
  } else if (window.location.search) {
    window.location.href += "&autorun=1";
  } else {
    window.location.href += "?autorun=1";
  }  
};

// Test Filtering Code

// Open the file referenced by runOnly|exclude and use that to compare against
// gTestList.  Return a modified version of gTestList
function filterTests(runOnly, exclude) {
  var filteredTests = [];
  var filterFile = null;

  if (runOnly) {
    filterFile = runOnly;
  } else if (exclude) {
    filterFile = exclude;
  }

  if (filterFile == null)
    return gTestList;

  var datafile = "http://mochi.test:8888/" + filterFile;
  var objXml = new XMLHttpRequest();
  objXml.open("GET",datafile,false);
  objXml.send(null);
  try {
    var filter = JSON.parse(objXml.responseText);
  } catch (ex) {
    dump("INFO | setup.js | error loading or parsing '" + datafile + "'\n");
    return gTestList;
  }
  
  for (var i = 0; i < gTestList.length; ++i) {
    var test_path = gTestList[i];
    
    //We use tmp_path to remove leading '/'
    var tmp_path = test_path.replace(/^\//, '');

    var found = false;

    for (var f in filter) {
      // Remove leading /tests/ if exists
      file = f.replace(/^\//, '')
      file = file.replace(/^tests\//, '')
      
      // Match directory or filename, gTestList has tests/<path>
      if (tmp_path.match("^tests/" + file) != null) {
        if (runOnly)
          filteredTests.push(test_path);
        found = true;
        break;
      }
    }

    if (exclude && !found)
      filteredTests.push(test_path);
  }

  return filteredTests;
}

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
};

function toggleNonTests (e) {
  e.preventDefault();
  var elems = document.getElementsClassName("non-test");
  for (var i="0"; i<elems.length; i++) {
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
  document.getElementById('runtests').onclick = RunSet.reloadAndRunAll;
  document.getElementById('toggleNonTests').onclick = toggleNonTests; 
  // run automatically if autorun specified
  if (params.autorun) {
    RunSet.runall();
  }
}
