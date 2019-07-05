/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { BinarySearch } = ChromeUtils.import(
  "resource://gre/modules/BinarySearch.jsm"
);

function run_test() {
  // empty array
  check([], 1, false, 0);

  // one-element array
  check([2], 2, true, 0);
  check([2], 1, false, 0);
  check([2], 3, false, 1);

  // two-element array
  check([2, 4], 2, true, 0);
  check([2, 4], 4, true, 1);
  check([2, 4], 1, false, 0);
  check([2, 4], 3, false, 1);
  check([2, 4], 5, false, 2);

  // three-element array
  check([2, 4, 6], 2, true, 0);
  check([2, 4, 6], 4, true, 1);
  check([2, 4, 6], 6, true, 2);
  check([2, 4, 6], 1, false, 0);
  check([2, 4, 6], 3, false, 1);
  check([2, 4, 6], 5, false, 2);
  check([2, 4, 6], 7, false, 3);

  // duplicates
  check([2, 2], 2, true, 0);
  check([2, 2], 1, false, 0);
  check([2, 2], 3, false, 2);

  // duplicates on the left
  check([2, 2, 4], 2, true, 1);
  check([2, 2, 4], 4, true, 2);
  check([2, 2, 4], 1, false, 0);
  check([2, 2, 4], 3, false, 2);
  check([2, 2, 4], 5, false, 3);

  // duplicates on the right
  check([2, 4, 4], 2, true, 0);
  check([2, 4, 4], 4, true, 1);
  check([2, 4, 4], 1, false, 0);
  check([2, 4, 4], 3, false, 1);
  check([2, 4, 4], 5, false, 3);

  // duplicates in the middle
  check([2, 4, 4, 6], 2, true, 0);
  check([2, 4, 4, 6], 4, true, 1);
  check([2, 4, 4, 6], 6, true, 3);
  check([2, 4, 4, 6], 1, false, 0);
  check([2, 4, 4, 6], 3, false, 1);
  check([2, 4, 4, 6], 5, false, 3);
  check([2, 4, 4, 6], 7, false, 4);

  // duplicates all around
  check([2, 2, 4, 4, 6, 6], 2, true, 0);
  check([2, 2, 4, 4, 6, 6], 4, true, 2);
  check([2, 2, 4, 4, 6, 6], 6, true, 4);
  check([2, 2, 4, 4, 6, 6], 1, false, 0);
  check([2, 2, 4, 4, 6, 6], 3, false, 2);
  check([2, 2, 4, 4, 6, 6], 5, false, 4);
  check([2, 2, 4, 4, 6, 6], 7, false, 6);
}

function check(array, target, expectedFound, expectedIdx) {
  let [found, idx] = BinarySearch.search(cmp, array, target);
  Assert.equal(found, expectedFound);
  Assert.equal(idx, expectedIdx);

  idx = expectedFound ? expectedIdx : -1;
  Assert.equal(BinarySearch.indexOf(cmp, array, target), idx);
  Assert.equal(BinarySearch.insertionIndexOf(cmp, array, target), expectedIdx);
}

function cmp(num1, num2) {
  return num1 - num2;
}
