/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

const { Subprocess } = ChromeUtils.importESModule(
  "resource://gre/modules/Subprocess.sys.mjs"
);

const ProcessTools = Cc["@mozilla.org/processtools-service;1"].getService(
  Ci.nsIProcessToolsService
);

let PYTHON;

// Find Python.
add_task(async function setup() {
  PYTHON = await Subprocess.pathSearch(Services.env.get("PYTHON"));
});

// Ensure that killing a process... kills the process.
add_task(async function test_subprocess_kill() {
  // We launch Python, as it's a long-running process and it exists
  // on all desktop platforms on which we run tests.
  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: [],
  });

  let isTerminated = false;

  proc.wait().then(() => {
    isTerminated = true;
  });

  await new Promise(resolve => setTimeout(resolve, 100));
  Assert.ok(
    !isTerminated,
    "We haven't killed the process yet, it should still be running."
  );

  // Time to kill the process.
  ProcessTools.kill(proc.pid);

  await new Promise(resolve => setTimeout(resolve, 100));
  Assert.ok(
    isTerminated,
    "We have killed the process already, it shouldn't be running anymore."
  );
});
