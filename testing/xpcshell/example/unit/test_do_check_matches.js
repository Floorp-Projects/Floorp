/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  do_check_matches({x: 1}, {x: 1});

  // Property order is irrelevant.
  do_check_matches({x: "foo", y: "bar"}, {y: "bar", x: "foo"});// pass

  // Patterns nest.
  do_check_matches({a: 1, b: {c: 2, d: 3}}, {a: 1, b: {c: 2, d: 3}});

  do_check_matches([3, 4, 5], [3, 4, 5]);
}
