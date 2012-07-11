/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/storageservice.js");

function run_test() {
  initTestLogging("Trace");

  run_next_test();
}

add_test(function test_bso_constructor() {
  _("Ensure created BSO instances are initialized properly.");

  let bso = new BasicStorageObject();
  do_check_eq(bso.id, null);
  do_check_eq(bso.collection, null);
  do_check_attribute_count(bso.data, 0);
  do_check_eq(bso.payload, null);
  do_check_eq(bso.modified, null);
  do_check_eq(bso.sortindex, 0);
  do_check_eq(bso.ttl, null);

  let bso = new BasicStorageObject("foobar");
  do_check_eq(bso.id, "foobar");
  do_check_eq(bso.collection, null);
  do_check_attribute_count(bso.data, 0);

  let bso = new BasicStorageObject("foo", "coll");
  do_check_eq(bso.id, "foo");
  do_check_eq(bso.collection, "coll");
  do_check_attribute_count(bso.data, 0);

  run_next_test();
});

add_test(function test_bso_attributes() {
  _("Ensure attribute getters and setters work.");

  let bso = new BasicStorageObject("foobar");
  bso.payload = "pay";
  do_check_eq(bso.payload, "pay");

  bso.modified = 35423;
  do_check_eq(bso.modified, 35423);

  bso.sortindex = 10;
  do_check_eq(bso.sortindex, 10);

  bso.ttl = 60;
  do_check_eq(bso.ttl, 60);

  run_next_test();
});

add_test(function test_bso_deserialize() {
  _("Ensure that deserialize() works.");

  _("A simple working test.");
  let json = '{"id": "foobar", "payload": "pay", "modified": 1223145532}';
  let bso = new BasicStorageObject();
  bso.deserialize(json);
  do_check_neq(bso, null);
  do_check_eq(bso.id, "foobar");
  do_check_eq(bso.payload, "pay");
  do_check_eq(bso.modified, 1223145532);

  _("Invalid JSON.");
  let json = '{id: "foobar}';
  let bso = new BasicStorageObject();
  try {
    bso.deserialize(json);
    do_check_true(false);
  } catch (ex) {
    do_check_eq(ex.name, "SyntaxError");
  }

  _("Invalid key in JSON.");
  let json = '{"id": "foo", "payload": "pay", "BADKEY": "irrelevant"}';
  let bso = new BasicStorageObject();
  try {
    bso.deserialize(json);
    do_check_true(false);
  } catch (ex) {
    do_check_eq(ex.name, "Error");
    do_check_eq(ex.message.indexOf("Invalid key"), 0);
  }

  _("Loading native JS objects works.");
  let bso = new BasicStorageObject();
  bso.deserialize({id: "foo", payload: "pay"});
  do_check_neq(bso, null);
  do_check_eq(bso.id, "foo");
  do_check_eq(bso.payload, "pay");

  _("Passing invalid type is caught.");
  let bso = new BasicStorageObject();
  try {
    bso.deserialize(["foo", "bar"]);
    do_check_true(false);
  } catch (ex) {
    do_check_eq(ex.name, "Error");
  }

  run_next_test();
});

add_test(function test_bso_toJSON() {
  _("Ensure JSON serialization works.");

  let bso = new BasicStorageObject();
  do_check_attribute_count(bso.toJSON(), 0);

  bso.id = "foo";
  bso.payload = "pay";
  let json = bso.toJSON();
  let original = json;

  do_check_attribute_count(original, 2);
  do_check_eq(original.id, "foo");
  do_check_eq(original.payload, "pay");

  run_next_test();
});
