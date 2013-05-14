/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let DeferredTask =
  Components.utils.import("resource://gre/modules/DeferredTask.jsm", {}).DeferredTask;

function test() {
  waitForExplicitFinish();

  nextTest();
}

function nextTest() {
  let test = tests.shift();
  if (test) {
    info("Starting " + test.name);
    executeSoon(test.bind(null, nextTest));
  } else {
    executeSoon(finish);
  }
}

let tests = [
  function checkRanOnce(next) {
    // Test that a deferred task runs only once.
    let deferredTaskCounter = 0;
    let deferredTask = new DeferredTask(function checkDeferred() {
      is(++deferredTaskCounter, 1, "Deferred task ran once");
      if (deferredTaskCounter > 1)
        return; // shouldn't happen!
      next();
    }, 100);

    deferredTask.start();
    deferredTask.start();
    deferredTask.start();
  },

  function testOrdering(next) {
    // Test the deferred task running order.
    let ARan, BRan;
    let deferredTaskA = new DeferredTask(function () {
      info("Deferred task A running");
      ARan = true;
      ok(!BRan, "Deferred task B shouldn't have run yet");
    }, 10);
    let deferredTaskB = new DeferredTask(function () {
      info("Deferred task B running");
      BRan = true;
      ok(ARan, "Deferred task A should have run already");
      executeSoon(next);
    }, 100);

    deferredTaskA.start();
    deferredTaskB.start();
  },

  function testFlush(next) {
    // Test the flush method.
    let deferedTaskCounter = 0;
    let deferredTask = new DeferredTask(function () {
      is(++deferedTaskCounter, 1, "Deferred task should only run once");
    }, 10);

    is(deferedTaskCounter, 0, "Deferred task shouldn't run right away");
    deferredTask.flush();
    is(deferedTaskCounter, 0, "Deferred task shouldn't run after flush() if it wasn't started");
    deferredTask.start();
    is(deferedTaskCounter, 0, "Deferred task shouldn't run immediately after start");
    deferredTask.flush();
    is(deferedTaskCounter, 1, "Deferred task should run after flush() after being started");
    next();
  },

  function testCancel(next) {
    // Test the cancel method.
    let deferredTask1, deferredTask2, deferredTask3;
    let deferredTask2Ran = false;
    deferredTask1 = new DeferredTask(function () {
      info("Deferred task 1 running");
      deferredTask2.cancel();
      info("Deferred task 2 canceled");
    }, 10);
    deferredTask2 = new DeferredTask(function () {
      info("Deferred task 2 running");
      deferredTask2Ran = true;
    }, 100);
    deferredTask3 = new DeferredTask(function () {
      info("Deferred task 3 running");
      is(deferredTask2Ran, false, "Deferred task 2 shouldn't run");
      next();
    }, 200);

    deferredTask1.start();
    deferredTask2.start();
    deferredTask3.start();
  },

  function testIsPending(next) {
    // Test the pending property.
    let deferredTask1, deferredTask2;
    let deferredTask1 = new DeferredTask(function () {
      info("Deferred task 1 running");
      is(deferredTask1.isPending, false, "Deferred task 1 shouldn't be pending");
      is(deferredTask2.isPending, true, "Deferred task 2 should be pending");
    }, 100);
    let deferredTask2 = new DeferredTask(function () {
      info("Deferred task 2 running");
      is(deferredTask1.isPending, false, "Deferred task 1 shouldn't be pending");
      is(deferredTask2.isPending, false, "Deferred task 2 shouldn't be pending");
      next();
    }, 200);

    is(deferredTask1.isPending, false, "Deferred task 1 shouldn't be pending");
    is(deferredTask2.isPending, false, "Deferred task 2 shouldn't be pending");
    deferredTask1.start();
    deferredTask2.start();
    is(deferredTask1.isPending, true, "Deferred task 1 should be pending");
    is(deferredTask2.isPending, true, "Deferred task 2 should be pending");
  },

  function testRestartingTask(next) {
    // Test restarting a task from other task. 
    let deferredTask1, deferredTask2, deferredTask3;
    let deferredTask1 = new DeferredTask(function () {
      info("Deferred task 1 running");
      is(deferredTask1.isPending, false, "Deferred task 1 shouldn't be pending");
      is(deferredTask2.isPending, false, "Deferred task 2 shouldn't be pending");
      is(deferredTask3.isPending, false, "Deferred task 3 shouldn't be pending");
      next();
    }, 100);
    let deferredTask2 = new DeferredTask(function () {
      info("Deferred task 2 running");
      deferredTask1.start();
      deferredTask3.start();
    }, 50);
    let deferredTask3 = new DeferredTask(function () {
      info("Deferred task 3 running");
      is(deferredTask1.isPending, true, "Deferred task 1 should be pending");
    }, 75);

    deferredTask1.start();
    deferredTask2.start();
  },

  function testRestartItselfTask(next) {
    // Test restarting a task from the same task callback. 
    let deferredTask;
    let taskReRun = false;
    let deferredTask = new DeferredTask(function () {
      info("Deferred task running");
      is(deferredTask.isPending, false, "Deferred task shouldn't be pending");
      if (!taskReRun) {
        taskReRun = true;
        info("Deferred task restart");
        deferredTask.start();
        is(deferredTask.isPending, true, "Deferred task should be pending");
      } else {
        next();
      }
    }, 100);

    deferredTask.start();
  }
];
