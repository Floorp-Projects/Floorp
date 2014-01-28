/* -*- Mode: js; js-indent-level: 2; -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test DevToolsUtils.safeErrorString

function run_test() {
  test_with_error();
  test_with_tricky_error();
  test_with_string();
  test_with_thrower();
  test_with_psychotic();
}

function test_with_error() {
  let s = DevToolsUtils.safeErrorString(new Error("foo bar"));
  // Got the message.
  do_check_true(s.contains("foo bar"));
  // Got the stack.
  do_check_true(s.contains("test_with_error"))
  do_check_true(s.contains("test_safeErrorString.js"));
  // Got the lineNumber and columnNumber.
  do_check_true(s.contains("Line"));
  do_check_true(s.contains("column"));
}

function test_with_tricky_error() {
  let e = new Error("batman");
  e.stack = { toString: Object.create(null) };
  let s = DevToolsUtils.safeErrorString(e);
  // Still got the message, despite a bad stack property.
  do_check_true(s.contains("batman"));
}

function test_with_string() {
  let s = DevToolsUtils.safeErrorString("not really an error");
  // Still get the message.
  do_check_true(s.contains("not really an error"));
}

function test_with_thrower() {
  let s = DevToolsUtils.safeErrorString({
    toString: () => {
      throw new Error("Muahahaha");
    }
  });
  // Still don't fail, get string back.
  do_check_eq(typeof s, "string");
}

function test_with_psychotic() {
  let s = DevToolsUtils.safeErrorString({
    toString: () => Object.create(null)
  });
  // Still get a string out, and no exceptions thrown
  do_check_eq(typeof s, "string");
}
