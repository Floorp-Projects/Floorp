/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/osfile.jsm");

Cu.import("resource://services-common/log4moz.js");

let testFormatter = {
  format: function format(message) {
    return message.loggerName + "\t" +
      message.levelDesc + "\t" +
      message.message + "\n";
  }
};

function MockAppender(formatter) {
  Log4Moz.Appender.call(this, formatter);
  this.messages = [];
}
MockAppender.prototype = {
  __proto__: Log4Moz.Appender.prototype,

  doAppend: function DApp_doAppend(message) {
    this.messages.push(message);
  }
};

function run_test() {
  run_next_test();
}

add_test(function test_Logger() {
  let log = Log4Moz.repository.getLogger("test.logger");
  let appender = new MockAppender(new Log4Moz.BasicFormatter());

  log.level = Log4Moz.Level.Debug;
  appender.level = Log4Moz.Level.Info;
  log.addAppender(appender);
  log.info("info test");
  log.debug("this should be logged but not appended.");

  do_check_eq(appender.messages.length, 1);
  do_check_true(appender.messages[0].indexOf("info test") > 0);
  do_check_true(appender.messages[0].indexOf("INFO") > 0);

  run_next_test();
});

add_test(function test_Logger_parent() {
  // Check whether parenting is correct
  let grandparentLog = Log4Moz.repository.getLogger("grandparent");
  let childLog = Log4Moz.repository.getLogger("grandparent.parent.child");
  do_check_eq(childLog.parent.name, "grandparent");

  let parentLog = Log4Moz.repository.getLogger("grandparent.parent");
  do_check_eq(childLog.parent.name, "grandparent.parent");

  // Check that appends are exactly in scope
  let gpAppender = new MockAppender(new Log4Moz.BasicFormatter());
  gpAppender.level = Log4Moz.Level.Info;
  grandparentLog.addAppender(gpAppender);
  childLog.info("child info test");
  Log4Moz.repository.rootLogger.info("this shouldn't show up in gpAppender");

  do_check_eq(gpAppender.messages.length, 1);
  do_check_true(gpAppender.messages[0].indexOf("child info test") > 0);

  run_next_test();
});

add_test(function test_StorageStreamAppender() {
  let appender = new Log4Moz.StorageStreamAppender(testFormatter);
  do_check_eq(appender.getInputStream(), null);

  // Log to the storage stream and verify the log was written and can be
  // read back.
  let logger = Log4Moz.repository.getLogger("test.StorageStreamAppender");
  logger.addAppender(appender);
  logger.info("OHAI");
  let inputStream = appender.getInputStream();
  let data = NetUtil.readInputStreamToString(inputStream,
                                             inputStream.available());
  do_check_eq(data, "test.StorageStreamAppender\tINFO\tOHAI\n");

  // We can read it again even.
  let sndInputStream = appender.getInputStream();
  let sameData = NetUtil.readInputStreamToString(sndInputStream,
                                                 sndInputStream.available());
  do_check_eq(data, sameData);

  // Reset the appender and log some more.
  appender.reset();
  do_check_eq(appender.getInputStream(), null);
  logger.debug("wut?!?");
  inputStream = appender.getInputStream();
  data = NetUtil.readInputStreamToString(inputStream,
                                         inputStream.available());
  do_check_eq(data, "test.StorageStreamAppender\tDEBUG\twut?!?\n");

  run_next_test();
});

function fileContents(path) {
  let decoder = new TextDecoder();
  return OS.File.read(path).then(array => {
    return decoder.decode(array);
  });
}

add_task(function test_FileAppender() {
  // This directory does not exist yet
  let dir = OS.Path.join(do_get_profile().path, "test_log4moz");
  do_check_false(yield OS.File.exists(dir));
  let path = OS.Path.join(dir, "test_FileAppender");
  let appender = new Log4Moz.FileAppender(path, testFormatter);
  let logger = Log4Moz.repository.getLogger("test.FileAppender");
  logger.addAppender(appender);

  // Logging to a file that can't be created won't do harm.
  do_check_false(yield OS.File.exists(path));
  logger.info("OHAI!");

  yield OS.File.makeDir(dir);
  logger.info("OHAI");
  yield appender._lastWritePromise;

  do_check_eq((yield fileContents(path)),
              "test.FileAppender\tINFO\tOHAI\n");

  logger.info("OHAI");
  yield appender._lastWritePromise;

  do_check_eq((yield fileContents(path)),
              "test.FileAppender\tINFO\tOHAI\n" +
              "test.FileAppender\tINFO\tOHAI\n");

  // Reset the appender and log some more.
  yield appender.reset();
  do_check_false(yield OS.File.exists(path));

  logger.debug("O RLY?!?");
  yield appender._lastWritePromise;
  do_check_eq((yield fileContents(path)),
              "test.FileAppender\tDEBUG\tO RLY?!?\n");

  yield appender.reset();
  logger.debug("1");
  logger.info("2");
  logger.info("3");
  logger.info("4");
  logger.info("5");
  // Waiting on only the last promise should account for all of these.
  yield appender._lastWritePromise;

  // Messages ought to be logged in order.
  do_check_eq((yield fileContents(path)),
              "test.FileAppender\tDEBUG\t1\n" +
              "test.FileAppender\tINFO\t2\n" +
              "test.FileAppender\tINFO\t3\n" +
              "test.FileAppender\tINFO\t4\n" +
              "test.FileAppender\tINFO\t5\n");
});

add_task(function test_BoundedFileAppender() {
  let dir = OS.Path.join(do_get_profile().path, "test_log4moz");
  let path = OS.Path.join(dir, "test_BoundedFileAppender");
  // This appender will hold about two lines at a time.
  let appender = new Log4Moz.BoundedFileAppender(path, testFormatter, 40);
  let logger = Log4Moz.repository.getLogger("test.BoundedFileAppender");
  logger.addAppender(appender);

  logger.info("ONE");
  logger.info("TWO");
  yield appender._lastWritePromise;

  do_check_eq((yield fileContents(path)),
              "test.BoundedFileAppender\tINFO\tONE\n" +
              "test.BoundedFileAppender\tINFO\tTWO\n");

  logger.info("THREE");
  logger.info("FOUR");

  do_check_neq(appender._removeFilePromise, undefined);
  yield appender._removeFilePromise;
  yield appender._lastWritePromise;

  do_check_eq((yield fileContents(path)),
              "test.BoundedFileAppender\tINFO\tTHREE\n" +
              "test.BoundedFileAppender\tINFO\tFOUR\n");

  yield appender.reset();
  logger.info("ONE");
  logger.info("TWO");
  logger.info("THREE");
  logger.info("FOUR");

  do_check_neq(appender._removeFilePromise, undefined);
  yield appender._removeFilePromise;
  yield appender._lastWritePromise;

  do_check_eq((yield fileContents(path)),
              "test.BoundedFileAppender\tINFO\tTHREE\n" +
              "test.BoundedFileAppender\tINFO\tFOUR\n");

});

