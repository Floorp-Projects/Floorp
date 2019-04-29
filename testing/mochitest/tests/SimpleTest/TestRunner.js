/* -*- js-indent-level: 4; indent-tabs-mode: nil -*- */
/*
 * e10s event dispatcher from content->chrome
 *
 * type = eventName (QuitApplication)
 * data = json object {"filename":filename} <- for LoggerInit
 */

"use strict";

function getElement(id) {
    return ((typeof(id) == "string") ?
        document.getElementById(id) : id);
}

this.$ = this.getElement;

function contentDispatchEvent(type, data, sync) {
  if (typeof(data) == "undefined") {
    data = {};
  }

  var event = new CustomEvent("contentEvent", {
    bubbles: true,
    detail: {
      "sync": sync,
      "type": type,
      "data": JSON.stringify(data)
    }
  });
  document.dispatchEvent(event);
}

function contentAsyncEvent(type, data) {
  contentDispatchEvent(type, data, 0);
}

/* Helper Function */
function extend(obj, /* optional */ skip) {
    // Extend an array with an array-like object starting
    // from the skip index
    if (!skip) {
        skip = 0;
    }
    if (obj) {
        var l = obj.length;
        var ret = [];
        for (var i = skip; i < l; i++) {
            ret.push(obj[i]);
        }
    }
    return ret;
}

function flattenArguments(lst/* ...*/) {
    var res = [];
    var args = extend(arguments);
    while (args.length) {
        var o = args.shift();
        if (o && typeof(o) == "object" && typeof(o.length) == "number") {
            for (var i = o.length - 1; i >= 0; i--) {
                args.unshift(o[i]);
            }
        } else {
            res.push(o);
        }
    }
    return res;
}

/**
 * TestRunner: A test runner for SimpleTest
 * TODO:
 *
 *  * Avoid moving iframes: That causes reloads on mozilla and opera.
 *
 *
**/
var TestRunner = {};
TestRunner.logEnabled = false;
TestRunner._currentTest = 0;
TestRunner._lastTestFinished = -1;
TestRunner._loopIsRestarting = false;
TestRunner.currentTestURL = "";
TestRunner.originalTestURL = "";
TestRunner._urls = [];
TestRunner._lastAssertionCount = 0;
TestRunner._expectedMinAsserts = 0;
TestRunner._expectedMaxAsserts = 0;

TestRunner.timeout = 300 * 1000; // 5 minutes.
TestRunner.maxTimeouts = 4; // halt testing after too many timeouts
TestRunner.runSlower = false;
TestRunner.dumpOutputDirectory = "";
TestRunner.dumpAboutMemoryAfterTest = false;
TestRunner.dumpDMDAfterTest = false;
TestRunner.slowestTestTime = 0;
TestRunner.slowestTestURL = "";
TestRunner.interactiveDebugger = false;
TestRunner.cleanupCrashes = false;

TestRunner._expectingProcessCrash = false;
TestRunner._structuredFormatter = new StructuredFormatter();

/**
 * Make sure the tests don't hang indefinitely.
**/
TestRunner._numTimeouts = 0;
TestRunner._currentTestStartTime = new Date().valueOf();
TestRunner._timeoutFactor = 1;

/**
* Used to collect code coverage with the js debugger.
*/
TestRunner.jscovDirPrefix = "";
var coverageCollector = {};

