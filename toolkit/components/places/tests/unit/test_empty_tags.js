/***************************** BEGIN LICENSE BLOCK *****************************
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with the
* License. You may obtain a copy of the License at http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
* the specific language governing rights and limitations under the License.
*
* The Original Code is Places Test Code.
*
* The Initial Developer of the Original Code is Mozilla Corporation.
* Portions created by the Initial Developer are Copyright (C) 2009 the Initial
* Developer. All Rights Reserved.
*
* Contributor(s):
*  Edward Lee <edilee@mozilla.com> (original author)
*
* Alternatively, the contents of this file may be used under the terms of either
* the GNU General Public License Version 2 or later (the "GPL"), or the GNU
* Lesser General Public License Version 2.1 or later (the "LGPL"), in which case
* the provisions of the GPL or the LGPL are applicable instead of those above.
* If you wish to allow use of your version of this file only under the terms of
* either the GPL or the LGPL, and not to allow others to use your version of
* this file under the terms of the MPL, indicate your decision by deleting the
* provisions above and replace them with the notice and other provisions
* required by the GPL or the LGPL. If you do not delete the provisions above, a
* recipient may use your version of this file under the terms of any one of the
* MPL, the GPL or the LGPL.
*
****************************** END LICENSE BLOCK ******************************/

/**
 * Test bug 489443 to make sure bookmarks with empty tags don't have the empty
 * tags shown.
 */

let ac = Cc["@mozilla.org/autocomplete/search;1?name=history"].
  getService(Ci.nsIAutoCompleteSearch);
let bm = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
  getService(Ci.nsINavBookmarksService);
let io = Cc["@mozilla.org/network/io-service;1"].
  getService(Ci.nsIIOService);
let ts = Cc["@mozilla.org/browser/tagging-service;1"].
  getService(Ci.nsITaggingService);

let _ = function(some, debug, message) print(Array.slice(arguments).join(" "));

function run_test() {
  // always search in history + bookmarks, no matter what the default is
  var prefs = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);
  prefs.setIntPref("browser.urlbar.search.sources", 3);
  prefs.setIntPref("browser.urlbar.default.behavior", 0);

  let uri = io.newURI("http://uri/", null, null);
  bm.insertBookmark(bm.toolbarFolder, uri, bm.DEFAULT_INDEX, "title");

  _("Adding 3 tags to the bookmark");
  let tagIds = [];
  for (let i = 0; i < 3; i++) {
    _("Tagging uri with tag:", i);
    ts.tagURI(uri, ["" + i]);
    let id = bm.getIdForItemAt(bm.tagsFolder, i);
    _("Saving bookmark id of tag to rename it later:", id);
    tagIds.push(id);
  }

  _("Search 4 times: make sure we get the right amount of tags then remove one");
  (function doSearch(query) {
    _("Searching for:", query);
    ac.startSearch(query, "", null, {
      onSearchResult: function(search, result) {
        _("Got results with status:", result.searchResult);
        if (result.searchResult != result.RESULT_SUCCESS)
          return;

        let comment = result.getCommentAt(0);
        _("Got the title/tags:", comment);

        _("Until we get bug 489443, we have title and tags separated by \u2013");
        if (comment.indexOf("\u2013") == -1) {
          let num = tagIds.length;
          _("No tags in result, so making sure we have no tags:", num);
          do_check_eq(num, 0);
          do_test_finished();
          return;
        }

        let num = tagIds.length;
        _("Making sure we get the expected number of tags:", num);
        do_check_eq(comment.split(",").length, num);

        _("Removing a tag from the tag ids array:", tagIds);
        let id = tagIds.shift();
        _("Setting an empty title for tag:", id);
        bm.setItemTitle(id, "");

        _("Creating a timer to do a new search to let the current one finish");
        let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
        let next = query.slice(1);
        _("Do a new, uncached search with a new query:", next);
        timer.initWithCallback({ notify: function() doSearch(next) },
          0, timer.TYPE_ONE_SHOT);
      }
    });
  })("title");

  do_test_pending();
}
