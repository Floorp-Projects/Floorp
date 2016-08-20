"use strict";

Cu.import("resource://testing-common/BrowserTestUtils.jsm", this);
Cu.import("resource://testing-common/ContentTask.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/AppConstants.jsm");

const kHighlightAllPref = "findbar.highlightAll";
const kPrefModalHighlight = "findbar.modalHighlight";
const kFixtureBaseURL = "https://example.com/browser/toolkit/modules/tests/browser/";

function promiseOpenFindbar(findbar) {
  findbar.onFindCommand()
  return gFindBar._startFindDeferred && gFindBar._startFindDeferred.promise;
}

function promiseFindResult(findbar, str = null) {
  let highlightFinished = false;
  let findFinished = false;
  return new Promise(resolve => {
    let listener = {
      onFindResult({ searchString }) {
        if (str !== null && str != searchString) {
          return;
        }
        findFinished = true;
        if (highlightFinished) {
          findbar.browser.finder.removeResultListener(listener);
          resolve();
        }
      },
      onHighlightFinished() {
        highlightFinished = true;
        if (findFinished) {
          findbar.browser.finder.removeResultListener(listener);
          resolve();
        }
      },
      onMatchesCountResult: () => {}
    };
    findbar.browser.finder.addResultListener(listener);
  });
}

function promiseEnterStringIntoFindField(findbar, str) {
  let promise = promiseFindResult(findbar, str);
  for (let i = 0; i < str.length; i++) {
    let event = document.createEvent("KeyEvents");
    event.initKeyEvent("keypress", true, true, null, false, false,
                       false, false, 0, str.charCodeAt(i));
    findbar._findField.inputField.dispatchEvent(event);
  }
  return promise;
}

function promiseTestHighlighterOutput(browser, word, expectedResult, extraTest = () => {}) {
  return ContentTask.spawn(browser, { word, expectedResult, extraTest: extraTest.toSource() },
    function* ({ word, expectedResult, extraTest }) {
    let document = content.document;

    return new Promise((resolve, reject) => {
      let stubbed = {};
      let callCounts = {
        insertCalls: [],
        removeCalls: []
      };

      // Amount of milliseconds to wait after the last time one of our stubs
      // was called.
      const kTimeoutMs = 1000;
      // The initial timeout may wait for a while for results to come in.
      let timeout = content.setTimeout(() => finish(false, "Timeout"), kTimeoutMs * 5);

      function finish(ok = true, message = "finished with error") {
        // Restore the functions we stubbed out.
        document.insertAnonymousContent = stubbed.insert;
        document.removeAnonymousContent = stubbed.remove;
        stubbed = {};
        content.clearTimeout(timeout);

        if (expectedResult.rectCount !== 0)
          Assert.ok(ok, message);

        Assert.greaterOrEqual(callCounts.insertCalls.length, expectedResult.insertCalls[0],
          `Min. insert calls should match for '${word}'.`);
        Assert.lessOrEqual(callCounts.insertCalls.length, expectedResult.insertCalls[1],
          `Max. insert calls should match for '${word}'.`);
        Assert.greaterOrEqual(callCounts.removeCalls.length, expectedResult.removeCalls[0],
          `Min. remove calls should match for '${word}'.`);
        Assert.lessOrEqual(callCounts.removeCalls.length, expectedResult.removeCalls[1],
          `Max. remove calls should match for '${word}'.`);

        // We reached the amount of calls we expected, so now we can check
        // the amount of rects.
        let lastMaskNode = callCounts.insertCalls.pop();
        if (!lastMaskNode && expectedResult.rectCount !== 0) {
          Assert.ok(false, `No mask node found, but expected ${expectedResult.rectCount} rects.`);
        }

        if (lastMaskNode) {
          Assert.equal(lastMaskNode.getElementsByTagName("div").length,
            expectedResult.rectCount, `Amount of inserted rects should match for '${word}'.`);
        }

        // Allow more specific assertions to be tested in `extraTest`.
        extraTest = eval(extraTest);
        extraTest(lastMaskNode);

        resolve();
      }

      // Create a function that will stub the original version and collects
      // the arguments so we can check the results later.
      function stub(which) {
        stubbed[which] = document[which + "AnonymousContent"];
        let prop = which + "Calls";
        return function(node) {
          callCounts[prop].push(node);
          content.clearTimeout(timeout);
          timeout = content.setTimeout(finish, kTimeoutMs);
          return stubbed[which].call(document, node);
        };
      }
      document.insertAnonymousContent = stub("insert");
      document.removeAnonymousContent = stub("remove");
    });
  });
}

add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({ set: [
    [kHighlightAllPref, true],
    [kPrefModalHighlight, true]
  ]});
});

