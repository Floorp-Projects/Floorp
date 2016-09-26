"use strict";

Cu.import("resource://testing-common/BrowserTestUtils.jsm", this);
Cu.import("resource://testing-common/ContentTask.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/Timer.jsm", this);
Cu.import("resource://gre/modules/AppConstants.jsm");

const kHighlightAllPref = "findbar.highlightAll";
const kPrefModalHighlight = "findbar.modalHighlight";
const kFixtureBaseURL = "https://example.com/browser/toolkit/modules/tests/browser/";
const kIteratorTimeout = Services.prefs.getIntPref("findbar.iteratorTimeout");

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
    let event = document.createEvent("KeyboardEvent");
    event.initKeyEvent("keypress", true, true, null, false, false,
                       false, false, 0, str.charCodeAt(i));
    findbar._findField.inputField.dispatchEvent(event);
  }
  return promise;
}

function promiseTestHighlighterOutput(browser, word, expectedResult, extraTest = () => {}) {
  return ContentTask.spawn(browser, { word, expectedResult, extraTest: extraTest.toSource() },
    function* ({ word, expectedResult, extraTest }) {
    Cu.import("resource://gre/modules/Timer.jsm", this);

    return new Promise((resolve, reject) => {
      let stubbed = {};
      let callCounts = {
        insertCalls: [],
        removeCalls: []
      };
      let lastMaskNode, lastOutlineNode;
      let rects = [];

      // Amount of milliseconds to wait after the last time one of our stubs
      // was called.
      const kTimeoutMs = 1000;
      // The initial timeout may wait for a while for results to come in.
      let timeout = setTimeout(() => finish(false, "Timeout"), kTimeoutMs * 5);

      function finish(ok = true, message = "finished with error") {
        // Restore the functions we stubbed out.
        try {
          content.document.insertAnonymousContent = stubbed.insert;
          content.document.removeAnonymousContent = stubbed.remove;
        } catch (ex) {}
        stubbed = {};
        clearTimeout(timeout);

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
        if (!lastMaskNode && expectedResult.rectCount !== 0) {
          Assert.ok(false, `No mask node found, but expected ${expectedResult.rectCount} rects.`);
        }

        Assert.equal(rects.length, expectedResult.rectCount,
          `Amount of inserted rects should match for '${word}'.`);

        // Allow more specific assertions to be tested in `extraTest`.
        extraTest = eval(extraTest);
        extraTest(lastMaskNode, lastOutlineNode, rects);

        resolve();
      }

      function stubAnonymousContentNode(domNode, anonNode) {
        let originals = [anonNode.setTextContentForElement,
          anonNode.setAttributeForElement, anonNode.removeAttributeForElement,
          anonNode.setCutoutRectsForElement];
        anonNode.setTextContentForElement = (id, text) => {
          try {
            (domNode.querySelector("#" + id) || domNode).textContent = text;
          } catch (ex) {}
          return originals[0].call(anonNode, id, text);
        };
        anonNode.setAttributeForElement = (id, attrName, attrValue) => {
          try {
            (domNode.querySelector("#" + id) || domNode).setAttribute(attrName, attrValue);
          } catch (ex) {}
          return originals[1].call(anonNode, id, attrName, attrValue);
        };
        anonNode.removeAttributeForElement = (id, attrName) => {
          try {
            let node = domNode.querySelector("#" + id) || domNode;
            if (node.hasAttribute(attrName))
              node.removeAttribute(attrName);
          } catch (ex) {}
          return originals[2].call(anonNode, id, attrName);
        };
        anonNode.setCutoutRectsForElement = (id, cutoutRects) => {
          rects = cutoutRects;
          return originals[3].call(anonNode, id, cutoutRects);
        };
      }

      // Create a function that will stub the original version and collects
      // the arguments so we can check the results later.
      function stub(which) {
        stubbed[which] = content.document[which + "AnonymousContent"];
        let prop = which + "Calls";
        return function(node) {
          callCounts[prop].push(node);
          if (which == "insert") {
            if (node.outerHTML.indexOf("outlineMask") > -1)
              lastMaskNode = node;
            else
              lastOutlineNode = node;
          }
          clearTimeout(timeout);
          timeout = setTimeout(() => {
            finish();
          }, kTimeoutMs);
          let res = stubbed[which].call(content.document, node);
          if (which == "insert")
            stubAnonymousContentNode(node, res);
          return res;
        };
      }
      content.document.insertAnonymousContent = stub("insert");
      content.document.removeAnonymousContent = stub("remove");
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
      rectCount: 1,
      insertCalls: [2, 4],
      removeCalls: [0, 1]
    }],
    ["their law might propagate their kind", {
      rectCount: 0,
      insertCalls: [28, 31],
      removeCalls: [28, 30],
      extraTest: function(maskNode, outlineNode, rects) {
        Assert.equal(outlineNode.getElementsByTagName("div").length, 2,
          "There should be multiple rects drawn");
      }
    }],
    ["ro", {
      rectCount: 40,
      insertCalls: [1, 4],
      removeCalls: [1, 3]
    }],
    ["new", {
      rectCount: 1,
      insertCalls: [1, 4],
      removeCalls: [0, 2]
    }],
    ["o", {
      rectCount: 491,
      insertCalls: [1, 4],
      removeCalls: [0, 2]
    }]
  ]);
  let url = kFixtureBaseURL + "file_FinderSample.html";
  yield BrowserTestUtils.withNewTab(url, function* (browser) {
    let findbar = gBrowser.getFindBar();

    for (let [word, expectedResult] of tests) {
      yield promiseOpenFindbar(findbar);
      Assert.ok(!findbar.hidden, "Findbar should be open now.");

      yield new Promise(resolve => setTimeout(resolve, kIteratorTimeout));
      let promise = promiseTestHighlighterOutput(browser, word, expectedResult,
        expectedResult.extraTest);
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
      rectCount: 1,
      insertCalls: [2, 4],
      removeCalls: [0, 1]
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
      rectCount: 1,
      insertCalls: [1, 3],
      removeCalls: [0, 1]
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
      rectCount: 1,
      insertCalls: [2, 4],
      removeCalls: [0, 1]
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
      rectCount: 1,
      insertCalls: [2, 4],
      removeCalls: [0, 1]
    };
    let promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    yield promiseEnterStringIntoFindField(findbar, word);
    yield promise;

    // We now know we have multiple rectangles highlighted, so it's a good time
    // to flip the pref.
    expectedResult = {
      rectCount: 0,
      insertCalls: [0, 1],
      removeCalls: [0, 1]
    };
    promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    yield SpecialPowers.pushPrefEnv({ "set": [[ kHighlightAllPref, false ]] });
    yield promise;

    // For posterity, let's switch back.
    expectedResult = {
      rectCount: 2,
      insertCalls: [1, 3],
      removeCalls: [0, 1]
    };
    promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    yield SpecialPowers.pushPrefEnv({ "set": [[ kHighlightAllPref, true ]] });
    yield promise;
  });
});

