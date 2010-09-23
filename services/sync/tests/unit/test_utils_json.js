_("Make sure json saves and loads from disk");
Cu.import("resource://services-sync/util.js");

function run_test() {
  _("Do a simple write of an array to json and read");
  let foo;
  Utils.jsonSave("foo", {}, ["v1", "v2"]);
  Utils.jsonLoad("foo", {}, function(val) {
    foo = val;
  });
  do_check_eq(typeof foo, "object");
  do_check_eq(foo.length, 2);
  do_check_eq(foo[0], "v1");
  do_check_eq(foo[1], "v2");

  _("Use the function callback version of jsonSave");
  let bar;
  Utils.jsonSave("bar", {}, function() ["v1", "v2"]);
  Utils.jsonLoad("bar", {}, function(val) {
    bar = val;
  });
  do_check_eq(typeof bar, "object");
  do_check_eq(bar.length, 2);
  do_check_eq(bar[0], "v1");
  do_check_eq(bar[1], "v2");

  _("Try saving simple strings");
  let str;
  Utils.jsonSave("str", {}, "hi");
  Utils.jsonLoad("str", {}, function(val) {
    str = val;
  });
  do_check_eq(typeof str, "string");
  do_check_eq(str.length, 2);
  do_check_eq(str[0], "h");
  do_check_eq(str[1], "i");

  _("Try saving a number");
  let num;
  Utils.jsonSave("num", {}, function() 42);
  Utils.jsonLoad("num", {}, function(val) {
    num = val;
  });
  do_check_eq(typeof num, "number");
  do_check_eq(num, 42);

  _("Verify that things get logged");
  let trace, debug;
  Utils.jsonSave("str",
                 {_log: {trace: function(msg) { trace = msg; }}},
                 "hi");
  do_check_true(!!trace);
  trace = undefined;
  Utils.jsonLoad("str",
                 {_log: {trace: function(msg) { trace = msg; },
                         debug: function(msg) { debug = msg; }}},
                 function(val) { throw "exception"; });
  do_check_true(!!trace);
  do_check_true(!!debug);
}
