/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This file makes the common testing infrastructure available to the chrome
 * tests located in this folder.  This is only used as an infrastructure file,
 * and new common functions should be added to the "head_common.js" file.
 */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);

Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/SimpleTest.js", this);

let ChromeUtils = {};
Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/ChromeUtils.js", ChromeUtils);

/* --- Adapters for the mochitest-chrome infrastructure --- */

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

let executeSoon = SimpleTest.executeSoon;

let gTestTasks = [];
let add_task = taskFn => gTestTasks.push(taskFn);

SimpleTest.waitForExplicitFinish();

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad);

  Task.spawn(function* () {
    try {
      for (let taskFn of gTestTasks) {
        info("Running " + taskFn.name);
        yield Task.spawn(taskFn);
      }
    } catch (ex) {
      ok(false, ex);
    }

    SimpleTest.finish();
  });
});

/* --- Shared infrastructure --- */

let headUrl = "chrome://mochitests/content/chrome/" +
              "toolkit/components/formautofill/test/chrome/head_common.js";
Services.scriptloader.loadSubScript(headUrl, this);
