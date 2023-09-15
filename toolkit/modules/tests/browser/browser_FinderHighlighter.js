/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

const kIteratorTimeout = Services.prefs.getIntPref("findbar.iteratorTimeout");
const kPrefHighlightAll = "findbar.highlightAll";
const kPrefModalHighlight = "findbar.modalHighlight";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      [kPrefHighlightAll, true],
      [kPrefModalHighlight, true],
    ],
  });
});

// Test the results of modal highlighting, which is on by default.
add_task(async function testModalResults() {
  let tests = new Map([
    [
      "Roland",
      {
        rectCount: 2,
        insertCalls: [2, 4],
        removeCalls: [0, 1],
        animationCalls: [1, 2],
      },
    ],
    [
      "their law might propagate their kind",
      {
        rectCount: 2,
        insertCalls: [5, 6],
        removeCalls: [4, 5],
        // eslint-disable-next-line object-shorthand
        extraTest: function (maskNode, outlineNode, rects) {
          Assert.equal(
            outlineNode.getElementsByTagName("div").length,
            2,
            "There should be multiple rects drawn"
          );
        },
      },
    ],
    [
      "ro",
      {
        rectCount: 41,
        insertCalls: [1, 4],
        removeCalls: [0, 2],
      },
    ],
    [
      "new",
      {
        rectCount: 2,
        insertCalls: [1, 4],
        removeCalls: [0, 2],
      },
    ],
    [
      "o",
      {
        rectCount: 492,
        insertCalls: [1, 4],
        removeCalls: [0, 2],
      },
    ],
  ]);
  let url = kFixtureBaseURL + "file_FinderSample.html";
  await BrowserTestUtils.withNewTab(url, async function (browser) {
    let findbar = await gBrowser.getFindBar();

    for (let [word, expectedResult] of tests) {
      await promiseOpenFindbar(findbar);
      Assert.ok(!findbar.hidden, "Findbar should be open now.");

      let timeout = kIteratorTimeout;
      if (word.length == 1) {
        timeout *= 4;
      } else if (word.length == 2) {
        timeout *= 2;
      }
      await new Promise(resolve => setTimeout(resolve, timeout));
      let promise = promiseTestHighlighterOutput(
        browser,
        word,
        expectedResult,
        expectedResult.extraTest
      );
      await promiseEnterStringIntoFindField(findbar, word);
      await promise;

      findbar.close(true);
    }
  });
});

// Test if runtime switching of highlight modes between modal and non-modal works
// as expected.
add_task(async function testModalSwitching() {
  let url = kFixtureBaseURL + "file_FinderSample.html";
  await BrowserTestUtils.withNewTab(url, async function (browser) {
    let findbar = await gBrowser.getFindBar();

    await promiseOpenFindbar(findbar);
    Assert.ok(!findbar.hidden, "Findbar should be open now.");

    let word = "Roland";
    let expectedResult = {
      rectCount: 2,
      insertCalls: [2, 4],
      removeCalls: [0, 1],
    };
    let promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    await promiseEnterStringIntoFindField(findbar, word);
    await promise;

    await SpecialPowers.pushPrefEnv({ set: [[kPrefModalHighlight, false]] });

    expectedResult = {
      rectCount: 0,
      insertCalls: [0, 0],
      removeCalls: [0, 0],
    };
    promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    findbar.clear();
    await promiseEnterStringIntoFindField(findbar, word);
    await promise;

    findbar.close(true);
  });

  await SpecialPowers.pushPrefEnv({ set: [[kPrefModalHighlight, true]] });
});

// Test if highlighting a dark page is detected properly.
add_task(async function testDarkPageDetection() {
  let url = kFixtureBaseURL + "file_FinderSample.html";
  await BrowserTestUtils.withNewTab(url, async function (browser) {
    let findbar = await gBrowser.getFindBar();

    await promiseOpenFindbar(findbar);

    let word = "Roland";
    let expectedResult = {
      rectCount: 2,
      insertCalls: [1, 3],
      removeCalls: [0, 1],
    };
    let promise = promiseTestHighlighterOutput(
      browser,
      word,
      expectedResult,
      function (node) {
        Assert.ok(
          node.style.background.startsWith("rgba(0, 0, 0"),
          "White HTML page should have a black background color set for the mask"
        );
      }
    );
    await promiseEnterStringIntoFindField(findbar, word);
    await promise;

    findbar.close(true);
  });

  await BrowserTestUtils.withNewTab(url, async function (browser) {
    let findbar = await gBrowser.getFindBar();

    await promiseOpenFindbar(findbar);

    let word = "Roland";
    let expectedResult = {
      rectCount: 2,
      insertCalls: [2, 4],
      removeCalls: [0, 1],
    };

    await SpecialPowers.spawn(browser, [], async function () {
      let dwu = content.windowUtils;
      let uri =
        "data:text/css;charset=utf-8," +
        encodeURIComponent(`
        body {
          background: maroon radial-gradient(circle, #a01010 0%, #800000 80%) center center / cover no-repeat;
          color: white;
        }`);
      try {
        dwu.loadSheetUsingURIString(uri, dwu.USER_SHEET);
      } catch (e) {}
    });

    let promise = promiseTestHighlighterOutput(
      browser,
      word,
      expectedResult,
      node => {
        Assert.ok(
          node.style.background.startsWith("rgba(255, 255, 255"),
          "Dark HTML page should have a white background color set for the mask"
        );
      }
    );
    await promiseEnterStringIntoFindField(findbar, word);
    await promise;

    findbar.close(true);
  });
});