TestRunner._checkForHangs = function() {
  function reportError(win, msg) {
    if ("SimpleTest" in win) {
      win.SimpleTest.ok(false, msg);
    } else if ("W3CTest" in win) {
      win.W3CTest.logFailure(msg);
    }
  }

  function killTest(win) {
    if ("SimpleTest" in win) {
      win.SimpleTest.timeout();
      win.SimpleTest.finish();
    } else if ("W3CTest" in win) {
      win.W3CTest.timeout();
    }
  }

  if (TestRunner._currentTest < TestRunner._urls.length) {
    var runtime = new Date().valueOf() - TestRunner._currentTestStartTime;
    if (runtime >= TestRunner.timeout * TestRunner._timeoutFactor) {
      var frameWindow = $('testframe').contentWindow.wrappedJSObject ||
                          $('testframe').contentWindow;
      // TODO : Do this in a way that reports that the test ended with a status "TIMEOUT"
      reportError(frameWindow, "Test timed out.");

      // If we have too many timeouts, give up. We don't want to wait hours
      // for results if some bug causes lots of tests to time out.
      if (++TestRunner._numTimeouts >= TestRunner.maxTimeouts) {
        TestRunner._haltTests = true;

        TestRunner.currentTestURL = "(SimpleTest/TestRunner.js)";
        reportError(frameWindow, TestRunner.maxTimeouts + " test timeouts, giving up.");
        var skippedTests = TestRunner._urls.length - TestRunner._currentTest;
        reportError(frameWindow, "Skipping " + skippedTests + " remaining tests.");
      }

      // Add a little (1 second) delay to ensure automation.py has time to notice
      // "Test timed out" log and process it (= take a screenshot).
      setTimeout(function delayedKillTest() { killTest(frameWindow); }, 1000);

      if (TestRunner._haltTests)
        return;
    }

    setTimeout(TestRunner._checkForHangs, 30000);
  }
}

TestRunner.requestLongerTimeout = function(factor) {
    TestRunner._timeoutFactor = factor;
}

/**
 * This is used to loop tests
**/
TestRunner.repeat = 0;
TestRunner._currentLoop = 1;

TestRunner.expectAssertions = function(min, max) {
    if (typeof(max) == "undefined") {
        max = min;
    }
    if (typeof(min) != "number" || typeof(max) != "number" ||
        min < 0 || max < min) {
        throw "bad parameter to expectAssertions";
    }
    TestRunner._expectedMinAsserts = min;
    TestRunner._expectedMaxAsserts = max;
}

/**
 * This function is called after generating the summary.
**/
TestRunner.onComplete = null;

/**
 * Adds a failed test case to a list so we can rerun only the failed tests
 **/
TestRunner._failedTests = {};
TestRunner._failureFile = "";

TestRunner.addFailedTest = function(testName) {
    if (TestRunner._failedTests[testName] == undefined) {
        TestRunner._failedTests[testName] = "";
    }
};

TestRunner.setFailureFile = function(fileName) {
    TestRunner._failureFile = fileName;
}

TestRunner.generateFailureList = function () {
    if (TestRunner._failureFile) {
        var failures = new SpecialPowersLogger(TestRunner._failureFile);
        failures.log(JSON.stringify(TestRunner._failedTests));
        failures.close();
    }
};

/**
 * If logEnabled is true, this is the logger that will be used.
 **/

// This delimiter is used to avoid interleaving Mochitest/Gecko logs.
var LOG_DELIMITER = String.fromCharCode(0xe175) + String.fromCharCode(0xee31) + String.fromCharCode(0x2c32) + String.fromCharCode(0xacbf);

// A log callback for StructuredLog.jsm
TestRunner._dumpMessage = function(message) {
  var str;

  // This is a directive to python to format these messages
  // for compatibility with mozharness. This can be removed
  // with the MochitestFormatter (see bug 1045525).
  message.js_source = 'TestRunner.js'
  if (TestRunner.interactiveDebugger && message.action in TestRunner._structuredFormatter) {
    str = TestRunner._structuredFormatter[message.action](message);
  } else {
    str = LOG_DELIMITER + JSON.stringify(message) + LOG_DELIMITER;
  }
  // BUGFIX: browser-chrome tests don't use LogController
  if (Object.keys(LogController.listeners).length !== 0) {
    LogController.log(str);
  } else {
    dump('\n' + str + '\n');
  }
  // Checking for error messages
  if (message.expected || message.level === "ERROR") {
    TestRunner.failureHandler();
  }
};

