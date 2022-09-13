/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* globals chrome */

const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

const PREF_MAX_READ = "webextensions.native-messaging.max-input-message-bytes";
const PREF_MAX_WRITE =
  "webextensions.native-messaging.max-output-message-bytes";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

const server = createHttpServer({ hosts: ["example.com"] });

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("<!DOCTYPE html><html></html>");
});

const ECHO_BODY = String.raw`
  import struct
  import sys

  stdin = getattr(sys.stdin, 'buffer', sys.stdin)
  stdout = getattr(sys.stdout, 'buffer', sys.stdout)

  while True:
    rawlen = stdin.read(4)
    if len(rawlen) == 0:
      sys.exit(0)
    msglen = struct.unpack('@I', rawlen)[0]
    msg = stdin.read(msglen)

    stdout.write(struct.pack('@I', msglen))
    stdout.write(msg)
`;

const INFO_BODY = String.raw`
  import json
  import os
  import struct
  import sys

  msg = json.dumps({"args": sys.argv, "cwd": os.getcwd()})
  if sys.version_info >= (3,):
    sys.stdout.buffer.write(struct.pack('@I', len(msg)))
  else:
    sys.stdout.write(struct.pack('@I', len(msg)))
  sys.stdout.write(msg)
  sys.exit(0)
`;

const DELAYED_ECHO_BODY = String.raw`
  import atexit
  import json
  import os
  import struct
  import sys
  import time

  stdin = getattr(sys.stdin, 'buffer', sys.stdin)
  stdout = getattr(sys.stdout, 'buffer', sys.stdout)
  pid = os.getpid()

  sys.stderr.write("nativeapp with pid %d is running\n" % pid)

  def onexit():
    sys.stderr.write("nativeapp with pid %d is exiting\n" % pid)

  atexit.register(onexit)

  while True:
    rawlen = stdin.read(4)
    if len(rawlen) == 0:
      sys.exit(0)
    msglen = struct.unpack('@I', rawlen)[0]
    msg = stdin.read(msglen)

    sys.stderr.write(
      "nativeapp with pid %d delaying echoing message '%s'\n" %
      (pid, str(msg, 'utf-8'))
    )

    time.sleep(5)
    stdout.write(struct.pack('@I', msglen))
    stdout.write(msg)

    sys.stderr.write(
      "nativeapp with pid %d replied to message '%s'\n" %
      (pid, str(msg, 'utf-8'))
    )
`;

const STDERR_LINES = ["hello stderr", "this should be a separate line"];
let STDERR_MSG = STDERR_LINES.join("\\n");

const STDERR_BODY = String.raw`
  import sys
  sys.stderr.write("${STDERR_MSG}")
`;

let SCRIPTS = [
  {
    name: "echo",
    description: "a native app that echoes back messages it receives",
    script: ECHO_BODY.replace(/^ {2}/gm, ""),
  },
  {
    name: "delayedecho",
    description:
      "a native app that echo messages received with a small artificial delay",
    script: DELAYED_ECHO_BODY.replace(/^ {2}/gm, ""),
  },
  {
    name: "info",
    description: "a native app that gives some info about how it was started",
    script: INFO_BODY.replace(/^ {2}/gm, ""),
  },
  {
    name: "stderr",
    description: "a native app that writes to stderr and then exits",
    script: STDERR_BODY.replace(/^ {2}/gm, ""),
  },
];

if (AppConstants.platform == "win") {
  SCRIPTS.push({
    name: "echocmd",
    description: "echo but using a .cmd file",
    scriptExtension: "cmd",
    script: ECHO_BODY.replace(/^ {2}/gm, ""),
  });
}

add_setup(async function setup() {
  optionalPermissionsPromptHandler.init();
  optionalPermissionsPromptHandler.acceptPrompt = true;
  await AddonTestUtils.promiseStartupManager();

  await setupHosts(SCRIPTS);
});

