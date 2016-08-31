/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var Cu = Components.utils;

Cu.import("resource://gre/modules/PromiseWorker.jsm", this);
Cu.import("resource://gre/modules/Timer.jsm", this);

// Worker must be loaded from a chrome:// uri, not a file://
// uri, so we first need to load it.

var WORKER_SOURCE_URI = "chrome://promiseworker/content/worker.js";
do_load_manifest("data/chrome.manifest");
var worker = new BasePromiseWorker(WORKER_SOURCE_URI);
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
  } catch (ex) {
    if (ex != error)
      throw ex;
    do_print("I threw the right error");
  }
});

// Test that we can transfer to the worker using argument `transfer`
add_task(function* test_transfer_args() {
  let array = new Uint8Array(4);
  for (let i = 0; i < 4; ++i) {
    array[i] = i;
  }
  Assert.equal(array.buffer.byteLength, 4, "The buffer is not detached yet");

  let result = (yield worker.post("bounce", [array.buffer], [], [array.buffer]))[0];

  // Check that the buffer has been sent
  Assert.equal(array.buffer.byteLength, 0, "The buffer has been detached");

  // Check that the result is correct
  Assert.equal(result.byteLength, 4, "The result has the right size");
  let array2 = new Uint8Array(result);
  for (let i = 0; i < 4; ++i) {
    Assert.equal(array2[i], i);
  }
});

// Test that we can transfer to the worker using an instance of `Meta`
add_task(function* test_transfer_with_meta() {
  let array = new Uint8Array(4);
  for (let i = 0; i < 4; ++i) {
    array[i] = i;
  }
  Assert.equal(array.buffer.byteLength, 4, "The buffer is not detached yet");

  let message = new BasePromiseWorker.Meta(array, {transfers: [array.buffer]});
  let result = (yield worker.post("bounce", [message]))[0];

  // Check that the buffer has been sent
  Assert.equal(array.buffer.byteLength, 0, "The buffer has been detached");

  // Check that the result is correct
  Assert.equal(Object.prototype.toString.call(result), "[object Uint8Array]",
               "The result appears to be a Typed Array");
  Assert.equal(result.byteLength, 4, "The result has the right size");

  for (let i = 0; i < 4; ++i) {
    Assert.equal(result[i], i);
  }
});

function run_test() {
  run_next_test();
}
