try {
  Cu.import("resource://weave/log4moz.js");
  Cu.import("resource://weave/util.js");
  Cu.import("resource://weave/async.js");
  Cu.import("resource://weave/auth.js");
  Cu.import("resource://weave/identity.js");
  Cu.import("resource://weave/base_records/wbo.js");
} catch (e) { do_throw(e); }

Function.prototype.async = Async.sugar;

let json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);

function record_handler(metadata, response) {
  let obj = {id: "asdf-1234-asdf-1234",
             modified: "2454725.98283",
             payload: json.encode({cheese: "roquefort"})};
  return httpd_basic_auth_handler(json.encode(obj), metadata, response);
}

function async_test() {
  let self = yield;
  let server;

  try {
    let log = Log4Moz.repository.getLogger('Test');
    Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

    log.info("Setting up server and authenticator");

    server = httpd_setup({"/record": record_handler});

    let auth = new BasicAuthenticator(new Identity("secret", "guest", "guest"));
    Auth.defaultAuthenticator = auth;

    log.info("Getting a WBO record");

    let res = new WBORecord("http://localhost:8080/record");
    let rec = yield res.get(self.cb);
    do_check_eq(rec.id, "record"); // NOT "asdf-1234-asdf-1234"!
    do_check_eq(rec.modified, 2454725.98283);
    do_check_eq(typeof(rec.payload), "object");
    do_check_eq(rec.payload.cheese, "roquefort");
    do_check_eq(res.lastRequest.status, 200);

    log.info("Done!");
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
