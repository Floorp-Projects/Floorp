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

function run_test() {
  Service.identity.username = "john@example.com";
  Service.identity.syncKey = "abcdeabcdeabcdeabcdeabcdea";
  generateNewKeys(Service.collectionKeys);
  let keyBundle = Service.identity.syncKeyBundle;

  let log = Log.repository.getLogger("Test");
  Log.repository.rootLogger.addAppender(new Log.DumpAppender());

  log.info("Creating a record");

  let u = "http://localhost:8080/storage/bookmarks/foo";
  let placesItem = new PlacesItem("bookmarks", "foo", "bookmark");
  let bookmarkItem = prepareBookmarkItem("bookmarks", "foo");

  log.info("Checking getTypeObject");
  do_check_eq(placesItem.getTypeObject(placesItem.type), Bookmark);
  do_check_eq(bookmarkItem.getTypeObject(bookmarkItem.type), Bookmark);

  bookmarkItem.encrypt(keyBundle);
  log.info("Ciphertext is " + bookmarkItem.ciphertext);
  do_check_true(bookmarkItem.ciphertext != null);

  log.info("Decrypting the record");

  let payload = bookmarkItem.decrypt(keyBundle);
  do_check_eq(payload.stuff, "my payload here");
  do_check_eq(bookmarkItem.getTypeObject(bookmarkItem.type), Bookmark);
  do_check_neq(payload, bookmarkItem.payload); // wrap.data.payload is the encrypted one
}
