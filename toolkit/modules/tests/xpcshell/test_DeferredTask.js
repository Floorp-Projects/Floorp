/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the DeferredTask.jsm module.
 */

// Globals

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DeferredTask",
                                  "resource://gre/modules/DeferredTask.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");

/**
 * Due to the nature of this module, most of the tests are time-dependent.  All
 * the timeouts are designed to occur at multiples of this granularity value,
 * in milliseconds, that should be high enough to prevent intermittent failures,
 * but low enough to prevent an excessive overall test execution time.
 */
const T = 100;

/**
 * Waits for the specified timeout before resolving the returned promise.
 */
function promiseTimeout(aTimeoutMs) {
  return new Promise(resolve => {
    do_timeout(aTimeoutMs, resolve);
  });
}

function run_test() {
  run_next_test();
}

// Tests

/**
 * Creates a simple DeferredTask and executes it once.
 */
add_test(function test_arm_simple() {
  new DeferredTask(run_next_test, 10).arm();
});

/**
 * Checks that the delay set for the task is respected.
 */
add_test(function test_arm_delay_respected() {
  let executed1 = false;
  let executed2 = false;

  new DeferredTask(function() {
    executed1 = true;
    do_check_false(executed2);
  }, 1 * T).arm();

  new DeferredTask(function() {
    executed2 = true;
    do_check_true(executed1);
    run_next_test();
  }, 2 * T).arm();
});

/**
 * Checks that calling "arm" again does not introduce further delay.
 */
add_test(function test_arm_delay_notrestarted() {
  let executed = false;

  // Create a task that will run later.
  let deferredTask = new DeferredTask(() => { executed = true; }, 4 * T);
  deferredTask.arm();

  // Before the task starts, call "arm" again.
  do_timeout(2 * T, () => deferredTask.arm());

  // The "arm" call should not have introduced further delays.
  do_timeout(5 * T, function() {
    do_check_true(executed);
    run_next_test();
  });
});

/**
 * Checks that a task runs only once when armed multiple times synchronously.
 */
add_test(function test_arm_coalesced() {
  let executed = false;

  let deferredTask = new DeferredTask(function() {
    do_check_false(executed);
    executed = true;
    run_next_test();
  }, 50);

  deferredTask.arm();
  deferredTask.arm();
});

/**
 * Checks that a task runs only once when armed multiple times synchronously,
 * even when it has been created with a delay of zero milliseconds.
 */
add_test(function test_arm_coalesced_nodelay() {
  let executed = false;

  let deferredTask = new DeferredTask(function() {
    do_check_false(executed);
    executed = true;
    run_next_test();
  }, 0);

  deferredTask.arm();
  deferredTask.arm();
});

/**
 * Checks that a task can be armed again while running.
 */
add_test(function test_arm_recursive() {
  let executed = false;

  let deferredTask = new DeferredTask(function() {
    if (!executed) {
      executed = true;
      deferredTask.arm();
    } else {
      run_next_test();
    }
  }, 50);

  deferredTask.arm();
});

/**
 * Checks that calling "arm" while an asynchronous task is running waits until
 * the task is finished before restarting the delay.
 */
add_test(function test_arm_async() {
  let finishedExecution = false;
  let finishedExecutionAgain = false;

  // Create a task that will run later.
  let deferredTask = new DeferredTask(async function() {
    await promiseTimeout(4 * T);
    if (!finishedExecution) {
      finishedExecution = true;
    } else if (!finishedExecutionAgain) {
      finishedExecutionAgain = true;
    }
  }, 2 * T);
  deferredTask.arm();

  // While the task is running, call "arm" again.  This will result in a wait
  // of 2*T until the task finishes, then another 2*T for the normal task delay
  // specified on construction.
  do_timeout(4 * T, function() {
    do_check_true(deferredTask.isRunning);
    do_check_false(finishedExecution);
    deferredTask.arm();
  });

  // This will fail in case the task was started without waiting 2*T after it
  // has finished.
  do_timeout(7 * T, function() {
    do_check_false(deferredTask.isRunning);
    do_check_true(finishedExecution);
  });

  // This is in the middle of the second execution.
  do_timeout(10 * T, function() {
    do_check_true(deferredTask.isRunning);
    do_check_false(finishedExecutionAgain);
  });

  // Wait enough time to verify that the task was executed as expected.
  do_timeout(13 * T, function() {
    do_check_false(deferredTask.isRunning);
    do_check_true(finishedExecutionAgain);
    run_next_test();
  });
});

/**
 * Checks that "arm" accepts a Task.jsm generator function.
 */
add_test(function test_arm_async_generator() {
  let deferredTask = new DeferredTask(async function() {
    await Promise.resolve();
    run_next_test();
  }, 50);

  deferredTask.arm();
});

/**
 * Checks that "arm" accepts a Task.jsm legacy generator function.
 */
add_test(function test_arm_async_legacy_generator() {
  // ESLint cannot parse legacy generator functions, so we need an eval block.
  /* eslint-disable no-eval */
  let deferredTask = new DeferredTask(eval(`(function() {
    yield Promise.resolve();
    run_next_test();
  })`), 50);
  /* eslint-enable no-eval */

  deferredTask.arm();
});

