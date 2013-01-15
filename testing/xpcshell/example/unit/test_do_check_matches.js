/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  do_check_matches({x:1}, {x:1});      // pass
  todo_check_matches({x:1}, {});       // fail: all pattern props required
  todo_check_matches({x:1}, {x:2});    // fail: values must match
  do_check_matches({x:1}, {x:1, y:2}); // pass: extra props tolerated

  // Property order is irrelevant.
  do_check_matches({x:"foo", y:"bar"}, {y:"bar", x:"foo"});// pass

  do_check_matches({x:undefined}, {x:1});// pass: 'undefined' is wildcard
  do_check_matches({x:undefined}, {x:2});
  todo_check_matches({x:undefined}, {y:2});// fail: 'x' must still be there

  // Patterns nest.
  do_check_matches({a:1, b:{c:2,d:undefined}}, {a:1, b:{c:2,d:3}});

  // 'length' property counts, even if non-enumerable.
  do_check_matches([3,4,5], [3,4,5]);    // pass
  todo_check_matches([3,4,5], [3,5,5]);  // fail; value doesn't match
  todo_check_matches([3,4,5], [3,4,5,6]);// fail; length doesn't match

  // functions in patterns get applied.
  do_check_matches({foo:function (v) v.length == 2}, {foo:"hi"});// pass
  todo_check_matches({foo:function (v) v.length == 2}, {bar:"hi"});// fail
  todo_check_matches({foo:function (v) v.length == 2}, {foo:"hello"});// fail

  // We don't check constructors, prototypes, or classes. However, if
  // pattern has a 'length' property, we require values to match that as
  // well, even if 'length' is non-enumerable in the pattern. So arrays
  // are useful as patterns.
  do_check_matches({0:0, 1:1, length:2}, [0,1]); // pass
  do_check_matches({0:1}, [1,2]);                // pass
  do_check_matches([0], {0:0, length:1});        // pass
}
