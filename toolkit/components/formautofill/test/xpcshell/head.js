/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This file makes the common testing infrastructure available to the xpcshell
 * tests located in this folder.  This is only used as an infrastructure file,
 * and new common functions should be added to the "head_common.js" file.
 */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);

/* --- Adapters for the xpcshell infrastructure --- */

let Output = {
  print: do_print,
};

let executeSoon = do_execute_soon;
let setTimeout = (fn, delay) => do_timeout(delay, fn);

function run_test() {
  do_get_profile();
  run_next_test();
}
