/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Sayre <sayrer@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
if (params.loops) {
  TestRunner.loops = params.loops;
} 

// closeWhenDone tells us to call quit.js when complete
if (params.closeWhenDone) {
  TestRunner.onComplete = goQuitApplication;
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
