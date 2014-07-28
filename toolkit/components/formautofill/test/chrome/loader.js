/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Infrastructure for the mochitest-chrome tests located in this folder.
 *
 * See "loader_common.js" in the parent folder for a general overview.
 *
 * Unless you are adding new features to the framework, you shouldn't have to
 * modify this file.  Use "head_common.js" or "head.js" for shared code.
 */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);

Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/SimpleTest.js", this);

let sharedUrl = SimpleTest.getTestFileURL("loader_common.js");
Services.scriptloader.loadSubScript(sharedUrl, this);

let ChromeUtils = {};
Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/ChromeUtils.js", ChromeUtils);

let parentScript = SpecialPowers.loadChromeScript(
                                 SimpleTest.getTestFileURL("loader_parent.js"));

// Replace the extension of the loaded HTML file with ".js"
let testUrl = location.href.replace(/\.\w+$/, ".js");

// Start loading the test script in the parent process.
let promiseParentInitFinished = new Promise(function (resolve) {
  parentScript.addMessageListener("finish_load_in_parent", resolve);
});
parentScript.sendAsyncMessage("start_load_in_parent", { testUrl: testUrl });

// Define output functions so they look the same across all frameworks.
let Output = {
  print: info,
};

// Define assertion functions so they look the same across all frameworks.
let Assert = {
  ok: _mochitestAssert.ok,
  equal: _mochitestAssert.equal,
};

let executeSoon = SimpleTest.executeSoon;

let gTestTasks = [];

// Define task registration functions, see description in "loader_common.js".
function add_task(taskFn) {
  gTestTasks.push([taskFn, "content", taskFn.name]);
}
function add_task_in_parent_process(taskFn, taskIdOverride) {
  let taskId = taskIdOverride || getTaskId(Components.stack.caller);
  gTestTasks.push([taskFn, "parent", taskId]);
};
function add_task_in_both_processes(taskFn) {
  // We need to define a task ID based on our direct caller.
  add_task_in_parent_process(taskFn, getTaskId(Components.stack.caller));
  add_task(taskFn);
};
let add_task_in_child_process = add_task;

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad);

  Task.spawn(function* () {
    try {
      for (let [taskFn, taskType, taskId] of gTestTasks) {
        if (taskType == "content") {
          // This is a normal task executed in the current process.
          info("Running " + taskFn.name);
          yield Task.spawn(taskFn);
        } else {
          // This is a task executed in the parent process.
          info("Running task in parent process: " + taskFn.name);
          let promiseFinished = new Promise(function (resolve) {
            parentScript.addMessageListener("finish_task_" + taskId, resolve);
          });
          parentScript.sendAsyncMessage("start_task_" + taskId);
          yield promiseFinished;
          info("Finished task in parent process: " + taskFn.name);
        }
      }
    } catch (ex) {
      ok(false, ex);
    }

    SimpleTest.finish();
  });
});

// Wait for the test script to be loaded in the parent process.  This means that
// test tasks are registered and ready, but have not been executed yet.
add_task(function* wait_loading_in_parent_process() {
  yield promiseParentInitFinished;
});

let headUrl = SimpleTest.getTestFileURL("head_common.js");
Services.scriptloader.loadSubScript(headUrl, this);

Output.print("Loading test file: " + testUrl);
Services.scriptloader.loadSubScript(testUrl, this);

// Register the execution of termination tasks after all other tasks.
add_task(terminationTaskFn);
add_task_in_parent_process(terminationTaskFn, terminationTaskFn.name);

SimpleTest.waitForExplicitFinish();

// Reminder: unless you are adding new features to the framework, you shouldn't
// have to modify this file.  Use "head_common.js" or "head.js" for shared code.
