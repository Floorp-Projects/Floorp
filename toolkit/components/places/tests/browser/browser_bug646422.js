/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for Bug 646224.  Make sure that after changing the URI via
 * history.pushState, the history service has a title stored for the new URI.
 **/

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com"
  );

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

      onBeginUpdateBatch() {},
      onEndUpdateBatch() {},
      onDeleteURI() {},
      onClearHistory() {},
      onPageChanged() {},
      onDeleteVisits() {},
      QueryInterface: ChromeUtils.generateQI(["nsINavHistoryObserver"]),
    };

    PlacesUtils.history.addObserver(observer);
  });

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    let title = content.document.title;
    content.history.pushState("", "", "new_page");
    Assert.ok(title, "Content window should initially have a title.");
  });

  let newtitle = await newTitlePromise;

  await SpecialPowers.spawn(tab.linkedBrowser, [{ newtitle }], async function(
    args
  ) {
    Assert.equal(
      args.newtitle,
      content.document.title,
      "Title after pushstate."
    );
  });

  await PlacesUtils.history.clear();
  gBrowser.removeTab(tab);
});