// From https://dxr.mozilla.org/mozilla-central/source/testing/modules/StructuredLog.jsm
TestRunner.structuredLogger = new StructuredLogger('mochitest', TestRunner._dumpMessage);
TestRunner.structuredLogger.deactivateBuffering = function() {
    TestRunner.structuredLogger._logData("buffering_off");
};
TestRunner.structuredLogger.activateBuffering = function() {
    TestRunner.structuredLogger._logData("buffering_on");
};

TestRunner.log = function(msg) {
    if (TestRunner.logEnabled) {
        TestRunner.structuredLogger.info(msg);
    } else {
        dump(msg + "\n");
    }
};

TestRunner.error = function(msg) {
    if (TestRunner.logEnabled) {
        TestRunner.structuredLogger.error(msg);
    } else {
        dump(msg + "\n");
        TestRunner.failureHandler();
    }
};

TestRunner.failureHandler = function() {
    if (TestRunner.runUntilFailure) {
      TestRunner._haltTests = true;
    }

    if (TestRunner.debugOnFailure) {
      // You've hit this line because you requested to break into the
      // debugger upon a testcase failure on your test run.
      debugger;
    }
};

/**
 * Toggle element visibility
**/
TestRunner._toggle = function(el) {
    if (el.className == "noshow") {
        el.className = "";
        el.style.cssText = "";
    } else {
        el.className = "noshow";
        el.style.cssText = "width:0px; height:0px; border:0px;";
    }
};

/**
 * Creates the iframe that contains a test
**/
TestRunner._makeIframe = function (url, retry) {
    var iframe = $('testframe');
    if (url != "about:blank" &&
        (("hasFocus" in document && !document.hasFocus()) ||
         ("activeElement" in document && document.activeElement != iframe))) {

        contentAsyncEvent("Focus");
        window.focus();
        SpecialPowers.focus();
        iframe.focus();
        if (retry < 3) {
            window.setTimeout(function() {TestRunner._makeIframe(url, retry+1)}, 1000);
            return;
        }

        TestRunner.structuredLogger.info("Error: Unable to restore focus, expect failures and timeouts.");
    }
    window.scrollTo(0, $('indicator').offsetTop);
    iframe.src = url;
    iframe.name = url;
    iframe.width = "500";
    return iframe;
};

/**
 * Returns the current test URL.
 * We use this to tell whether the test has navigated to another test without
 * being finished first.
 */
TestRunner.getLoadedTestURL = function () {
    var prefix = "";
    // handle mochitest-chrome URIs
    if ($('testframe').contentWindow.location.protocol == "chrome:") {
      prefix = "chrome://mochitests";
    }
    return prefix + $('testframe').contentWindow.location.pathname;
};

TestRunner.setParameterInfo = function (params) {
    this._params = params;
};

TestRunner.getParameterInfo = function() {
    return this._params;
};

/**
 * TestRunner entry point.
 *
 * The arguments are the URLs of the test to be ran.
 *
**/
TestRunner.runTests = function (/*url...*/) {
    TestRunner.structuredLogger.info("SimpleTest START");
    TestRunner.originalTestURL = $("current-test").innerHTML;

    SpecialPowers.registerProcessCrashObservers();

    // Initialize code coverage
    if (TestRunner.jscovDirPrefix != "") {
        var CoverageCollector = SpecialPowers.Cu.import("resource://testing-common/CoverageUtils.jsm", {}).CoverageCollector;
        coverageCollector = new CoverageCollector(TestRunner.jscovDirPrefix);
    }

    SpecialPowers.requestResetCoverageCounters().then(() => {
        TestRunner._urls = flattenArguments(arguments);

        var singleTestRun = this._urls.length <= 1 && TestRunner.repeat <= 1;
        TestRunner.showTestReport = singleTestRun;
        var frame = $('testframe');
        frame.src = "";
        if (singleTestRun) {
            // Can't use document.body because this runs in a XUL doc as well...
            var body = document.getElementsByTagName("body")[0];
            body.setAttribute("singletest", "true");
            frame.removeAttribute("scrolling");
        }
        TestRunner._checkForHangs();
        TestRunner.runNextTest();
    });
};

