"use strict";

// This function here will prevent test() function in the HTML to run.
//add_task(async function noop() {
//});

// This does too.
/*
add_task(async function noop_with_promise() {
  await Promise.resolve();
});
*/

// This does too.
/*
add_task(async function noop_with_one_executeSoon() {
  await new Promise(resolve => SimpleTest.executeSoon(resolve));
});
*/

// While the function here can workaround the bug
// (need at least two executeSoon())
add_task(async function workaround_function_in_header() {
  await new Promise(resolve => SimpleTest.executeSoon(resolve));
  await new Promise(resolve => SimpleTest.executeSoon(resolve));
});
