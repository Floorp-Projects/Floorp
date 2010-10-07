Cu.import("resource://services-sync/util.js");

function run_test() {
  let thing = {o: {foo: "foo", bar: ["bar"]}, a: ["foo", {bar: "bar"}]};
  let ret = Utils.deepCopy(thing);
  do_check_neq(ret, thing)
  do_check_neq(ret.o, thing.o);
  do_check_neq(ret.o.bar, thing.o.bar);
  do_check_neq(ret.a, thing.a);
  do_check_neq(ret.a[1], thing.a[1]);
  do_check_eq(ret.o.foo, thing.o.foo);
  do_check_eq(ret.o.bar[0], thing.o.bar[0]);
  do_check_eq(ret.a[0], thing.a[0]);
  do_check_eq(ret.a[1].bar, thing.a[1].bar);
}