/**
 * Used for running a set of tests in a loop for debugging purposes
 * Takes an array of URLs
**/
TestRunner.resetTests = function(listURLs) {
  TestRunner._currentTest = 0;
  // Reset our "Current-test" line - functionality depends on it
  $("current-test").innerHTML = TestRunner.originalTestURL;
  if (TestRunner.logEnabled)
    TestRunner.structuredLogger.info("SimpleTest START Loop " + TestRunner._currentLoop);

  TestRunner._urls = listURLs;
  $('testframe').src="";
  TestRunner._checkForHangs();
  TestRunner.runNextTest();
}

TestRunner.getNextUrl = function() {
    var url = "";
    // sometimes we have a subtest/harness which doesn't use a manifest
    if ((TestRunner._urls[TestRunner._currentTest] instanceof Object) && ('test' in TestRunner._urls[TestRunner._currentTest])) {
        url = TestRunner._urls[TestRunner._currentTest]['test']['url'];
        TestRunner.expected = TestRunner._urls[TestRunner._currentTest]['test']['expected'];
    } else {
        url = TestRunner._urls[TestRunner._currentTest];
        TestRunner.expected = 'pass';
    }
    return url;
}

/**
 * Run the next test. If no test remains, calls onComplete().
 **/
TestRunner._haltTests = false;
TestRunner.runNextTest = function() {
    if (TestRunner._currentTest < TestRunner._urls.length &&
        !TestRunner._haltTests)
    {
        var url = TestRunner.getNextUrl();
        TestRunner.currentTestURL = url;

        $("current-test-path").innerHTML = url;

        TestRunner._currentTestStartTime = new Date().valueOf();
        TestRunner._timeoutFactor = 1;
        TestRunner._expectedMinAsserts = 0;
        TestRunner._expectedMaxAsserts = 0;

        TestRunner.structuredLogger.testStart(url);

        TestRunner._makeIframe(url, 0);
    } else {
        $("current-test").innerHTML = "<b>Finished</b>";
        // Only unload the last test to run if we're running more than one test.
        if (TestRunner._urls.length > 1) {
            TestRunner._makeIframe("about:blank", 0);
        }

        var passCount = parseInt($("pass-count").innerHTML, 10);
        var failCount = parseInt($("fail-count").innerHTML, 10);
        var todoCount = parseInt($("todo-count").innerHTML, 10);

        if (passCount === 0 &&
            failCount === 0 &&
            todoCount === 0)
        {
            // No |$('testframe').contentWindow|, so manually update: ...
            // ... the log,
            TestRunner.structuredLogger.error("SimpleTest/TestRunner.js | No checks actually run");
            // ... the count,
            $("fail-count").innerHTML = 1;
            // ... the indicator.
            var indicator = $("indicator");
            indicator.innerHTML = "Status: Fail (No checks actually run)";
            indicator.style.backgroundColor = "red";
        }

        let e10sMode = SpecialPowers.isMainProcess() ? "non-e10s" : "e10s";

        TestRunner.structuredLogger.info("TEST-START | Shutdown");
        TestRunner.structuredLogger.info("Passed:  " + passCount);
        TestRunner.structuredLogger.info("Failed:  " + failCount);
        TestRunner.structuredLogger.info("Todo:    " + todoCount);
        TestRunner.structuredLogger.info("Mode:    " + e10sMode);
        TestRunner.structuredLogger.info("Slowest: " + TestRunner.slowestTestTime + 'ms - ' + TestRunner.slowestTestURL);

        // If we are looping, don't send this cause it closes the log file,
        // also don't unregister the crash observers until we're done.
        if (TestRunner.repeat === 0) {
          SpecialPowers.unregisterProcessCrashObservers();
          TestRunner.structuredLogger.info("SimpleTest FINISHED");
        }

        if (TestRunner.repeat === 0 && TestRunner.onComplete) {
             TestRunner.onComplete();
         }

        if (TestRunner._currentLoop <= TestRunner.repeat && !TestRunner._haltTests) {
          TestRunner._currentLoop++;
          TestRunner.resetTests(TestRunner._urls);
          TestRunner._loopIsRestarting = true;
        } else {
          // Loops are finished
          if (TestRunner.logEnabled) {
            TestRunner.structuredLogger.info("TEST-INFO | Ran " + TestRunner._currentLoop + " Loops");
            TestRunner.structuredLogger.info("SimpleTest FINISHED");
          }

          if (TestRunner.onComplete)
            TestRunner.onComplete();
        }
        TestRunner.generateFailureList();

        if (TestRunner.jscovDirPrefix != "") {
          coverageCollector.finalize();
        }
    }
};

