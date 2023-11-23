let lastMessage;
const consoleListener = {
  observe(message) {
    dump(" >> new message: " + message.errorMessage + "\n");
    lastMessage = message;
  },
};
Services.console.registerListener(consoleListener);

// The Console Service notifies its listener after one event loop cycle.
// So wait for one tick after each action dispatching a message/error to the service.
function waitForATick() {
  return new Promise(resolve => Services.tm.dispatchToMainThread(resolve));
}

add_task(async function customScriptError() {
  const scriptError = Cc["@mozilla.org/scripterror;1"].createInstance(
    Ci.nsIScriptError
  );
  scriptError.init(
    "foo",
    "file.js",
    null,
    1,
    2,
    Ci.nsIScriptError.warningFlag,
    "some javascript"
  );
  Services.console.logMessage(scriptError);

  await waitForATick();

  Assert.equal(
    lastMessage,
    scriptError,
    "We receive the exact same nsIScriptError object"
  );

  Assert.equal(lastMessage.errorMessage, "foo");
  Assert.equal(lastMessage.sourceName, "file.js");
  Assert.equal(lastMessage.lineNumber, 1);
  Assert.equal(lastMessage.columnNumber, 2);
  Assert.equal(lastMessage.flags, Ci.nsIScriptError.warningFlag);
  Assert.equal(lastMessage.category, "some javascript");

  Assert.equal(
    lastMessage.stack,
    undefined,
    "Custom nsIScriptError object created from JS can't convey any stack"
  );
});

add_task(async function callFunctionAndLogExceptionWithChromeGlobal() {
  try {
    Services.console.callFunctionAndLogException(globalThis, function () {
      throw new Error("custom exception");
    });
    Assert.fail("callFunctionAndLogException should throw");
  } catch (e) {
    Assert.equal(
      e.name,
      "NS_ERROR_XPC_JAVASCRIPT_ERROR",
      "callFunctionAndLogException thrown"
    );
  }

  await waitForATick();

  Assert.ok(!!lastMessage, "Got the message");
  Assert.ok(
    lastMessage instanceof Ci.nsIScriptError,
    "This is a nsIScriptError"
  );

  Assert.equal(lastMessage.errorMessage, "Error: custom exception");
  Assert.equal(lastMessage.sourceName, _TEST_FILE);
  Assert.equal(lastMessage.lineNumber, 56);
  Assert.equal(lastMessage.columnNumber, 13);
  Assert.equal(lastMessage.flags, Ci.nsIScriptError.errorFlag);
  Assert.equal(lastMessage.category, "chrome javascript");
  Assert.ok(lastMessage.stack, "It has a stack");
  Assert.equal(lastMessage.stack.source, _TEST_FILE);
  Assert.equal(lastMessage.stack.line, 56);
  Assert.equal(lastMessage.stack.column, 13);
  Assert.ok(!!lastMessage.stack.parent, "stack has a parent frame");
  Assert.equal(
    lastMessage.innerWindowID,
    0,
    "The message isn't bound to any WindowGlobal"
  );
});

add_task(async function callFunctionAndLogExceptionWithContentGlobal() {
  const window = createContentWindow();
  try {
    Services.console.callFunctionAndLogException(window, function () {
      throw new Error("another custom exception");
    });
    Assert.fail("callFunctionAndLogException should throw");
  } catch (e) {
    Assert.equal(
      e.name,
      "NS_ERROR_XPC_JAVASCRIPT_ERROR",
      "callFunctionAndLogException thrown"
    );
  }

  await waitForATick();

  Assert.ok(!!lastMessage, "Got the message");
  Assert.ok(
    lastMessage instanceof Ci.nsIScriptError,
    "This is a nsIScriptError"
  );

  Assert.equal(lastMessage.errorMessage, "Error: another custom exception");
  Assert.equal(lastMessage.sourceName, _TEST_FILE);
  Assert.equal(lastMessage.lineNumber, 97);
  Assert.equal(lastMessage.columnNumber, 13);
  Assert.equal(lastMessage.flags, Ci.nsIScriptError.errorFlag);
  Assert.equal(lastMessage.category, "content javascript");
  Assert.ok(lastMessage.stack, "It has a stack");
  Assert.equal(lastMessage.stack.source, _TEST_FILE);
  Assert.equal(lastMessage.stack.line, 97);
  Assert.equal(lastMessage.stack.column, 13);
  Assert.ok(!!lastMessage.stack.parent, "stack has a parent frame");
  Assert.ok(
    !!window.windowGlobalChild.innerWindowId,
    "The window has a innerWindowId"
  );
  Assert.equal(
    lastMessage.innerWindowID,
    window.windowGlobalChild.innerWindowId,
    "The message is bound to the content window"
  );
});

