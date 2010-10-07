Cu.import("resource://services-sync/base_records/keys.js");
Cu.import("resource://services-sync/auth.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/util.js");

function pubkey_handler(metadata, response) {
  let obj = {id: "asdf-1234-asdf-1234",
             modified: "2454725.98283",
             payload: JSON.stringify({type: "pubkey",
                                   privateKeyUri: "http://localhost:8080/privkey",
                                   keyData: "asdfasdfasf..."})};
  return httpd_basic_auth_handler(JSON.stringify(obj), metadata, response);
}

function privkey_handler(metadata, response) {
  let obj = {id: "asdf-1234-asdf-1234-2",
             modified: "2454725.98283",
             payload: JSON.stringify({type: "privkey",
                                   publicKeyUri: "http://localhost:8080/pubkey",
                                   keyData: "asdfasdfasf..."})};
  return httpd_basic_auth_handler(JSON.stringify(obj), metadata, response);
}

function test_get() {
  let log = Log4Moz.repository.getLogger("Test");
  Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

  log.info("Setting up authenticator");

  let auth = new BasicAuthenticator(new Identity("secret", "guest", "guest"));
  Auth.defaultAuthenticator = auth;

  log.info("Getting a public key");

  let pubkey = PubKeys.get("http://localhost:8080/pubkey");
  do_check_eq(pubkey.data.payload.type, "pubkey");
  do_check_eq(PubKeys.response.status, 200);

  log.info("Getting matching private key");

  let privkey = PrivKeys.get(pubkey.privateKeyUri);
  do_check_eq(privkey.data.payload.type, "privkey");
  do_check_eq(PrivKeys.response.status, 200);

  log.info("Done!");
}


function test_createKeypair() {
  let passphrase = "moneyislike$\u20ac\u00a5\u5143";
  let id = ID.set('foo', new Identity('foo', 'luser'));
  id.password = passphrase;

  _("Key pair requires URIs for both keys.");
  let error;
  try {
    let result = PubKeys.createKeypair(id);
  } catch(ex) {
    error = ex;
  }
  do_check_eq(error, "Missing or null parameter 'pubkeyUri'.");

  error = undefined;
  try {
    let result = PubKeys.createKeypair(id, "http://host/pub/key");
  } catch(ex) {
    error = ex;
  }
  do_check_eq(error, "Missing or null parameter 'privkeyUri'.");

  _("Generate a key pair.");
  let result = PubKeys.createKeypair(id, "http://host/pub/key", "http://host/priv/key");

  _("Check that salt and IV are of correct length.");
  // 16 bytes = 24 base64 encoded characters
  do_check_eq(result.privkey.salt.length, 24);
  do_check_eq(result.privkey.iv.length, 24);

  _("URIs are set.");
  do_check_eq(result.pubkey.uri.spec, "http://host/pub/key");
  do_check_eq(result.pubkey.privateKeyUri.spec, "http://host/priv/key");
  do_check_eq(result.pubkey.payload.privateKeyUri, "../priv/key");

  do_check_eq(result.privkey.uri.spec, "http://host/priv/key");
  do_check_eq(result.privkey.publicKeyUri.spec, "http://host/pub/key");
  do_check_eq(result.privkey.payload.publicKeyUri, "../pub/key");

  _("UTF8 encoded passphrase was used.");
  do_check_true(Svc.Crypto.verifyPassphrase(result.privkey.keyData,
                                            Utils.encodeUTF8(passphrase),
                                            result.privkey.salt,
                                            result.privkey.payload.iv));
}

function run_test() {
  do_test_pending();
  let server;
  try {
    server = httpd_setup({"/pubkey": pubkey_handler,
                          "/privkey": privkey_handler});

    test_get();
    test_createKeypair();
  } finally {
    server.stop(do_test_finished);
  }
}
