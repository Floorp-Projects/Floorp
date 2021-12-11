/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function() {
  const url =
    "data:text/html;base64," +
    btoa(
      '<body><iframe srcdoc="content"/></iframe>' +
        '<a href="http://test.com">test link</a>'
    );
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  let finder = tab.linkedBrowser.finder;
  let listener = {
    onFindResult() {
      ok(false, "onFindResult callback wasn't replaced");
    },
    onHighlightFinished() {
      ok(false, "onHighlightFinished callback wasn't replaced");
    },
  };
  finder.addResultListener(listener);

  function waitForFind(which = "onFindResult") {
    return new Promise(resolve => {
      listener[which] = resolve;
    });
  }

  let promiseFind = waitForFind("onHighlightFinished");
  finder.highlight(true, "content");
  let findResult = await promiseFind;
  Assert.ok(findResult.found, "should find string");

  promiseFind = waitForFind("onHighlightFinished");
  finder.highlight(true, "Bla");
  findResult = await promiseFind;
  Assert.ok(!findResult.found, "should not find string");

  // Search only for links and draw outlines.
  promiseFind = waitForFind();
  finder.fastFind("test link", true, true);
  findResult = await promiseFind;
  is(findResult.result, Ci.nsITypeAheadFind.FIND_FOUND, "should find link");

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function(arg) {
    Assert.ok(
      !!content.document.getElementsByTagName("a")[0].style.outline,
      "outline set"
    );
  });

  // Just a simple search for "test link".
  promiseFind = waitForFind();
  finder.fastFind("test link", false, false);
  findResult = await promiseFind;
  is(
    findResult.result,
    Ci.nsITypeAheadFind.FIND_FOUND,
    "should find link again"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function(arg) {
    Assert.ok(
      !content.document.getElementsByTagName("a")[0].style.outline,
      "outline not set"
    );
  });

  finder.removeResultListener(listener);
  gBrowser.removeTab(tab);
});
