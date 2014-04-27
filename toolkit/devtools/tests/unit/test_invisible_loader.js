/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/jsdebugger.jsm");
addDebuggerToGlobal(this);

const COLOR_URI = "resource://gre/modules/devtools/css-color.js";

/**
 * Ensure that sandboxes created via the Dev Tools loader respect the
 * invisibleToDebugger flag.
 */
function run_test() {
  visible_loader();
  invisible_loader();
}

function visible_loader() {
  let loader = new DevToolsLoader();
  loader.invisibleToDebugger = false;
  loader.require("devtools/css-color");

  let dbg = new Debugger();
  let sandbox = loader._provider.loader.sandboxes[COLOR_URI];

  try {
    dbg.addDebuggee(sandbox);
    do_check_true(true);
  } catch(e) {
    do_throw("debugger could not add visible value");
  }
}

function invisible_loader() {
  let loader = new DevToolsLoader();
  loader.invisibleToDebugger = true;
  loader.require("devtools/css-color");

  let dbg = new Debugger();
  let sandbox = loader._provider.loader.sandboxes[COLOR_URI];

  try {
    dbg.addDebuggee(sandbox);
    do_throw("debugger added invisible value");
  } catch(e) {
    do_check_true(true);
  }
}