TestRunner.expectChildProcessCrash = function() {
    TestRunner._expectingProcessCrash = true;
};

/**
 * This stub is called by SimpleTest when a test is finished.
**/
TestRunner.testFinished = function(tests) {
    // Need to track subtests recorded here separately or else they'll
    // trigger the `result after SimpleTest.finish()` error.
    var extraTests = [];
    var result = "OK";

    // Prevent a test from calling finish() multiple times before we
    // have a chance to unload it.
    if (TestRunner._currentTest == TestRunner._lastTestFinished &&
        !TestRunner._loopIsRestarting) {
        TestRunner.structuredLogger.testEnd(TestRunner.currentTestURL,
                                            "ERROR",
                                            "OK",
                                            "called finish() multiple times");
        TestRunner.updateUI([{ result: false }]);
        return;
    }

    if (TestRunner.jscovDirPrefix != "") {
        coverageCollector.recordTestCoverage(TestRunner.currentTestURL);
    }

    SpecialPowers.requestDumpCoverageCounters().then(() => {
        TestRunner._lastTestFinished = TestRunner._currentTest;
        TestRunner._loopIsRestarting = false;

        // TODO : replace this by a function that returns the mem data as an object
        // that's dumped later with the test_end message
        MemoryStats.dump(TestRunner._currentTest,
                         TestRunner.currentTestURL,
                         TestRunner.dumpOutputDirectory,
                         TestRunner.dumpAboutMemoryAfterTest,
                         TestRunner.dumpDMDAfterTest);

        function cleanUpCrashDumpFiles() {
            if (!SpecialPowers.removeExpectedCrashDumpFiles(TestRunner._expectingProcessCrash)) {
                var subtest = "expected-crash-dump-missing";
                TestRunner.structuredLogger.testStatus(TestRunner.currentTestURL,
                                                       subtest,
                                                       "ERROR",
                                                       "PASS",
                                                       "This test did not leave any crash dumps behind, but we were expecting some!");
                extraTests.push({ name: subtest, result: false });
                result = "ERROR";
            }

            var unexpectedCrashDumpFiles =
                SpecialPowers.findUnexpectedCrashDumpFiles();
            TestRunner._expectingProcessCrash = false;
            if (unexpectedCrashDumpFiles.length) {
                var subtest = "unexpected-crash-dump-found";
                TestRunner.structuredLogger.testStatus(TestRunner.currentTestURL,
                                                       subtest,
                                                       "ERROR",
                                                       "PASS",
                                                       "This test left crash dumps behind, but we " +
                                                       "weren't expecting it to!",
                                                       null,
                                                       {unexpected_crashdump_files: unexpectedCrashDumpFiles});
                extraTests.push({ name: subtest, result: false });
                result = "CRASH";
                unexpectedCrashDumpFiles.sort().forEach(function(aFilename) {
                    TestRunner.structuredLogger.info("Found unexpected crash dump file " +
                                                     aFilename + ".");
                });
            }

            if (TestRunner.cleanupCrashes) {
                if (SpecialPowers.removePendingCrashDumpFiles()) {
                    TestRunner.structuredLogger.info("This test left pending crash dumps");
                }
            }
        }

        function runNextTest() {
            if (TestRunner.currentTestURL != TestRunner.getLoadedTestURL()) {
                TestRunner.structuredLogger.testStatus(TestRunner.currentTestURL,
                                                       TestRunner.getLoadedTestURL(),
                                                       "FAIL",
                                                       "PASS",
                                                       "finished in a non-clean fashion, probably" +
                                                       " because it didn't call SimpleTest.finish()",
                                                       {loaded_test_url: TestRunner.getLoadedTestURL()});
                extraTests.push({ name: "clean-finish", result: false });
                result = result != "CRASH" ? "ERROR": result
            }

            var runtime = new Date().valueOf() - TestRunner._currentTestStartTime;

            TestRunner.structuredLogger.testEnd(TestRunner.currentTestURL,
                                                result,
                                                "OK",
                                                "Finished in " + runtime + "ms",
                                                {runtime: runtime}
            );

            if (TestRunner.slowestTestTime < runtime && TestRunner._timeoutFactor >= 1) {
              TestRunner.slowestTestTime = runtime;
              TestRunner.slowestTestURL = TestRunner.currentTestURL;
            }

            TestRunner.updateUI(tests.concat(extraTests));

            // Don't show the interstitial if we just run one test with no repeats:
            if (TestRunner._urls.length == 1 && TestRunner.repeat <= 1) {
                TestRunner.testUnloaded();
                return;
            }

            var interstitialURL;
            if ($('testframe').contentWindow.location.protocol == "chrome:") {
                interstitialURL = "tests/SimpleTest/iframe-between-tests.html";
            } else {
                interstitialURL = "/tests/SimpleTest/iframe-between-tests.html";
            }
            // check if there were test run after SimpleTest.finish, which should never happen
            $('testframe').contentWindow.addEventListener('unload', function() {
               var testwin = $('testframe').contentWindow;
               if (testwin.SimpleTest && testwin.SimpleTest._tests.length != testwin.SimpleTest.testsLength) {
                 var wrongtestlength = testwin.SimpleTest._tests.length - testwin.SimpleTest.testsLength;
                 var wrongtestname = '';
                 for (var i = 0; i < wrongtestlength; i++) {
                   wrongtestname = testwin.SimpleTest._tests[testwin.SimpleTest.testsLength + i].name;
                   TestRunner.structuredLogger.error(TestRunner.currentTestURL + " logged result after SimpleTest.finish(): " + wrongtestname);
                 }
                 TestRunner.updateUI([{ result: false }]);
               }
            });
            TestRunner._makeIframe(interstitialURL, 0);
        }

        SpecialPowers.executeAfterFlushingMessageQueue(function() {
            SpecialPowers.waitForCrashes(TestRunner._expectingProcessCrash)
                         .then(() => {
                cleanUpCrashDumpFiles();
                SpecialPowers.flushPermissions(function () {
                    SpecialPowers.flushPrefEnv(runNextTest);
                });
            });
        });
    });
};

