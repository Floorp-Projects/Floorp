var ExtensionTestUtils = {};

ExtensionTestUtils.loadExtension = function(ext, id = null)
{
  var testResolve;
  var testDone = new Promise(resolve => { testResolve = resolve; });

  var messageHandler = new Map();
  var messageAwaiter = new Map();

  var messageQueue = [];

  SimpleTest.registerCleanupFunction(() => {
    if (messageQueue.length) {
      SimpleTest.is(messageQueue.length, 0, "message queue is empty");
    }
    if (messageAwaiter.size) {
      SimpleTest.is(messageAwaiter.size, 0, "no tasks awaiting on messages");
    }
  });

  function checkMessages() {
    while (messageQueue.length) {
      let [msg, ...args] = messageQueue[0];

      let listener = messageAwaiter.get(msg);
      if (!listener) {
        break;
      }

      messageQueue.shift();
      messageAwaiter.delete(msg);

      listener.resolve(...args);
    }
  }

  function checkDuplicateListeners(msg) {
    if (messageHandler.has(msg) || messageAwaiter.has(msg)) {
      throw new Error("only one message handler allowed");
    }
  }

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
      if (handler) {
        handler(...args);
      } else {
        messageQueue.push([msg, ...args]);
        checkMessages();
      }

    },
  };

  var extension = SpecialPowers.loadExtension(id, ext, handler);

  extension.awaitMessage = (msg) => {
    return new Promise(resolve => {
      checkDuplicateListeners(msg);

      messageAwaiter.set(msg, {resolve});
      checkMessages();
    });
  };

  extension.onMessage = (msg, callback) => {
    checkDuplicateListeners(msg);
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
