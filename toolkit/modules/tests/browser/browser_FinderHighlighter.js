"use strict";

Cu.import("resource://testing-common/BrowserTestUtils.jsm", this);
Cu.import("resource://testing-common/ContentTask.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/AppConstants.jsm");

const kPrefModalHighlight = "findbar.modalHighlight";

function promiseOpenFindbar(findbar) {
  findbar.onFindCommand()
  return gFindBar._startFindDeferred && gFindBar._startFindDeferred.promise;
}

function promiseFindResult(findbar, str = null) {
  return new Promise(resolve => {
    let listener = {
      onFindResult: function({ searchString }) {
        if (str !== null && str != searchString) {
          return;
        }
        findbar.browser.finder.removeResultListener(listener);
        resolve();
      }
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

function promiseTestHighlighterOutput(browser, word, expectedResult) {
  return ContentTask.spawn(browser, { word, expectedResult }, function* ({ word, expectedResult }) {
    let document = content.document;

    return new Promise((resolve, reject) => {
      let stubbed = [document.insertAnonymousContent,
        document.removeAnonymousContent];
      let callCounts = {
        insertCalls: [],
        removeCalls: []
      };

      // Amount of milliseconds to wait after the last time one of our stubs
      // was called.
      const kTimeoutMs = 500;
      // The initial timeout may wait for a while for results to come in.
      let timeout = content.setTimeout(finish, kTimeoutMs * 4);

      function finish(ok = true, message) {
        // Restore the functions we stubbed out.
        document.insertAnonymousContent = stubbed[0];
        document.removeAnonymousContent = stubbed[1];
        content.clearTimeout(timeout);

        Assert.equal(callCounts.insertCalls.length, expectedResult.insertCalls,
          `Insert calls should match for '${word}'.`);
        Assert.equal(callCounts.removeCalls.length, expectedResult.removeCalls,
          `Remove calls should match for '${word}'.`);

        // We reached the amount of calls we expected, so now we can check
        // the amount of rects.
        let lastSVGNode = callCounts.insertCalls.pop();
        if (!lastSVGNode && expectedResult.rectCount !== 0) {
          Assert.ok(false, `No SVG node found, but expected ${expectedResult.rectCount} rects.`);
        }
        if (lastSVGNode) {
          Assert.equal(lastSVGNode.getElementsByTagName("mask")[0]
            .getElementsByTagName("rect").length, expectedResult.rectCount,
            `Amount of inserted rects should match for '${word}'.`);
        }

        resolve();
      }

      // Create a function that will stub the original version and collects
      // the arguments so we can check the results later.
      function stub(which) {
        let prop = which + "Calls";
        return function(node) {
          callCounts[prop].push(node);
          content.clearTimeout(timeout);
          timeout = content.setTimeout(finish, kTimeoutMs);
          return node;
        };
      }
      document.insertAnonymousContent = stub("insert");
      document.removeAnonymousContent = stub("remove");
    });
  });
}

add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({ set: [
    ["findbar.highlightAll", true],
    ["findbar.modalHighlight", true]
  ]});
});

// Test the results of modal highlighting, which is on by default.
add_task(function* testModalResults() {
  let tests = new Map([
    ["mo", {
      rectCount: 5,
      insertCalls: 2,
      removeCalls: AppConstants.platform == "linux" ? 1 : 2
    }],
    ["m", {
      rectCount: 9,
      insertCalls: 1,
      removeCalls: 1
    }],
    ["new", {
      rectCount: 2,
      insertCalls: 1,
      removeCalls: 1
    }],
    ["o", {
      rectCount: 1218,
      insertCalls: 1,
      removeCalls: 1
    }]
  ]);
  yield BrowserTestUtils.withNewTab("about:mozilla", function* (browser) {
    // We're inserting 1200 additional o's at the end of the document.
    yield ContentTask.spawn(browser, null, function* () {
      let document = content.document;
      document.getElementsByTagName("section")[0].innerHTML += "<p>" +
        (new Array(1200).join(" o ")) + "</p>";
    });

    let findbar = gBrowser.getFindBar();

    for (let [word, expectedResult] of tests) {
      yield promiseOpenFindbar(findbar);
      Assert.ok(!findbar.hidden, "Findbar should be open now.");

      let promise = promiseTestHighlighterOutput(browser, word, expectedResult);
      yield promiseEnterStringIntoFindField(findbar, word);
      yield promise;

      findbar.close();
    }
  });
});

// Test if runtime switching of highlight modes between modal and non-modal works
// as expected.
add_task(function* testModalSwitching() {
  yield BrowserTestUtils.withNewTab("about:mozilla", function* (browser) {
    let findbar = gBrowser.getFindBar();

    yield promiseOpenFindbar(findbar);
    Assert.ok(!findbar.hidden, "Findbar should be open now.");

    let word = "mo";
    let expectedResult = {
      rectCount: 5,
      insertCalls: 2,
      removeCalls: AppConstants.platform == "linux" ? 1 : 2
    };
    let promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    yield promiseEnterStringIntoFindField(findbar, word);
    yield promise;

    yield SpecialPowers.pushPrefEnv({ "set": [[ kPrefModalHighlight, false ]] });

    expectedResult = {
      rectCount: 0,
      insertCalls: 0,
      removeCalls: 0
    };
    promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    findbar.clear();
    yield promiseEnterStringIntoFindField(findbar, word);
    yield promise;
  });
});