// Test the basic operation of native messaging with a simple
// script that echoes back whatever message is sent to it.
add_task(async function test_happy_path() {
  async function background() {
    let port;
    browser.test.onMessage.addListener(async (what, payload) => {
      if (what == "request") {
        await browser.permissions.request({ permissions: ["nativeMessaging"] });
        // connectNative requires permission
        port = browser.runtime.connectNative("echo");
        port.onMessage.addListener(msg => {
          browser.test.sendMessage("message", msg);
        });
        browser.test.sendMessage("ready");
      } else if (what == "send") {
        if (payload._json) {
          let json = payload._json;
          payload.toJSON = () => json;
          delete payload._json;
        }
        port.postMessage(payload);
      }
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      applications: { gecko: { id: ID } },
      optional_permissions: ["nativeMessaging"],
    },
    useAddonManager: "temporary",
  });

  await extension.startup();
  await withHandlingUserInput(extension, async () => {
    extension.sendMessage("request");
    await extension.awaitMessage("ready");
  });
  const tests = [
    {
      data: "this is a string",
      what: "simple string",
    },
    {
      data: "Это юникода",
      what: "unicode string",
    },
    {
      data: { test: "hello" },
      what: "simple object",
    },
    {
      data: {
        what: "An object with a few properties",
        number: 123,
        bool: true,
        nested: { what: "another object" },
      },
      what: "object with several properties",
    },

    {
      data: {
        ignoreme: true,
        _json: { data: "i have a tojson method" },
      },
      expected: { data: "i have a tojson method" },
      what: "object with toJSON() method",
    },
  ];
  for (let test of tests) {
    extension.sendMessage("send", test.data);
    let response = await extension.awaitMessage("message");
    let expected = test.expected || test.data;
    deepEqual(response, expected, `Echoed a message of type ${test.what}`);
  }

  let procCount = await getSubprocessCount();
  equal(procCount, 1, "subprocess is still running");
  let exitPromise = waitForSubprocessExit();
  await extension.unload();
  await exitPromise;
});

