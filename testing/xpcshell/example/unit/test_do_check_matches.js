/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  Assert.deepEqual({x: 1}, {x: 1});

  // Property order is irrelevant.
  Assert.deepEqual({x: "foo", y: "bar"}, {y: "bar", x: "foo"});// pass

  // Patterns nest.
  Assert.deepEqual({a: 1, b: {c: 2, d: 3}}, {a: 1, b: {c: 2, d: 3}});

  Assert.deepEqual([3, 4, 5], [3, 4, 5]);
}