add_task(async function testHighlightAllToggle() {
  let url = kFixtureBaseURL + "file_FinderSample.html";
  await BrowserTestUtils.withNewTab(url, async function (browser) {
    let findbar = await gBrowser.getFindBar();

    await promiseOpenFindbar(findbar);

    let word = "Roland";
    let expectedResult = {
      rectCount: 2,
      insertCalls: [2, 4],
      removeCalls: [0, 1],
    };
    let promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    await promiseEnterStringIntoFindField(findbar, word);
    await promise;

    // We now know we have multiple rectangles highlighted, so it's a good time
    // to flip the pref.
    expectedResult = {
      rectCount: 0,
      insertCalls: [0, 1],
      removeCalls: [1, 2],
    };
    promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    await SpecialPowers.pushPrefEnv({ set: [[kPrefHighlightAll, false]] });
    await promise;

    // For posterity, let's switch back.
    expectedResult = {
      rectCount: 2,
      insertCalls: [1, 3],
      removeCalls: [0, 1],
    };
    promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    await SpecialPowers.pushPrefEnv({ set: [[kPrefHighlightAll, true]] });
    await promise;
  });
});

add_task(async function testXMLDocument() {
  let url =
    "data:text/xml;charset=utf-8," +
    encodeURIComponent(`<?xml version="1.0"?>
<result>
  <Title>Example</Title>
  <Error>Error</Error>
</result>`);
  await BrowserTestUtils.withNewTab(url, async function (browser) {
    let findbar = await gBrowser.getFindBar();

    await promiseOpenFindbar(findbar);

    let word = "Example";
    let expectedResult = {
      rectCount: 0,
      insertCalls: [1, 4],
      removeCalls: [0, 1],
    };
    let promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    await promiseEnterStringIntoFindField(findbar, word);
    await promise;

    findbar.close(true);
  });
});

add_task(async function testHideOnLocationChange() {
  let url = kFixtureBaseURL + "file_FinderSample.html";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  let browser = tab.linkedBrowser;
  let findbar = await gBrowser.getFindBar();

  await promiseOpenFindbar(findbar);

  let word = "Roland";
  let expectedResult = {
    rectCount: 2,
    insertCalls: [2, 4],
    removeCalls: [0, 1],
  };
  let promise = promiseTestHighlighterOutput(browser, word, expectedResult);
  await promiseEnterStringIntoFindField(findbar, word);
  await promise;

  // Now we try to navigate away! (Using the same page)
  promise = promiseTestHighlighterOutput(browser, word, {
    rectCount: 0,
    insertCalls: [0, 0],
    removeCalls: [1, 2],
  });
  BrowserTestUtils.startLoadingURIString(browser, url);
  await promise;

  BrowserTestUtils.removeTab(tab);
});

add_task(async function testHideOnClear() {
  let url = kFixtureBaseURL + "file_FinderSample.html";
  await BrowserTestUtils.withNewTab(url, async function (browser) {
    let findbar = await gBrowser.getFindBar();
    await promiseOpenFindbar(findbar);

    let word = "Roland";
    let expectedResult = {
      rectCount: 2,
      insertCalls: [2, 4],
      removeCalls: [0, 2],
    };
    let promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    await promiseEnterStringIntoFindField(findbar, word);
    await promise;

    await new Promise(resolve => setTimeout(resolve, kIteratorTimeout));
    promise = promiseTestHighlighterOutput(browser, "", {
      rectCount: 0,
      insertCalls: [0, 0],
      removeCalls: [1, 2],
    });
    findbar.clear();
    await promise;

    findbar.close(true);
  });
});

add_task(async function testRectsAndTexts() {
  let url =
    "data:text/html;charset=utf-8," +
    encodeURIComponent(
      '<div style="width: 150px; border: 1px solid black">' +
        "Here are a lot of words Please use find to highlight some words that wrap" +
        " across a line boundary and see what happens.</div>"
    );
  await BrowserTestUtils.withNewTab(url, async function (browser) {
    let findbar = await gBrowser.getFindBar();
    await promiseOpenFindbar(findbar);

    let word = "words please use find to";
    let expectedResult = {
      rectCount: 2,
      insertCalls: [2, 4],
      removeCalls: [0, 2],
    };
    let promise = promiseTestHighlighterOutput(
      browser,
      word,
      expectedResult,
      (maskNode, outlineNode) => {
        let boxes = outlineNode.getElementsByTagName("span");
        Assert.equal(
          boxes.length,
          2,
          "There should be two outline boxes containing text"
        );
        Assert.equal(
          boxes[0].textContent.trim(),
          "words",
          "First text should match"
        );
        Assert.equal(
          boxes[1].textContent.trim(),
          "Please use find to",
          "Second word should match"
        );
      }
    );
    await promiseEnterStringIntoFindField(findbar, word);
    await promise;
  });
});

add_task(async function testTooLargeToggle() {
  let url = kFixtureBaseURL + "file_FinderSample.html";
  await BrowserTestUtils.withNewTab(url, async function (browser) {
    let findbar = await gBrowser.getFindBar();
    await promiseOpenFindbar(findbar);

    await SpecialPowers.spawn(browser, [], async function () {
      let dwu = content.windowUtils;
      let uri =
        "data:text/css;charset=utf-8," +
        encodeURIComponent(`
        body {
          min-height: 1234567px;
        }`);
      try {
        dwu.loadSheetUsingURIString(uri, dwu.USER_SHEET);
      } catch (e) {}
    });

    let word = "Roland";
    let expectedResult = {
      rectCount: 2,
      insertCalls: [2, 4],
      removeCalls: [0, 2],
      // No animations should be triggered when the page is too large.
      animationCalls: [0, 0],
    };
    let promise = promiseTestHighlighterOutput(browser, word, expectedResult);
    await promiseEnterStringIntoFindField(findbar, word);
    await promise;
  });
});