// Test the results of modal highlighting, which is on by default.
add_task(function* testModalResults() {
  let tests = new Map([
    ["Roland", {
      rectCount: 2,
      insertCalls: [2, 4],
      removeCalls: [1, 2]
    }],
    ["ro", {
      rectCount: 41,
      insertCalls: [1, 4],
      removeCalls: [1, 3]
    }],
    ["new", {
      rectCount: 2,
      insertCalls: [1, 4],
      removeCalls: [1, 3]
    }],
    ["o", {
      rectCount: 492,
      insertCalls: [1, 3],
      removeCalls: [1, 2]
    }]
  ]);
  let url = kFixtureBaseURL + "file_FinderSample.html";
  yield BrowserTestUtils.withNewTab(url, function* (browser) {
    let findbar = gBrowser.getFindBar();

    for (let [word, expectedResult] of tests) {
      yield promiseOpenFindbar(findbar);
      Assert.ok(!findbar.hidden, "Findbar should be open now.");

      let promise = promiseTestHighlighterOutput(browser, word, expectedResult);
      yield promiseEnterStringIntoFindField(findbar, word);
      yield promise;

      findbar.close(true);
    }
  });
});

// Test if runtime switching of highlight modes between modal and non-modal works
// as expected.
add_task(function* testModalSwitching() {
  let url = kFixtureBaseURL + "file_FinderSample.html";
  yield BrowserTestUtils.withNewTab(url, function* (browser) {
    let findbar = gBrowser.getFindBar();

    yield promiseOpenFindbar(findbar);
    Assert.ok(!findbar.hidden, "Findbar should be open now.");

    let word = "Roland";
    let expectedResult = {
      rectCount: 2,
      insertCalls: [2, 4],
      removeCalls: [1, 2]
    };
    let promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    yield promiseEnterStringIntoFindField(findbar, word);
    yield promise;

    yield SpecialPowers.pushPrefEnv({ "set": [[ kPrefModalHighlight, false ]] });

    expectedResult = {
      rectCount: 0,
      insertCalls: [0, 0],
      removeCalls: [0, 0]
    };
    promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    findbar.clear();
    yield promiseEnterStringIntoFindField(findbar, word);
    yield promise;

    findbar.close(true);
  });

  yield SpecialPowers.pushPrefEnv({ "set": [[ kPrefModalHighlight, true ]] });
});

// Test if highlighting a dark page is detected properly.
add_task(function* testDarkPageDetection() {
  let url = kFixtureBaseURL + "file_FinderSample.html";
  yield BrowserTestUtils.withNewTab(url, function* (browser) {
    let findbar = gBrowser.getFindBar();

    yield promiseOpenFindbar(findbar);

    let word = "Roland";
    let expectedResult = {
      rectCount: 2,
      insertCalls: [2, 4],
      removeCalls: [1, 2]
    };
    let promise = promiseTestHighlighterOutput(browser, word, expectedResult, function(node) {
      Assert.ok(!node.hasAttribute("brighttext"), "White HTML page shouldn't have 'brighttext' set");
    });
    yield promiseEnterStringIntoFindField(findbar, word);
    yield promise;

    findbar.close(true);
  });

  yield BrowserTestUtils.withNewTab(url, function* (browser) {
    let findbar = gBrowser.getFindBar();

    yield promiseOpenFindbar(findbar);

    let word = "Roland";
    let expectedResult = {
      rectCount: 2,
      insertCalls: [2, 4],
      removeCalls: [1, 2]
    };

    yield ContentTask.spawn(browser, null, function* () {
      let dwu = content.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils);
      let uri = "data:text/css;charset=utf-8," + encodeURIComponent(`
        body {
          background: maroon radial-gradient(circle, #a01010 0%, #800000 80%) center center / cover no-repeat;
          color: white;
        }`);
      try {
        dwu.loadSheetUsingURIString(uri, dwu.USER_SHEET);
      } catch (e) {}
    });

    let promise = promiseTestHighlighterOutput(browser, word, expectedResult, node => {
      Assert.ok(node.hasAttribute("brighttext"), "Dark HTML page should have 'brighttext' set");
    });
    yield promiseEnterStringIntoFindField(findbar, word);
    yield promise;

    findbar.close(true);
  });
});

add_task(function* testHighlightAllToggle() {
  let url = kFixtureBaseURL + "file_FinderSample.html";
  yield BrowserTestUtils.withNewTab(url, function* (browser) {
    let findbar = gBrowser.getFindBar();

    yield promiseOpenFindbar(findbar);

    let word = "Roland";
    let expectedResult = {
      rectCount: 2,
      insertCalls: [2, 4],
      removeCalls: [1, 2]
    };
    let promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    yield promiseEnterStringIntoFindField(findbar, word);
    yield promise;

    // We now know we have multiple rectangles highlighted, so it's a good time
    // to flip the pref.
    expectedResult = {
      rectCount: 0,
      insertCalls: [0, 1],
      removeCalls: [1, 1]
    };
    promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    yield SpecialPowers.pushPrefEnv({ "set": [[ kHighlightAllPref, false ]] });
    yield promise;

    // For posterity, let's switch back.
    expectedResult = {
      rectCount: 2,
      insertCalls: [2, 4],
      removeCalls: [1, 2]
    };
    promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    yield SpecialPowers.pushPrefEnv({ "set": [[ kHighlightAllPref, true ]] });
    yield promise;
  });
});
