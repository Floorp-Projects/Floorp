/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  do_check_matches({x:1}, {});         // fail: all pattern props required
  do_check_matches({x:1}, {x:2});      // fail: values must match
  do_check_matches({x:undefined}, {});

  // 'length' property counts, even if non-enumerable.
  do_check_matches([3,4,5], [3,5,5]);  // fail; value doesn't match
  do_check_matches([3,4,5], [3,4,5,6]);// fail; length doesn't match
}
