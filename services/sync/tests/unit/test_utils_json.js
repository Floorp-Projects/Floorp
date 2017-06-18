/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://services-sync/util.js");

function run_test() {
  initTestLogging();
  run_next_test();
}

add_test(function test_roundtrip() {
  _("Do a simple write of an array to json and read");
  Utils.jsonSave("foo", {}, ["v1", "v2"], ensureThrows(function(error) {
    do_check_eq(error, null);

    Utils.jsonLoad("foo", {}, ensureThrows(function(val) {
      let foo = val;
      do_check_eq(typeof foo, "object");
      do_check_eq(foo.length, 2);
      do_check_eq(foo[0], "v1");
      do_check_eq(foo[1], "v2");
      run_next_test();
    }));
  }));
});

add_test(function test_string() {
  _("Try saving simple strings");
  Utils.jsonSave("str", {}, "hi", ensureThrows(function(error) {
    do_check_eq(error, null);

    Utils.jsonLoad("str", {}, ensureThrows(function(val) {
      let str = val;
      do_check_eq(typeof str, "string");
      do_check_eq(str.length, 2);
      do_check_eq(str[0], "h");
      do_check_eq(str[1], "i");
      run_next_test();
    }));
  }));
});

add_test(function test_number() {
  _("Try saving a number");
  Utils.jsonSave("num", {}, 42, ensureThrows(function(error) {
    do_check_eq(error, null);

    Utils.jsonLoad("num", {}, ensureThrows(function(val) {
      let num = val;
      do_check_eq(typeof num, "number");
      do_check_eq(num, 42);
      run_next_test();
    }));
  }));
});

add_test(function test_nonexistent_file() {
  _("Try loading a non-existent file.");
  Utils.jsonLoad("non-existent", {}, ensureThrows(function(val) {
    do_check_eq(val, undefined);
    run_next_test();
  }));
});

add_test(function test_save_logging() {
  _("Verify that writes are logged.");
  let trace;
  Utils.jsonSave("log", {_log: {trace(msg) { trace = msg; }}},
                       "hi", ensureThrows(function() {
    do_check_true(!!trace);
    run_next_test();
  }));
});

add_test(function test_load_logging() {
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
  Utils.jsonLoad("log", obj, ensureThrows(function(val) {
    do_check_true(!val);
    do_check_true(!!trace);
    do_check_true(!!debug);
    run_next_test();
  }));
});

add_task(async function test_undefined_callback() {
  await Utils.jsonSave("foo", {}, ["v1", "v2"]);
});
