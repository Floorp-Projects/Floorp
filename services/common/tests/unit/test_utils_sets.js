/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://services-common/utils.js");

const EMPTY = new Set();
const A = new Set(["a"]);
const ABC = new Set(["a", "b", "c"]);
const ABCD = new Set(["a", "b", "c", "d"]);
const BC = new Set(["b", "c"]);
const BCD = new Set(["b", "c", "d"]);
const FGH = new Set(["f", "g", "h"]);
const BCDFGH = new Set(["b", "c", "d", "f", "g", "h"]);

let union = CommonUtils.union;
let difference = CommonUtils.difference;
let intersection = CommonUtils.intersection;
let setEqual = CommonUtils.setEqual;

function do_check_setEqual(a, b) {
  do_check_true(setEqual(a, b));
}

function do_check_not_setEqual(a, b) {
  do_check_false(setEqual(a, b));
}

function run_test() {
  run_next_test();
}

add_test(function test_setEqual() {
  do_check_setEqual(EMPTY, EMPTY);
  do_check_setEqual(EMPTY, new Set());
  do_check_setEqual(A, A);
  do_check_setEqual(A, new Set(["a"]));
  do_check_setEqual(new Set(["a"]), A);
  do_check_not_setEqual(A, EMPTY);
  do_check_not_setEqual(EMPTY, A);
  do_check_not_setEqual(ABC, A);
  run_next_test();
});

add_test(function test_union() {
  do_check_setEqual(EMPTY, union(EMPTY, EMPTY));
  do_check_setEqual(ABC, union(EMPTY, ABC));
  do_check_setEqual(ABC, union(ABC, ABC));
  do_check_setEqual(ABCD, union(ABC, BCD));
  do_check_setEqual(ABCD, union(BCD, ABC));
  do_check_setEqual(BCDFGH, union(BCD, FGH));
  run_next_test();
});

add_test(function test_difference() {
  do_check_setEqual(EMPTY, difference(EMPTY, EMPTY));
  do_check_setEqual(EMPTY, difference(EMPTY, A));
  do_check_setEqual(EMPTY, difference(A, A));
  do_check_setEqual(ABC, difference(ABC, EMPTY));
  do_check_setEqual(ABC, difference(ABC, FGH));
  do_check_setEqual(A, difference(ABC, BCD));
  run_next_test();
});

add_test(function test_intersection() {
  do_check_setEqual(EMPTY, intersection(EMPTY, EMPTY));
  do_check_setEqual(EMPTY, intersection(ABC, EMPTY));
  do_check_setEqual(EMPTY, intersection(ABC, FGH));
  do_check_setEqual(BC, intersection(ABC, BCD));
  run_next_test();
});
