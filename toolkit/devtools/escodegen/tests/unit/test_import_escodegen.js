/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can require escodegen.
 */

function run_test() {
  const escodegen = require("escodegen/escodegen");
  do_check_eq(typeof escodegen.generate, "function");
}
