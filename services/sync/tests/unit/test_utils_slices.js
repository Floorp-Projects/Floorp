Cu.import("resource://services-sync/util.js");

function run_test() {
  let input = [1, 2, 3, 4, 5];

  let err;
  try {
    Utils.slices(input, 0).next();
  } catch (ex) {
    err = ex;
  }
  do_check_eq("Invalid slice size.", err);

  err = undefined;
  try {
    Utils.slices(input).next();
  } catch (ex) {
    err = ex;
  }
  do_check_eq("Invalid slice size.", err);

  function slurp(g) [x for each (x in g)]

  let sliced1 = slurp(Utils.slices(input, 1));
  let sliced2 = slurp(Utils.slices(input, 2));
  let sliced3 = slurp(Utils.slices(input, 5));
  let sliced4 = slurp(Utils.slices(input, 7));

  do_check_eq(sliced1.length, 5);
  do_check_eq(sliced2.length, 3);
  do_check_eq(sliced3.length, 1);
  do_check_eq(sliced4.length, 1);
  sliced1.every(function(x) x.length == 1);
  _(JSON.stringify(sliced2));
  do_check_eq(sliced2[0].length, 2);
  do_check_eq(sliced2[1].length, 2);
  do_check_eq(sliced2[2].length, 1);
  sliced3.every(function(x) x.length == 5);
  sliced4.every(function(x) x.length == 5);

  let sliced5 = slurp(Utils.slices(["foo"], 50));
  do_check_eq(sliced5.length, 1);
  do_check_eq(sliced5[0], "foo");

  let sliced6 = slurp(Utils.slices([], 50));
  do_check_eq(sliced6.length, 1);
  do_check_eq(sliced6[0].length, 0);
}
