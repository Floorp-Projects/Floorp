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

// repeat tells us how many times to repeat the tests
if (params.repeat) {
  TestRunner.repeat = params.repeat;
} 

if (params.runUntilFailure) {
  TestRunner.runUntilFailure = true;
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

// A temporary hack for android 4.0 where Fennec utilizes the pandaboard so much it reboots
if (params.runSlower) {
  TestRunner.runSlower = true;
}


var gTestList = [];
var RunSet = {}
RunSet.runall = function(e) {
  // Filter tests to include|exclude tests based on data in params.filter.
  // This allows for including or excluding tests from the gTestList
  gTestList = filterTests(params.testManifest, params.runOnly);

  // Which tests we're going to run
  var my_tests = gTestList;

  if (params.totalChunks && params.thisChunk) {
    my_tests = chunkifyTests(my_tests, params.totalChunks, params.thisChunk, params.chunkByDir, TestRunner.logger);
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
function filterTests(filterFile, runOnly) {
  var filteredTests = [];
  var removedTests = [];
  var runtests = {};
  var excludetests = {};

  if (filterFile == null) {
    return gTestList;
  }

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

  if ('runtests' in filter) {
    runtests = filter.runtests;
  }
  if ('excludetests' in filter)
    excludetests = filter.excludetests;
  if (!('runtests' in filter) && !('excludetests' in filter)) {
    if (runOnly == 'true') {
      runtests = filter;
    } else
      excludetests = filter;
  }

  var testRoot = config.testRoot || "tests";
  // Start with gTestList, and put everything that's in 'runtests' in
  // filteredTests.
  if (Object.keys(runtests).length) {
    for (var i = 0; i < gTestList.length; i++) {
      var test_path = gTestList[i];
      var tmp_path = test_path.replace(/^\//, '');
      for (var f in runtests) {
        // Remove leading /tests/ if exists
        file = f.replace(/^\//, '')
        file = file.replace(/^tests\//, '')

        // Match directory or filename, gTestList has <testroot>/<path>
        if (tmp_path.match(testRoot + "/" + file) != null) {
          filteredTests.push(test_path);
          break;
        }
      }
    }
  }
  else {
    filteredTests = gTestList.slice(0);
  }

  // Continue with filteredTests, and deselect everything that's in
  // excludedtests.
  if (Object.keys(excludetests).length) {
    var refilteredTests = [];
    for (var i = 0; i < filteredTests.length; i++) {
      var found = false;
      var test_path = filteredTests[i];
      var tmp_path = test_path.replace(/^\//, '');
      for (var f in excludetests) {
        // Remove leading /tests/ if exists
        file = f.replace(/^\//, '')
        file = file.replace(/^tests\//, '')

        // Match directory or filename, gTestList has <testroot>/<path>
        if (tmp_path.match(testRoot + "/" + file) != null) {
          found = true;
          break;
        }
      }
      if (!found) {
        refilteredTests.push(test_path);
      }
    }
    filteredTests = refilteredTests;
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
  var elems = document.getElementsByClassName("non-test");
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
