try {
  Cu.import("resource://services-sync/auth.js");
  Cu.import("resource://services-sync/base_records/wbo.js");
  Cu.import("resource://services-sync/base_records/collection.js");
  Cu.import("resource://services-sync/identity.js");
  Cu.import("resource://services-sync/log4moz.js");
  Cu.import("resource://services-sync/resource.js");
  Cu.import("resource://services-sync/util.js");
} catch (e) { do_throw(e); }

function record_handler(metadata, response) {
  let obj = {id: "asdf-1234-asdf-1234",
             modified: 2454725.98283,
             payload: JSON.stringify({cheese: "roquefort"})};
  return httpd_basic_auth_handler(JSON.stringify(obj), metadata, response);
}

function record_handler2(metadata, response) {
  let obj = {id: "record2",
             modified: 2454725.98284,
             payload: JSON.stringify({cheese: "gruyere"})};
  return httpd_basic_auth_handler(JSON.stringify(obj), metadata, response);
}

function coll_handler(metadata, response) {
  let obj = [{id: "record2",
              modified: 2454725.98284,
              payload: JSON.stringify({cheese: "gruyere"})}];
  return httpd_basic_auth_handler(JSON.stringify(obj), metadata, response);
}

function run_test() {
  let server;
  do_test_pending();

  try {
    let log = Log4Moz.repository.getLogger('Test');
    Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

    log.info("Setting up server and authenticator");

    server = httpd_setup({"/record": record_handler,
                          "/record2": record_handler2,
                          "/coll": coll_handler});

    let auth = new BasicAuthenticator(new Identity("secret", "guest", "guest"));
    Auth.defaultAuthenticator = auth;

    log.info("Getting a WBO record");

    let res = new Resource("http://localhost:8080/record");
    let resp = res.get();

    let rec = new WBORecord("http://localhost:8080/record");
    rec.deserialize(res.data);
    do_check_eq(rec.id, "asdf-1234-asdf-1234"); // NOT "record"!

    rec.uri = res.uri;
    do_check_eq(rec.id, "record"); // NOT "asdf-1234-asdf-1234"!

    do_check_eq(rec.modified, 2454725.98283);
    do_check_eq(typeof(rec.payload), "object");
    do_check_eq(rec.payload.cheese, "roquefort");
    do_check_eq(resp.status, 200);

    log.info("Getting a WBO record using the record manager");

    let rec2 = Records.get("http://localhost:8080/record2");
    do_check_eq(rec2.id, "record2");
    do_check_eq(rec2.modified, 2454725.98284);
    do_check_eq(typeof(rec2.payload), "object");
    do_check_eq(rec2.payload.cheese, "gruyere");
    do_check_eq(Records.response.status, 200);

    log.info("Done!");
  }
  catch (e) { do_throw(e); }
  finally { server.stop(do_test_finished); }
}
