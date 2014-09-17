/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let Cu = Components.utils;

Cu.import("resource://gre/modules/PromiseWorker.jsm", this);
Cu.import("resource://gre/modules/Timer.jsm", this);

// Worker must be loaded from a chrome:// uri, not a file://
// uri, so we first need to load it.

let WORKER_SOURCE_URI = "chrome://promiseworker/content/worker.js";
do_load_manifest("data/chrome.manifest");
let worker = new BasePromiseWorker(WORKER_SOURCE_URI);
worker.log = function(...args) {
  do_print("Controller: " + args.join(" "));
};

// Test that simple messages work
add_task(function* test_simple_args() {
  let message = ["test_simple_args", Math.random()];
  let result = yield worker.post("bounce", message);
  Assert.equal(JSON.stringify(result), JSON.stringify(message));
});

// Test that it works when we don't provide a message
add_task(function* test_no_args() {
  let result = yield worker.post("bounce");
  Assert.equal(JSON.stringify(result), JSON.stringify([]));
});

// Test that messages with promise work
add_task(function* test_promise_args() {
  let message = ["test_promise_args", Promise.resolve(Math.random())];
  let stringified = JSON.stringify((yield Promise.resolve(Promise.all(message))));
  let result = yield worker.post("bounce", message);
  Assert.equal(JSON.stringify(result), stringified);
});

// Test that messages with delayed promise work
add_task(function* test_delayed_promise_args() {
  let promise = new Promise(resolve => setTimeout(() => resolve(Math.random()), 10));
  let message = ["test_delayed_promise_args", promise];
  let stringified = JSON.stringify((yield Promise.resolve(Promise.all(message))));
  let result = yield worker.post("bounce", message);
  Assert.equal(JSON.stringify(result), stringified);
});

// Test that messages with rejected promise cause appropriate errors
add_task(function* test_rejected_promise_args() {
  let error = new Error();
  let message = ["test_promise_args", Promise.reject(error)];
  try {
    yield worker.post("bounce", message);
    do_throw("I shound have thrown an error by now");
  } catch (ex if ex == error) {
    do_print("I threw the right error");
  }
});

function run_test() {
  run_next_test();
}
