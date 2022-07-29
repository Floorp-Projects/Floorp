/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

const { Subprocess } = ChromeUtils.import(
  "resource://gre/modules/Subprocess.jsm"
);

const ProcessTools = Cc["@mozilla.org/processtools-service;1"].getService(
  Ci.nsIProcessToolsService
);

const env = Cc["@mozilla.org/process/environment;1"].getService(
  Ci.nsIEnvironment
);

let PYTHON;

// Find Python.
add_task(async function setup() {
  PYTHON = await Subprocess.pathSearch(env.get("PYTHON"));
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
