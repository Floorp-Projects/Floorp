/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  do_check_true(Services.search.getVisibleEngines().length > 1);
  do_check_true(Services.search.isInitialized);

  // Remove the current engine...
  let currentEngine = Services.search.currentEngine;
  Services.search.removeEngine(currentEngine);

  // ... and verify a new current engine has been set.
  do_check_neq(Services.search.currentEngine.name, currentEngine.name);
  do_check_true(currentEngine.hidden);

  // Remove all the other engines.
  Services.search.getVisibleEngines().forEach(Services.search.removeEngine);
  do_check_eq(Services.search.getVisibleEngines().length, 0);

  // Verify the original default engine is used as a fallback and no
  // longer hidden.
  do_check_eq(Services.search.currentEngine.name, currentEngine.name);
  do_check_false(currentEngine.hidden);
  do_check_eq(Services.search.getVisibleEngines().length, 1);
}
