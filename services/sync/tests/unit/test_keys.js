/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/keys.js");
Cu.import("resource://services-sync/main.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/browserid_identity.js");
Cu.import("resource://testing-common/services/sync/utils.js");

var collectionKeys = new CollectionKeyManager();

function sha256HMAC(message, key) {
  let h = Utils.makeHMACHasher(Ci.nsICryptoHMAC.SHA256, key);
  return Utils.digestBytes(message, h);
}

function do_check_keypair_eq(a, b) {
  do_check_eq(2, a.length);
  do_check_eq(2, b.length);
  do_check_eq(a[0], b[0]);
  do_check_eq(a[1], b[1]);
}

function test_time_keyFromString(iterations) {
  let o;
  let b = new BulkKeyBundle("dummy");
  let d = Utils.decodeKeyBase32("ababcdefabcdefabcdefabcdef");
  b.generateRandom();

  _("Running " + iterations + " iterations of hmacKeyObject + sha256HMAC.");
  for (let i = 0; i < iterations; ++i) {
    let k = b.hmacKeyObject;
    o = sha256HMAC(d, k);
  }
  do_check_true(!!o);
  _("Done.");
}

add_test(function test_set_invalid_values() {
  _("Ensure that setting invalid encryption and HMAC key values is caught.");

  let bundle = new BulkKeyBundle("foo");

  let thrown = false;
  try {
    bundle.encryptionKey = null;
  } catch (ex) {
    thrown = true;
    do_check_eq(ex.message.indexOf("Encryption key can only be set to"), 0);
  } finally {
    do_check_true(thrown);
    thrown = false;
  }

  try {
    bundle.encryptionKey = ["trollololol"];
  } catch (ex) {
    thrown = true;
    do_check_eq(ex.message.indexOf("Encryption key can only be set to"), 0);
  } finally {
    do_check_true(thrown);
    thrown = false;
  }

  try {
    bundle.hmacKey = Utils.generateRandomBytes(15);
  } catch (ex) {
    thrown = true;
    do_check_eq(ex.message.indexOf("HMAC key must be at least 128"), 0);
  } finally {
    do_check_true(thrown);
    thrown = false;
  }

  try {
    bundle.hmacKey = null;
  } catch (ex) {
    thrown = true;
    do_check_eq(ex.message.indexOf("HMAC key can only be set to string"), 0);
  } finally {
    do_check_true(thrown);
    thrown = false;
  }

  try {
    bundle.hmacKey = ["trollolol"];
  } catch (ex) {
    thrown = true;
    do_check_eq(ex.message.indexOf("HMAC key can only be set to"), 0);
  } finally {
    do_check_true(thrown);
    thrown = false;
  }

  try {
    bundle.hmacKey = Utils.generateRandomBytes(15);
  } catch (ex) {
    thrown = true;
    do_check_eq(ex.message.indexOf("HMAC key must be at least 128"), 0);
  } finally {
    do_check_true(thrown);
    thrown = false;
  }

  run_next_test();
});

add_test(function test_repeated_hmac() {
  let testKey = "ababcdefabcdefabcdefabcdef";
  let k = Utils.makeHMACKey("foo");
  let one = sha256HMAC(Utils.decodeKeyBase32(testKey), k);
  let two = sha256HMAC(Utils.decodeKeyBase32(testKey), k);
  do_check_eq(one, two);

  run_next_test();
});

