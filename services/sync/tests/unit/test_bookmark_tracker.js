Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/util.js");
try {
  Cu.import("resource://gre/modules/PlacesUtils.jsm");
}
catch(ex) {
  Cu.import("resource://gre/modules/utils.js");
}

// Grab a backstage pass so we can fetch the anno constant. Evil but useful.
let bsp = Cu.import("resource://services-sync/engines/bookmarks.js");
const SYNC_GUID_ANNO = bsp.GUID_ANNO;      // Not the same as Places' GUID_ANNO!

Engines.register(BookmarksEngine);
let engine = Engines.get("bookmarks");
let store  = engine._store;
store.wipe();

function test_copying_places() {

  // Gecko <2.0
  if (store._haveGUIDColumn) {
    _("We have a GUID column; not testing anno GUID fixing.");
    return;
  }

  //
  // Copied and simplified from PlacesUIUtils, to which we don't have easy
  // access.
  //
  let ptm;

  try {
    ptm = Components.classes["@mozilla.org/browser/placesTransactionsService;1"]
                      .getService(Ci.nsIPlacesTransactionsService);
  } catch (ex) {
    _("Can't test transactions -- not running with a browser context.");
    return;
  }

  function getBookmarkItemCopyTransaction(aData, aContainer, aIndex,
                                          aExcludeAnnotations) {
    var itemURL = PlacesUtils._uri(aData.uri);
    var itemTitle = aData.title;
    var keyword = aData.keyword || null;
    var annos = aData.annos || [];
    // always exclude GUID when copying any item
    var excludeAnnos = [PlacesUtils.GUID_ANNO];
    if (aExcludeAnnotations)
      excludeAnnos = excludeAnnos.concat(aExcludeAnnotations);
    annos = annos.filter(function(aValue, aIndex, aArray) {
      return excludeAnnos.indexOf(aValue.name) == -1;
    });
    var childTxns = [];
    if (aData.dateAdded)
      childTxns.push(ptm.editItemDateAdded(null, aData.dateAdded));
    if (aData.lastModified)
      childTxns.push(ptm.editItemLastModified(null, aData.lastModified));
    if (aData.tags) {
      var tags = aData.tags.split(", ");
      var storedTags = PlacesUtils.tagging.getTagsForURI(itemURL, {});
      tags = tags.filter(function (aTag) {
        return (storedTags.indexOf(aTag) == -1);
      }, this);
      if (tags.length)
        childTxns.push(ptm.tagURI(itemURL, tags));
    }

    return ptm.createItem(itemURL, aContainer, aIndex, itemTitle, keyword,
                          annos, childTxns);
  }

  function getFolderCopyTransaction(aData, aContainer, aIndex) {
    function getChildItemsTransactions(aChildren) {
      var childItemsTransactions = [];
      var cc = aChildren.length;
      var index = aIndex;
      for (var i = 0; i < cc; ++i) {
        var txn = null;
        var node = aChildren[i];

        // Make sure that items are given the correct index, this will be
        // passed by the transaction manager to the backend for the insertion.
        // Insertion behaves differently if index == DEFAULT_INDEX (append)
        if (aIndex != PlacesUtils.bookmarks.DEFAULT_INDEX)
          index = i;

        if (node.type == PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER)
          txn = getFolderCopyTransaction(node, aContainer, index);
        else // (node.type == PlacesUtils.TYPE_X_MOZ_PLACE)
          txn = getBookmarkItemCopyTransaction(node, -1, index);

        if (txn)
          childItemsTransactions.push(txn);
      }
      return childItemsTransactions;
    }

    // tag folders use tag transactions
    if (aContainer == PlacesUtils.tagsFolderId) {
      var txns = [];
      if (aData.children) {
        aData.children.forEach(function(aChild) {
          txns.push(ptm.tagURI(PlacesUtils._uri(aChild.uri), [aData.title]));
        }, this);
      }
      return ptm.aggregateTransactions("addTags", txns);
    }
    else {
      var childItems = getChildItemsTransactions(aData.children);
      if (aData.dateAdded)
        childItems.push(ptm.editItemDateAdded(null, aData.dateAdded));
      if (aData.lastModified)
        childItems.push(ptm.editItemLastModified(null, aData.lastModified));

      var annos = aData.annos || [];
      annos = annos.filter(function(aAnno) {
        // always exclude GUID when copying any item
        return aAnno.name != PlacesUtils.GUID_ANNO;
      });
      return ptm.createFolder(aData.title, aContainer, aIndex, annos, childItems);
    }
  }

  //
  // End simplified PlacesUIUtils code.
  //

  let tracker = engine._tracker;
  tracker.clearChangedIDs();

  try {
    _("Test copying using Places transactions.");
    Svc.Obs.notify("weave:engine:start-tracking");

    _("Set up source and destination folders.");
    let f1 = Svc.Bookmark.createFolder(PlacesUtils.toolbarFolderId, "Folder One",
                                       Svc.Bookmark.DEFAULT_INDEX);
    let f2 = Svc.Bookmark.createFolder(PlacesUtils.toolbarFolderId, "Folder Two",
                                       Svc.Bookmark.DEFAULT_INDEX);

    let b1 = Svc.Bookmark.insertBookmark(f1,
                                         Utils.makeURI("http://example.com/"),
                                         Svc.Bookmark.DEFAULT_INDEX,
                                         "Example!");

    _("F1: " + f1 + "; F2: " + f2 + "; B1: " + b1);

    _("Fetch GUIDs so our anno exists.");
    let f1GUID = store.GUIDForId(f1);
    let f2GUID = store.GUIDForId(f2);
    let b1GUID = store.GUIDForId(b1);
    do_check_true(!!f1GUID);
    do_check_true(!!f2GUID);
    do_check_true(!!b1GUID);

    _("Make sure the destination folder is empty.");
    do_check_eq(PlacesUtils.getFolderContents(f2, false, true).root.childCount, 0);

    let f1Node = PlacesUtils.getFolderContents(f1, false, false);
    _("Node to copy: " + f1Node.root.itemId);

    let serialized = PlacesUtils.wrapNode(f1Node.root, PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER);
    _("Serialized to " + serialized);

    let raw = PlacesUtils.unwrapNodes(serialized, PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER).shift();
    let transaction = getFolderCopyTransaction(raw, f2, Svc.Bookmark.DEFAULT_INDEX, true);

    _("Run the copy transaction.");
    ptm.doTransaction(transaction);

    _("Verify that items have been copied.");
    let f2Node = PlacesUtils.getFolderContents(f2, false, true);
    do_check_eq(f2Node.root.childCount, 1);

    _("Verify that the copied folder has different GUIDs.");
    let c0 = f2Node.root.getChild(0);
    do_check_eq(c0.title, "Folder One");
    do_check_neq(c0.itemId, f1);
    do_check_neq(store.GUIDForId(c0.itemId), f1GUID);

    _("Verify that this copied folder contains a copied bookmark with different GUIDs.");
    c0 = c0.QueryInterface(Ci.nsINavHistoryContainerResultNode);
    c0.containerOpen = true;

    do_check_eq(c0.childCount, 1);
    let b0 = c0.getChild(0);

    do_check_eq(b0.title, "Example!");
    do_check_eq(b0.uri, "http://example.com/");
    do_check_neq(b0.itemId, b1);
    do_check_neq(store.GUIDForId(b0.itemId), b1GUID);

  } finally {
    tracker.clearChangedIDs();
    Svc.Obs.notify("weave:engine:stop-tracking");
  }
}

