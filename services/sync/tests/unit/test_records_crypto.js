try {
  Cu.import("resource://weave/log4moz.js");
  Cu.import("resource://weave/util.js");
  Cu.import("resource://weave/auth.js");
  Cu.import("resource://weave/identity.js");
  Cu.import("resource://weave/base_records/keys.js");
  Cu.import("resource://weave/base_records/crypto.js");
} catch (e) {
  do_throw(e);
}

let keys, cryptoMeta, cryptoWrap;

function pubkey_handler(metadata, response) {
  let obj = {id: "ignore-me",
             modified: keys.pubkey.modified,
             payload: JSON.stringify(keys.pubkey.payload)};
  return httpd_basic_auth_handler(JSON.stringify(obj), metadata, response);
}

function privkey_handler(metadata, response) {
  let obj = {id: "ignore-me-2",
             modified: keys.privkey.modified,
             payload: JSON.stringify(keys.privkey.payload)};
  return httpd_basic_auth_handler(JSON.stringify(obj), metadata, response);
}

function crypted_resource_handler(metadata, response) {
  let obj = {id: "ignore-me-3",
             modified: cryptoWrap.modified,
             payload: JSON.stringify(cryptoWrap.payload)};
  return httpd_basic_auth_handler(JSON.stringify(obj), metadata, response);
}

function crypto_meta_handler(metadata, response) {
  let obj = {id: "ignore-me-4",
             modified: cryptoMeta.modified,
             payload: JSON.stringify(cryptoMeta.payload)};
  return httpd_basic_auth_handler(JSON.stringify(obj), metadata, response);
}

function run_test() {
  let server;

  try {
    let log = Log4Moz.repository.getLogger();
    Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

    log.info("Setting up server and authenticator");

    server = httpd_setup({"/pubkey": pubkey_handler,
                          "/privkey": privkey_handler,
                          "/crypted-resource": crypted_resource_handler,
                          "/crypto-meta": crypto_meta_handler});

    let auth = new BasicAuthenticator(new Identity("secret", "guest", "guest"));
    Auth.defaultAuthenticator = auth;

    log.info("Generating keypair + symmetric key");

    PubKeys.defaultKeyUri = "http://localhost:8080/pubkey";
    keys = PubKeys.createKeypair("my passphrase",
                                 "http://localhost:8080/pubkey",
                                 "http://localhost:8080/privkey");
    let crypto = Cc["@labs.mozilla.com/Weave/Crypto;1"].
                 getService(Ci.IWeaveCrypto);
    keys.symkey = crypto.generateRandomKey();
    keys.wrappedkey = crypto.wrapSymmetricKey(keys.symkey, keys.pubkey.keyData);

    log.info("Setting up keyring");

    cryptoMeta = new CryptoMeta("http://localhost:8080/crypto-meta", auth);
    cryptoMeta.generateIV();
    cryptoMeta.addUnwrappedKey(keys.pubkey, keys.symkey);

    log.info("Creating and encrypting a record");

    cryptoWrap = new CryptoWrapper("http://localhost:8080/crypted-resource", auth);
    cryptoWrap.encryption = "http://localhost:8080/crypto-meta";
    cryptoWrap.cleartext = "my payload here";
    cryptoWrap.encrypt("my passphrase");

    log.info("Decrypting the record");

    let payload = cryptoWrap.decrypt("my passphrase");
    do_check_eq(payload, "my payload here");
    do_check_neq(payload, cryptoWrap.payload); // wrap.data.payload is the encrypted one

    log.info("Re-encrypting the record with alternate payload");

    cryptoWrap.cleartext = "another payload";
    cryptoWrap.encrypt("my passphrase");
    payload = cryptoWrap.decrypt("my passphrase");
    do_check_eq(payload, "another payload");

    log.info("Done!");
  }
  catch (e) { do_throw(e); }
  finally { server.stop(); }
}
