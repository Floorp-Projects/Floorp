/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/keys.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

function prepareBookmarkItem(collection, id) {
  let b = new Bookmark(collection, id);
  b.cleartext.stuff = "my payload here";
  return b;
}

add_task(async function test_bookmark_record() {
  await configureIdentity();

  await generateNewKeys(Service.collectionKeys);
  let keyBundle = Service.identity.syncKeyBundle;

  _("Creating a record");

  let placesItem = new PlacesItem("bookmarks", "foo", "bookmark");
  let bookmarkItem = prepareBookmarkItem("bookmarks", "foo");

  _("Checking getTypeObject");
  Assert.equal(placesItem.getTypeObject(placesItem.type), Bookmark);
  Assert.equal(bookmarkItem.getTypeObject(bookmarkItem.type), Bookmark);

  await bookmarkItem.encrypt(keyBundle);
  _("Ciphertext is " + bookmarkItem.ciphertext);
  Assert.ok(bookmarkItem.ciphertext != null);

  _("Decrypting the record");

  let payload = await bookmarkItem.decrypt(keyBundle);
  Assert.equal(payload.stuff, "my payload here");
  Assert.equal(bookmarkItem.getTypeObject(bookmarkItem.type), Bookmark);
  Assert.notEqual(payload, bookmarkItem.payload); // wrap.data.payload is the encrypted one
});
