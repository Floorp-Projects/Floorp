/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

Cu.import("resource://services-common/log4moz.js");

let testFormatter = {
  format: function format(message) {
    return message.loggerName + "\t" + message.levelDesc + "\t" +
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

function readFile(file) {
  let fstream = Cc["@mozilla.org/network/file-input-stream;1"]
                  .createInstance(Ci.nsIFileInputStream);
  fstream.init(file, -1, 0, 0);
  let data = NetUtil.readInputStreamToString(fstream, fstream.available());
  fstream.close();
  return data;
}

add_test(function test_FileAppender() {
  // This directory does not exist yet
  let dir = FileUtils.getFile("ProfD", ["test_log4moz"]);
  do_check_false(dir.exists());

  let file = dir.clone();
  file.append("test_FileAppender");
  let appender = new Log4Moz.FileAppender(file, testFormatter);

  // Logging against to a file that can't be created won't do harm.
  let logger = Log4Moz.repository.getLogger("test.FileAppender");
  logger.addAppender(appender);
  logger.info("OHAI");

  // The file will be written Once the directory leading up to the file exists.
  dir.create(Ci.nsILocalFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  logger.info("OHAI");
  do_check_true(file.exists());
  do_check_eq(readFile(file), "test.FileAppender\tINFO\tOHAI\n");

  // Reset the appender and log some more.
  appender.reset();
  do_check_false(file.exists());
  logger.debug("O RLY?!?");
  do_check_true(file.exists());
  do_check_eq(readFile(file), "test.FileAppender\tDEBUG\tO RLY?!?\n");

  run_next_test();
});
