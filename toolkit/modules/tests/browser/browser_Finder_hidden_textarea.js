/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
add_task(function* test_bug1174036() {
  const URI =
    "<body><textarea>e1</textarea><textarea>e2</textarea><textarea>e3</textarea></body>";
  yield BrowserTestUtils.withNewTab({ gBrowser, url: "data:text/html;charset=utf-8," + encodeURIComponent(URI) },
    function* (browser) {
      // Hide the first textarea.
      yield ContentTask.spawn(browser, null, function() {
        content.document.getElementsByTagName("textarea")[0].style.display = "none";
      });

      let finder = browser.finder;
      let listener = {
        onFindResult: function() {
          ok(false, "callback wasn't replaced");
        }
      };
      finder.addResultListener(listener);

      function waitForFind() {
        return new Promise(resolve => {
          listener.onFindResult = resolve;
        })
      }

      // Find the first 'e' (which should be in the second textarea).
      let promiseFind = waitForFind();
      finder.fastFind("e", false, false);
      let findResult = yield promiseFind;
      is(findResult.result, Ci.nsITypeAheadFind.FIND_FOUND, "find first string");

      let firstRect = findResult.rect;

      // Find the second 'e' (in the third textarea).
      promiseFind = waitForFind();
      finder.findAgain(false, false, false);
      findResult = yield promiseFind;
      is(findResult.result, Ci.nsITypeAheadFind.FIND_FOUND, "find second string");
      ok(!findResult.rect.equals(firstRect), "found new string");

      // Ensure that we properly wrap to the second textarea.
      promiseFind = waitForFind();
      finder.findAgain(false, false, false);
      findResult = yield promiseFind;
      is(findResult.result, Ci.nsITypeAheadFind.FIND_WRAPPED, "wrapped to first string");
      ok(findResult.rect.equals(firstRect), "wrapped to original string");

      finder.removeResultListener(listener);
    });
});
