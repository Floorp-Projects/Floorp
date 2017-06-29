/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

setExpectedFailuresForSelfTest(8);

// Keep "JSMPromise" separate so "Promise" still refers to native Promises.
let JSMPromise = Cu.import("resource://gre/modules/Promise.jsm", {}).Promise;

async function rejectOnNextTick(error) {
  await Promise.resolve();

  Promise.reject(error);
  JSMPromise.reject(error);
}

add_task(async function failWithoutError() {
  await rejectOnNextTick(undefined);
});

add_task(async function failWithString() {
  await rejectOnNextTick("This is a string");
});

add_task(async function failWithInt() {
  await rejectOnNextTick(42);
});

// This one should display a stack trace
add_task(async function failWithError() {
  await rejectOnNextTick(new Error("This is an error"));
});
