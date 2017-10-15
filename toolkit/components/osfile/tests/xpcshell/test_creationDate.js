"use strict";

function run_test() {
  do_test_pending();
  run_next_test();
}

/**
 * Test to ensure that deprecation warning is issued on use
 * of creationDate.
 */
add_task(async function test_deprecatedCreationDate() {
  let currentDir = await OS.File.getCurrentDirectory();
  let path = OS.Path.join(currentDir, "test_creationDate.js");

  let consoleMessagePromise = new Promise(resolve => {
    let consoleListener = {
      observe(aMessage) {
        if (aMessage.message.indexOf("Field 'creationDate' is deprecated.") > -1) {
        do_print("Deprecation message printed");
          do_check_true(true);
          Services.console.unregisterListener(consoleListener);
          resolve();
        }
      }
    };
    Services.console.registerListener(consoleListener);
  });

  (await OS.File.stat(path)).creationDate;
  await consoleMessagePromise;
});

add_task(do_test_finished);