TestRunner.testUnloaded = function() {
    // If we're in a debug build, check assertion counts.  This code is
    // similar to the code in Tester_nextTest in browser-test.js used
    // for browser-chrome mochitests.
    if (SpecialPowers.isDebugBuild) {
        var newAssertionCount = SpecialPowers.assertionCount();
        var numAsserts = newAssertionCount - TestRunner._lastAssertionCount;
        TestRunner._lastAssertionCount = newAssertionCount;

        var max = TestRunner._expectedMaxAsserts;
        var min = TestRunner._expectedMinAsserts;
        if (Array.isArray(TestRunner.expected)) {
            // Accumulate all assertion counts recorded in the failure pattern file.
            let additionalAsserts = TestRunner.expected.reduce((acc, [pat, count]) => {
                return pat == "ASSERTION" ? acc + count : acc;
            }, 0);
            min += additionalAsserts;
            max += additionalAsserts;
        }
        TestRunner.structuredLogger.assertionCount(TestRunner.currentTestURL, numAsserts, min, max);
    }

    TestRunner._currentTest++;
    if (TestRunner.runSlower) {
        setTimeout(TestRunner.runNextTest, 1000);
    } else {
        TestRunner.runNextTest();
    }
};

/**
 * Get the results.
 */
TestRunner.countResults = function(tests) {
  var nOK = 0;
  var nNotOK = 0;
  var nTodo = 0;
  for (var i = 0; i < tests.length; ++i) {
    var test = tests[i];
    if (test.todo && !test.result) {
      nTodo++;
    } else if (test.result && !test.todo) {
      nOK++;
    } else {
      nNotOK++;
    }
  }
  return {"OK": nOK, "notOK": nNotOK, "todo": nTodo};
}

