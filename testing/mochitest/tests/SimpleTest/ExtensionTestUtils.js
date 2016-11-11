var ExtensionTestUtils = {};

ExtensionTestUtils.loadExtension = function(ext)
{
  // Cleanup functions need to be registered differently depending on
  // whether we're in browser chrome or plain mochitests.
  var registerCleanup;
  if (typeof registerCleanupFunction != "undefined") {
    registerCleanup = registerCleanupFunction;
  } else {
    registerCleanup = SimpleTest.registerCleanupFunction.bind(SimpleTest);
  }

  var testResolve;
  var testDone = new Promise(resolve => { testResolve = resolve; });

  var messageHandler = new Map();
  var messageAwaiter = new Map();

  var messageQueue = new Set();

  registerCleanup(() => {
    if (messageQueue.size) {
      let names = Array.from(messageQueue, ([msg]) => msg);
      SimpleTest.is(JSON.stringify(names), "[]", "message queue is empty");
    }
    if (messageAwaiter.size) {
      let names = Array.from(messageAwaiter.keys());
      SimpleTest.is(JSON.stringify(names), "[]", "no tasks awaiting on messages");
    }
  });

  function checkMessages() {
    for (let message of messageQueue) {
      let [msg, ...args] = message;

      let listener = messageAwaiter.get(msg);
      if (listener) {
        messageQueue.delete(message);
        messageAwaiter.delete(msg);

        listener.resolve(...args);
        return;
      }
    }
  }

  function checkDuplicateListeners(msg) {
    if (messageHandler.has(msg) || messageAwaiter.has(msg)) {
      throw new Error("only one message handler allowed");
    }
  }

  function testHandler(kind, pass, msg, ...args) {
    if (kind == "test-eq") {
      let [expected, actual, stack] = args;
      SimpleTest.ok(pass, `${msg} - Expected: ${expected}, Actual: ${actual}`, undefined, stack);
    } else if (kind == "test-log") {
      SimpleTest.info(msg);
    } else if (kind == "test-result") {
      SimpleTest.ok(pass, msg, undefined, args[0]);
    }
  }

  var handler = {
    testResult(kind, pass, msg, ...args) {
      if (kind == "test-done") {
        SimpleTest.ok(pass, msg, undefined, args[0]);
        return testResolve(msg);
      }
      testHandler(kind, pass, msg, ...args);
    },

    testMessage(msg, ...args) {
      var handler = messageHandler.get(msg);
      if (handler) {
        handler(...args);
      } else {
        messageQueue.add([msg, ...args]);
        checkMessages();
      }

    },
  };

  // Mimic serialization of functions as done in `Extension.generateXPI` and
  // `Extension.generateZipFile` because functions are dropped when `ext` object
  // is sent to the main process via the message manager.
  ext = Object.assign({}, ext);
  if (ext.files) {
    ext.files = Object.assign({}, ext.files);
    for (let filename of Object.keys(ext.files)) {
      let file = ext.files[filename];
      if (typeof file == "function") {
        ext.files[filename] = `(${file})();`
      }
    }
  }
  if (typeof ext.background == "function") {
    ext.background = `(${ext.background})();`
  }

  var extension = SpecialPowers.loadExtension(ext, handler);

  registerCleanup(() => {
    if (extension.state == "pending" || extension.state == "running") {
      SimpleTest.ok(false, "Extension left running at test shutdown")
      return extension.unload();
    } else if (extension.state == "unloading") {
      SimpleTest.ok(false, "Extension not fully unloaded at test shutdown")
    }
  });

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
