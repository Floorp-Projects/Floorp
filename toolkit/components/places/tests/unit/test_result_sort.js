/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const NHQO = Ci.nsINavHistoryQueryOptions;

/**
 * Waits for onItemVisited notifications to be received.
 */
function promiseOnItemVisited() {
  let defer = Promise.defer();
  let bookmarksObserver = {
    __proto__: NavBookmarkObserver.prototype,
    onItemVisited: function BO_onItemVisited() {
      PlacesUtils.bookmarks.removeObserver(this);
      // Enqueue to be sure that all onItemVisited notifications ran.
      do_execute_soon(defer.resolve);
    }
  };
  PlacesUtils.bookmarks.addObserver(bookmarksObserver, false);
  return defer.promise;
}

function run_test() {
  run_next_test();
}

add_task(function test() {
  let testFolder = PlacesUtils.bookmarks.createFolder(
    PlacesUtils.bookmarks.placesRoot,
    "Result-sort functionality tests root",
    PlacesUtils.bookmarks.DEFAULT_INDEX);

  let uri1 = NetUtil.newURI("http://foo.tld/a");
  let uri2 = NetUtil.newURI("http://foo.tld/b");

  let id1 = PlacesUtils.bookmarks.insertBookmark(
    testFolder, uri1, PlacesUtils.bookmarks.DEFAULT_INDEX, "b");
  let id2 = PlacesUtils.bookmarks.insertBookmark(
    testFolder, uri2, PlacesUtils.bookmarks.DEFAULT_INDEX, "a");
  // url of id1, title of id2
  let id3 = PlacesUtils.bookmarks.insertBookmark(
    testFolder, uri1, PlacesUtils.bookmarks.DEFAULT_INDEX, "a");

  // query with natural order
  let result = PlacesUtils.getFolderContents(testFolder);
  let root = result.root;

  do_check_eq(root.childCount, 3);

  function checkOrder(a, b, c) {
    do_check_eq(root.getChild(0).itemId, a);
    do_check_eq(root.getChild(1).itemId, b);
    do_check_eq(root.getChild(2).itemId, c);
  }

  // natural order
  do_print("Natural order");
  checkOrder(id1, id2, id3);

  // title: id3 should precede id2 since we fall-back to URI-based sorting
  do_print("Sort by title asc");
  result.sortingMode = NHQO.SORT_BY_TITLE_ASCENDING;
  checkOrder(id3, id2, id1);

  // In reverse
  do_print("Sort by title desc");
  result.sortingMode = NHQO.SORT_BY_TITLE_DESCENDING;
  checkOrder(id1, id2, id3);

  // uri sort: id1 should precede id3 since we fall-back to natural order
  do_print("Sort by uri asc");
  result.sortingMode = NHQO.SORT_BY_URI_ASCENDING;
  checkOrder(id1, id3, id2);

  // test live update
  do_print("Change bookmark uri liveupdate");
  PlacesUtils.bookmarks.changeBookmarkURI(id1, uri2);
  checkOrder(id3, id1, id2);
  PlacesUtils.bookmarks.changeBookmarkURI(id1, uri1);
  checkOrder(id1, id3, id2);

  // keyword sort
  do_print("Sort by keyword asc");
  result.sortingMode = NHQO.SORT_BY_KEYWORD_ASCENDING;
  checkOrder(id3, id2, id1);  // no keywords set - falling back to title sort
  yield PlacesUtils.keywords.insert({ url: uri1.spec, keyword: "a" });
  yield PlacesUtils.keywords.insert({ url: uri2.spec, keyword: "z" });
  checkOrder(id3, id1, id2);

  // XXXtodo: test history sortings (visit count, visit date)
  // XXXtodo: test different item types once folderId and bookmarkId are merged.
  // XXXtodo: test sortingAnnotation functionality with non-bookmark nodes

  do_print("Sort by annotation desc");
  PlacesUtils.annotations.setItemAnnotation(id1, "testAnno", "a", 0, 0);
  PlacesUtils.annotations.setItemAnnotation(id3, "testAnno", "b", 0, 0);
  result.sortingAnnotation = "testAnno";
  result.sortingMode = NHQO.SORT_BY_ANNOTATION_DESCENDING;

  // id1 precedes id2 per title-descending fallback
  checkOrder(id3, id1, id2);

  // XXXtodo:  test dateAdded sort
  // XXXtodo:  test lastModified sort

  // test live update
  do_print("Annotation liveupdate");
  PlacesUtils.annotations.setItemAnnotation(id1, "testAnno", "c", 0, 0);
  checkOrder(id1, id3, id2);

  // Add a visit, then check frecency ordering.

  // When the bookmarks service gets onVisit, it asynchronously fetches all
  // items for that visit, and then notifies onItemVisited.  Thus we must
  // explicitly wait for that.
  let waitForVisited = promiseOnItemVisited();
  yield PlacesTestUtils.addVisits({ uri: uri2, transition: TRANSITION_TYPED });
  yield waitForVisited;

  do_print("Sort by frecency desc");
  result.sortingMode = NHQO.SORT_BY_FRECENCY_DESCENDING;
  for (let i = 0; i < root.childCount; ++i) {
    print(root.getChild(i).uri + " " + root.getChild(i).title);
  }
  // For id1 and id3, since they have same frecency and no visits, fallback
  // to sort by the newest bookmark.
  checkOrder(id2, id3, id1);
  do_print("Sort by frecency asc");
  result.sortingMode = NHQO.SORT_BY_FRECENCY_ASCENDING;
  for (let i = 0; i < root.childCount; ++i) {
    print(root.getChild(i).uri + " " + root.getChild(i).title);
  }
  checkOrder(id1, id3, id2);

  root.containerOpen = false;
});
