try {
  Cu.import("resource://weave/log4moz.js");
  Cu.import("resource://weave/util.js");
  Cu.import("resource://weave/async.js");
  Cu.import("resource://weave/auth.js");
  Cu.import("resource://weave/identity.js");
  Cu.import("resource://weave/base_records/keys.js");
  Cu.import("resource://weave/base_records/crypto.js");
} catch (e) {
  do_throw(e);
}
Function.prototype.async = Async.sugar;

let jsonSvc, cryptoSvc, salt, iv, symKey, wrappedKey, pubKey, privKey;

function pubkey_handler(metadata, response) {
  let obj = {modified: "2454725.98283",
             payload: {type: "pubkey",
                       private_key: "http://localhost:8080/privkey",
                       key_data: pubKey}};
  return httpd_basic_auth_handler(jsonSvc.encode(obj), metadata, response);
}

function privkey_handler(metadata, response) {
  let obj = {modified: "2454725.98283",
             payload: {type: "privkey",
                       public_key: "http://localhost:8080/pubkey",
                       key_data: privKey}};
  return httpd_basic_auth_handler(jsonSvc.encode(obj), metadata, response);
}

function crypted_resource_handler(metadata, response) {
  let obj = {modified: "2454725.98283",
             encryption: "http://localhost:8080/crypto-meta",
             payload: cryptoSvc.encrypt("my payload here", symKey, iv)};
  return httpd_basic_auth_handler(jsonSvc.encode(obj), metadata, response);
}

function crypto_meta_handler(metadata, response) {
  let obj = {modified: "2454725.98283",
             payload: {type: "crypto-meta",
                       salt: salt,
                       iv: iv,
                       keyring: {
                         "pubkey": wrappedKey
                       }}};
  return httpd_basic_auth_handler(jsonSvc.encode(obj), metadata, response);
}

function async_test() {
  let self = yield;
  let server;

  try {
    let log = Log4Moz.repository.getLogger();
    Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

    jsonSvc = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
    cryptoSvc = Cc["@labs.mozilla.com/Weave/Crypto;1"].
      getService(Ci.IWeaveCrypto);

    let auth = new BasicAuthenticator(new Identity("secret", "guest", "guest"));
    Auth.defaultAuthenticator = auth;
    PubKeys.defaultKeyUri = "http://localhost:8080/pubkey";

    server = httpd_setup({"/pubkey": pubkey_handler,
                          "/privkey": privkey_handler,
                          "/crypted-resource": crypted_resource_handler,
                          "/crypto-meta": crypto_meta_handler});

    salt = cryptoSvc.generateRandomBytes(16);
    iv = cryptoSvc.generateRandomIV();

    let pubOut = {}, privOut = {};
    cryptoSvc.generateKeypair("my passphrase", salt, iv, pubOut, privOut);
    pubKey = pubOut.value;
    privKey = privOut.value;

    symKey = cryptoSvc.generateRandomKey();
    wrappedKey = cryptoSvc.wrapSymmetricKey(symKey, pubKey);

    let wrap = new CryptoWrapper("http://localhost:8080/crypted-resource", auth);
    yield wrap.get(self.cb);

    let payload = yield wrap.decrypt(self.cb, "my passphrase");
    do_check_eq(payload, "my payload here");
    do_check_neq(payload, wrap.data.payload); // wrap.data.payload is the encrypted one

    wrap.cleartext = "another payload";
    yield wrap.encrypt(self.cb, "my passphrase");
    payload = yield wrap.decrypt(self.cb, "my passphrase");
    do_check_eq(payload, "another payload");

    do_test_finished();
  }
  catch (e) { do_throw(e); }
  finally { server.stop(); }

  self.done();
}

function run_test() {
  async_test.async(this);
  do_test_pending();
}
