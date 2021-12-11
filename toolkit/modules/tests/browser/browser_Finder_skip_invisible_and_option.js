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
        finder.findAgain("a", false, false, false);
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

add_task(async function test_find_anon_content() {
  const URI = `
    <!doctype html>
    <style>
      div::before { content: "before content"; }
      div::after { content: "after content"; }
      span::after { content: ","; }
    </style>
    <div> </div>
    <img alt="Some fallback text">
    <input type="submit" value="Some button text">
    <input type="password" value="password">
    <p>1<span></span>234</p>

  `;
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

      async function assertFindable(text, findable = true) {
        let promiseFind = waitForFind();
        finder.fastFind(text, false, false);
        let findResult = await promiseFind;
        is(
          findResult.result,
          findable
            ? Ci.nsITypeAheadFind.FIND_FOUND
            : Ci.nsITypeAheadFind.FIND_NOTFOUND,
          `${text} should ${findable ? "" : "not "}be findable`
        );
      }

      await assertFindable("before content");
      await assertFindable("after content");
      await assertFindable("fallback text");
      await assertFindable("button text");
      await assertFindable("password", false);

      // TODO(emilio): In an ideal world we could select the comma as well and
      // then you'd find it with "1,234" instead...
      await assertFindable("1234");

      finder.removeResultListener(listener);
    }
  );
});
