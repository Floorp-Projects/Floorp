/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

setExpectedFailuresForSelfTest(4);

function rejectOnNextTick(error) {
  return new Promise((resolve, reject) => executeSoon(() => reject(error)));
}

add_task(function failWithoutError() {
  yield rejectOnNextTick(undefined);
});

add_task(function failWithString() {
  yield rejectOnNextTick("This is a string");
});

add_task(function failWithInt() {
  yield rejectOnNextTick(42);
});

// This one should display a stack trace
add_task(function failWithError() {
  yield rejectOnNextTick(new Error("This is an error"));
});
