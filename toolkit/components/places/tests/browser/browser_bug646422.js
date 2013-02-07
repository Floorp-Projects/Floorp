/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for Bug 646224.  Make sure that after changing the URI via
 * history.pushState, the history service has a title stored for the new URI.
 **/

function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab('http://example.com');
  let tabBrowser = tab.linkedBrowser;

  tabBrowser.addEventListener('load', function(aEvent) {
    tabBrowser.removeEventListener('load', arguments.callee, true);

    // Control should now flow down to observer.onTitleChanged().
    let cw = tabBrowser.contentWindow;
    ok(cw.document.title, 'Content window should initially have a title.');
    cw.history.pushState('', '', 'new_page');
  }, true);

  let observer = {
    onTitleChanged: function(uri, title) {
      // If the uri of the page whose title is changing ends with 'new_page',
      // then it's the result of our pushState.
      if (/new_page$/.test(uri.spec)) {
        is(title, tabBrowser.contentWindow.document.title,
           'Title after pushstate.');
        PlacesUtils.history.removeObserver(this);
        gBrowser.removeTab(tab);
        finish();
      }
    },

    onBeginUpdateBatch: function() { },
    onEndUpdateBatch: function() { },
    onVisit: function() { },
    onDeleteURI: function() { },
    onClearHistory: function() { },
    onPageChanged: function() { },
    onDeleteVisits: function() { },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
  };

  PlacesUtils.history.addObserver(observer, false);
}