function test_tracking() {
  _("Verify we've got an empty tracker to work with.");
  let tracker = engine._tracker;
  do_check_eq([id for (id in tracker.changedIDs)].length, 0);

  let folder = Svc.Bookmark.createFolder(Svc.Bookmark.bookmarksMenuFolder,
                                         "Test Folder",
                                         Svc.Bookmark.DEFAULT_INDEX);
  function createBmk() {
    return Svc.Bookmark.insertBookmark(folder,
                                       Utils.makeURI("http://getfirefox.com"),
                                       Svc.Bookmark.DEFAULT_INDEX,
                                       "Get Firefox!");
  }

  try {
    _("Create bookmark. Won't show because we haven't started tracking yet");
    createBmk();
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);

    _("Tell the tracker to start tracking changes.");
    Svc.Obs.notify("weave:engine:start-tracking");
    createBmk();
    // We expect two changed items because the containing folder
    // changed as well (new child).
    do_check_eq([id for (id in tracker.changedIDs)].length, 2);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:start-tracking");
    createBmk();
    do_check_eq([id for (id in tracker.changedIDs)].length, 3);

    _("Let's stop tracking again.");
    tracker.clearChangedIDs();
    Svc.Obs.notify("weave:engine:stop-tracking");
    createBmk();
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:stop-tracking");
    createBmk();
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);

  } finally {
    _("Clean up.");
    store.wipe();
    tracker.clearChangedIDs();
    Svc.Obs.notify("weave:engine:stop-tracking");
  }
}

function test_guid_stripping() {
  // Gecko <2.0
  if (store._haveGUIDColumn) {
    _("We have a GUID column; not testing anno GUID fixing.");
    return;
  }

  _("Verify we've got an empty tracker to work with.");
  let tracker = engine._tracker;
  let folder = Svc.Bookmark.createFolder(Svc.Bookmark.bookmarksMenuFolder,
                                         "Test Folder",
                                         Svc.Bookmark.DEFAULT_INDEX);
  function createBmk() {
    return Svc.Bookmark.insertBookmark(folder,
                                       Utils.makeURI("http://getfirefox.com"),
                                       Svc.Bookmark.DEFAULT_INDEX,
                                       "Get Firefox!");
  }

  do_check_eq([id for (id in tracker.changedIDs)].length, 0);

  try {
    _("Directly testing GUID stripping.");

    Svc.Obs.notify("weave:engine:start-tracking");
    tracker.ignoreAll = false;
    let suspect = createBmk();
    let victim = createBmk();

    _("Suspect: " + suspect + ", victim: " + victim);

    let suspectGUID = store.GUIDForId(suspect);
    let victimGUID  = store.GUIDForId(victim);
    _("Set the GUID on one entry to be the same as another.");
    do_check_neq(suspectGUID, victimGUID);
    Utils.anno(suspect, SYNC_GUID_ANNO, store.GUIDForId(victim));

    _("Tracker changed it to something else.");
    let newGUID = store.GUIDForId(suspect);
    do_check_neq(newGUID, victimGUID);
    do_check_neq(newGUID, suspectGUID);

    _("Victim GUID remains unchanged.");
    do_check_eq(victimGUID, store.GUIDForId(victim));

    _("New GUID is in the tracker.");
    _(JSON.stringify(tracker.changedIDs));
    do_check_neq(tracker.changedIDs[newGUID], null);
  } finally {
    _("Clean up.");
    store.wipe();
    tracker.clearChangedIDs();
    Svc.Obs.notify("weave:engine:stop-tracking");
  }
}

function run_test() {
  initTestLogging("Trace");

  Log4Moz.repository.getLogger("Engine.Bookmarks").level = Log4Moz.Level.Trace;
  Log4Moz.repository.getLogger("Store.Bookmarks").level = Log4Moz.Level.Trace;
  Log4Moz.repository.getLogger("Tracker.Bookmarks").level = Log4Moz.Level.Trace;

  test_copying_places();
  test_tracking();
  test_guid_stripping();
}

