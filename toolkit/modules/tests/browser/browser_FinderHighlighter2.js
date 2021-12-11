"use strict";

const kPrefHighlightAll = "findbar.highlightAll";
const kPrefModalHighlight = "findbar.modalHighlight";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [kPrefHighlightAll, true],
      [kPrefModalHighlight, true],
    ],
  });
});

add_task(async function testIframeOffset() {
  let url = kFixtureBaseURL + "file_FinderIframeTest.html";

  await BrowserTestUtils.withNewTab(url, async function(browser) {
    let findbar = gBrowser.getFindBar();
    await promiseOpenFindbar(findbar);

    let word = "frame";
    let expectedResult = {
      rectCount: 12,
      insertCalls: [2, 4],
      removeCalls: [0, 2],
    };
    let promise = promiseTestHighlighterOutput(
      browser,
      word,
      expectedResult,
      (maskNode, outlineNode, rects) => {
        Assert.equal(
          rects.length,
          expectedResult.rectCount,
          "Rect counts should match"
        );
        // Checks to guard against regressing this functionality:
        let expectedOffsets = [
          { x: 16, y: 60 },
          { x: 68, y: 104 },
          { x: 21, y: 215 },
          { x: 78, y: 264 },
          { x: 21, y: 375 },
          { x: 78, y: 424 },
          { x: 20, y: 534 },
          { x: 93, y: 534 },
          { x: 71, y: 577 },
          { x: 145, y: 577 },
        ];
        for (let i = 1, l = rects.length - 1; i < l; ++i) {
          let rect = rects[i];
          let expected = expectedOffsets[i - 1];
          Assert.equal(
            Math.floor(rect.x),
            expected.x,
            "Horizontal offset should match for rect " + i
          );
          Assert.equal(
            Math.floor(rect.y),
            expected.y,
            "Vertical offset should match for rect " + i
          );
        }
      }
    );
    await promiseEnterStringIntoFindField(findbar, word);
    await promise;
  });
});
