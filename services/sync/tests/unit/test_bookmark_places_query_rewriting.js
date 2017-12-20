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

  let uri = "place:folder=499&type=7&queryType=1";
  let tagRecord = makeTagRecord("abcdefabcdef", uri);

  _("Type: " + tagRecord.type);
  _("Folder name: " + tagRecord.folderName);
  await store.applyIncoming(tagRecord);

  let tagID = -1;
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`
    SELECT id FROM moz_bookmarks
    WHERE parent = :tagsFolderId AND
          title = :title`,
    { tagsFolderId: PlacesUtils.tagsFolderId,
      title: "bar" });
  equal(rows.length, 1);
  tagID = rows[0].getResultByName("id");

  _("Tag ID: " + tagID);
  let insertedRecord = await store.createRecord("abcdefabcdef", "bookmarks");
  Assert.equal(insertedRecord.bmkUri, uri.replace("499", tagID));

  _("... but not if the type is wrong.");
  let wrongTypeURI = "place:folder=499&type=2&queryType=1";
  let wrongTypeRecord = makeTagRecord("fedcbafedcba", wrongTypeURI);
  await store.applyIncoming(wrongTypeRecord);

  insertedRecord = await store.createRecord("fedcbafedcba", "bookmarks");
  Assert.equal(insertedRecord.bmkUri, wrongTypeURI);
});