add_task(async function callFunctionAndLogExceptionForContentScriptSandboxes() {
  const { sandbox, window } = createContentScriptSandbox();
  Cu.evalInSandbox(
    `function foo() { throw new Error("sandbox exception"); }`,
    sandbox,
    null,
    "sandbox-file.js",
    1,
    0
  );
  try {
    Services.console.callFunctionAndLogException(window, sandbox.foo);
    Assert.fail("callFunctionAndLogException should throw");
  } catch (e) {
    Assert.equal(
      e.name,
      "NS_ERROR_XPC_JAVASCRIPT_ERROR",
      "callFunctionAndLogException thrown"
    );
  }

  await waitForATick();

  Assert.ok(!!lastMessage, "Got the message");
  // Note that it is important to "instanceof" in order to expose the nsIScriptError attributes.
  Assert.ok(
    lastMessage instanceof Ci.nsIScriptError,
    "This is a nsIScriptError"
  );

  Assert.equal(lastMessage.errorMessage, "Error: sandbox exception");
  Assert.equal(lastMessage.sourceName, "sandbox-file.js");
  Assert.equal(lastMessage.lineNumber, 1);
  Assert.equal(lastMessage.columnNumber, 24);
  Assert.equal(lastMessage.flags, Ci.nsIScriptError.errorFlag);
  Assert.equal(lastMessage.category, "content javascript");
  Assert.ok(lastMessage.stack, "It has a stack");
  Assert.equal(lastMessage.stack.source, "sandbox-file.js");
  Assert.equal(lastMessage.stack.line, 1);
  Assert.equal(lastMessage.stack.column, 24);
  Assert.ok(!!lastMessage.stack.parent, "stack has a parent frame");
  Assert.ok(
    !!window.windowGlobalChild.innerWindowId,
    "The sandbox's prototype is a window and has a innerWindowId"
  );
  Assert.equal(
    lastMessage.innerWindowID,
    window.windowGlobalChild.innerWindowId,
    "The message is bound to the sandbox's prototype WindowGlobal"
  );
});

add_task(
  async function callFunctionAndLogExceptionForContentScriptSandboxesWrappedInChrome() {
    const { sandbox, window } = createContentScriptSandbox();
    Cu.evalInSandbox(
      `function foo() { throw new Error("sandbox exception"); }`,
      sandbox,
      null,
      "sandbox-file.js",
      1,
      0
    );
    try {
      Services.console.callFunctionAndLogException(window, function () {
        sandbox.foo();
      });
      Assert.fail("callFunctionAndLogException should throw");
    } catch (e) {
      Assert.equal(
        e.name,
        "NS_ERROR_XPC_JAVASCRIPT_ERROR",
        "callFunctionAndLogException thrown"
      );
    }

    await waitForATick();

    Assert.ok(!!lastMessage, "Got the message");
    // Note that it is important to "instanceof" in order to expose the nsIScriptError attributes.
    Assert.ok(
      lastMessage instanceof Ci.nsIScriptError,
      "This is a nsIScriptError"
    );

    Assert.ok(
      !!window.windowGlobalChild.innerWindowId,
      "The sandbox's prototype is a window and has a innerWindowId"
    );
    Assert.equal(
      lastMessage.innerWindowID,
      window.windowGlobalChild.innerWindowId,
      "The message is bound to the sandbox's prototype WindowGlobal"
    );
  }
);

add_task(function teardown() {
  Services.console.unregisterListener(consoleListener);
});

// We are in xpcshell, so we can't have a real DOM Window as in Firefox
// but let's try to have a fake one.
function createContentWindow() {
  const principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "http://example.com/"
    );

  const webnav = Services.appShell.createWindowlessBrowser(false);

  webnav.docShell.createAboutBlankDocumentViewer(principal, principal);

  return webnav.document.defaultView;
}

// Create a Sandbox as in WebExtension content scripts
function createContentScriptSandbox() {
  const window = createContentWindow();
  // The sandboxPrototype is the key here in order to
  // make xpc::SandboxWindowOrNull ignore the sandbox
  // and instead retrieve its prototype and link the error message
  // to the window instead of the sandbox.
  return {
    sandbox: Cu.Sandbox(window, { sandboxPrototype: window }),
    window,
  };
}