add_task(function* testXMLDocument() {
  let url = "data:text/xml;charset=utf-8," + encodeURIComponent(`<?xml version="1.0"?>
<result>
  <Title>Example</Title>
  <Error>Error</Error>
</result>`);
  yield BrowserTestUtils.withNewTab(url, function* (browser) {
    let findbar = gBrowser.getFindBar();

    yield promiseOpenFindbar(findbar);

    let word = "Example";
    let expectedResult = {
      rectCount: 0,
      insertCalls: [1, 4],
      removeCalls: [0, 1]
    };
    let promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    yield promiseEnterStringIntoFindField(findbar, word);
    yield promise;

    findbar.close(true);
  });
});

add_task(function* testHideOnLocationChange() {
  let url = kFixtureBaseURL + "file_FinderSample.html";
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  let browser = tab.linkedBrowser;
  let findbar = gBrowser.getFindBar();

  yield promiseOpenFindbar(findbar);

  let word = "Roland";
  let expectedResult = {
    rectCount: 1,
    insertCalls: [2, 4],
    removeCalls: [0, 1]
  };
  let promise = promiseTestHighlighterOutput(browser, word, expectedResult);
  yield promiseEnterStringIntoFindField(findbar, word);
  yield promise;

  // Now we try to navigate away! (Using the same page)
  promise = promiseTestHighlighterOutput(browser, word, {
    rectCount: 0,
    insertCalls: [0, 0],
    removeCalls: [1, 2]
  });
  yield BrowserTestUtils.loadURI(browser, url);
  yield promise;

  yield BrowserTestUtils.removeTab(tab);
});

add_task(function* testHideOnClear() {
  let url = kFixtureBaseURL + "file_FinderSample.html";
  yield BrowserTestUtils.withNewTab(url, function* (browser) {
    let findbar = gBrowser.getFindBar();
    yield promiseOpenFindbar(findbar);

    let word = "Roland";
    let expectedResult = {
      rectCount: 1,
      insertCalls: [2, 4],
      removeCalls: [1, 2]
    };
    let promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    yield promiseEnterStringIntoFindField(findbar, word);
    yield promise;

    yield new Promise(resolve => setTimeout(resolve, kIteratorTimeout));
    promise = promiseTestHighlighterOutput(browser, "", {
      rectCount: 0,
      insertCalls: [0, 0],
      removeCalls: [1, 2]
    });
    findbar.clear();
    yield promise;

    findbar.close(true);
  });
});
