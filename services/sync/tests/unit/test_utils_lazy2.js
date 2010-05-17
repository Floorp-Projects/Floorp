_("Make sure lazy2 function calling/assignment works");
Cu.import("resource://weave/util.js");

function run_test() {
  let count = 0;
  let Foo = function() {
    return ++count;
  }

  _("Make a thing instance of Foo but make sure it isn't initialized yet");
  let obj = {};
  Utils.lazy2(obj, "thing", Foo);
  do_check_eq(count, 0);

  _("Access the property to make it evaluates");
  do_check_eq(typeof obj.thing, "number");
  do_check_eq(count, 1);
  do_check_eq(obj.thing, 1);

  _("Additional accesses don't evaluate again (nothing should change");
  do_check_eq(typeof obj.thing, "number");
  do_check_eq(count, 1);
  do_check_eq(obj.thing, 1);

  _("More lazy properties will constrct more");
  do_check_eq(typeof obj.other, "undefined");
  Utils.lazy2(obj, "other", Foo);
  do_check_eq(typeof obj.other, "number");
  do_check_eq(count, 2);
  do_check_eq(obj.other, 2);

  _("Sanity check that the original property didn't change");
  do_check_eq(typeof obj.thing, "number");
  do_check_eq(count, 2);
  do_check_eq(obj.thing, 1);
}
