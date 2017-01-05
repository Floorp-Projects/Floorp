/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Infrastructure for the mochitest-chrome tests located in this folder, always
 * executed in the parent process.
 *
 * See "loader_common.js" in the parent folder for a general overview.
 *
 * Unless you are adding new features to the framework, you shouldn't have to
 * modify this file.  Use "head_common.js" or "head.js" for shared code.
 */

"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);

/* import-globals-from ../loader_common.js */
var sharedUrl = "chrome://mochitests/content/chrome/" +
                "toolkit/components/formautofill/test/chrome/loader_common.js";
Services.scriptloader.loadSubScript(sharedUrl, this);

// Define output functions so they look the same across all frameworks.  Since
// we don't have an output function available here, we report as TEST-PASS.
var Output = {
  print: message => assert.ok(true, message),
};

// Define assertion functions so they look the same across all frameworks.
var Assert = {
  ok: assert.ok,
  equal: assert.equal,
};

// Define task registration functions, see description in "loader_common.js".
function add_task_in_parent_process(taskFn, taskIdOverride) {
  let taskId = taskIdOverride || getTaskId(Components.stack.caller);
  Output.print("Registering in the parent process: " + taskId);
  addMessageListener("start_task_" + taskId, function() {
    Task.spawn(function* () {
      try {
        Output.print("Running in the parent process " + taskId);
        yield Task.spawn(taskFn);
      } catch (ex) {
        assert.ok(false, ex);
      }

      sendAsyncMessage("finish_task_" + taskId, {});
    });
  });
}
var add_task = function() {};
var add_task_in_child_process = function() {};
var add_task_in_both_processes = add_task_in_parent_process;

// We need to wait for the child process to send us the path of the test file
// to load before we can actually start loading it.
var context = this;
addMessageListener("start_load_in_parent", function(message) {
  Output.print("Starting loading infrastructure in parent process.");
  /* import-globals-from ../head_common.js */
  let headUrl = "chrome://mochitests/content/chrome/" +
                "toolkit/components/formautofill/test/chrome/head_common.js";
  Services.scriptloader.loadSubScript(headUrl, context);

  Services.scriptloader.loadSubScript(message.testUrl, context);

  // Register the execution of termination tasks after all other tasks.
  add_task_in_parent_process(terminationTaskFn, terminationTaskFn.name);

  Output.print("Finished loading infrastructure in parent process.");
  sendAsyncMessage("finish_load_in_parent", {});
});

// Reminder: unless you are adding new features to the framework, you shouldn't
// have to modify this file.  Use "head_common.js" or "head.js" for shared code.
