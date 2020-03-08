"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "setTimeout",
  "resource://gre/modules/Timer.jsm"
);

const kFixtureBaseURL =
  "https://example.com/browser/toolkit/modules/tests/browser/";

function removeDupes(list) {
  let j = 0;
  for (let i = 1; i < list.length; i++) {
    if (list[i] != list[j]) {
      j++;
      if (i != j) {
        list[j] = list[i];
      }
    }
  }
  list.length = j + 1;
}

function compareLists(list1, list2, kind) {
  list1.sort();
  removeDupes(list1);
  list2.sort();
  removeDupes(list2);
  is(String(list1), String(list2), `${kind} URLs correct`);
}

async function promiseOpenFindbar(findbar) {
  await gBrowser.getFindBar();
  findbar.onFindCommand();
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
      onMatchesCountResult: () => {},
    };
    findbar.browser.finder.addResultListener(listener);
  });
}

function promiseEnterStringIntoFindField(findbar, str) {
  let promise = promiseFindResult(findbar, str);
  for (let i = 0; i < str.length; i++) {
    let event = document.createEvent("KeyboardEvent");
    event.initKeyEvent(
      "keypress",
      true,
      true,
      null,
      false,
      false,
      false,
      false,
      0,
      str.charCodeAt(i)
    );
    findbar._findField.dispatchEvent(event);
  }
  return promise;
}

function promiseTestHighlighterOutput(
  browser,
  word,
  expectedResult,
  extraTest = () => {}
) {
  return SpecialPowers.spawn(
    browser,
    [{ word, expectedResult, extraTest: extraTest.toSource() }],
    async function({ word, expectedResult, extraTest }) {
      return new Promise((resolve, reject) => {
        let stubbed = {};
        let callCounts = {
          insertCalls: [],
          removeCalls: [],
          animationCalls: [],
        };
        let lastMaskNode, lastOutlineNode;
        let rects = [];

        // Amount of milliseconds to wait after the last time one of our stubs
        // was called.
        const kTimeoutMs = 1000;
        // The initial timeout may wait for a while for results to come in.
        let timeout = content.setTimeout(
          () => finish(false, "Timeout"),
          kTimeoutMs * 5
        );

        function finish(ok = true, message = "finished with error") {
          // Restore the functions we stubbed out.
          try {
            content.document.insertAnonymousContent = stubbed.insert;
            content.document.removeAnonymousContent = stubbed.remove;
          } catch (ex) {}
          stubbed = {};
          content.clearTimeout(timeout);

          if (expectedResult.rectCount !== 0) {
            Assert.ok(ok, message);
          }

          Assert.greaterOrEqual(
            callCounts.insertCalls.length,
            expectedResult.insertCalls[0],
            `Min. insert calls should match for '${word}'.`
          );
          Assert.lessOrEqual(
            callCounts.insertCalls.length,
            expectedResult.insertCalls[1],
            `Max. insert calls should match for '${word}'.`
          );
          Assert.greaterOrEqual(
            callCounts.removeCalls.length,
            expectedResult.removeCalls[0],
            `Min. remove calls should match for '${word}'.`
          );
          Assert.lessOrEqual(
            callCounts.removeCalls.length,
            expectedResult.removeCalls[1],
            `Max. remove calls should match for '${word}'.`
          );

          // We reached the amount of calls we expected, so now we can check
          // the amount of rects.
          if (!lastMaskNode && expectedResult.rectCount !== 0) {
            Assert.ok(
              false,
              `No mask node found, but expected ${expectedResult.rectCount} rects.`
            );
          }

          Assert.equal(
            rects.length,
            expectedResult.rectCount,
            `Amount of inserted rects should match for '${word}'.`
          );

          if ("animationCalls" in expectedResult) {
            Assert.greaterOrEqual(
              callCounts.animationCalls.length,
              expectedResult.animationCalls[0],
              `Min. animation calls should match for '${word}'.`
            );
            Assert.lessOrEqual(
              callCounts.animationCalls.length,
              expectedResult.animationCalls[1],
              `Max. animation calls should match for '${word}'.`
            );
          }

          // Allow more specific assertions to be tested in `extraTest`.
          // eslint-disable-next-line no-eval
          extraTest = eval(extraTest);
          extraTest(lastMaskNode, lastOutlineNode, rects);

          resolve();
        }

        function stubAnonymousContentNode(domNode, anonNode) {
          let originals = [
            anonNode.setTextContentForElement,
            anonNode.setAttributeForElement,
            anonNode.removeAttributeForElement,
            anonNode.setCutoutRectsForElement,
            anonNode.setAnimationForElement,
          ];
          anonNode.setTextContentForElement = (id, text) => {
            try {
              (domNode.querySelector("#" + id) || domNode).textContent = text;
            } catch (ex) {}
            return originals[0].call(anonNode, id, text);
          };
          anonNode.setAttributeForElement = (id, attrName, attrValue) => {
            try {
              (domNode.querySelector("#" + id) || domNode).setAttribute(
                attrName,
                attrValue
              );
            } catch (ex) {}
            return originals[1].call(anonNode, id, attrName, attrValue);
          };
          anonNode.removeAttributeForElement = (id, attrName) => {
            try {
              let node = domNode.querySelector("#" + id) || domNode;
              if (node.hasAttribute(attrName)) {
                node.removeAttribute(attrName);
              }
            } catch (ex) {}
            return originals[2].call(anonNode, id, attrName);
          };
          anonNode.setCutoutRectsForElement = (id, cutoutRects) => {
            rects = cutoutRects;
            return originals[3].call(anonNode, id, cutoutRects);
          };
          anonNode.setAnimationForElement = (id, keyframes, options) => {
            callCounts.animationCalls.push([keyframes, options]);
            return originals[4].call(anonNode, id, keyframes, options);
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
              if (node.outerHTML.indexOf("outlineMask") > -1) {
                lastMaskNode = node;
              } else {
                lastOutlineNode = node;
              }
            }
            content.clearTimeout(timeout);
            timeout = content.setTimeout(() => {
              finish();
            }, kTimeoutMs);
            let res = stubbed[which].call(content.document, node);
            if (which == "insert") {
              stubAnonymousContentNode(node, res);
            }
            return res;
          };
        }
        content.document.insertAnonymousContent = stub("insert");
        content.document.removeAnonymousContent = stub("remove");
      });
    }
  );
}
