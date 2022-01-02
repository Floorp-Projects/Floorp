/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that we throw if a test created with add_task()
// calls run_next_test

add_task(async function run_next_throws() {
  let err = null;
  try {
    run_next_test();
  } catch (e) {
    err = e;
    info("run_next_test threw " + err);
  }
  ok(err, "run_next_test() should throw an error inside an add_task test");
});