/**
 * Checks that an armed task can be disarmed.
 */
add_test(function test_disarm() {
  // Create a task that will run later.
  let deferredTask = new DeferredTask(function() {
    do_throw("This task should not run.");
  }, 2 * T);
  deferredTask.arm();

  // Disable execution later, but before the task starts.
  do_timeout(1 * T, () => deferredTask.disarm());

  // Wait enough time to verify that the task did not run.
  do_timeout(3 * T, run_next_test);
});

/**
 * Checks that calling "disarm" allows the delay to be restarted.
 */
add_test(function test_disarm_delay_restarted() {
  let executed = false;

  let deferredTask = new DeferredTask(() => { executed = true; }, 4 * T);
  deferredTask.arm();

  do_timeout(2 * T, function() {
    deferredTask.disarm();
    deferredTask.arm();
  });

  do_timeout(5 * T, function() {
    do_check_false(executed);
  });

  do_timeout(7 * T, function() {
    do_check_true(executed);
    run_next_test();
  });
});

/**
 * Checks that calling "disarm" while an asynchronous task is running does not
 * prevent the task to finish.
 */
add_test(function test_disarm_async() {
  let finishedExecution = false;

  let deferredTask = new DeferredTask(async function() {
    deferredTask.arm();
    await promiseTimeout(2 * T);
    finishedExecution = true;
  }, 1 * T);
  deferredTask.arm();

  do_timeout(2 * T, function() {
    do_check_true(deferredTask.isRunning);
    do_check_true(deferredTask.isArmed);
    do_check_false(finishedExecution);
    deferredTask.disarm();
  });

  do_timeout(4 * T, function() {
    do_check_false(deferredTask.isRunning);
    do_check_false(deferredTask.isArmed);
    do_check_true(finishedExecution);
    run_next_test();
  });
});

/**
 * Checks that calling "arm" immediately followed by "disarm" while an
 * asynchronous task is running does not cause it to run again.
 */
add_test(function test_disarm_immediate_async() {
  let executed = false;

  let deferredTask = new DeferredTask(async function() {
    do_check_false(executed);
    executed = true;
    await promiseTimeout(2 * T);
  }, 1 * T);
  deferredTask.arm();

  do_timeout(2 * T, function() {
    do_check_true(deferredTask.isRunning);
    do_check_false(deferredTask.isArmed);
    deferredTask.arm();
    deferredTask.disarm();
  });

  do_timeout(4 * T, function() {
    do_check_true(executed);
    do_check_false(deferredTask.isRunning);
    do_check_false(deferredTask.isArmed);
    run_next_test();
  });
});

/**
 * Checks the isArmed and isRunning properties with a synchronous task.
 */
add_test(function test_isArmed_isRunning() {
  let deferredTask = new DeferredTask(function() {
    do_check_true(deferredTask.isRunning);
    do_check_false(deferredTask.isArmed);
    deferredTask.arm();
    do_check_true(deferredTask.isArmed);
    deferredTask.disarm();
    do_check_false(deferredTask.isArmed);
    run_next_test();
  }, 50);

  do_check_false(deferredTask.isArmed);
  deferredTask.arm();
  do_check_true(deferredTask.isArmed);
  do_check_false(deferredTask.isRunning);
});

/**
 * Checks that the "finalize" method executes a synchronous task.
 */
add_test(function test_finalize() {
  let executed = false;
  let timePassed = false;

  let deferredTask = new DeferredTask(function() {
    do_check_false(timePassed);
    executed = true;
  }, 2 * T);
  deferredTask.arm();

  do_timeout(1 * T, () => { timePassed = true; });

  // This should trigger the immediate execution of the task.
  deferredTask.finalize().then(function() {
    do_check_true(executed);
    run_next_test();
  });
});

/**
 * Checks that the "finalize" method executes the task again from start to
 * finish in case it is already running.
 */
add_test(function test_finalize_executes_entirely() {
  let executed = false;
  let executedAgain = false;
  let timePassed = false;

  let deferredTask = new DeferredTask(async function() {
    // The first time, we arm the timer again and set up the finalization.
    if (!executed) {
      deferredTask.arm();
      do_check_true(deferredTask.isArmed);
      do_check_true(deferredTask.isRunning);

      deferredTask.finalize().then(function() {
        // When we reach this point, the task must be finished.
        do_check_true(executedAgain);
        do_check_false(timePassed);
        do_check_false(deferredTask.isArmed);
        do_check_false(deferredTask.isRunning);
        run_next_test();
      });

      // The second execution triggered by the finalization waits 1*T for the
      // current task to finish (see the timeout below), but then it must not
      // wait for the 2*T specified on construction as normal task delay.  The
      // second execution will finish after the timeout below has passed again,
      // for a total of 2*T of wait time.
      do_timeout(3 * T, () => { timePassed = true; });
    }

    await promiseTimeout(1 * T);

    // Just before finishing, indicate if we completed the second execution.
    if (executed) {
      do_check_true(deferredTask.isRunning);
      executedAgain = true;
    } else {
      executed = true;
    }
  }, 2 * T);

  deferredTask.arm();
});
