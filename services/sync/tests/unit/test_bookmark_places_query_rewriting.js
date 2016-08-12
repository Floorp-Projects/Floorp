/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

_("Rewrite place: URIs.");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

var engine = new BookmarksEngine(Service);
var store = engine._store;

function makeTagRecord(id, uri) {
  let tagRecord = new BookmarkQuery("bookmarks", id);
  tagRecord.queryId = "MagicTags";
  tagRecord.parentName = "Bookmarks Toolbar";
  tagRecord.bmkUri = uri;
  tagRecord.title = "tagtag";
  tagRecord.folderName = "bar";
  tagRecord.parentid = PlacesUtils.bookmarks.toolbarGuid;
  return tagRecord;
}

function run_test() {
  initTestLogging("Trace");
  Log.repository.getLogger("Sync.Engine.Bookmarks").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.Store.Bookmarks").level = Log.Level.Trace;

  let uri = "place:folder=499&type=7&queryType=1";
  let tagRecord = makeTagRecord("abcdefabcdef", uri);

  _("Type: " + tagRecord.type);
  _("Folder name: " + tagRecord.folderName);
  store.applyIncoming(tagRecord);

  let tags = store._getNode(PlacesUtils.tagsFolderId);
  tags.containerOpen = true;
  let tagID;
  for (let i = 0; i < tags.childCount; ++i) {
    let child = tags.getChild(i);
    if (child.title == "bar")
      tagID = child.itemId;
  }
  tags.containerOpen = false;

  _("Tag ID: " + tagID);
  let insertedRecord = store.createRecord("abcdefabcdef", "bookmarks");
  do_check_eq(insertedRecord.bmkUri, uri.replace("499", tagID));

  _("... but not if the type is wrong.");
  let wrongTypeURI = "place:folder=499&type=2&queryType=1";
  let wrongTypeRecord = makeTagRecord("fedcbafedcba", wrongTypeURI);
  store.applyIncoming(wrongTypeRecord);

  insertedRecord = store.createRecord("fedcbafedcba", "bookmarks");
  do_check_eq(insertedRecord.bmkUri, wrongTypeURI);
}
