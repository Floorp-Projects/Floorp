/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const bmsvc = PlacesUtils.bookmarks;
const histsvc = PlacesUtils.history;

function run_test() {
  run_next_test();
}

add_task(function* test_addBookmarksAndCheckGuids() {
  let folder = bmsvc.createFolder(bmsvc.placesRoot, "test folder", bmsvc.DEFAULT_INDEX);
  let b1 = bmsvc.insertBookmark(folder, uri("http://test1.com/"),
                                bmsvc.DEFAULT_INDEX, "1 title");
  let b2 = bmsvc.insertBookmark(folder, uri("http://test2.com/"),
                                bmsvc.DEFAULT_INDEX, "2 title");
  let b3 = bmsvc.insertBookmark(folder, uri("http://test3.com/"),
                                bmsvc.DEFAULT_INDEX, "3 title");
  let s1 = bmsvc.insertSeparator(folder, bmsvc.DEFAULT_INDEX);
  let f1 = bmsvc.createFolder(folder, "test folder 2", bmsvc.DEFAULT_INDEX);

  let root = PlacesUtils.getFolderContents(folder).root;
  do_check_eq(root.childCount, 5);

  // check bookmark guids
  let bookmarkGuidZero = root.getChild(0).bookmarkGuid;
  do_check_eq(bookmarkGuidZero.length, 12);
  // bookmarks have bookmark guids
  do_check_eq(root.getChild(1).bookmarkGuid.length, 12);
  do_check_eq(root.getChild(2).bookmarkGuid.length, 12);
  // separator has bookmark guid
  do_check_eq(root.getChild(3).bookmarkGuid.length, 12);
  // folder has bookmark guid
  do_check_eq(root.getChild(4).bookmarkGuid.length, 12);
  // all bookmark guids are different.
  do_check_neq(bookmarkGuidZero, root.getChild(1).bookmarkGuid);
  do_check_neq(root.getChild(1).bookmarkGuid, root.getChild(2).bookmarkGuid);
  do_check_neq(root.getChild(2).bookmarkGuid, root.getChild(3).bookmarkGuid);
  do_check_neq(root.getChild(3).bookmarkGuid, root.getChild(4).bookmarkGuid);

  // check page guids
  let pageGuidZero = root.getChild(0).pageGuid;
  do_check_eq(pageGuidZero.length, 12);
  // bookmarks have page guids
  do_check_eq(root.getChild(1).pageGuid.length, 12);
  do_check_eq(root.getChild(2).pageGuid.length, 12);
  // folder and separator don't have page guids
  do_check_eq(root.getChild(3).pageGuid, "");
  do_check_eq(root.getChild(4).pageGuid, "");

  do_check_neq(pageGuidZero, root.getChild(1).pageGuid);
  do_check_neq(root.getChild(1).pageGuid, root.getChild(2).pageGuid);

  root.containerOpen = false;

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_updateBookmarksAndCheckGuids() {
  let folder = bmsvc.createFolder(bmsvc.placesRoot, "test folder", bmsvc.DEFAULT_INDEX);
  let b1 = bmsvc.insertBookmark(folder, uri("http://test1.com/"),
                                bmsvc.DEFAULT_INDEX, "1 title");
  let f1 = bmsvc.createFolder(folder, "test folder 2", bmsvc.DEFAULT_INDEX);

  let root = PlacesUtils.getFolderContents(folder).root;
  do_check_eq(root.childCount, 2);

  // ensure the bookmark and page guids remain the same after modifing other property.
  let bookmarkGuidZero = root.getChild(0).bookmarkGuid;
  let pageGuidZero = root.getChild(0).pageGuid;
  bmsvc.setItemTitle(b1, "1 title mod");
  do_check_eq(root.getChild(0).title, "1 title mod");
  do_check_eq(root.getChild(0).bookmarkGuid, bookmarkGuidZero);
  do_check_eq(root.getChild(0).pageGuid, pageGuidZero);

  let bookmarkGuidOne = root.getChild(1).bookmarkGuid;
  let pageGuidOne = root.getChild(1).pageGuid;
  bmsvc.setItemTitle(f1, "test foolder 234");
  do_check_eq(root.getChild(1).title, "test foolder 234");
  do_check_eq(root.getChild(1).bookmarkGuid, bookmarkGuidOne);
  do_check_eq(root.getChild(1).pageGuid, pageGuidOne);

  root.containerOpen = false;

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_addVisitAndCheckGuid() {
  // add a visit and test page guid and non-existing bookmark guids.
  let now = Date.now() * 1000;
  let sourceURI = uri("http://test4.com/");
  yield PlacesTestUtils.addVisits({ uri: sourceURI });
  do_check_eq(bmsvc.getBookmarkedURIFor(sourceURI), null);

  let options = histsvc.getNewQueryOptions();
  let query = histsvc.getNewQuery();
  query.uri = sourceURI;
  let root = histsvc.executeQuery(query, options).root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 1);

  do_check_valid_places_guid(root.getChild(0).pageGuid);
  do_check_eq(root.getChild(0).bookmarkGuid, "");
  root.containerOpen = false;

  yield PlacesTestUtils.clearHistory();
});

add_task(function* test_addItemsWithInvalidGUIDsFails() {
  const INVALID_GUID = "XYZ";
  try {
    bmsvc.createFolder(bmsvc.placesRoot, "XYZ folder",
                       bmsvc.DEFAULT_INDEX, INVALID_GUID);
    do_throw("Adding a folder with an invalid guid should fail");
  }
  catch(ex) { }

  let folder = bmsvc.createFolder(bmsvc.placesRoot, "test folder",
                                  bmsvc.DEFAULT_INDEX);
  try {
    bmsvc.insertBookmark(folder, uri("http://test.tld"), bmsvc.DEFAULT_INDEX,
                         "title", INVALID_GUID);
    do_throw("Adding a bookmark with an invalid guid should fail");
  }
  catch(ex) { }

  try {
    bmsvc.insertSeparator(folder, bmsvc.DEFAULT_INDEX, INVALID_GUID);
    do_throw("Adding a separator with an invalid guid should fail");
  }
  catch(ex) { }

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_addItemsWithGUIDs() {
  const FOLDER_GUID     = "FOLDER--GUID";
  const BOOKMARK_GUID   = "BM------GUID";
  const SEPARATOR_GUID  = "SEP-----GUID";

  let folder = bmsvc.createFolder(bmsvc.placesRoot, "test folder",
                                  bmsvc.DEFAULT_INDEX, FOLDER_GUID);
  bmsvc.insertBookmark(folder, uri("http://test1.com/"), bmsvc.DEFAULT_INDEX,
                       "1 title", BOOKMARK_GUID);
  bmsvc.insertSeparator(folder, bmsvc.DEFAULT_INDEX, SEPARATOR_GUID);

  let root = PlacesUtils.getFolderContents(folder).root;
  do_check_eq(root.childCount, 2);
  do_check_eq(root.bookmarkGuid, FOLDER_GUID);
  do_check_eq(root.getChild(0).bookmarkGuid, BOOKMARK_GUID);
  do_check_eq(root.getChild(1).bookmarkGuid, SEPARATOR_GUID);

  root.containerOpen = false;
  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_emptyGUIDIgnored() {
  let folder = bmsvc.createFolder(bmsvc.placesRoot, "test folder",
                                  bmsvc.DEFAULT_INDEX, "");
  do_check_valid_places_guid(PlacesUtils.getFolderContents(folder)
                                        .root.bookmarkGuid);
  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_usingSameGUIDFails() {
  const GUID = "XYZXYZXYZXYZ";
  bmsvc.createFolder(bmsvc.placesRoot, "test folder",
                     bmsvc.DEFAULT_INDEX, GUID);
  try {
    bmsvc.createFolder(bmsvc.placesRoot, "test folder 2",
                       bmsvc.DEFAULT_INDEX, GUID);
    do_throw("Using the same guid twice should fail");
  }
  catch(ex) { }

  yield PlacesUtils.bookmarks.eraseEverything();
});
