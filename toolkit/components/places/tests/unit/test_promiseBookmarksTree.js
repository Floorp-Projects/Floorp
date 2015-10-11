/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function* check_has_child(aParentGuid, aChildGuid) {
  let parentTree = yield PlacesUtils.promiseBookmarksTree(aParentGuid);
  do_check_true("children" in parentTree);
  do_check_true(parentTree.children.find( e => e.guid == aChildGuid ) != null);
}

function* compareToNode(aItem, aNode, aIsRootItem, aExcludedGuids = []) {
  // itemId==-1 indicates a non-bookmark node, which is unexpected.
  do_check_neq(aNode.itemId, -1);

  function check_unset(...aProps) {
    aProps.forEach( p => { do_check_false(p in aItem); } );
  }
  function strict_eq_check(v1, v2) {
    dump("v1: " + v1 + " v2: " + v2 + "\n");
    do_check_eq(typeof v1, typeof v2);
    do_check_eq(v1, v2);
  }
  function compare_prop(aItemProp, aNodeProp = aItemProp, aOptional = false) {
    if (aOptional && aNode[aNodeProp] === null)
      check_unset(aItemProp);
    else
      strict_eq_check(aItem[aItemProp], aNode[aNodeProp]);
  }
  function compare_prop_to_value(aItemProp, aValue, aOptional = true) {
    if (aOptional && aValue === null)
      check_unset(aItemProp);
    else
      strict_eq_check(aItem[aItemProp], aValue);
  }

  // Bug 1013053 - bookmarkIndex is unavailable for the query's root
  if (aNode.bookmarkIndex == -1) {
    compare_prop_to_value("index",
                          PlacesUtils.bookmarks.getItemIndex(aNode.itemId),
                          false);
  }
  else {
    compare_prop("index", "bookmarkIndex");
  }

  compare_prop("dateAdded");
  compare_prop("lastModified");

  if (aIsRootItem && aNode.itemId != PlacesUtils.placesRootId) {
    do_check_true("parentGuid" in aItem);
    yield check_has_child(aItem.parentGuid, aItem.guid)
  }
  else {
    check_unset("parentGuid");
  }

  let expectedAnnos = PlacesUtils.getAnnotationsForItem(aItem.id);
  if (expectedAnnos.length > 0) {
    let annosToString = annos => {
      return [(a.name + ":" + a.value) for (a of annos)].sort().join(",");
    };
    do_check_true(Array.isArray(aItem.annos))
    do_check_eq(annosToString(aItem.annos), annosToString(expectedAnnos));
  }
  else {
    check_unset("annos");
  }
  const BOOKMARK_ONLY_PROPS = ["uri", "iconuri", "tags", "charset", "keyword"];
  const FOLDER_ONLY_PROPS = ["children", "root"];

  let nodesCount = 1;

  switch (aNode.type) {
    case Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER:
      do_check_eq(aItem.type, PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER);
      compare_prop("title", "title", true);
      check_unset(...BOOKMARK_ONLY_PROPS);

      let expectedChildrenNodes = [];

      PlacesUtils.asContainer(aNode);
      if (!aNode.containerOpen)
        aNode.containerOpen = true;

      for (let i = 0; i < aNode.childCount; i++) {
        let childNode = aNode.getChild(i);
        if (childNode.itemId == PlacesUtils.tagsFolderId ||
            aExcludedGuids.includes(childNode.bookmarkGuid)) {
          continue;
        }
        expectedChildrenNodes.push(childNode);
      }

      if (expectedChildrenNodes.length > 0) {
        do_check_true(Array.isArray(aItem.children));
        do_check_eq(aItem.children.length, expectedChildrenNodes.length);
        for (let i = 0; i < aItem.children.length; i++) {
          nodesCount +=
            yield compareToNode(aItem.children[i], expectedChildrenNodes[i],
                                false, aExcludedGuids);
        }
      }
      else {
        check_unset("children");
      }

      switch (aItem.id) {
      case PlacesUtils.placesRootId:
        compare_prop_to_value("root", "placesRoot");
        break;
      case PlacesUtils.bookmarksMenuFolderId:
        compare_prop_to_value("root", "bookmarksMenuFolder");
        break;
      case PlacesUtils.toolbarFolderId:
        compare_prop_to_value("root", "toolbarFolder");
        break;
      case PlacesUtils.unfiledBookmarksFolderId:
        compare_prop_to_value("root", "unfiledBookmarksFolder");
        break;
      default:
        check_unset("root");
      }
      break;
    case Ci.nsINavHistoryResultNode.RESULT_TYPE_SEPARATOR:
      do_check_eq(aItem.type, PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR);
      check_unset(...BOOKMARK_ONLY_PROPS, ...FOLDER_ONLY_PROPS);
      break;
    default:
      do_check_eq(aItem.type, PlacesUtils.TYPE_X_MOZ_PLACE);
      compare_prop("uri");
      // node.tags's format is "a, b" whilst promiseBoookmarksTree is "a,b"
      if (aNode.tags === null)
        check_unset("tags");
      else
        compare_prop_to_value("tags", aNode.tags.replace(/, /g, ","), false);

      if (aNode.icon) {
        let nodeIconData = aNode.icon.replace("moz-anno:favicon:","");
        compare_prop_to_value("iconuri", nodeIconData);
      }
      else {
        check_unset(aItem.iconuri);
      }

      check_unset(...FOLDER_ONLY_PROPS);

      let itemURI = uri(aNode.uri);
      compare_prop_to_value("charset",
                            yield PlacesUtils.getCharsetForURI(itemURI));

      let entry = yield PlacesUtils.keywords.fetch({ url: aNode.uri });
      compare_prop_to_value("keyword", entry ? entry.keyword : null);

      if ("title" in aItem)
        compare_prop("title");
      else
        do_check_null(aNode.title);
  }

  if (aIsRootItem)
    do_check_eq(aItem.itemsCount, nodesCount);

  return nodesCount;
}

