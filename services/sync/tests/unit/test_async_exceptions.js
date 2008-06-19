Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");

Function.prototype.async = Async.sugar;

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
  var fts = new FakeTimerService();
  runTestGenerator.async({});
  for (var i = 0; fts.processCallback(); i++) {}
  do_check_eq(i, 4);
  do_check_eq(Async.outstandingGenerators, 0);
}
