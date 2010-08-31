/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is Places Test Code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (Original Author)
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

let bs = PlacesUtils.bookmarks;
let db = DBConn();

function check_keyword(aItemId, aExpectedBookmarkKeyword, aExpectedURIKeyword) {
  // All keywords are handled lowercased internally.
  if (aExpectedURIKeyword)
    aExpectedURIKeyword = aExpectedURIKeyword.toLowerCase();
  if (aExpectedBookmarkKeyword)
    aExpectedBookmarkKeyword = aExpectedBookmarkKeyword.toLowerCase();

  if (aItemId) {
    print("Check keyword for bookmark");
    do_check_eq(bs.getKeywordForBookmark(aItemId), aExpectedBookmarkKeyword);

    print("Check keyword for uri");
    let uri = bs.getBookmarkURI(aItemId);
    do_check_eq(bs.getKeywordForURI(uri), aExpectedURIKeyword);

    print("Check uri for keyword");
    // This API can't tell which uri the user wants, so it returns a random one.
    if (aExpectedURIKeyword) {
      do_check_true(/http:\/\/test[0-9]\.mozilla\.org/.test(bs.getURIForKeyword(aExpectedURIKeyword).spec));
      // Check case insensitivity.
      do_check_true(/http:\/\/test[0-9]\.mozilla\.org/.test(bs.getURIForKeyword(aExpectedURIKeyword.toUpperCase()).spec));
    }
  }
  else {
    stmt = db.createStatement(
      "SELECT id FROM moz_keywords WHERE keyword = :keyword"
    );
    stmt.params.keyword = aExpectedBookmarkKeyword;
    try {
      do_check_false(stmt.executeStep());
    } finally {
      stmt.finalize();
    }
  }

  print("Check there are no orphan database entries");
  let stmt = db.createStatement(
    "SELECT b.id FROM moz_bookmarks b "
  + "LEFT JOIN moz_keywords k ON b.keyword_id = k.id "
  + "WHERE keyword_id NOTNULL AND k.id ISNULL"
  );
  try {
    do_check_false(stmt.executeStep());
  } finally {
    stmt.finalize();
  }
}

function run_test() {
  print("Check that leyword does not exist");
  do_check_eq(bs.getURIForKeyword("keyword"), null);

  let folderId = bs.createFolder(PlacesUtils.unfiledBookmarksFolderId,
                                 "folder", bs.DEFAULT_INDEX);

  print("Add a bookmark with a keyword");
  let itemId1 = bs.insertBookmark(folderId,
                                  uri("http://test1.mozilla.org/"),
                                  bs.DEFAULT_INDEX,
                                  "test1");
  check_keyword(itemId1, null, null);
  bs.setKeywordForBookmark(itemId1, "keyword");
  check_keyword(itemId1, "keyword", "keyword");
  // Check case insensitivity.
  check_keyword(itemId1, "kEyWoRd", "kEyWoRd");

  print("Add another bookmark with the same uri, should not inherit keyword.");
  let itemId1_bis = bs.insertBookmark(folderId,
                                      uri("http://test1.mozilla.org/"),
                                      bs.DEFAULT_INDEX,
                                      "test1_bis");

  check_keyword(itemId1_bis, null, "keyword");
  // Check case insensitivity.
  check_keyword(itemId1_bis, null, "kEyWoRd");

  print("Set same keyword on another bookmark with a different uri.");
  let itemId2 = bs.insertBookmark(folderId,
                                  uri("http://test2.mozilla.org/"),
                                  bs.DEFAULT_INDEX,
                                  "test2");
  check_keyword(itemId2, null, null);
  bs.setKeywordForBookmark(itemId2, "kEyWoRd");
  check_keyword(itemId1, "kEyWoRd", "kEyWoRd");
  // Check case insensitivity.
  check_keyword(itemId1, "keyword", "keyword");
  check_keyword(itemId1_bis, null, "keyword");
  check_keyword(itemId2, "keyword", "keyword");

  print("Remove a bookmark with a keyword, it should not be removed from others");
  bs.removeItem(itemId2);
  check_keyword(itemId1, "keyword", "keyword");

  print("Remove a folder containing bookmarks with keywords");
  // Keyword should be removed as well.
  bs.removeItem(folderId);
  check_keyword(null, "keyword");
}

