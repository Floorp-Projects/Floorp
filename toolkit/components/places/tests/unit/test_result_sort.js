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
  return new Promise(resolve => {
    let bookmarksObserver = {
      __proto__: NavBookmarkObserver.prototype,
      onItemVisited: function BO_onItemVisited() {
        PlacesUtils.bookmarks.removeObserver(this);
        // Enqueue to be sure that all onItemVisited notifications ran.
        do_execute_soon(resolve);
      }
    };
    PlacesUtils.bookmarks.addObserver(bookmarksObserver);
  });
}

add_task(async function test() {
  const uri1 = "http://foo.tld/a";
  const uri2 = "http://foo.tld/b";

  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      title: "Result-sort functionality tests root",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      children: [{
        title: "b",
        url: uri1,
      }, {
        title: "a",
        url: uri2,
      }, {
        // url of the first child, title of second
        title: "a",
        url: uri1,
      }, ]
    }]
  });

  let testFolderId = await PlacesUtils.promiseItemId(bookmarks[0].guid);

  let guid1 = bookmarks[1].guid;
  let guid2 = bookmarks[2].guid;
  let guid3 = bookmarks[3].guid;

  // query with natural order
  let result = PlacesUtils.getFolderContents(testFolderId);
  let root = result.root;

  do_check_eq(root.childCount, 3);

  function checkOrder(a, b, c) {
    do_check_eq(root.getChild(0).bookmarkGuid, a);
    do_check_eq(root.getChild(1).bookmarkGuid, b);
    do_check_eq(root.getChild(2).bookmarkGuid, c);
  }

  // natural order
  do_print("Natural order");
  checkOrder(guid1, guid2, guid3);

  // title: guid3 should precede guid2 since we fall-back to URI-based sorting
  do_print("Sort by title asc");
  result.sortingMode = NHQO.SORT_BY_TITLE_ASCENDING;
  checkOrder(guid3, guid2, guid1);

  // In reverse
  do_print("Sort by title desc");
  result.sortingMode = NHQO.SORT_BY_TITLE_DESCENDING;
  checkOrder(guid1, guid2, guid3);

  // uri sort: guid1 should precede guid3 since we fall-back to natural order
  do_print("Sort by uri asc");
  result.sortingMode = NHQO.SORT_BY_URI_ASCENDING;
  checkOrder(guid1, guid3, guid2);

  // test live update
  do_print("Change bookmark uri liveupdate");
  await PlacesUtils.bookmarks.update({
    guid: guid1,
    url: uri2,
  });
  checkOrder(guid3, guid1, guid2);
  await PlacesUtils.bookmarks.update({
    guid: guid1,
    url: uri1,
  });
  checkOrder(guid1, guid3, guid2);

  // keyword sort
  do_print("Sort by keyword asc");
  result.sortingMode = NHQO.SORT_BY_KEYWORD_ASCENDING;
  checkOrder(guid3, guid2, guid1);  // no keywords set - falling back to title sort
  await PlacesUtils.keywords.insert({ url: uri1, keyword: "a" });
  await PlacesUtils.keywords.insert({ url: uri2, keyword: "z" });
  checkOrder(guid3, guid1, guid2);

  // XXXtodo: test history sortings (visit count, visit date)
  // XXXtodo: test different item types once folderId and bookmarkId are merged.
  // XXXtodo: test sortingAnnotation functionality with non-bookmark nodes

  do_print("Sort by annotation desc");
  let ids = await PlacesUtils.promiseManyItemIds([guid1, guid3]);
  PlacesUtils.annotations.setItemAnnotation(ids.get(guid1), "testAnno", "a", 0, 0);
  PlacesUtils.annotations.setItemAnnotation(ids.get(guid3), "testAnno", "b", 0, 0);
  result.sortingAnnotation = "testAnno";
  result.sortingMode = NHQO.SORT_BY_ANNOTATION_DESCENDING;

  // guid1 precedes guid2 per title-descending fallback
  checkOrder(guid3, guid1, guid2);

  // XXXtodo:  test dateAdded sort
  // XXXtodo:  test lastModified sort

  // test live update
  do_print("Annotation liveupdate");
  PlacesUtils.annotations.setItemAnnotation(ids.get(guid1), "testAnno", "c", 0, 0);
  checkOrder(guid1, guid3, guid2);

  // Add a visit, then check frecency ordering.

  // When the bookmarks service gets onVisit, it asynchronously fetches all
  // items for that visit, and then notifies onItemVisited.  Thus we must
  // explicitly wait for that.
  let waitForVisited = promiseOnItemVisited();
  await PlacesTestUtils.addVisits({ uri: uri2, transition: TRANSITION_TYPED });
  await waitForVisited;

  do_print("Sort by frecency desc");
  result.sortingMode = NHQO.SORT_BY_FRECENCY_DESCENDING;
  for (let i = 0; i < root.childCount; ++i) {
    print(root.getChild(i).uri + " " + root.getChild(i).title);
  }
  // For guid1 and guid3, since they have same frecency and no visits, fallback
  // to sort by the newest bookmark.
  checkOrder(guid2, guid3, guid1);
  do_print("Sort by frecency asc");
  result.sortingMode = NHQO.SORT_BY_FRECENCY_ASCENDING;
  for (let i = 0; i < root.childCount; ++i) {
    print(root.getChild(i).uri + " " + root.getChild(i).title);
  }
  checkOrder(guid1, guid3, guid2);

  root.containerOpen = false;
});
