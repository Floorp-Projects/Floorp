Cu.import("resource://services-sync/base_records/crypto.js");
Cu.import("resource://services-sync/base_records/keys.js");
Cu.import("resource://services-sync/auth.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/util.js");

let keys, cryptoMeta, cryptoWrap;

function pubkey_handler(metadata, response) {
  let obj = {id: "pubkey",
             modified: keys.pubkey.modified,
             payload: JSON.stringify(keys.pubkey.payload)};
  return httpd_basic_auth_handler(JSON.stringify(obj), metadata, response);
}

function privkey_handler(metadata, response) {
  let obj = {id: "privkey",
             modified: keys.privkey.modified,
             payload: JSON.stringify(keys.privkey.payload)};
  return httpd_basic_auth_handler(JSON.stringify(obj), metadata, response);
}

function crypted_resource_handler(metadata, response) {
  let obj = {id: "resource",
             modified: cryptoWrap.modified,
             payload: JSON.stringify(cryptoWrap.payload)};
  return httpd_basic_auth_handler(JSON.stringify(obj), metadata, response);
}

function crypto_meta_handler(metadata, response) {
  let obj = {id: "steam",
             modified: cryptoMeta.modified,
             payload: JSON.stringify(cryptoMeta.payload)};
  return httpd_basic_auth_handler(JSON.stringify(obj), metadata, response);
}

function run_test() {
  let server;
  do_test_pending();

  let passphrase = ID.set("WeaveCryptoID", new Identity());
  passphrase.password = "passphrase";

  try {
    let log = Log4Moz.repository.getLogger("Test");
    Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

    log.info("Setting up server and authenticator");

    server = httpd_setup({"/keys/pubkey": pubkey_handler,
                          "/keys/privkey": privkey_handler,
                          "/steam/resource": crypted_resource_handler,
                          "/crypto/steam": crypto_meta_handler});

    let auth = new BasicAuthenticator(new Identity("secret", "guest", "guest"));
    Auth.defaultAuthenticator = auth;

    log.info("Generating keypair + symmetric key");

    PubKeys.defaultKeyUri = "http://localhost:8080/keys/pubkey";
    keys = PubKeys.createKeypair(passphrase,
                                 "http://localhost:8080/keys/pubkey",
                                 "http://localhost:8080/keys/privkey");
    let crypto = Svc.Crypto;
    keys.symkey = crypto.generateRandomKey();
    keys.wrappedkey = crypto.wrapSymmetricKey(keys.symkey, keys.pubkey.keyData);

    log.info("Setting up keyring");

    cryptoMeta = new CryptoMeta("http://localhost:8080/crypto/steam", auth);
    cryptoMeta.addUnwrappedKey(keys.pubkey, keys.symkey);
    CryptoMetas.set(cryptoMeta.uri, cryptoMeta);

    log.info("Creating a record");

    let cryptoUri = "http://localhost:8080/crypto/steam";
    cryptoWrap = new CryptoWrapper("http://localhost:8080/steam/resource");
    cryptoWrap.encryption = cryptoUri;
    do_check_eq(cryptoWrap.encryption, cryptoUri);
    do_check_eq(cryptoWrap.payload.encryption, "../crypto/steam");

    log.info("Encrypting a record");

    cryptoWrap.cleartext.stuff = "my payload here";
    cryptoWrap.encrypt(passphrase);
    let firstIV = cryptoWrap.IV;

    log.info("Decrypting the record");

    let payload = cryptoWrap.decrypt(passphrase, cryptoUri);
    do_check_eq(payload.stuff, "my payload here");
    do_check_neq(payload, cryptoWrap.payload); // wrap.data.payload is the encrypted one

    log.info("Re-encrypting the record with alternate payload");

    cryptoWrap.cleartext.stuff = "another payload";
    cryptoWrap.encrypt(passphrase);
    let secondIV = cryptoWrap.IV;
    payload = cryptoWrap.decrypt(passphrase, cryptoUri);
    do_check_eq(payload.stuff, "another payload");

    log.info("Make sure multiple encrypts use different IVs");
    do_check_neq(firstIV, secondIV);

    log.info("Make sure differing ids cause failures");
    cryptoWrap.encrypt(passphrase);
    cryptoWrap.data.id = "other";
    let error = "";
    try {
      cryptoWrap.decrypt(passphrase, cryptoUri);
    }
    catch(ex) {
      error = ex;
    }
    do_check_eq(error, "Record id mismatch: resource,other");

    log.info("Make sure wrong hmacs cause failures");
    cryptoWrap.encrypt(passphrase);
    cryptoWrap.hmac = "foo";
    error = "";
    try {
      cryptoWrap.decrypt(passphrase, cryptoUri);
    }
    catch(ex) {
      error = ex;
    }
    do_check_eq(error, "Record SHA256 HMAC mismatch: foo");

    log.info("Done!");
  }
  finally {
    server.stop(do_test_finished);
  }
}
