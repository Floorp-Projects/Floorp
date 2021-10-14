/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

setExpectedFailuresForSelfTest(4);

async function rejectOnNextTick(error) {
  await Promise.resolve();

  Promise.reject(error);
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
