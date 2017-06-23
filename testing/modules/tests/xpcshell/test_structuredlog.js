/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  Components.utils.import("resource://testing-common/StructuredLog.jsm");

  let testBuffer = [];

  let appendBuffer = function(msg) {
    testBuffer.push(JSON.stringify(msg));
  }

  let assertLastMsg = function(refData) {
    // Check all fields in refData agree with those in the
    // last message logged, and pop that message.
    let lastMsg = JSON.parse(testBuffer.pop());
    for (let field in refData) {
      deepEqual(lastMsg[field], refData[field]);
    }
    // The logger should always set the source to the logger name.
    equal(lastMsg.source, "test_log");
    // The source_file field is always set by the mutator function.
    equal(lastMsg.source_file, "test_structuredlog.js");
  }

  let addFileName = function(data) {
    data.source_file = "test_structuredlog.js";
  }

  let logger = new StructuredLogger("test_log", appendBuffer, [addFileName]);

  // Test unstructured logging
  logger.info("Test message");
  assertLastMsg({
    action: "log",
    message: "Test message",
    level: "INFO",
  });

  logger.info("Test message",
              extra = {foo: "bar"});
  assertLastMsg({
    action: "log",
    message: "Test message",
    level: "INFO",
    extra: {foo: "bar"},
  });

  // Test end / start actions
  logger.testStart("aTest");
  assertLastMsg({
    test: "aTest",
    action: "test_start",
  });

  logger.testEnd("aTest", "OK");
  assertLastMsg({
    test: "aTest",
    action: "test_end",
    status: "OK"
  });

  // A failed test populates the "expected" field.
  logger.testStart("aTest");
  logger.testEnd("aTest", "FAIL", "PASS");
  assertLastMsg({
    action: "test_end",
    test: "aTest",
    status: "FAIL",
    expected: "PASS"
  });

  // A failed test populates the "expected" field.
  logger.testStart("aTest");
  logger.testEnd("aTest", "FAIL", "PASS", null, "Many\nlines\nof\nstack\n");
  assertLastMsg({
    action: "test_end",
    test: "aTest",
    status: "FAIL",
    expected: "PASS",
    stack: "Many\nlines\nof\nstack\n"
  });

  // Skipped tests don't log failures
  logger.testStart("aTest");
  logger.testEnd("aTest", "SKIP", "PASS");
  ok(!JSON.parse(testBuffer[testBuffer.length - 1]).hasOwnProperty("expected"));
  assertLastMsg({
    action: "test_end",
    test: "aTest",
    status: "SKIP"
  });

  logger.testStatus("aTest", "foo", "PASS", "PASS", "Passed test");
  ok(!JSON.parse(testBuffer[testBuffer.length - 1]).hasOwnProperty("expected"));
  assertLastMsg({
    action: "test_status",
    test: "aTest",
    subtest: "foo",
    status: "PASS",
    message: "Passed test"
  });

  logger.testStatus("aTest", "bar", "FAIL");
  assertLastMsg({
    action: "test_status",
    test: "aTest",
    subtest: "bar",
    status: "FAIL",
    expected: "PASS"
  });

  logger.testStatus("aTest", "bar", "FAIL", "PASS", null,
                    "Many\nlines\nof\nstack\n");
  assertLastMsg({
    action: "test_status",
    test: "aTest",
    subtest: "bar",
    status: "FAIL",
    expected: "PASS",
    stack: "Many\nlines\nof\nstack\n"
  });

  // Skipped tests don't log failures
  logger.testStatus("aTest", "baz", "SKIP");
  ok(!JSON.parse(testBuffer[testBuffer.length - 1]).hasOwnProperty("expected"));
  assertLastMsg({
    action: "test_status",
    test: "aTest",
    subtest: "baz",
    status: "SKIP"
  });

  // Suite start and end messages.
  logger.suiteStart(["aTest"]);
  assertLastMsg({
    action: "suite_start",
    tests: ["aTest"],
  });

  logger.suiteEnd();
  assertLastMsg({
    action: "suite_end",
  });
}