/**
 * Print out table of any error messages found during looped run
 */
TestRunner.displayLoopErrors = function(tableName, tests) {
  if(TestRunner.countResults(tests).notOK >0){
    var table = $(tableName);
    var curtest;
    if (table.rows.length == 0) {
      //if table headers are not yet generated, make them
      var row = table.insertRow(table.rows.length);
      var cell = row.insertCell(0);
      var textNode = document.createTextNode("Test File Name:");
      cell.appendChild(textNode);
      cell = row.insertCell(1);
      textNode = document.createTextNode("Test:");
      cell.appendChild(textNode);
      cell = row.insertCell(2);
      textNode = document.createTextNode("Error message:");
      cell.appendChild(textNode);
    }

    //find the broken test
    for (var testnum in tests){
      curtest = tests[testnum];
      if( !((curtest.todo && !curtest.result) || (curtest.result && !curtest.todo)) ){
        //this is a failed test or the result of todo test. Display the related message
        row = table.insertRow(table.rows.length);
        cell = row.insertCell(0);
        textNode = document.createTextNode(TestRunner.currentTestURL);
        cell.appendChild(textNode);
        cell = row.insertCell(1);
        textNode = document.createTextNode(curtest.name);
        cell.appendChild(textNode);
        cell = row.insertCell(2);
        textNode = document.createTextNode((curtest.diag ? curtest.diag : "" ));
        cell.appendChild(textNode);
      }
    }
  }
}

TestRunner.updateUI = function(tests) {
  var results = TestRunner.countResults(tests);
  var passCount = parseInt($("pass-count").innerHTML) + results.OK;
  var failCount = parseInt($("fail-count").innerHTML) + results.notOK;
  var todoCount = parseInt($("todo-count").innerHTML) + results.todo;
  $("pass-count").innerHTML = passCount;
  $("fail-count").innerHTML = failCount;
  $("todo-count").innerHTML = todoCount;

  // Set the top Green/Red bar
  var indicator = $("indicator");
  if (failCount > 0) {
    indicator.innerHTML = "Status: Fail";
    indicator.style.backgroundColor = "red";
  } else if (passCount > 0) {
    indicator.innerHTML = "Status: Pass";
    indicator.style.backgroundColor = "#0d0";
  } else {
    indicator.innerHTML = "Status: ToDo";
    indicator.style.backgroundColor = "orange";
  }

  // Set the table values
  var trID = "tr-" + $('current-test-path').innerHTML;
  var row = $(trID);

  // Only update the row if it actually exists (autoUI)
  if (row != null) {
    var tds = row.getElementsByTagName("td");
    tds[0].style.backgroundColor = "#0d0";
    tds[0].innerHTML = parseInt(tds[0].innerHTML) + parseInt(results.OK);
    tds[1].style.backgroundColor = results.notOK > 0 ? "red" : "#0d0";
    tds[1].innerHTML = parseInt(tds[1].innerHTML) + parseInt(results.notOK);
    tds[2].style.backgroundColor = results.todo > 0 ? "orange" : "#0d0";
    tds[2].innerHTML = parseInt(tds[2].innerHTML) + parseInt(results.todo);
  }

  //if we ran in a loop, display any found errors
  if (TestRunner.repeat > 0) {
    TestRunner.displayLoopErrors('fail-table', tests);
  }
}
