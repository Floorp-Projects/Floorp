Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");

Function.prototype.async = Async.sugar;

function _ensureExceptionIsValid(e) {
  do_check_eq(e.message, "intentional failure");
  do_check_eq(typeof(e.asyncStack), "string");
  do_check_true(e.asyncStack.indexOf("thirdGen") > -1);
  do_check_true(e.asyncStack.indexOf("secondGen") > -1);
  do_check_true(e.asyncStack.indexOf("runTestGenerator") > -1);
  do_check_true(e.stack.indexOf("thirdGen") > -1);
  do_check_eq(e.stack.indexOf("secondGen"), -1);
  do_check_eq(e.stack.indexOf("runTestGenerator"), -1);
}

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
    ensureExceptionIsValid(e);
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
    ensureExceptionIsValid(e);
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
  do_check_eq(Async.outstandingGenerators.length, 0);
}
