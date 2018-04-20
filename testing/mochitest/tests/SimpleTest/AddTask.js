// add_task(generatorFunction):
// Call `add_task(generatorFunction)` for each separate
// asynchronous task in a mochitest. Tasks are run consecutively.
// Before the first task, `SimpleTest.waitForExplicitFinish()`
// will be called automatically, and after the last task,
// `SimpleTest.finish()` will be called.
var add_task = (function () {
  // The list of tasks to run.
  var task_list = [];
  var run_only_this_task = null;

  function isGenerator(value) {
    return value && typeof value === "object" && typeof value.next === "function";
  }

  // The "add_task" function
  return function (generatorFunction) {
    if (task_list.length === 0) {
      // This is the first time add_task has been called.
      // First, confirm that SimpleTest is available.
      if (!SimpleTest) {
        throw new Error("SimpleTest not available.");
      }
      // Don't stop tests until asynchronous tasks are finished.
      SimpleTest.waitForExplicitFinish();
      // Because the client is using add_task for this set of tests,
      // we need to spawn a "master task" that calls each task in succesion.
      // Use setTimeout to ensure the master task runs after the client
      // script finishes.
      setTimeout(function () {
        (async () => {
          // Allow for a task to be skipped; we need only use the structured logger
          // for this, whilst deactivating log buffering to ensure that messages
          // are always printed to stdout.
          function skipTask(name) {
            let logger = parentRunner && parentRunner.structuredLogger;
            if (!logger) {
              info("AddTask.js | Skipping test " + name);
              return;
            }
            logger.deactivateBuffering();
            logger.testStatus(SimpleTest._getCurrentTestURL(), name, "SKIP");
            logger.warning("AddTask.js | Skipping test " + name);
            logger.activateBuffering();
          }

          // We stop the entire test file at the first exception because this
          // may mean that the state of subsequent tests may be corrupt.
          try {
            for (var task of task_list) {
              var name = task.name || "";
              if (task.__skipMe || (run_only_this_task && task != run_only_this_task)) {
                skipTask(name);
                continue;
              }
              info("AddTask.js | Entering test " + name);
              let result = await task();
              if (isGenerator(result)) {
                ok(false, "Task returned a generator");
              }
              info("AddTask.js | Leaving test " + name);
            }
          } catch (ex) {
            try {
              ok(false, "" + ex, "Should not throw any errors", ex.stack);
            } catch (ex2) {
              ok(false, "(The exception cannot be converted to string.)",
                 "Should not throw any errors", ex.stack);
            }
          }
          // All tasks are finished.
          SimpleTest.finish();
        })();
      });
    }
    generatorFunction.skip = () => generatorFunction.__skipMe = true;
    generatorFunction.only = () => run_only_this_task = generatorFunction;
    // Add the task to the list of tasks to run after
    // the main thread is finished.
    task_list.push(generatorFunction);
    return generatorFunction;
  };
})();
