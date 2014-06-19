/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This file makes the common testing infrastructure available to the browser
 * tests located in this folder.  This is only used as an infrastructure file,
 * and new common functions should be added to the "head_common.js" file.
 */

"use strict";

let ChromeUtils = {};
Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/ChromeUtils.js", ChromeUtils);

/* --- Adapters for the mochitest-browser-chrome infrastructure --- */

let Output = {
  print: info,
};

let Assert = {
  ok: function (actual) {
    let stack = Components.stack.caller;
    ok(actual, "[" + stack.name + " : " + stack.lineNumber + "] " + actual +
               " == true");
  },
  equal: function (actual, expected) {
    let stack = Components.stack.caller;
    is(actual, expected, "[" + stack.name + " : " + stack.lineNumber + "] " +
               actual + " == " + expected);
  },
};

/* --- Shared infrastructure --- */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/" +
  "toolkit/components/formautofill/test/browser/head_common.js", this);