add_task(async function test_ensureLoggedIn() {
  let log = Log.repository.getLogger("Test");
  Log.repository.rootLogger.addAppender(new Log.DumpAppender());

  let identityConfig = makeIdentityConfig();
  let browseridManager = new BrowserIDManager();
  configureFxAccountIdentity(browseridManager, identityConfig);
  await browseridManager.ensureLoggedIn();

  let keyBundle = browseridManager.syncKeyBundle;

  /*
   * Build a test version of storage/crypto/keys.
   * Encrypt it with the sync key.
   * Pass it into the CollectionKeyManager.
   */

  log.info("Building storage keys...");
  let storage_keys = new CryptoWrapper("crypto", "keys");
  let default_key64 = Weave.Crypto.generateRandomKey();
  let default_hmac64 = Weave.Crypto.generateRandomKey();
  let bookmarks_key64 = Weave.Crypto.generateRandomKey();
  let bookmarks_hmac64 = Weave.Crypto.generateRandomKey();

  storage_keys.cleartext = {
    "default": [default_key64, default_hmac64],
    "collections": {"bookmarks": [bookmarks_key64, bookmarks_hmac64]},
  };
  storage_keys.modified = Date.now() / 1000;
  storage_keys.id = "keys";

  log.info("Encrypting storage keys...");

  // Use passphrase (sync key) itself to encrypt the key bundle.
  storage_keys.encrypt(keyBundle);

  // Sanity checking.
  do_check_true(null == storage_keys.cleartext);
  do_check_true(null != storage_keys.ciphertext);

  log.info("Updating collection keys.");

  // updateContents decrypts the object, releasing the payload for us to use.
  // Returns true, because the default key has changed.
  do_check_true(collectionKeys.updateContents(keyBundle, storage_keys));
  let payload = storage_keys.cleartext;

  _("CK: " + JSON.stringify(collectionKeys._collections));

  // Test that the CollectionKeyManager returns a similar WBO.
  let wbo = collectionKeys.asWBO("crypto", "keys");

  _("WBO: " + JSON.stringify(wbo));
  _("WBO cleartext: " + JSON.stringify(wbo.cleartext));

  // Check the individual contents.
  do_check_eq(wbo.collection, "crypto");
  do_check_eq(wbo.id, "keys");
  do_check_eq(undefined, wbo.modified);
  do_check_eq(collectionKeys.lastModified, storage_keys.modified);
  do_check_true(!!wbo.cleartext.default);
  do_check_keypair_eq(payload.default, wbo.cleartext.default);
  do_check_keypair_eq(payload.collections.bookmarks, wbo.cleartext.collections.bookmarks);

  do_check_true("bookmarks" in collectionKeys._collections);
  do_check_false("tabs" in collectionKeys._collections);

  _("Updating contents twice with the same data doesn't proceed.");
  storage_keys.encrypt(keyBundle);
  do_check_false(collectionKeys.updateContents(keyBundle, storage_keys));

  /*
   * Test that we get the right keys out when we ask for
   * a collection's tokens.
   */
  let b1 = new BulkKeyBundle("bookmarks");
  b1.keyPairB64 = [bookmarks_key64, bookmarks_hmac64];
  let b2 = collectionKeys.keyForCollection("bookmarks");
  do_check_keypair_eq(b1.keyPair, b2.keyPair);

  // Check key equality.
  do_check_true(b1.equals(b2));
  do_check_true(b2.equals(b1));

  b1 = new BulkKeyBundle("[default]");
  b1.keyPairB64 = [default_key64, default_hmac64];

  do_check_false(b1.equals(b2));
  do_check_false(b2.equals(b1));

  b2 = collectionKeys.keyForCollection(null);
  do_check_keypair_eq(b1.keyPair, b2.keyPair);

  /*
   * Checking for update times.
   */
  let info_collections = {};
  do_check_true(collectionKeys.updateNeeded(info_collections));
  info_collections["crypto"] = 5000;
  do_check_false(collectionKeys.updateNeeded(info_collections));
  info_collections["crypto"] = 1 + (Date.now() / 1000);              // Add one in case computers are fast!
  do_check_true(collectionKeys.updateNeeded(info_collections));

  collectionKeys.lastModified = null;
  do_check_true(collectionKeys.updateNeeded({}));

  /*
   * Check _compareKeyBundleCollections.
   */
  function newBundle(name) {
    let r = new BulkKeyBundle(name);
    r.generateRandom();
    return r;
  }
  let k1 = newBundle("k1");
  let k2 = newBundle("k2");
  let k3 = newBundle("k3");
  let k4 = newBundle("k4");
  let k5 = newBundle("k5");
  let coll1 = {"foo": k1, "bar": k2};
  let coll2 = {"foo": k1, "bar": k2};
  let coll3 = {"foo": k1, "bar": k3};
  let coll4 = {"foo": k4};
  let coll5 = {"baz": k5, "bar": k2};
  let coll6 = {};

  let d1 = collectionKeys._compareKeyBundleCollections(coll1, coll2); // []
  let d2 = collectionKeys._compareKeyBundleCollections(coll1, coll3); // ["bar"]
  let d3 = collectionKeys._compareKeyBundleCollections(coll3, coll2); // ["bar"]
  let d4 = collectionKeys._compareKeyBundleCollections(coll1, coll4); // ["bar", "foo"]
  let d5 = collectionKeys._compareKeyBundleCollections(coll5, coll2); // ["baz", "foo"]
  let d6 = collectionKeys._compareKeyBundleCollections(coll6, coll1); // ["bar", "foo"]
  let d7 = collectionKeys._compareKeyBundleCollections(coll5, coll5); // []
  let d8 = collectionKeys._compareKeyBundleCollections(coll6, coll6); // []

  do_check_true(d1.same);
  do_check_false(d2.same);
  do_check_false(d3.same);
  do_check_false(d4.same);
  do_check_false(d5.same);
  do_check_false(d6.same);
  do_check_true(d7.same);
  do_check_true(d8.same);

  do_check_array_eq(d1.changed, []);
  do_check_array_eq(d2.changed, ["bar"]);
  do_check_array_eq(d3.changed, ["bar"]);
  do_check_array_eq(d4.changed, ["bar", "foo"]);
  do_check_array_eq(d5.changed, ["baz", "foo"]);
  do_check_array_eq(d6.changed, ["bar", "foo"]);
});

function run_test() {
  // Only do 1,000 to avoid a 5-second pause in test runs.
  test_time_keyFromString(1000);

  run_next_test();
}
