Components.utils.import("resource://weave/log4moz.js");

function MockAppender(formatter) {
  this._formatter = formatter;
  this.messages = [];
}
MockAppender.prototype = {
  doAppend: function DApp_doAppend(message) {
    this.messages.push(message);
  }
};
MockAppender.prototype.__proto__ = new Log4Moz.Appender();

function run_test() {
  var log = Log4Moz.repository.rootLogger;
  var appender = new MockAppender(new Log4Moz.BasicFormatter());

  log.level = Log4Moz.Level.Debug;
  appender.level = Log4Moz.Level.Info;
  log.addAppender(appender);
  log.info("info test");
  log.debug("this should be logged but not appended.");

  do_check_eq(appender.messages.length, 1);
  do_check_true(appender.messages[0].indexOf("info test") > 0);
  do_check_true(appender.messages[0].indexOf("INFO") > 0);
}
