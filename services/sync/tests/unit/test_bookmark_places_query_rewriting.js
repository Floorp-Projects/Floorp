/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

_("Rewrite place: URIs.");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

let engine = new BookmarksEngine(Service);
let store = engine._store;

function run_test() {
  initTestLogging("Trace");
  Log.repository.getLogger("Sync.Engine.Bookmarks").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.Store.Bookmarks").level = Log.Level.Trace;

  let tagRecord = new BookmarkQuery("bookmarks", "abcdefabcdef");
  let uri = "place:folder=499&type=7&queryType=1";
  tagRecord.queryId = "MagicTags";
  tagRecord.parentName = "Bookmarks Toolbar";
  tagRecord.bmkUri = uri;
  tagRecord.title = "tagtag";
  tagRecord.folderName = "bar";

  _("Type: " + tagRecord.type);
  _("Folder name: " + tagRecord.folderName);
  store.preprocessTagQuery(tagRecord);

  _("Verify that the URI has been rewritten.");
  do_check_neq(tagRecord.bmkUri, uri);

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
  do_check_eq(tagRecord.bmkUri, uri.replace("499", tagID));

  _("... but not if the type is wrong.");
  let wrongTypeURI = "place:folder=499&type=2&queryType=1";
  tagRecord.bmkUri = wrongTypeURI;
  store.preprocessTagQuery(tagRecord);
  do_check_eq(tagRecord.bmkUri, wrongTypeURI);
}