// Just test that the given app (which should be the echo script above)
// can be started.  Used to test corner cases in how the native application
// is located/launched.
async function simpleTest(app) {
  function background(appname) {
    let port = browser.runtime.connectNative(appname);
    let MSG = "test";
    port.onMessage.addListener(msg => {
      browser.test.assertEq(MSG, msg, "Got expected message back");
      browser.test.sendMessage("done");
    });
    port.postMessage(MSG);
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${background})(${JSON.stringify(app)});`,
    manifest: {
      applications: { gecko: { id: ID } },
      permissions: ["nativeMessaging"],
    },
  });

  await extension.startup();
  await extension.awaitMessage("done");

  let procCount = await getSubprocessCount();
  equal(procCount, 1, "subprocess is still running");
  let exitPromise = waitForSubprocessExit();
  await extension.unload();
  await exitPromise;
}

if (AppConstants.platform == "win") {
  // "relative.echo" has a relative path in the host manifest.
  add_task(function test_relative_path() {
    return simpleTest("relative.echo");
  });

  // "echocmd" uses a .cmd file instead of a .bat file
  add_task(function test_cmd_file() {
    return simpleTest("echocmd");
  });
}

// Test sendNativeMessage()
add_task(async function test_sendNativeMessage() {
  async function background() {
    let MSG = { test: "hello world" };

    // Check error handling
    await browser.test.assertRejects(
      browser.runtime.sendNativeMessage("nonexistent", MSG),
      /Attempt to postMessage on disconnected port/,
      "sendNativeMessage() to a nonexistent app failed"
    );

    // Check regular message exchange
    let reply = await browser.runtime.sendNativeMessage("echo", MSG);

    let expected = JSON.stringify(MSG);
    let received = JSON.stringify(reply);
    browser.test.assertEq(expected, received, "Received echoed native message");

    browser.test.sendMessage("finished");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      applications: { gecko: { id: ID } },
      permissions: ["nativeMessaging"],
    },
  });

  await extension.startup();
  await extension.awaitMessage("finished");

  // With sendNativeMessage(), the subprocess should be disconnected
  // after exchanging a single message.
  await waitForSubprocessExit();

  await extension.unload();
});

// Test calling Port.disconnect()
add_task(async function test_disconnect() {
  function background() {
    let port = browser.runtime.connectNative("echo");
    port.onMessage.addListener((msg, msgPort) => {
      browser.test.assertEq(
        port,
        msgPort,
        "onMessage handler should receive the port as the second argument"
      );
      browser.test.sendMessage("message", msg);
    });
    port.onDisconnect.addListener(msgPort => {
      browser.test.fail("onDisconnect should not be called for disconnect()");
    });
    browser.test.onMessage.addListener((what, payload) => {
      if (what == "send") {
        if (payload._json) {
          let json = payload._json;
          payload.toJSON = () => json;
          delete payload._json;
        }
        port.postMessage(payload);
      } else if (what == "disconnect") {
        try {
          port.disconnect();
          browser.test.assertThrows(
            () => port.postMessage("void"),
            "Attempt to postMessage on disconnected port"
          );
          browser.test.sendMessage("disconnect-result", { success: true });
        } catch (err) {
          browser.test.sendMessage("disconnect-result", {
            success: false,
            errmsg: err.message,
          });
        }
      }
    });
    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      applications: { gecko: { id: ID } },
      permissions: ["nativeMessaging"],
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  extension.sendMessage("send", "test");
  let response = await extension.awaitMessage("message");
  equal(response, "test", "Echoed a string");

  let procCount = await getSubprocessCount();
  equal(procCount, 1, "subprocess is running");

  extension.sendMessage("disconnect");
  response = await extension.awaitMessage("disconnect-result");
  equal(response.success, true, "disconnect succeeded");

  info("waiting for subprocess to exit");
  await waitForSubprocessExit();
  procCount = await getSubprocessCount();
  equal(procCount, 0, "subprocess is no longer running");

  extension.sendMessage("disconnect");
  response = await extension.awaitMessage("disconnect-result");
  equal(response.success, true, "second call to disconnect silently ignored");

  await extension.unload();
});

// Test the limit on message size for writing
add_task(async function test_write_limit() {
  Services.prefs.setIntPref(PREF_MAX_WRITE, 10);
  function clearPref() {
    Services.prefs.clearUserPref(PREF_MAX_WRITE);
  }
  registerCleanupFunction(clearPref);

  function background() {
    const PAYLOAD = "0123456789A";
    let port = browser.runtime.connectNative("echo");
    try {
      port.postMessage(PAYLOAD);
      browser.test.sendMessage("result", null);
    } catch (ex) {
      browser.test.sendMessage("result", ex.message);
    }
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      applications: { gecko: { id: ID } },
      permissions: ["nativeMessaging"],
    },
  });

  await extension.startup();

  let errmsg = await extension.awaitMessage("result");
  notEqual(
    errmsg,
    null,
    "native postMessage() failed for overly large message"
  );

  await extension.unload();
  await waitForSubprocessExit();

  clearPref();
});

// Test the limit on message size for reading
add_task(async function test_read_limit() {
  Services.prefs.setIntPref(PREF_MAX_READ, 10);
  function clearPref() {
    Services.prefs.clearUserPref(PREF_MAX_READ);
  }
  registerCleanupFunction(clearPref);

  function background() {
    const PAYLOAD = "0123456789A";
    let port = browser.runtime.connectNative("echo");
    port.onDisconnect.addListener(msgPort => {
      browser.test.assertEq(
        port,
        msgPort,
        "onDisconnect handler should receive the port as the first argument"
      );
      browser.test.assertEq(
        "Native application tried to send a message of 13 bytes, which exceeds the limit of 10 bytes.",
        port.error && port.error.message
      );
      browser.test.sendMessage("result", "disconnected");
    });
    port.onMessage.addListener(msg => {
      browser.test.sendMessage("result", "message");
    });
    port.postMessage(PAYLOAD);
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      applications: { gecko: { id: ID } },
      permissions: ["nativeMessaging"],
    },
  });

  await extension.startup();

  let result = await extension.awaitMessage("result");
  equal(
    result,
    "disconnected",
    "native port disconnected on receiving large message"
  );

  await extension.unload();
  await waitForSubprocessExit();

  clearPref();
});

// Test that an extension without the nativeMessaging permission cannot
// use native messaging.
add_task(async function test_ext_permission() {
  function background() {
    browser.test.assertEq(
      chrome.runtime.connectNative,
      undefined,
      "chrome.runtime.connectNative does not exist without nativeMessaging permission"
    );
    browser.test.assertEq(
      browser.runtime.connectNative,
      undefined,
      "browser.runtime.connectNative does not exist without nativeMessaging permission"
    );
    browser.test.assertEq(
      chrome.runtime.sendNativeMessage,
      undefined,
      "chrome.runtime.sendNativeMessage does not exist without nativeMessaging permission"
    );
    browser.test.assertEq(
      browser.runtime.sendNativeMessage,
      undefined,
      "browser.runtime.sendNativeMessage does not exist without nativeMessaging permission"
    );
    browser.test.sendMessage("finished");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {},
  });

  await extension.startup();
  await extension.awaitMessage("finished");
  await extension.unload();
});

// Test that an extension that is not listed in allowed_extensions for
// a native application cannot use that application.
add_task(async function test_app_permission() {
  function background() {
    let port = browser.runtime.connectNative("echo");
    port.onDisconnect.addListener(msgPort => {
      browser.test.assertEq(
        port,
        msgPort,
        "onDisconnect handler should receive the port as the first argument"
      );
      browser.test.assertEq(
        "No such native application echo",
        port.error && port.error.message
      );
      browser.test.sendMessage("result", "disconnected");
    });
    port.onMessage.addListener(msg => {
      browser.test.sendMessage("result", "message");
    });
    port.postMessage({ test: "test" });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["nativeMessaging"],
    },
  });

  await extension.startup();

  let result = await extension.awaitMessage("result");
  equal(
    result,
    "disconnected",
    "connectNative() failed without native app permission"
  );

  await extension.unload();

  let procCount = await getSubprocessCount();
  equal(procCount, 0, "No child process was started");
});

// Test that the command-line arguments and working directory for the
// native application are as expected.
add_task(async function test_child_process() {
  function background() {
    let port = browser.runtime.connectNative("info");
    port.onMessage.addListener(msg => {
      browser.test.sendMessage("result", msg);
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      applications: { gecko: { id: ID } },
      permissions: ["nativeMessaging"],
    },
  });

  await extension.startup();

  let msg = await extension.awaitMessage("result");
  equal(msg.args.length, 3, "Received two command line arguments");
  equal(
    msg.args[1],
    getPath("info.json"),
    "Command line argument is the path to the native host manifest"
  );
  equal(
    msg.args[2],
    ID,
    "Second command line argument is the ID of the calling extension"
  );
  equal(
    msg.cwd.replace(/^\/private\//, "/"),
    PathUtils.join(tmpDir.path, TYPE_SLUG),
    "Working directory is the directory containing the native appliation"
  );

  let exitPromise = waitForSubprocessExit();
  await extension.unload();
  await exitPromise;
});

add_task(async function test_stderr() {
  function background() {
    let port = browser.runtime.connectNative("stderr");
    port.onDisconnect.addListener(msgPort => {
      browser.test.assertEq(
        port,
        msgPort,
        "onDisconnect handler should receive the port as the first argument"
      );
      browser.test.assertEq(
        null,
        port.error,
        "Normal application exit is not an error"
      );
      browser.test.sendMessage("finished");
    });
  }

  let { messages } = await promiseConsoleOutput(async function() {
    let extension = ExtensionTestUtils.loadExtension({
      background,
      manifest: {
        applications: { gecko: { id: ID } },
        permissions: ["nativeMessaging"],
      },
    });

    await extension.startup();
    await extension.awaitMessage("finished");
    await extension.unload();

    await waitForSubprocessExit();
  });

  let lines = STDERR_LINES.map(line =>
    messages.findIndex(msg => msg.message.includes(line))
  );
  notEqual(lines[0], -1, "Saw first line of stderr output on the console");
  notEqual(lines[1], -1, "Saw second line of stderr output on the console");
  notEqual(
    lines[0],
    lines[1],
    "Stderr output lines are separated in the console"
  );
});

// Test that calling connectNative() multiple times works
// (see bug 1313980 for a previous regression in this area)
add_task(async function test_multiple_connects() {
  async function background() {
    function once() {
      return new Promise(resolve => {
        let MSG = "hello";
        let port = browser.runtime.connectNative("echo");

        port.onMessage.addListener(msg => {
          browser.test.assertEq(MSG, msg, "Got expected message back");
          port.disconnect();
          resolve();
        });
        port.postMessage(MSG);
      });
    }

    await once();
    await once();
    browser.test.notifyPass("multiple-connect");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      applications: { gecko: { id: ID } },
      permissions: ["nativeMessaging"],
    },
  });

  await extension.startup();
  await extension.awaitFinish("multiple-connect");
  await extension.unload();
});

// Test that native messaging is always rejected on content scripts
add_task(async function test_connect_native_from_content_script() {
  async function testScript() {
    let port = browser.runtime.connectNative("echo");
    port.onDisconnect.addListener(msgPort => {
      browser.test.assertEq(
        port,
        msgPort,
        "onDisconnect handler should receive the port as the first argument"
      );
      browser.test.assertEq(
        "An unexpected error occurred",
        port.error && port.error.message
      );
      browser.test.sendMessage("result", "disconnected");
    });
    port.onMessage.addListener(msg => {
      browser.test.sendMessage("result", "message");
    });
    port.postMessage({ test: "test" });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          run_at: "document_end",
          js: ["test.js"],
          matches: ["http://example.com/dummy"],
        },
      ],
      applications: { gecko: { id: ID } },
      permissions: ["nativeMessaging"],
    },
    files: {
      "test.js": testScript,
    },
  });

  await extension.startup();

  const page = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy"
  );

  let result = await extension.awaitMessage("result");
  equal(result, "disconnected", "connectNative() failed from content script");

  await page.close();
  await extension.unload();

  let procCount = await getSubprocessCount();
  equal(procCount, 0, "No child process was started");
});

// Testing native app messaging against idle timeout.
async function startupExtensionAndRequestPermission() {
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      applications: { gecko: { id: ID } },
      optional_permissions: ["nativeMessaging"],
      background: { persistent: false },
    },
    async background() {
      browser.runtime.onSuspend.addListener(() => {
        browser.test.sendMessage("bgpage:suspending");
      });

      let port;
      browser.test.onMessage.addListener(async (msg, ...args) => {
        switch (msg) {
          case "request-permission": {
            await browser.permissions.request({
              permissions: ["nativeMessaging"],
            });
            break;
          }
          case "delayedecho-sendmessage": {
            browser.runtime
              .sendNativeMessage("delayedecho", args[0])
              .then(msg =>
                browser.test.sendMessage(
                  `delayedecho-sendmessage:got-reply`,
                  msg
                )
              );
            break;
          }
          case "connectNative": {
            if (port) {
              browser.test.fail(`Unexpected already connected NativeApp port`);
            } else {
              port = browser.runtime.connectNative("echo");
            }
            break;
          }
          case "disconnectNative": {
            if (!port) {
              browser.test.fail(`Unexpected undefined NativeApp port`);
            }
            port?.disconnect();
            break;
          }
          default:
            browser.test.fail(`Got an unexpected test message: ${msg}`);
        }

        browser.test.sendMessage(`${msg}:done`);
      });
      browser.test.sendMessage("bg:ready");
    },
  });
  await extension.startup();
  await extension.awaitMessage("bg:ready");
  const contextId = extension.extension.backgroundContext.contextId;
  notEqual(contextId, undefined, "Got a contextId for the background context");

  await withHandlingUserInput(extension, async () => {
    extension.sendMessage("request-permission");
    await extension.awaitMessage("request-permission:done");
  });

  return { extension, contextId };
}

async function expectTerminateBackgroundToResetIdle({ extension, contextId }) {
  info("Wait for hasActiveNativeAppPorts to become true");
  await TestUtils.waitForCondition(
    () => extension.extension.backgroundContext,
    "Parent proxy context should be active"
  );

  await TestUtils.waitForCondition(
    () => extension.extension.backgroundContext?.hasActiveNativeAppPorts,
    "Parent proxy context should have active native app ports tracked"
  );

  clearHistograms();
  assertHistogramEmpty(WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT);
  assertKeyedHistogramEmpty(WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT_BY_ADDONID);

  info("Trigger background script idle timeout and expect to be reset");
  const promiseResetIdle = promiseExtensionEvent(
    extension,
    "background-script-reset-idle"
  );
  await extension.terminateBackground();
  info("Wait for 'background-script-reset-idle' event to be emitted");
  await promiseResetIdle;
  equal(
    extension.extension.backgroundContext.contextId,
    contextId,
    "Initial background context is still available as expected"
  );

  assertHistogramCategoryNotEmpty(WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT, {
    category: "reset_nativeapp",
    categories: HISTOGRAM_EVENTPAGE_IDLE_RESULT_CATEGORIES,
  });

  assertHistogramCategoryNotEmpty(
    WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT_BY_ADDONID,
    {
      keyed: true,
      key: extension.id,
      category: "reset_nativeapp",
      categories: HISTOGRAM_EVENTPAGE_IDLE_RESULT_CATEGORIES,
    }
  );
}

async function testSendNativeMessage({ extension, contextId }) {
  extension.sendMessage("delayedecho-sendmessage", "delayed-echo");
  await extension.awaitMessage("delayedecho-sendmessage:done");

  await expectTerminateBackgroundToResetIdle({ extension, contextId });

  // We expect exactly two replies (one for the previous queued message
  // and one more for the last message sent right above).
  equal(
    await extension.awaitMessage("delayedecho-sendmessage:got-reply"),
    "delayed-echo",
    "Got the expected reply for the first message sent"
  );

  await TestUtils.waitForCondition(
    () => !extension.extension.backgroundContext?.hasActiveNativeAppPorts,
    "Parent proxy context should not have any active native app ports tracked"
  );

  info("terminating the background script");
  await extension.terminateBackground();
  info("wait for runtime.onSuspend listener to have been called");
  await extension.awaitMessage("bgpage:suspending");
}

async function testConnectNative({ extension, contextId }) {
  extension.sendMessage("connectNative");
  await extension.awaitMessage("connectNative:done");

  await expectTerminateBackgroundToResetIdle({ extension, contextId });

  // Disconnect the NativeApp and confirm that the background page
  // will be suspending as expected.
  extension.sendMessage("disconnectNative");
  await extension.awaitMessage("disconnectNative:done");

  await TestUtils.waitForCondition(
    () => !extension.extension.backgroundContext?.hasActiveNativeAppPorts,
    "Parent proxy context should not have any active native app ports tracked"
  );

  info("terminating the background script");
  await extension.terminateBackground();
  info("wait for runtime.onSuspend listener to have been called");
  await extension.awaitMessage("bgpage:suspending");
}

add_task(
  {
    pref_set: [["extensions.eventPages.enabled", true]],
  },
  async function test_pending_sendNativeMessageReply_resets_bgscript_idle_timeout() {
    const {
      extension,
      contextId,
    } = await startupExtensionAndRequestPermission();
    await testSendNativeMessage({ extension, contextId });
    await waitForSubprocessExit();
    await extension.unload();
  }
);

add_task(
  {
    pref_set: [["extensions.eventPages.enabled", true]],
  },
  async function test_open_connectNativePort_resets_bgscript_idle_timeout() {
    const {
      extension,
      contextId,
    } = await startupExtensionAndRequestPermission();
    await testConnectNative({ extension, contextId });
    await waitForSubprocessExit();
    await extension.unload();
  }
);
