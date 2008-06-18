Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");

Function.prototype.async = Async.sugar;

function secondGen() {
  let self = yield;

  Utils.makeTimerForCall(self.cb);

  self.done();
}

function runTestGenerator() {
  let self = yield;

  secondGen.async({}, self.cb);
  let result = yield;

  self.done();
}

function run_test() {
  Cu.import("resource://weave/log4moz.js");
  Cu.import("resource://weave/async.js");

  var fts = new FakeTimerService();
  var logStats = initTestLogging();

  runTestGenerator.async({});
  while (fts.processCallback()) {};
  do_check_eq(logStats.errorsLogged, 3);
}
