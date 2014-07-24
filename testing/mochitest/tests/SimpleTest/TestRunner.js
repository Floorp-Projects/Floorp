/* -*- js-indent-level: 4; indent-tabs-mode: nil -*- */
/*
 * e10s event dispatcher from content->chrome
 *
 * type = eventName (QuitApplication)
 * data = json object {"filename":filename} <- for LoggerInit
 */
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
 * StructuredFormatter: Formatter class turning structured messages
 * into human-readable messages.
 */
this.StructuredFormatter = function() {
    this.testStartTimes = {};
};

StructuredFormatter.prototype.log = function(message) {
  return message.message;
};

StructuredFormatter.prototype.suite_start = function(message) {
    this.suiteStartTime = message.time;
    return "SUITE-START | Running " +  message.tests.length + " tests";
};

StructuredFormatter.prototype.test_start = function(message) {
    this.testStartTimes[message.test] = new Date().getTime();
    return "TEST-START | " + message.test;
};

StructuredFormatter.prototype.test_status = function(message) {
    var statusInfo = message.test + " | " + message.subtest +
                    (message.message ? " | " + message.message : "");
    if (message.expected) {
        return "TEST-UNEXPECTED-" + message.status + " | " + statusInfo +
               " - expected: " + message.expected;
    } else {
        return "TEST-" + message.status + " | " + statusInfo;
    }
};

StructuredFormatter.prototype.test_end = function(message) {
    var startTime = this.testStartTimes[message.test];
    delete this.testStartTimes[message.test];
    var statusInfo = message.test + (message.message ? " | " + String(message.message) : "");
    var result;
    if (message.expected) {
        result = "TEST-UNEXPECTED-" + message.status + " | " + statusInfo +
                 " - expected: " + message.expected;
    } else {
        return "TEST-" + message.status + " | " + statusInfo;
    }
    result = " | took " + message.time - startTime + "ms";
};

StructuredFormatter.prototype.suite_end = function(message) {
    return "SUITE-END | took " + message.time - this.suiteStartTime + "ms";
};

/**
 * StructuredLogger: Structured logger class following the mozlog.structured protocol
 *
 *
**/
var VALID_ACTIONS = ['suite_start', 'suite_end', 'test_start', 'test_end', 'test_status', 'process_output', 'log'];
// This delimiter is used to avoid interleaving Mochitest/Gecko logs.
var LOG_DELIMITER = String.fromCharCode(0xe175) + String.fromCharCode(0xee31) + String.fromCharCode(0x2c32) + String.fromCharCode(0xacbf);

