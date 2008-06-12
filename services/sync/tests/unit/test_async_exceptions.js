const Cu = Components.utils;

Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");

Function.prototype.async = Async.sugar;

var callbackQueue = [];

Utils.makeTimerForCall = function fake_makeTimerForCall(cb) {
  // Just add the callback to our queue and we'll call it later, so
  // as to simulate a real nsITimer.
  callbackQueue.push(cb);
  return "fake nsITimer";
};

function thirdGen() {
  let self = yield;

  Utils.makeTimerForCall(self.cb);
  yield;

  throw new Error("intentional failure");
}

function secondGen() {
  let self = yield;

  thirdGen.async({}, self.cb);
  try {
    let result = yield;
  } catch (e) {
    do_check_eq(e.message, "Error: intentional failure");
    throw e;
  }

  self.done();
}

function runTestGenerator() {
  let self = yield;

  secondGen.async({}, self.cb);
  let wasCaught = false;
  try {
    let result = yield;
  } catch (e) {
    do_check_eq(e.message, "Error: intentional failure");
    wasCaught = true;
  }

  do_check_true(wasCaught);
  self.done();
}

function run_test() {
  runTestGenerator.async({});
  let i = 1;
  while (callbackQueue.length > 0) {
    let cb = callbackQueue.pop();
    cb();
    i += 1;
  }
  do_check_eq(i, 5);
}
