/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");


function test_toJSON() {
  _("Create a record, for now without a TTL.");
  let wbo = new WBORecord("coll", "a_record");
  wbo.modified = 12345;
  wbo.sortindex = 42;
  wbo.payload = {};

  _("Verify that the JSON representation contains the WBO properties, but not TTL.");
  let json = JSON.parse(JSON.stringify(wbo));
  do_check_eq(json.modified, 12345);
  do_check_eq(json.sortindex, 42);
  do_check_eq(json.payload, "{}");
  do_check_false("ttl" in json);

  _("Set a TTL, make sure it's present in the JSON representation.");
  wbo.ttl = 30 * 60;
  json = JSON.parse(JSON.stringify(wbo));
  do_check_eq(json.ttl, 30 * 60);
}


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
    rec.fetch(Service.resource(server.baseURI + "/record"));
    do_check_eq(rec.id, "asdf-1234-asdf-1234"); // NOT "record"!

    do_check_eq(rec.modified, 2454725.98283);
    do_check_eq(typeof(rec.payload), "object");
    do_check_eq(rec.payload.cheese, "roquefort");

    _("Fetching a WBO record using the record manager");
    let rec2 = Service.recordManager.get(server.baseURI + "/record2");
    do_check_eq(rec2.id, "record2");
    do_check_eq(rec2.modified, 2454725.98284);
    do_check_eq(typeof(rec2.payload), "object");
    do_check_eq(rec2.payload.cheese, "gruyere");
    do_check_eq(Service.recordManager.response.status, 200);

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

  test_toJSON();
  test_fetch();
}
