/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function run_test() {
  let uuid = CommonUtils.generateUUID();
  do_check_eq(uuid.length, 36);
  do_check_eq(uuid[8], "-");

  run_next_test();
}
