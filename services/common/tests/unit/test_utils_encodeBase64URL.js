/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/utils.js");

function run_test() {
  run_next_test();
}

add_test(function test_simple() {
  let expected = {
    hello: "aGVsbG8=",
    "<>?": "PD4_",
  };

  for (let [k, v] in Iterator(expected)) {
    do_check_eq(CommonUtils.encodeBase64URL(k), v);
  }

  run_next_test();
});

add_test(function test_no_padding() {
  do_check_eq(CommonUtils.encodeBase64URL("hello", false), "aGVsbG8");

  run_next_test();
});
