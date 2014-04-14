"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");

function run_test() {
  run_next_test();
}

// Check if Scheduler.queue returned by OS.File.queue is resolved initially.
add_task(function* check_init() {
  yield OS.File.queue;
  do_print("Function resolved");
});

// Check if Scheduler.queue returned by OS.File.queue is resolved
// after an operation is successful.
add_task(function* check_success() {
  do_print("Attempting to open a file correctly");
  let openedFile = yield OS.File.open(OS.Path.join(do_get_cwd().path, "test_queue.js"));
  do_print("File opened correctly");
  yield OS.File.queue;
  do_print("Function resolved");
});

// Check if Scheduler.queue returned by OS.File.queue is resolved
// after an operation fails.
add_task(function* check_failure() {
  let exception;
  try {
    do_print("Attempting to open a non existing file");
    yield OS.File.open(OS.Path.join(".", "Bigfoot"));
  } catch (err) {
    exception = err;
    yield OS.File.queue;
  }  
  do_check_true(exception!=null);
  do_print("Function resolved");
});