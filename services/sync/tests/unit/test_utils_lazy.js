_("Make sure lazy constructor calling/assignment works");
Cu.import("resource://services-sync/util.js");

function run_test() {
  let count = 0;
  let Foo = function() {
    this.num = ++count;
  }

  _("Make a thing instance of Foo but make sure it isn't initialized yet");
  let obj = {};
  Utils.lazy(obj, "thing", Foo);
  do_check_eq(count, 0);

  _("Access the property to make it construct");
  do_check_eq(typeof obj.thing, "object");
  do_check_eq(obj.thing.constructor, Foo);
  do_check_eq(count, 1);
  do_check_eq(obj.thing.num, 1);

  _("Additional accesses don't construct again (nothing should change");
  do_check_eq(typeof obj.thing, "object");
  do_check_eq(obj.thing.constructor, Foo);
  do_check_eq(count, 1);
  do_check_eq(obj.thing.num, 1);

  _("More lazy properties will constrct more");
  do_check_eq(typeof obj.other, "undefined");
  Utils.lazy(obj, "other", Foo);
  do_check_eq(typeof obj.other, "object");
  do_check_eq(obj.other.constructor, Foo);
  do_check_eq(count, 2);
  do_check_eq(obj.other.num, 2);

  _("Sanity check that the original property didn't change");
  do_check_eq(typeof obj.thing, "object");
  do_check_eq(obj.thing.constructor, Foo);
  do_check_eq(count, 2);
  do_check_eq(obj.thing.num, 1);
}
