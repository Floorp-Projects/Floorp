/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_vertical_text() {
  const URI = '<body><div style="max-height: 100px; max-width: 100px; overflow: scroll;"><div style="padding-left: 100px; max-height: 100px; max-width: 200px; overflow: auto;">d<br/><br/><br/><br/>c----------------b<br/><br/><br/><br/>a</div></div></body>';
  await BrowserTestUtils.withNewTab({ gBrowser, url: "data:text/html;charset=utf-8," + encodeURIComponent(URI) },
    async function(browser) {
      let finder = browser.finder;
      let listener = {
        onFindResult() {
          ok(false, "callback wasn't replaced");
        }
      };
      finder.addResultListener(listener);

      function waitForFind() {
        return new Promise(resolve => {
          listener.onFindResult = resolve;
        });
      }

      let targets = [
        "a",
        "b",
        "c",
        "d",
      ];

      for (let i = 0; i < targets.length; ++i) {
        // Find the target text.
        let target = targets[i];
        let promiseFind = waitForFind();
        finder.fastFind(target, false, false);
        let findResult = await promiseFind;
        isnot(findResult.result, Ci.nsITypeAheadFind.FIND_NOTFOUND, "Found target text '" + target + "'.");
      }

      finder.removeResultListener(listener);
    });
});
