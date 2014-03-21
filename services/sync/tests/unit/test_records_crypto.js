/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/keys.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

let cryptoWrap;

function crypted_resource_handler(metadata, response) {
  let obj = {id: "resource",
             modified: cryptoWrap.modified,
             payload: JSON.stringify(cryptoWrap.payload)};
  return httpd_basic_auth_handler(JSON.stringify(obj), metadata, response);
}

function prepareCryptoWrap(collection, id) {
  let w = new CryptoWrapper();
  w.cleartext.stuff = "my payload here";
  w.collection = collection;
  w.id = id;
  return w;
}

function run_test() {
  let server;
  do_test_pending();

  Service.identity.username = "john@example.com";
  Service.identity.syncKey = "a-abcde-abcde-abcde-abcde-abcde";
  let keyBundle = Service.identity.syncKeyBundle;

  try {
    let log = Log.repository.getLogger("Test");
    Log.repository.rootLogger.addAppender(new Log.DumpAppender());

    log.info("Setting up server and authenticator");

    server = httpd_setup({"/steam/resource": crypted_resource_handler});

    log.info("Creating a record");

    let cryptoUri = "http://localhost:8080/crypto/steam";
    cryptoWrap = prepareCryptoWrap("steam", "resource");

    log.info("cryptoWrap: " + cryptoWrap.toString());

    log.info("Encrypting a record");

    cryptoWrap.encrypt(keyBundle);
    log.info("Ciphertext is " + cryptoWrap.ciphertext);
    do_check_true(cryptoWrap.ciphertext != null);

    let firstIV = cryptoWrap.IV;

    log.info("Decrypting the record");

    let payload = cryptoWrap.decrypt(keyBundle);
    do_check_eq(payload.stuff, "my payload here");
    do_check_neq(payload, cryptoWrap.payload); // wrap.data.payload is the encrypted one

    log.info("Make sure multiple decrypts cause failures");
    let error = "";
    try {
      payload = cryptoWrap.decrypt(keyBundle);
    }
    catch(ex) {
      error = ex;
    }
    do_check_eq(error, "No ciphertext: nothing to decrypt?");

    log.info("Re-encrypting the record with alternate payload");

    cryptoWrap.cleartext.stuff = "another payload";
    cryptoWrap.encrypt(keyBundle);
    let secondIV = cryptoWrap.IV;
    payload = cryptoWrap.decrypt(keyBundle);
    do_check_eq(payload.stuff, "another payload");

    log.info("Make sure multiple encrypts use different IVs");
    do_check_neq(firstIV, secondIV);

    log.info("Make sure differing ids cause failures");
    cryptoWrap.encrypt(keyBundle);
    cryptoWrap.data.id = "other";
    error = "";
    try {
      cryptoWrap.decrypt(keyBundle);
    }
    catch(ex) {
      error = ex;
    }
    do_check_eq(error, "Record id mismatch: resource != other");

    log.info("Make sure wrong hmacs cause failures");
    cryptoWrap.encrypt(keyBundle);
    cryptoWrap.hmac = "foo";
    error = "";
    try {
      cryptoWrap.decrypt(keyBundle);
    }
    catch(ex) {
      error = ex;
    }
    do_check_eq(error.substr(0, 42), "Record SHA256 HMAC mismatch: should be foo");

    // Checking per-collection keys and default key handling.

    generateNewKeys(Service.collectionKeys);
    let bu = "http://localhost:8080/storage/bookmarks/foo";
    let bookmarkItem = prepareCryptoWrap("bookmarks", "foo");
    bookmarkItem.encrypt(Service.collectionKeys.keyForCollection("bookmarks"));
    log.info("Ciphertext is " + bookmarkItem.ciphertext);
    do_check_true(bookmarkItem.ciphertext != null);
    log.info("Decrypting the record explicitly with the default key.");
    do_check_eq(bookmarkItem.decrypt(Service.collectionKeys._default).stuff, "my payload here");

    // Per-collection keys.
    // Generate a key for "bookmarks".
    generateNewKeys(Service.collectionKeys, ["bookmarks"]);
    bookmarkItem = prepareCryptoWrap("bookmarks", "foo");
    do_check_eq(bookmarkItem.collection, "bookmarks");

    // Encrypt. This'll use the "bookmarks" encryption key, because we have a
    // special key for it. The same key will need to be used for decryption.
    bookmarkItem.encrypt(Service.collectionKeys.keyForCollection("bookmarks"));
    do_check_true(bookmarkItem.ciphertext != null);

    // Attempt to use the default key, because this is a collision that could
    // conceivably occur in the real world. Decryption will error, because
    // it's not the bookmarks key.
    let err;
    try {
      bookmarkItem.decrypt(Service.collectionKeys._default);
    } catch (ex) {
      err = ex;
    }
    do_check_eq("Record SHA256 HMAC mismatch", err.substr(0, 27));

    // Explicitly check that it's using the bookmarks key.
    // This should succeed.
    do_check_eq(bookmarkItem.decrypt(Service.collectionKeys.keyForCollection("bookmarks")).stuff,
        "my payload here");

    log.info("Done!");
  }
  finally {
    server.stop(do_test_finished);
  }
}
