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

function secondGen() {
  let self = yield;

  callbackQueue.push(self.cb);

  self.done();
}

function runTestGenerator() {
  let self = yield;

  secondGen.async({}, self.cb);
  let result = yield;

  self.done();
}

function run_test() {
  const Cu = Components.utils;

  Cu.import("resource://weave/log4moz.js");
  Cu.import("resource://weave/async.js");

  var errorsLogged = 0;

  function _TestFormatter() {}
  _TestFormatter.prototype = {
    format: function BF_format(message) {
      if (message.level == Log4Moz.Level.Error)
        errorsLogged += 1;
      return message.loggerName + "\t" + message.levelDesc + "\t" +
        message.message + "\n";
    }
  };
  _TestFormatter.prototype.__proto__ = new Log4Moz.Formatter();

  var log = Log4Moz.Service.rootLogger;
  var formatter = new _TestFormatter();
  var appender = new Log4Moz.DumpAppender(formatter);
  log.level = Log4Moz.Level.Trace;
  appender.level = Log4Moz.Level.Trace;
  log.addAppender(appender);

  runTestGenerator.async({});
  let i = 1;
  while (callbackQueue.length > 0) {
    let cb = callbackQueue.pop();
    cb();
    i += 1;
  }

  do_check_eq(errorsLogged, 3);
}
