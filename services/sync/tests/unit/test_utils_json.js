/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://services-sync/util.js");

initTestLogging();

add_task(async function test_roundtrip() {
  _("Do a simple write of an array to json and read");
  await Utils.jsonSave("foo", {}, ["v1", "v2"]);

  let foo = await Utils.jsonLoad("foo", {});
  do_check_eq(typeof foo, "object");
  do_check_eq(foo.length, 2);
  do_check_eq(foo[0], "v1");
  do_check_eq(foo[1], "v2");
});

add_task(async function test_string() {
  _("Try saving simple strings");
  await Utils.jsonSave("str", {}, "hi");

  let str = await Utils.jsonLoad("str", {});
  do_check_eq(typeof str, "string");
  do_check_eq(str.length, 2);
  do_check_eq(str[0], "h");
  do_check_eq(str[1], "i");
});

add_task(async function test_number() {
  _("Try saving a number");
  await Utils.jsonSave("num", {}, 42);

  let num = await Utils.jsonLoad("num", {});
  do_check_eq(typeof num, "number");
  do_check_eq(num, 42);
});

add_task(async function test_nonexistent_file() {
  _("Try loading a non-existent file.");
  let val = await Utils.jsonLoad("non-existent", {});
  do_check_eq(val, undefined);
});

add_task(async function test_save_logging() {
  _("Verify that writes are logged.");
  let trace;
  await Utils.jsonSave("log", {_log: {trace(msg) { trace = msg; }}}, "hi");
  do_check_true(!!trace);
});

add_task(async function test_load_logging() {
  _("Verify that reads and read errors are logged.");

  // Write a file with some invalid JSON
  let filePath = "weave/log.json";
  let file = FileUtils.getFile("ProfD", filePath.split("/"), true);
  let fos = Cc["@mozilla.org/network/file-output-stream;1"]
              .createInstance(Ci.nsIFileOutputStream);
  let flags = FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE
              | FileUtils.MODE_TRUNCATE;
  fos.init(file, flags, FileUtils.PERMS_FILE, fos.DEFER_OPEN);
  let stream = Cc["@mozilla.org/intl/converter-output-stream;1"]
                 .createInstance(Ci.nsIConverterOutputStream);
  stream.init(fos, "UTF-8");
  stream.writeString("invalid json!");
  stream.close();

  let trace, debug;
  let obj = {
    _log: {
      trace(msg) {
        trace = msg;
      },
      debug(msg) {
        debug = msg;
      }
    }
  };
  let val = await Utils.jsonLoad("log", obj);
  do_check_true(!val);
  do_check_true(!!trace);
  do_check_true(!!debug);
});
