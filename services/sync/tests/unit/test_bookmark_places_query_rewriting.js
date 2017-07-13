/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

_("Rewrite place: URIs.");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

let engine = new BookmarksEngine(Service);
let store = engine._store;

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

add_task(async function run_test() {
  initTestLogging("Trace");
  Log.repository.getLogger("Sync.Engine.Bookmarks").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.Store.Bookmarks").level = Log.Level.Trace;

  let uri = "place:folder=499&type=7&queryType=1";
  let tagRecord = makeTagRecord("abcdefabcdef", uri);

  _("Type: " + tagRecord.type);
  _("Folder name: " + tagRecord.folderName);
  await store.applyIncoming(tagRecord);

  let tags = PlacesUtils.getFolderContents(PlacesUtils.tagsFolderId).root;
  let tagID;
  try {
    for (let i = 0; i < tags.childCount; ++i) {
      let child = tags.getChild(i);
      if (child.title == "bar") {
        tagID = child.itemId;
      }
    }
  } finally {
    tags.containerOpen = false;
  }

  _("Tag ID: " + tagID);
  let insertedRecord = await store.createRecord("abcdefabcdef", "bookmarks");
  do_check_eq(insertedRecord.bmkUri, uri.replace("499", tagID));

  _("... but not if the type is wrong.");
  let wrongTypeURI = "place:folder=499&type=2&queryType=1";
  let wrongTypeRecord = makeTagRecord("fedcbafedcba", wrongTypeURI);
  await store.applyIncoming(wrongTypeRecord);

  insertedRecord = await store.createRecord("fedcbafedcba", "bookmarks");
  do_check_eq(insertedRecord.bmkUri, wrongTypeURI);
});
