function testHandler(kind, pass, msg, ...args) {
  if (kind == "test-eq") {
    let [expected, actual] = args;
    SimpleTest.ok(pass, `${msg} - Expected: ${expected}, Actual: ${actual}`);
  } else if (kind == "test-log") {
    SimpleTest.info(msg);
  } else if (kind == "test-result") {
    SimpleTest.ok(pass, msg);
  }
}

function loadExtension(name)
{
  let testResolve;
  let testDone = new Promise(resolve => { testResolve = resolve; });

  let messageResolve = new Map();

  let handler = {
    testResult(kind, pass, msg, ...args) {
      if (kind == "test-done") {
        ok(pass, msg);
        return testResolve();
      }
      testHandler(kind, pass, msg, ...args);
    },

    testMessage(msg, ...args) {
      let resolve = messageResolve.get(msg);
      if (!resolve) {
        return;
      }
      messageResolve.delete(msg);

      resolve(...args);
    },
  };

  let target = "resource://testing-common/extensions/" + name + "/";
  let resourceHandler = SpecialPowers.Services.io.getProtocolHandler("resource")
                                     .QueryInterface(SpecialPowers.Ci.nsISubstitutingProtocolHandler);
  let url = SpecialPowers.Services.io.newURI(target, null, null);
  let filePath = resourceHandler.resolveURI(url);

  let extension = SpecialPowers.loadExtension(filePath, handler);

  extension.awaitMessage = (msg) => {
    return new Promise(resolve => {
      messageResolve.set(msg, resolve);
    });
  };

  extension.awaitFinish = () => {
    return testDone;
  };

  SimpleTest.info(`Extension ${name} loaded`);
  return extension;
}
