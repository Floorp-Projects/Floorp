/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for Bug 646224.  Make sure that after changing the URI via
 * history.pushState, the history service has a title stored for the new URI.
 **/

add_task(function* () {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com");

  let newTitlePromise = new Promise(resolve => {
    let observer = {
      onTitleChanged(uri, title) {
        // If the uri of the page whose title is changing ends with 'new_page',
        // then it's the result of our pushState.
        if (/new_page$/.test(uri.spec)) {
          resolve(title);
          PlacesUtils.history.removeObserver(observer);
        }
      },

      onBeginUpdateBatch() { },
      onEndUpdateBatch() { },
      onVisit() { },
      onDeleteURI() { },
      onClearHistory() { },
      onPageChanged() { },
      onDeleteVisits() { },
      QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
    };

    PlacesUtils.history.addObserver(observer, false);
  });

  yield ContentTask.spawn(tab.linkedBrowser, null, function* () {
    let title =  content.document.title;
    content.history.pushState("", "", "new_page");
    Assert.ok(title, "Content window should initially have a title.");
  });

  let newtitle = yield newTitlePromise;

  yield ContentTask.spawn(tab.linkedBrowser, { newtitle }, function* (args) {
    Assert.equal(args.newtitle, content.document.title, "Title after pushstate.");
  });

  yield PlacesTestUtils.clearHistory();
  gBrowser.removeTab(tab);
});
