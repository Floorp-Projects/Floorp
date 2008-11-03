try {
  Cu.import("resource://weave/log4moz.js");
  Cu.import("resource://weave/util.js");
  Cu.import("resource://weave/async.js");
  Cu.import("resource://weave/auth.js");
  Cu.import("resource://weave/identity.js");
  Cu.import("resource://weave/base_records/keys.js");
} catch (e) {
  do_throw(e);
}
Function.prototype.async = Async.sugar;

let json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);

function pubkey_handler(metadata, response) {
  let obj = {modified: "2454725.98283",
             payload: {type: "pubkey",
                       private_key: "http://localhost:8080/privkey",
                       key_data: "asdfasdfasf..."}};
  return httpd_basic_auth_handler(json.encode(obj), metadata, response);
}

function privkey_handler(metadata, response) {
  let obj = {modified: "2454725.98283",
             payload: {type: "privkey",
                       public_key: "http://localhost:8080/pubkey",
                       key_data: "asdfasdfasf..."}};
  let json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
  return httpd_basic_auth_handler(json.encode(obj), metadata, response);
}

function async_test() {
  let self = yield;
  let server;

  try {
    let log = Log4Moz.repository.getLogger();
    Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

    let auth = new BasicAuthenticator(new Identity("secret", "guest", "guest"));
    Auth.defaultAuthenticator = auth;
    server = httpd_setup({"/pubkey": pubkey_handler,
                          "/privkey": privkey_handler});

    let pubkey = yield PubKeys.get(self.cb, "http://localhost:8080/pubkey");
    do_check_eq(pubkey.data.payload.type, "pubkey");
    do_check_eq(pubkey.lastRequest.status, 200);

    let privkey = yield PrivKeys.get(self.cb, pubkey.privateKeyUri);
    do_check_eq(privkey.data.payload.type, "privkey");
    do_check_eq(privkey.lastRequest.status, 200);

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
