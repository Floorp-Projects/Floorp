_("Make sure various combinations of deferGetSet arguments correctly defer getting/setting properties to another object");
Cu.import("resource://services-sync/util.js");

function run_test() {
  let base = function() {};
  base.prototype = {
    dst: {},

    get a() {
      return "a";
    },
    set b(val) {
      this.dst.b = val + "!!!";
    }
  };
  let src = new base();

  _("get/set a single property");
  Utils.deferGetSet(base, "dst", "foo");
  src.foo = "bar";
  do_check_eq(src.dst.foo, "bar");
  do_check_eq(src.foo, "bar");

  _("editing the target also updates the source");
  src.dst.foo = "baz";
  do_check_eq(src.dst.foo, "baz");
  do_check_eq(src.foo, "baz");

  _("handle multiple properties");
  Utils.deferGetSet(base, "dst", ["p1", "p2"]);
  src.p1 = "v1";
  src.p2 = "v2";
  do_check_eq(src.p1, "v1");
  do_check_eq(src.dst.p1, "v1");
  do_check_eq(src.p2, "v2");
  do_check_eq(src.dst.p2, "v2");

  _("make sure existing getter keeps its functionality");
  Utils.deferGetSet(base, "dst", "a");
  src.a = "not a";
  do_check_eq(src.dst.a, "not a");
  do_check_eq(src.a, "a");

  _("make sure existing setter keeps its functionality");
  Utils.deferGetSet(base, "dst", "b");
  src.b = "b";
  do_check_eq(src.dst.b, "b!!!");
  do_check_eq(src.b, "b!!!");
}
