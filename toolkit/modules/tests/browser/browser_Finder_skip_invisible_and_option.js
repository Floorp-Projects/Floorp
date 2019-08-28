/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_skip_invisible() {
  const URI = `
    <body>
      <div>
        a
        <div style="visibility:hidden;">a</div>
        <div style="visibility: hidden"><span style="visibility: visible">a</div>
        <select>
          <option>a</option>
        </select>
        <select size=2>
          <option>a</option>
          <option>a</option>
        </select>
        <input placeholder="a">
      </body>`;
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

      // Find the target text. There should be three results.
      let target = "a";
      let promiseFind = waitForFind();
      finder.fastFind(target, false, false);
      let findResult = await promiseFind;

      // Check the results and repeat four times. After the final repeat, make
      // sure we've wrapped to the beginning.
      let i = 0;
      for (; i < 4; i++) {
        isnot(
          findResult.result,
          Ci.nsITypeAheadFind.FIND_NOTFOUND,
          "Should find target text '" + target + "' instance " + (i + 1) + "."
        );

        promiseFind = waitForFind();
        finder.findAgain(false, false, false);
        findResult = await promiseFind;
      }
      is(
        findResult.result,
        Ci.nsITypeAheadFind.FIND_WRAPPED,
        "After " + (i + 1) + " searches, we should wrap to first target text."
      );

      finder.removeResultListener(listener);
    }
  );
});
