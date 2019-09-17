/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_offscreen_text() {
  // Generate URI of a big DOM that contains the target text at several
  // line positions (to force some targets to be offscreen).
  const linesToGenerate = 155;
  const linesToInsertTargetText = [5, 50, 150];
  let targetCount = linesToInsertTargetText.length;
  let t = 0;
  const TARGET_TEXT = "findthis";

  let URI = "<body>";
  for (let i = 0; i < linesToGenerate; i++) {
    URI += i + "<br>";
    if (t < targetCount && linesToInsertTargetText[t] == i) {
      URI += TARGET_TEXT;
      t++;
    }
  }
  URI += "</body>";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "data:text/html;charset=utf-8," + encodeURIComponent(URI),
    },
    async function(browser) {
      let finder = browser.finder;
      let listener = {
        onFindResult() {
          ok(false, "callback wasn't replaced");
        },
      };
      finder.addResultListener(listener);

      function waitForFind() {
        return new Promise(resolve => {
          listener.onFindResult = resolve;
        });
      }

      // Find each of the targets.
      for (let t = 0; t < targetCount; ++t) {
        let promiseFind = waitForFind();
        if (t == 0) {
          finder.fastFind(TARGET_TEXT, false, false);
        } else {
          finder.findAgain(TARGET_TEXT, false, false, false);
        }
        let findResult = await promiseFind;
        is(
          findResult.result,
          Ci.nsITypeAheadFind.FIND_FOUND,
          "Found target " + t
        );
      }

      // Find one more time and make sure we wrap.
      let promiseFind = waitForFind();
      finder.findAgain(TARGET_TEXT, false, false, false);
      let findResult = await promiseFind;
      is(
        findResult.result,
        Ci.nsITypeAheadFind.FIND_WRAPPED,
        "Wrapped to first target"
      );

      finder.removeResultListener(listener);
    }
  );
});