function StructuredLogger(name) {
    this.name = name;
    this.testsStarted = [];
    this.interactiveDebugger = false;
    this.structuredFormatter = new StructuredFormatter();

    /* test logs */

    this.testStart = function(test) {
        var data = {test: test};
        this._logData("test_start", data);
    };

    this.testStatus = function(test, subtest, status, expected="PASS", message=null) {
        var data = {test: test, subtest: subtest, status: status};

        if (message !== null) {
            data.message = String(message);
        }
        if (expected != status && status != 'SKIP') {
            data.expected = expected;
        }

        this._logData("test_status", data);
    };

    this.testEnd = function(test, status, expected="OK", message=null, extra=null) {
        var data = {test: test, status: status};

        if (message !== null) {
            data.message = String(message);
        }
        if (expected != status) {
            data.expected = expected;
        }
        if (extra !== null) {
            data.extra = extra;
        }

        if (!this.testsStarted.pop(test)) {
            this.error("Test '" + test + "' was ended more than once or never started. " +
                       "Ended with this data: " + JSON.stringify(data));
            return;
        }

        this._logData("test_end", data);
    };

    this.suiteStart = function(tests, runinfo) {
        runinfo = typeof runinfo !== "undefined" ? runinfo : null;

        var data = {tests: tests};
        if (runinfo !== null) {
            data.runinfo = runinfo;
        }

        this._logData("suite_start", data);
    };

    this.suiteEnd = function() {
        this._logData("suite_end");
    };

    this.testStart = function(test) {
        this.testsStarted.push(test);
        var data = {test: test};
        this._logData("test_start", data);
    };

    /* log action: human readable logs */

    this._log = function(level, message) {
        // Coercing the message parameter to a string, in case an invalid value is passed.
        message = String(message);
        var data = {level: level, message: message};
        this._logData('log', data);
    };

    this.debug = function(message) {
        this._log('DEBUG', message);
    };

    this.info = function(message) {
        this._log('INFO', message);
    };

    this.warning = function(message) {
        this._log('WARNING', message);
    };

    this.error = function(message) {
        this._log("ERROR", message);
    };

    this.critical = function(message) {
        this._log('CRITICAL', message);
    };

    /* Special mochitest messages for deactivating/activating buffering */

    this.deactivateBuffering = function() {
        this._logData("buffering_off");
    };
    this.activateBuffering = function() {
        this._logData("buffering_on");
    };

    /* dispatches a log to handlers */

    this._logData = function(action, data) {
        data = typeof data !== "undefined" ? data : null;

        if (data === null) {
            data = {};
        }

        var allData = {action: action,
                       time: new Date().getTime(),
                       thread: null,
                       pid: null,
                       source: this.name};

        for (var attrname in data) {
            allData[attrname] = data[attrname];
        }

        this._dumpMessage(allData);
    };

    this._dumpMessage = function(message) {
        var str;
        if (this.interactiveDebugger) {
            str = this.structuredFormatter[message.action](message);
        } else {
            str = LOG_DELIMITER + JSON.stringify(message) + LOG_DELIMITER;
        }
        // BUGFIX: browser-chrome tests doesn't use LogController
        if (Object.keys(LogController.listeners).length !== 0) {
            LogController.log(str);
        } else {
            dump('\n' + str + '\n');
        }
    };

    /* Message validation. Only checking the action for now */
    this.validMessage = function(message) {
        return message.action !== undefined && VALID_ACTIONS.indexOf(message.action) >= 0;
    };

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

TestRunner.timeout = 5 * 60 * 1000; // 5 minutes.
TestRunner.maxTimeouts = 4; // halt testing after too many timeouts
TestRunner.runSlower = false;
TestRunner.dumpOutputDirectory = "";
TestRunner.dumpAboutMemoryAfterTest = false;
TestRunner.dumpDMDAfterTest = false;
TestRunner.slowestTestTime = 0;
TestRunner.slowestTestURL = "";

TestRunner._expectingProcessCrash = false;

/**
 * Make sure the tests don't hang indefinitely.
**/
TestRunner._numTimeouts = 0;
TestRunner._currentTestStartTime = new Date().valueOf();
TestRunner._timeoutFactor = 1;

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
TestRunner.structuredLogger = new StructuredLogger('mochitest');

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
    }

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
            window.setTimeout('TestRunner._makeIframe("'+url+'", '+(retry+1)+')', 1000);
            return;
        }

        TestRunner.structuredLogger.error("Unable to restore focus, expect failures and timeouts.");
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

    TestRunner._urls = flattenArguments(arguments);

    $('testframe').src="";
    TestRunner._checkForHangs();
    TestRunner.runNextTest();
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

/**
 * Run the next test. If no test remains, calls onComplete().
 **/