var itemsCount = 0;
function* new_bookmark(aInfo) {
  let currentItem = ++itemsCount;
  if (!("url" in aInfo))
    aInfo.url = uri("http://test.item." + itemsCount);

  if (!("title" in aInfo))
    aInfo.title = "Test Item (bookmark) " + itemsCount;

  yield PlacesTransactions.NewBookmark(aInfo).transact();
}

function* new_folder(aInfo) {
  if (!("title" in aInfo))
    aInfo.title = "Test Item (folder) " + itemsCount;
  return yield PlacesTransactions.NewFolder(aInfo).transact();
}

// Walks a result nodes tree and test promiseBookmarksTree for each node.
// DO NOT COPY THIS LOGIC:  It is done here to accomplish a more comprehensive
// test of the API (the entire hierarchy data is available in the very test).
function* test_promiseBookmarksTreeForEachNode(aNode, aOptions, aExcludedGuids) {
  do_check_true(aNode.bookmarkGuid && aNode.bookmarkGuid.length > 0);
  let item = yield PlacesUtils.promiseBookmarksTree(aNode.bookmarkGuid, aOptions);
  yield* compareToNode(item, aNode, true, aExcludedGuids);

  for (let i = 0; i < aNode.childCount; i++) {
    let child = aNode.getChild(i);
    if (child.itemId != PlacesUtils.tagsFolderId)
      yield test_promiseBookmarksTreeForEachNode(child,
                                                 { includeItemIds: true },
                                                 aExcludedGuids);
  }
  return item;
}

function* test_promiseBookmarksTreeAgainstResult(aItemGuid = "",
                                                 aOptions = { includeItemIds: true },
                                                 aExcludedGuids) {
  let itemId = aItemGuid ?
    yield PlacesUtils.promiseItemId(aItemGuid) : PlacesUtils.placesRootId;
  let node = PlacesUtils.getFolderContents(itemId).root;
  return yield test_promiseBookmarksTreeForEachNode(node, aOptions, aExcludedGuids);
}

add_task(function* () {
  // Add some bookmarks to cover various use cases.
  yield new_bookmark({ parentGuid: PlacesUtils.bookmarks.toolbarGuid });
  yield new_folder({ parentGuid: PlacesUtils.bookmarks.menuGuid
                   , annotations: [{ name: "TestAnnoA", value: "TestVal"
                                   , name: "TestAnnoB", value: 0 }]});
  let sepInfo = { parentGuid: PlacesUtils.bookmarks.menuGuid };
  yield PlacesTransactions.NewSeparator(sepInfo).transact();
  let folderGuid = yield new_folder({ parentGuid: PlacesUtils.bookmarks.menuGuid });
  yield new_bookmark({ title: null
                     , parentGuid: folderGuid
                     , keyword: "test_keyword"
                     , tags: ["TestTagA", "TestTagB"]
                     , annotations: [{ name: "TestAnnoA", value: "TestVal2"}]});
  let urlWithCharsetAndFavicon = uri("http://charset.and.favicon");
  yield new_bookmark({ parentGuid: folderGuid, url: urlWithCharsetAndFavicon });
  yield PlacesUtils.setCharsetForURI(urlWithCharsetAndFavicon, "UTF-8");
  yield promiseSetIconForPage(urlWithCharsetAndFavicon, SMALLPNG_DATA_URI);
  // Test the default places root without specifying it.
  yield test_promiseBookmarksTreeAgainstResult();

  // Do specify it
  yield test_promiseBookmarksTreeAgainstResult(PlacesUtils.bookmarks.rootGuid);

  // Exclude the bookmarks menu.
  // The calllback should be four times - once for the toolbar, once for
  // the bookmark we inserted under, and once for the menu (and not
  // at all for any of its descendants) and once for the unsorted bookmarks
  // folder.  However, promiseBookmarksTree is called multiple times, so
  // rather than counting the calls, we count the number of unique items
  // passed in.
  let guidsPassedToExcludeCallback = new Set();
  let placesRootWithoutTheMenu =
  yield test_promiseBookmarksTreeAgainstResult(PlacesUtils.bookmarks.rootGuid, {
    excludeItemsCallback: aItem =>  {
      guidsPassedToExcludeCallback.add(aItem.guid);
      return aItem.root == "bookmarksMenuFolder";
    },
    includeItemIds: true
  }, [PlacesUtils.bookmarks.menuGuid]);
  do_check_eq(guidsPassedToExcludeCallback.size, 4);
  do_check_eq(placesRootWithoutTheMenu.children.length, 2);
});
