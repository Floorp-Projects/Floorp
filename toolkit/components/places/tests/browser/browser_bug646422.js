/* ***** BEGIN LICENSE BLOCK *****
 *   Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Justin Lebar <justin.lebar@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 * 
 * ***** END LICENSE BLOCK ***** */

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
    onBeforeDeleteURI: function() { },
    onDeleteURI: function() { },
    onClearHistory: function() { },
    onPageChanged: function() { },
    onDeleteVisits: function() { },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
  };

  PlacesUtils.history.addObserver(observer, false);
}
