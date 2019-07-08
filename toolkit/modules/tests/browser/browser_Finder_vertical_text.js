/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_vertical_text() {
  const URI =
    '<body><div style="writing-mode: vertical-rl">vertical-rl</div><div style="writing-mode: vertical-lr">vertical-lr</div><div style="writing-mode: sideways-rl">sideways-rl</div><div style="writing-mode: sideways-lr">sideways-lr</div></body>';
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

      let targets = [
        "vertical-rl",
        "vertical-lr",
        "sideways-rl",
        "sideways-lr",
      ];

      for (let i = 0; i < targets.length; ++i) {
        // Find the target text.
        let target = targets[i];
        let promiseFind = waitForFind();
        finder.fastFind(target, false, false);
        let findResult = await promiseFind;
        is(
          findResult.result,
          Ci.nsITypeAheadFind.FIND_FOUND,
          "Found target text '" + target + "'."
        );
      }

      finder.removeResultListener(listener);
    }
  );
});
