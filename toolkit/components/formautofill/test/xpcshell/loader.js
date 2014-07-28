/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Infrastructure for the xpcshell tests located in this folder.
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
  Services.io.newFileURI(do_get_file("loader_common.js")).spec, this);

// Define output functions so they look the same across all frameworks.
let Output = {
  print: do_print,
};

let executeSoon = do_execute_soon;
let setTimeout = (fn, delay) => do_timeout(delay, fn);

// Define task registration functions, see description in "loader_common.js".
let add_task_in_parent_process = add_task;
let add_task_in_child_process = function () {};
let add_task_in_both_processes = add_task;

Services.scriptloader.loadSubScript(
  Services.io.newFileURI(do_get_file("head_common.js")).spec, this);

// Tests are always run asynchronously and with the profile loaded.
function run_test() {
  do_get_profile();
  run_next_test();
}

// Reminder: unless you are adding new features to the framework, you shouldn't
// have to modify this file.  Use "head_common.js" or "head.js" for shared code.
