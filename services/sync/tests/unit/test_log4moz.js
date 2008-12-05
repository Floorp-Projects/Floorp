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

  // Test - check whether parenting is correct
  let grandparentLog = Log4Moz.repository.getLogger("grandparent");
  let childLog = Log4Moz.repository.getLogger("grandparent.parent.child");
  do_check_eq(childLog.parent.name, "grandparent");

  let parentLog = Log4Moz.repository.getLogger("grandparent.parent");
  do_check_eq(childLog.parent.name, "grandparent.parent");

  // Test - check that appends are exactly in scope
  let gpAppender = new MockAppender(new Log4Moz.BasicFormatter());
  gpAppender.level = Log4Moz.Level.Info;
  grandparentLog.addAppender(gpAppender);
  childLog.info("child info test");
  log.info("this shouldn't show up in gpAppender");

  do_check_eq(gpAppender.messages.length, 1);
  do_check_true(gpAppender.messages[0].indexOf("child info test") > 0);
}
