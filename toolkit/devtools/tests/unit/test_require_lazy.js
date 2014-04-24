/* -*- Mode: js; js-indent-level: 2; -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test devtools.lazyRequireGetter

function run_test() {
  const o = {};
  devtools.lazyRequireGetter(o, "asyncUtils", "devtools/async-utils");
  const asyncUtils = devtools.require("devtools/async-utils");
  // XXX: do_check_eq only works on primitive types, so we have this
  // do_check_true of an equality expression.
  do_check_true(o.asyncUtils === asyncUtils);
}
