try {
  Cu.import("resource://weave/log4moz.js");
  Cu.import("resource://weave/util.js");
  Cu.import("resource://weave/auth.js");
  Cu.import("resource://weave/identity.js");
  Cu.import("resource://weave/base_records/keys.js");
} catch (e) { do_throw(e); }

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

function run_test() {
  let server;

  try {
    let log = Log4Moz.repository.getLogger();
    Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

    log.info("Setting up server and authenticator");

    server = httpd_setup({"/pubkey": pubkey_handler,
                          "/privkey": privkey_handler});

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
  catch (e) { do_throw(e); }
  finally { server.stop(function() {}); }
}
