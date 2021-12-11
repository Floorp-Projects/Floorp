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
        // Full matches use one path in our find code.
        "vertical-rl",
        "vertical-lr",
        "sideways-rl",
        "sideways-lr",
        // Partial matches use a second path in our find code.
        "l-r",
        "l-l",
        "s-r",
        "s-l",
      ];

      for (let i = 0; i < targets.length; ++i) {
        // Find the target text.
        let target = targets[i];
        let promiseFind = waitForFind();
        finder.fastFind(target, false, false);
        let findResult = await promiseFind;

        // We check the logical inversion of not not found, because found and wrapped are
        // two different correct results, but not found is the only incorrect result.
        isnot(
          findResult.result,
          Ci.nsITypeAheadFind.FIND_NOTFOUND,
          "Found target text '" + target + "'."
        );
      }

      finder.removeResultListener(listener);
    }
  );
});
