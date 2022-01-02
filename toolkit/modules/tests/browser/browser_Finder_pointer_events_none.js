/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_offscreen_text() {
  const URI = '<body><div style="pointer-events:none">find this</div></body>';
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

      // Find the target text.
      let promiseFind = waitForFind();
      finder.fastFind("find this", false, false);
      let findResult = await promiseFind;
      is(
        findResult.result,
        Ci.nsITypeAheadFind.FIND_FOUND,
        "Found target text."
      );

      finder.removeResultListener(listener);
    }
  );
});
