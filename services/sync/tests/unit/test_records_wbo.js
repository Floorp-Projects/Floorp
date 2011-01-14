Cu.import("resource://services-sync/auth.js");
Cu.import("resource://services-sync/base_records/wbo.js");
Cu.import("resource://services-sync/base_records/collection.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/util.js");


function test_fetch() {
  let record = {id: "asdf-1234-asdf-1234",
                modified: 2454725.98283,
                payload: JSON.stringify({cheese: "roquefort"})};
  let record2 = {id: "record2",
                 modified: 2454725.98284,
                 payload: JSON.stringify({cheese: "gruyere"})};
  let coll = [{id: "record2",
               modified: 2454725.98284,
               payload: JSON.stringify({cheese: "gruyere"})}];

  _("Setting up server.");
  let server = httpd_setup({
    "/record":  httpd_handler(200, "OK", JSON.stringify(record)),
    "/record2": httpd_handler(200, "OK", JSON.stringify(record2)),
    "/coll":    httpd_handler(200, "OK", JSON.stringify(coll))
  });
  do_test_pending();

  try {
    _("Fetching a WBO record");
    let rec = new WBORecord("coll", "record");
    rec.fetch("http://localhost:8080/record");
    do_check_eq(rec.id, "asdf-1234-asdf-1234"); // NOT "record"!

    do_check_eq(rec.modified, 2454725.98283);
    do_check_eq(typeof(rec.payload), "object");
    do_check_eq(rec.payload.cheese, "roquefort");

    _("Fetching a WBO record using the record manager");
    let rec2 = Records.get("http://localhost:8080/record2");
    do_check_eq(rec2.id, "record2");
    do_check_eq(rec2.modified, 2454725.98284);
    do_check_eq(typeof(rec2.payload), "object");
    do_check_eq(rec2.payload.cheese, "gruyere");
    do_check_eq(Records.response.status, 200);

    // Testing collection extraction.
    _("Extracting collection.");
    let rec3 = new WBORecord("tabs", "foo");   // Create through constructor.
    do_check_eq(rec3.collection, "tabs");

  } finally {
    server.stop(do_test_finished);
  }
}

function run_test() {
  initTestLogging("Trace");
  test_fetch();
}
