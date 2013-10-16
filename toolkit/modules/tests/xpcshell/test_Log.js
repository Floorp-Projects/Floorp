/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/osfile.jsm");

Cu.import("resource://gre/modules/Log.jsm");

let testFormatter = {
  format: function format(message) {
    return message.loggerName + "\t" +
      message.levelDesc + "\t" +
      message.message + "\n";
  }
};

function MockAppender(formatter) {
  Log.Appender.call(this, formatter);
  this.messages = [];
}
MockAppender.prototype = {
  __proto__: Log.Appender.prototype,

  doAppend: function DApp_doAppend(message) {
    this.messages.push(message);
  }
};

function run_test() {
  run_next_test();
}

add_test(function test_Logger() {
  let log = Log.repository.getLogger("test.logger");
  let appender = new MockAppender(new Log.BasicFormatter());

  log.level = Log.Level.Debug;
  appender.level = Log.Level.Info;
  log.addAppender(appender);
  log.info("info test");
  log.debug("this should be logged but not appended.");

  do_check_eq(appender.messages.length, 1);

  let msgRe = /\d+\ttest.logger\t\INFO\tinfo test/;
  do_check_true(msgRe.test(appender.messages[0]));

  run_next_test();
});

add_test(function test_Logger_parent() {
  // Check whether parenting is correct
  let grandparentLog = Log.repository.getLogger("grandparent");
  let childLog = Log.repository.getLogger("grandparent.parent.child");
  do_check_eq(childLog.parent.name, "grandparent");

  let parentLog = Log.repository.getLogger("grandparent.parent");
  do_check_eq(childLog.parent.name, "grandparent.parent");

  // Check that appends are exactly in scope
  let gpAppender = new MockAppender(new Log.BasicFormatter());
  gpAppender.level = Log.Level.Info;
  grandparentLog.addAppender(gpAppender);
  childLog.info("child info test");
  Log.repository.rootLogger.info("this shouldn't show up in gpAppender");

  do_check_eq(gpAppender.messages.length, 1);
  do_check_true(gpAppender.messages[0].indexOf("child info test") > 0);

  run_next_test();
});

// A utility method for checking object equivalence.
// Fields with a reqular expression value in expected will be tested
// against the corresponding value in actual. Otherwise objects
// are expected to have the same keys and equal values.
function checkObjects(expected, actual) {
  do_check_true(expected instanceof Object);
  do_check_true(actual instanceof Object);
  for (let key in expected) {
    do_check_neq(actual[key], undefined);
    if (expected[key] instanceof RegExp) {
      do_check_true(expected[key].test(actual[key].toString()));
    } else {
      if (expected[key] instanceof Object) {
        checkObjects(expected[key], actual[key]);
      } else {
        do_check_eq(expected[key], actual[key]);
      }
    }
  }

  for (let key in actual) {
    do_check_neq(expected[key], undefined);
  }
}

add_test(function test_StructuredLogCommands() {
  let appender = new MockAppender(new Log.StructuredFormatter());
  let logger = Log.repository.getLogger("test.StructuredOutput");
  logger.addAppender(appender);
  logger.level = Log.Level.Info;

  logger.logStructured("test_message", {_message: "message string one"});
  logger.logStructured("test_message", {_message: "message string two",
                                        _level: "ERROR",
                                        source_file: "test_Log.js"});
  logger.logStructured("test_message");
  logger.logStructured("test_message", {source_file: "test_Log.js",
                                        message_position: 4});

  let messageOne = {"_time": /\d+/,
                    "_namespace": "test.StructuredOutput",
                    "_level": "INFO",
                    "_message": "message string one",
                    "action": "test_message"};

  let messageTwo = {"_time": /\d+/,
                    "_namespace": "test.StructuredOutput",
                    "_level": "ERROR",
                    "_message": "message string two",
                    "action": "test_message",
                    "source_file": "test_Log.js"};

  let messageThree = {"_time": /\d+/,
                      "_namespace": "test.StructuredOutput",
                      "_level": "INFO",
                      "action": "test_message"};

  let messageFour = {"_time": /\d+/,
                     "_namespace": "test.StructuredOutput",
                     "_level": "INFO",
                     "action": "test_message",
                     "source_file": "test_Log.js",
                     "message_position": 4};

  checkObjects(messageOne, JSON.parse(appender.messages[0]));
  checkObjects(messageTwo, JSON.parse(appender.messages[1]));
  checkObjects(messageThree, JSON.parse(appender.messages[2]));
  checkObjects(messageFour, JSON.parse(appender.messages[3]));

  let errored = false;
  try {
    logger.logStructured("", {_message: "invalid message"});
  } catch (e) {
    errored = true;
    do_check_eq(e, "An action is required when logging a structured message.");
  } finally {
    do_check_true(errored);
  }

  let errored = false;
  try {
    logger.logStructured("message_action", "invalid params");
  } catch (e) {
    errored = true;
    do_check_eq(e, "The params argument is required to be an object.");
  } finally {
    do_check_true(errored);
  }

  // Logging with unstructured interface should produce the same messages
  // as the structured interface for these cases.
  let appender = new MockAppender(new Log.StructuredFormatter());
  let logger = Log.repository.getLogger("test.StructuredOutput1");
  messageOne._namespace = "test.StructuredOutput1";
  messageTwo._namespace = "test.StructuredOutput1";
  logger.addAppender(appender);
  logger.level = Log.Level.All;
  logger.info("message string one", {action: "test_message"});
  logger.error("message string two", {action: "test_message",
                                      source_file: "test_Log.js"});

  checkObjects(messageOne, JSON.parse(appender.messages[0]));
  checkObjects(messageTwo, JSON.parse(appender.messages[1]));

  run_next_test();
});

add_test(function test_StorageStreamAppender() {
  let appender = new Log.StorageStreamAppender(testFormatter);
  do_check_eq(appender.getInputStream(), null);

  // Log to the storage stream and verify the log was written and can be
  // read back.
  let logger = Log.repository.getLogger("test.StorageStreamAppender");
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
  let dir = OS.Path.join(do_get_profile().path, "test_Log");
  do_check_false(yield OS.File.exists(dir));
  let path = OS.Path.join(dir, "test_FileAppender");
  let appender = new Log.FileAppender(path, testFormatter);
  let logger = Log.repository.getLogger("test.FileAppender");
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
  let dir = OS.Path.join(do_get_profile().path, "test_Log");
  let path = OS.Path.join(dir, "test_BoundedFileAppender");
  // This appender will hold about two lines at a time.
  let appender = new Log.BoundedFileAppender(path, testFormatter, 40);
  let logger = Log.repository.getLogger("test.BoundedFileAppender");
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

