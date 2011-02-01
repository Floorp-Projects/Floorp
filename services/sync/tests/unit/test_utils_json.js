_("Make sure json saves and loads from disk");
Cu.import("resource://services-sync/util.js");

function run_test() {
  do_test_pending();
  Utils.asyncChain(function (next) {

    _("Do a simple write of an array to json and read");
    Utils.jsonSave("foo", {}, ["v1", "v2"], ensureThrows(function() {
      Utils.jsonLoad("foo", {}, ensureThrows(function(val) {
        let foo = val;
        do_check_eq(typeof foo, "object");
        do_check_eq(foo.length, 2);
        do_check_eq(foo[0], "v1");
        do_check_eq(foo[1], "v2");
        next();
      }));
    }));

  }, function (next) {

    _("Try saving simple strings");
    Utils.jsonSave("str", {}, "hi", ensureThrows(function() {
      Utils.jsonLoad("str", {}, ensureThrows(function(val) {
        let str = val;
        do_check_eq(typeof str, "string");
        do_check_eq(str.length, 2);
        do_check_eq(str[0], "h");
        do_check_eq(str[1], "i");
        next();
      }));
    }));

  }, function (next) {

    _("Try saving a number");
    Utils.jsonSave("num", {}, 42, ensureThrows(function() {
      Utils.jsonLoad("num", {}, ensureThrows(function(val) {
        let num = val;
        do_check_eq(typeof num, "number");
        do_check_eq(num, 42);
        next();
      }));
    }));

  }, function (next) {

    _("Try loading a non-existent file.");
    Utils.jsonLoad("non-existent", {}, ensureThrows(function(val) {
      do_check_eq(val, undefined);
      next();
    }));
    
  }, function (next) {

    _("Verify that writes are logged.");
    let trace;
    Utils.jsonSave("log", {_log: {trace: function(msg) { trace = msg; }}},
                   "hi", ensureThrows(function () {
      do_check_true(!!trace);
      next();
    }));

  }, function (next) {

    _("Verify that reads and read errors are logged.");

    // Write a file with some invalid JSON
    let file = Utils.getProfileFile({ autoCreate: true,
                                      path: "weave/log.json" });
    let [fos] = Utils.open(file, ">");
    fos.writeString("invalid json!");
    fos.close();

    let trace, debug;
    Utils.jsonLoad("log",
                   {_log: {trace: function(msg) { trace = msg; },
                           debug: function(msg) { debug = msg; }}},
                   ensureThrows(function(val) {
      do_check_true(!!trace);
      do_check_true(!!debug);
      next();
    }));

  }, do_test_finished)();

}
