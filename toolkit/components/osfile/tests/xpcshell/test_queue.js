"use strict";

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

// Check if Scheduler.queue returned by OS.File.queue is resolved initially.
add_task(async function check_init() {
  await OS.File.queue;
  info("Function resolved");
});

// Check if Scheduler.queue returned by OS.File.queue is resolved
// after an operation is successful.
add_task(async function check_success() {
  info("Attempting to open a file correctly");
  await OS.File.open(OS.Path.join(do_get_cwd().path, "test_queue.js"));
  info("File opened correctly");
  await OS.File.queue;
  info("Function resolved");
});

// Check if Scheduler.queue returned by OS.File.queue is resolved
// after an operation fails.
add_task(async function check_failure() {
  let exception;
  try {
    info("Attempting to open a non existing file");
    await OS.File.open(OS.Path.join(".", "Bigfoot"));
  } catch (err) {
    exception = err;
    await OS.File.queue;
  }
  Assert.ok(exception != null);
  info("Function resolved");
});