TestRunner._haltTests = false;
TestRunner.runNextTest = function() {
    if (TestRunner._currentTest < TestRunner._urls.length &&
        !TestRunner._haltTests)
    {
        var url = TestRunner._urls[TestRunner._currentTest];
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
        TestRunner._makeIframe("about:blank", 0);

        var passCount = parseInt($("pass-count").innerHTML, 10);
        var failCount = parseInt($("fail-count").innerHTML, 10);
        var todoCount = parseInt($("todo-count").innerHTML, 10);

        if (passCount === 0 &&
            failCount === 0 &&
            todoCount === 0)
        {
            // No |$('testframe').contentWindow|, so manually update: ...
            // ... the log,
            TestRunner.structuredLogger.testEnd('SimpleTest/TestRunner.js',
                                                "ERROR",
                                                "OK",
                                                "No checks actually run");
            // ... the count,
            $("fail-count").innerHTML = 1;
            // ... the indicator.
            var indicator = $("indicator");
            indicator.innerHTML = "Status: Fail (No checks actually run)";
            indicator.style.backgroundColor = "red";
        }

        SpecialPowers.unregisterProcessCrashObservers();

        TestRunner.structuredLogger.info("TEST-START | Shutdown");
        TestRunner.structuredLogger.info("Passed:  " + passCount);
        TestRunner.structuredLogger.info("Failed:  " + failCount);
        TestRunner.structuredLogger.info("Todo:    " + todoCount);
        TestRunner.structuredLogger.info("Slowest: " + TestRunner.slowestTestTime + 'ms - ' + TestRunner.slowestTestURL);

        // If we are looping, don't send this cause it closes the log file
        if (TestRunner.repeat === 0) {
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
    }
};

TestRunner.expectChildProcessCrash = function() {
    TestRunner._expectingProcessCrash = true;
};

/**
 * This stub is called by SimpleTest when a test is finished.
**/
TestRunner.testFinished = function(tests) {
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
    TestRunner._lastTestFinished = TestRunner._currentTest;
    TestRunner._loopIsRestarting = false;

    // TODO : replace this by a function that returns the mem data as an object
    // that's dumped later with the test_end message
    MemoryStats.dump(TestRunner.structuredLogger, TestRunner._currentTest,
                     TestRunner.currentTestURL,
                     TestRunner.dumpOutputDirectory,
                     TestRunner.dumpAboutMemoryAfterTest,
                     TestRunner.dumpDMDAfterTest);

    function cleanUpCrashDumpFiles() {
        if (!SpecialPowers.removeExpectedCrashDumpFiles(TestRunner._expectingProcessCrash)) {
            TestRunner.structuredLogger.testEnd(TestRunner.currentTestURL,
                                                "ERROR",
                                                "OK",
                                                "This test did not leave any crash dumps behind, but we were expecting some!");
            tests.push({ result: false });
        }
        var unexpectedCrashDumpFiles =
            SpecialPowers.findUnexpectedCrashDumpFiles();
        TestRunner._expectingProcessCrash = false;
        if (unexpectedCrashDumpFiles.length) {
            TestRunner.structuredLogger.testEnd(TestRunner.currentTestURL,
                                                "ERROR",
                                                "OK",
                                                "This test left crash dumps behind, but we " +
                                                "weren't expecting it to!",
                                                {unexpected_crashdump_files: unexpectedCrashDumpFiles});
            tests.push({ result: false });
            unexpectedCrashDumpFiles.sort().forEach(function(aFilename) {
                TestRunner.structuredLogger.info("Found unexpected crash dump file " +
                                                 aFilename + ".");
            });
        }
    }

    function runNextTest() {
        if (TestRunner.currentTestURL != TestRunner.getLoadedTestURL()) {
            TestRunner.structuredLogger.testStatus(TestRunner.currentTestURL,
                                                   TestRunner.getLoadedTestURL(),
                                                   "ERROR",
                                                   "OK",
                                                   "finished in a non-clean fashion, probably" +
                                                   " because it didn't call SimpleTest.finish()",
                                                   {loaded_test_url: TestRunner.getLoadedTestURL()});
            tests.push({ result: false });
        }

        var runtime = new Date().valueOf() - TestRunner._currentTestStartTime;

        TestRunner.structuredLogger.testEnd(TestRunner.currentTestURL,
                                            "OK",
                                            undefined,
                                            "Finished in " + runtime + "ms",
                                            {runtime: runtime}
        );

        if (TestRunner.slowestTestTime < runtime && TestRunner._timeoutFactor == 1) {
          TestRunner.slowestTestTime = runtime;
          TestRunner.slowestTestURL = TestRunner.currentTestURL;
        }

        TestRunner.updateUI(tests);

        var interstitialURL;
        if ($('testframe').contentWindow.location.protocol == "chrome:") {
            interstitialURL = "tests/SimpleTest/iframe-between-tests.html";
        } else {
            interstitialURL = "/tests/SimpleTest/iframe-between-tests.html";
        }
        TestRunner._makeIframe(interstitialURL, 0);
    }

    SpecialPowers.executeAfterFlushingMessageQueue(function() {
        cleanUpCrashDumpFiles();
        SpecialPowers.flushAllAppsLaunchable();
        SpecialPowers.flushPermissions(function () { SpecialPowers.flushPrefEnv(runNextTest); });
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

        var url = TestRunner._urls[TestRunner._currentTest];
        var max = TestRunner._expectedMaxAsserts;
        var min = TestRunner._expectedMinAsserts;
        if (numAsserts > max) {
            TestRunner.structuredLogger.testEnd(url,
                                                "ERROR",
                                                "OK",
                                                "Assertion count " + numAsserts + " is greater than expected range " +
                                                min + "-" + max + " assertions.",
                                                {assertions: numAsserts, min_asserts: min, max_asserts: max});
            TestRunner.updateUI([{ result: false }]);
        } else if (numAsserts < min) {
            TestRunner.structuredLogger.testEnd(url,
                                                "OK",
                                                "ERROR",
                                                "Assertion count " + numAsserts + " is less than expected range " +
                                                min + "-" + max + " assertions.",
                                                {assertions: numAsserts, min_asserts: min, max_asserts: max});
            TestRunner.updateUI([{ result: false }]);
        } else if (numAsserts > 0) {
            TestRunner.structuredLogger.testEnd(url,
                                                "ERROR",
                                                "ERROR",
                                                "Assertion count " + numAsserts + " within expected range " +
                                                min + "-" + max + " assertions.",
                                                {assertions: numAsserts, min_asserts: min, max_asserts: max});
        }
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
