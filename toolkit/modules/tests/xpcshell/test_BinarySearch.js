/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/BinarySearch.jsm");

function run_test() {
  // empty array
  ok([], 1, false, 0);

  // one-element array
  ok([2], 2, true, 0);
  ok([2], 1, false, 0);
  ok([2], 3, false, 1);

  // two-element array
  ok([2, 4], 2, true, 0);
  ok([2, 4], 4, true, 1);
  ok([2, 4], 1, false, 0);
  ok([2, 4], 3, false, 1);
  ok([2, 4], 5, false, 2);

  // three-element array
  ok([2, 4, 6], 2, true, 0);
  ok([2, 4, 6], 4, true, 1);
  ok([2, 4, 6], 6, true, 2);
  ok([2, 4, 6], 1, false, 0);
  ok([2, 4, 6], 3, false, 1);
  ok([2, 4, 6], 5, false, 2);
  ok([2, 4, 6], 7, false, 3);

  // duplicates
  ok([2, 2], 2, true, 0);
  ok([2, 2], 1, false, 0);
  ok([2, 2], 3, false, 2);

  // duplicates on the left
  ok([2, 2, 4], 2, true, 1);
  ok([2, 2, 4], 4, true, 2);
  ok([2, 2, 4], 1, false, 0);
  ok([2, 2, 4], 3, false, 2);
  ok([2, 2, 4], 5, false, 3);

  // duplicates on the right
  ok([2, 4, 4], 2, true, 0);
  ok([2, 4, 4], 4, true, 1);
  ok([2, 4, 4], 1, false, 0);
  ok([2, 4, 4], 3, false, 1);
  ok([2, 4, 4], 5, false, 3);

  // duplicates in the middle
  ok([2, 4, 4, 6], 2, true, 0);
  ok([2, 4, 4, 6], 4, true, 1);
  ok([2, 4, 4, 6], 6, true, 3);
  ok([2, 4, 4, 6], 1, false, 0);
  ok([2, 4, 4, 6], 3, false, 1);
  ok([2, 4, 4, 6], 5, false, 3);
  ok([2, 4, 4, 6], 7, false, 4);

  // duplicates all around
  ok([2, 2, 4, 4, 6, 6], 2, true, 0);
  ok([2, 2, 4, 4, 6, 6], 4, true, 2);
  ok([2, 2, 4, 4, 6, 6], 6, true, 4);
  ok([2, 2, 4, 4, 6, 6], 1, false, 0);
  ok([2, 2, 4, 4, 6, 6], 3, false, 2);
  ok([2, 2, 4, 4, 6, 6], 5, false, 4);
  ok([2, 2, 4, 4, 6, 6], 7, false, 6);
}

function ok(array, target, expectedFound, expectedIdx) {
  let [found, idx] = BinarySearch.search(cmp, array, target);
  do_check_eq(found, expectedFound);
  do_check_eq(idx, expectedIdx);

  idx = expectedFound ? expectedIdx : -1;
  do_check_eq(BinarySearch.indexOf(cmp, array, target), idx);
  do_check_eq(BinarySearch.insertionIndexOf(cmp, array, target), expectedIdx);
}

function cmp(num1, num2) {
  return num1 - num2;
}
