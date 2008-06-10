const Cu = Components.utils;

Cu.import("resource://weave/sharing.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/log4moz.js");

function runTestGenerator() {
  let self = yield;

  let fakeDav = {
    POST: function fakeDav_POST(url, data, callback) {
      let result = {status: 200, responseText: "OK"};
      Utils.makeTimerForCall(function() { callback(result); });
    }
  };

  let api = new Sharing.Api(fakeDav);
  api.shareWithUsers("/fake/dir", ["johndoe"], self.cb);
  let result = yield;

  do_check_eq(result.wasSuccessful, true);
  self.done();
}

function BasicFormatter() {
  this.errors = 0;
}
BasicFormatter.prototype = {
  format: function BF_format(message) {
    if (message.level == Log4Moz.Level.Error)
      this.errors += 1;
    return message.loggerName + "\t" + message.levelDesc + "\t" +
      message.message + "\n";
  }
};
BasicFormatter.prototype.__proto__ = new Log4Moz.Formatter();

function run_test() {
  var log = Log4Moz.Service.rootLogger;
  var formatter = new BasicFormatter();
  var appender = new Log4Moz.DumpAppender(formatter);
  log.level = Log4Moz.Level.Debug;
  appender.level = Log4Moz.Level.Debug;
  log.addAppender(appender);

  do_test_pending();

  let onComplete = function() {
    if (formatter.errors)
      do_throw("Errors were logged.");
    else
      do_test_finished();
  };

  Async.run({}, runTestGenerator, onComplete);
}
