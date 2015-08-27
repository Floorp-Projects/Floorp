var ExtensionTestUtils = {};

ExtensionTestUtils.loadExtension = function(name)
{
  var testResolve;
  var testDone = new Promise(resolve => { testResolve = resolve; });

  var messageHandler = new Map();

  function testHandler(kind, pass, msg, ...args) {
    if (kind == "test-eq") {
      var [expected, actual] = args;
      SimpleTest.ok(pass, `${msg} - Expected: ${expected}, Actual: ${actual}`);
    } else if (kind == "test-log") {
      SimpleTest.info(msg);
    } else if (kind == "test-result") {
      SimpleTest.ok(pass, msg);
    }
  }

  var handler = {
    testResult(kind, pass, msg, ...args) {
      if (kind == "test-done") {
        SimpleTest.ok(pass, msg);
        return testResolve(msg);
      }
      testHandler(kind, pass, msg, ...args);
    },

    testMessage(msg, ...args) {
      var handler = messageHandler.get(msg);
      if (!handler) {
        return;
      }

      handler(...args);
    },
  };

  var extension = SpecialPowers.loadExtension(name, handler);

  extension.awaitMessage = (msg) => {
    return new Promise(resolve => {
      if (messageHandler.has(msg)) {
        throw new Error("only one message handler allowed");
      }

      messageHandler.set(msg, (...args) => {
        messageHandler.delete(msg);
        resolve(...args);
      });
    });
  };

  extension.onMessage = (msg, callback) => {
    if (messageHandler.has(msg)) {
      throw new Error("only one message handler allowed");
    }
    messageHandler.set(msg, callback);
  };

  extension.awaitFinish = (msg) => {
    return testDone.then(actual => {
      if (msg) {
        SimpleTest.is(actual, msg, "test result correct");
      }
      return actual;
    });
  };

  SimpleTest.info(`Extension loaded`);
  return extension;
}
