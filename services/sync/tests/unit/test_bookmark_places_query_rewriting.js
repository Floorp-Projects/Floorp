/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

_("Rewrite place: URIs.");
const { BookmarkQuery, BookmarksEngine } = ChromeUtils.import(
  "resource://services-sync/engines/bookmarks.js"
);
const { Service } = ChromeUtils.import("resource://services-sync/service.js");

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

  let insertedRecord = await store.createRecord("abcdefabcdef", "bookmarks");
  Assert.equal(insertedRecord.bmkUri, "place:tag=bar");

  _("... but not if the type is wrong.");
  let wrongTypeURI = "place:folder=499&type=2&queryType=1";
  let wrongTypeRecord = makeTagRecord("fedcbafedcba", wrongTypeURI);
  await store.applyIncoming(wrongTypeRecord);

  insertedRecord = await store.createRecord("fedcbafedcba", "bookmarks");
  Assert.equal(insertedRecord.bmkUri, wrongTypeURI);
});
