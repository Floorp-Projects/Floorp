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
TestRunner.currentTestURL = "";
TestRunner._urls = [];

TestRunner.timeout = 300; // seconds
TestRunner.maxTimeouts = 4; // halt testing after too many timeouts

/**
 * Make sure the tests don't hang indefinitely.
**/
TestRunner._numTimeouts = 0;
TestRunner._currentTestStartTime = new Date().valueOf();

TestRunner._checkForHangs = function() {
  if (TestRunner._currentTest < TestRunner._urls.length) {
    var runtime = (new Date().valueOf() - TestRunner._currentTestStartTime) / 1000;
    if (runtime >= TestRunner.timeout) {
      var frameWindow = $('testframe').contentWindow.wrappedJSObject ||
                       	$('testframe').contentWindow;
      frameWindow.SimpleTest.ok(false, "Test timed out.");

      // If we have too many timeouts, give up. We don't want to wait hours
      // for results if some bug causes lots of tests to time out.
      if (++TestRunner._numTimeouts >= TestRunner.maxTimeouts) {
        TestRunner._haltTests = true;
        frameWindow.SimpleTest.ok(false, "Too many test timeouts, giving up.");
      }

      frameWindow.SimpleTest.finish();
    }
    TestRunner.deferred = callLater(30, TestRunner._checkForHangs);
  }
}

/**
 * This function is called after generating the summary.
**/
TestRunner.onComplete = null;

/**
 * If logEnabled is true, this is the logger that will be used.
**/
TestRunner.logger = MochiKit.Logging.logger;

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
    if (url != "about:blank" && (!document.hasFocus() ||
        document.activeElement != iframe)) {
        // typically calling ourselves from setTimeout is sufficient
        // but we'll try focus() just in case that's needed
        window.focus();
        iframe.focus();
        if (retry < 3) {
            window.setTimeout('TestRunner._makeIframe("'+url+'", '+(retry+1)+')', 1000);
            return;
        }

        if (TestRunner.logEnabled) {
            var frameWindow = $('testframe').contentWindow.wrappedJSObject ||
                              $('testframe').contentWindow;
            TestRunner.logger.log("Error: Unable to restore focus, expect failures and timeouts.");
        }
    }
    window.scrollTo(0, $('indicator').offsetTop);
    iframe.src = url;
    iframe.name = url;
    iframe.width = "500";
    return iframe;
};

/**
 * TestRunner entry point.
 *
 * The arguments are the URLs of the test to be ran.
 *
**/
TestRunner.runTests = function (/*url...*/) {
    if (TestRunner.logEnabled)
        TestRunner.logger.log("SimpleTest START");

    TestRunner._urls = flattenArguments(arguments);
    $('testframe').src="";
    TestRunner._checkForHangs();
    window.focus();
    $('testframe').focus();
    TestRunner.runNextTest();
};

/**
 * Run the next test. If no test remains, calls makeSummary
**/
TestRunner._haltTests = false;
TestRunner.runNextTest = function() {
    if (TestRunner._currentTest < TestRunner._urls.length &&
        !TestRunner._haltTests) {
        var url = TestRunner._urls[TestRunner._currentTest];
        TestRunner.currentTestURL = url;

        $("current-test-path").innerHTML = url;

        TestRunner._currentTestStartTime = new Date().valueOf();

        if (TestRunner.logEnabled)
            TestRunner.logger.log("Running " + url + "...");

        TestRunner._makeIframe(url, 0);
    }  else {
        $("current-test").innerHTML = "<b>Finished</b>";
        TestRunner._makeIframe("about:blank", 0);
        if (TestRunner.logEnabled) {
            TestRunner.logger.log("Passed: " + $("pass-count").innerHTML);
            TestRunner.logger.log("Failed: " + $("fail-count").innerHTML);
            TestRunner.logger.log("Todo:   " + $("todo-count").innerHTML);
            TestRunner.logger.log("SimpleTest FINISHED");
        }
        if (TestRunner.onComplete)
            TestRunner.onComplete();
    }
};

/**
 * This stub is called by SimpleTest when a test is finished.
**/
TestRunner.testFinished = function(doc) {
    var finishedURL = TestRunner._urls[TestRunner._currentTest];

    if (TestRunner.logEnabled)
        TestRunner.logger.debug("SimpleTest finished " + finishedURL);

    TestRunner.updateUI();
    TestRunner._currentTest++;
    TestRunner.runNextTest();
};

/**
 * Get the results.
 */
TestRunner.countResults = function(win) {
  var nOK = 0;
  var nNotOK = 0;
  var nTodo = 0;
  var tests = win.SimpleTest._tests;
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

TestRunner.updateUI = function() {
  var results = TestRunner.countResults($('testframe').contentWindow);
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
    indicator.style.backgroundColor = "green";
  }

  // Set the table values
  var trID = "tr-" + $('current-test-path').innerHTML;
  var row = $(trID);
  replaceChildNodes(row,
    TD({'style':
        {'backgroundColor': results.notOK > 0 ? "#f00":"#0d0"}}, results.OK),
    TD({'style':
        {'backgroundColor': results.notOK > 0 ? "#f00":"#0d0"}}, results.notOK),
    TD({'style': {'backgroundColor':
                   results.todo > 0 ? "orange":"#0d0"}}, results.todo)
  );
}
