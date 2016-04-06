/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Infrastructure common to the test frameworks located in subfolders.
 *
 * A copy of this file is installed in each of the framework subfolders, this
 * means it becomes a sibling of the test files in the final layout.  This is
 * determined by how manifest "support-files" installation works.
 *
 * Unless you are adding new features to the framework, you shouldn't have to
 * modify this file.  Use "head_common.js" or the "head.js" file of each
 * framework for shared code.
 */

"use strict";

/*
 * --------------------
 *  FRAMEWORK OVERVIEW
 * --------------------
 *
 * This framework is designed in such a way that test can be written in similar
 * ways in the xpcshell, mochitest-chrome, and mochitest-browser frameworks,
 * both when tests are running in the parent process or in a content process.
 *
 * There are some basic self-documenting assertion and output functions:
 *
 * Assert.ok(actualValue);
 * Assert.is(actualValue, expectedValue);
 * Output.print(string);
 *
 * Test cases and initialization functions are declared in shared head files
 * ("head_common.js" and "head.js") as well as individual test files.  When
 * tests run in an Elecrolysis (e10s) environment, they are executed in both
 * processes at first.  Normally, at this point only the registration of test
 * cases happen.  When everything has finished loading, tests are started and
 * appropriately synchronized between processes.
 *
 * Tests can be declared using the add_task syntax:
 *
 * add_task(function* test_something () { ... });
 *   This adds a test either in the parent process or child process:
 *     - Parent: xpcshell, mochitest-chrome --disable-e10s, mochitest-browser
 *     - Child: mochitest-chrome with e10s
 *   In the future, these might run in the child process for "xpcshell".
 *
 * add_task_in_parent_process(function* test_something () { ... });
 *   This test runs in the parent process, but the child process will wait for
 *   its completion before continuing with the next task.  This wait currently
 *   happens only in mochitest-chrome with e10s, in other frameworks that run
 *   only in the parent process this is the same as a normal add_task.
 *
 * add_task_in_child_process(function* test_something () { ... });
 *   This test runs only in the child process.  This means that the test is not
 *   run unless this is an e10s test, currently mochitest-chrome with e10s.
 *
 * add_task_in_both_processes(function* test_something () { ... });
 *   Useful for initialization that must be done both in the parent and the
 *   child, like setting preferences.
 *
 * add_termination_task(function* () { ... });
 *   Registers a new asynchronous termination task.  This is executed after all
 *   test cases in the file finished, and always in the same process where the
 *   termination task is registered.
 */
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

var gTerminationTasks = [];

/**
 * None of the testing frameworks support asynchronous termination functions, so
 * this task must be registered later, after the other "add_task" calls.
 *
 * Even xpcshell doesn't support calling "add_task" in the "tail.js" file,
 * because it registers the task but does not wait for its termination,
 * potentially leading to intermittent failures in subsequent tests.
 */
function* terminationTaskFn() {
  for (let taskFn of gTerminationTasks) {
    try {
      yield Task.spawn(taskFn);
    } catch (ex) {
      Output.print(ex);
      Assert.ok(false);
    }
  }
}

function add_termination_task(taskFn) {
  gTerminationTasks.push(taskFn);
}

/**
 * Returns a unique identifier used for synchronizing the given test task
 * between the parent and child processes.
 */
function getTaskId(stackFrame) {
  return stackFrame.filename + ":" + stackFrame.lineNumber;
}

// This is a shared helper for mochitest-chrome and mochitest-browser.
var _mochitestAssert = {
  ok: function (actual) {
    let stack = Components.stack.caller;
    ok(actual, "[" + stack.name + " : " + stack.lineNumber + "] " + actual +
               " == true");
  },
  equal: function (actual, expected) {
    let stack = Components.stack.caller;
    is(actual, expected, "[" + stack.name + " : " + stack.lineNumber + "] " +
               actual + " == " + expected);
  },
};

// Reminder: unless you are adding new features to the framework, you shouldn't
// have to modify this file.  Use "head_common.js" or "head.js" for shared code.
