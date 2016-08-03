/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* globals chrome */

const PREF_MAX_READ = "webextensions.native-messaging.max-input-message-bytes";
const PREF_MAX_WRITE = "webextensions.native-messaging.max-output-message-bytes";

const ECHO_BODY = String.raw`
  import struct
  import sys

  while True:
      rawlen = sys.stdin.read(4)
      if len(rawlen) == 0:
          sys.exit(0)
      msglen = struct.unpack('@I', rawlen)[0]
      msg = sys.stdin.read(msglen)

      sys.stdout.write(struct.pack('@I', msglen))
      sys.stdout.write(msg)
`;

const INFO_BODY = String.raw`
  import json
  import os
  import struct
  import sys

  msg = json.dumps({"args": sys.argv, "cwd": os.getcwd()})
  sys.stdout.write(struct.pack('@I', len(msg)))
  sys.stdout.write(msg)
  sys.exit(0)
`;

const WONTDIE_BODY = String.raw`
  import signal
  import struct
  import sys
  import time

  signal.signal(signal.SIGTERM, signal.SIG_IGN)

  def spin():
      while True:
          try:
              signal.pause()
          except AttributeError:
              time.sleep(5)

  while True:
      rawlen = sys.stdin.read(4)
      if len(rawlen) == 0:
          spin()

      msglen = struct.unpack('@I', rawlen)[0]
      msg = sys.stdin.read(msglen)

      sys.stdout.write(struct.pack('@I', msglen))
      sys.stdout.write(msg)
`;

const STDERR_LINES = ["hello stderr", "this should be a separate line"];
let STDERR_MSG = STDERR_LINES.join("\\n");

const STDERR_BODY = String.raw`
  import sys
  sys.stderr.write("${STDERR_MSG}")
`;

const SCRIPTS = [
  {
    name: "echo",
    description: "a native app that echoes back messages it receives",
    script: ECHO_BODY.replace(/^ {2}/gm, ""),
  },
  {
    name: "info",
    description: "a native app that gives some info about how it was started",
    script: INFO_BODY.replace(/^ {2}/gm, ""),
  },
  {
    name: "wontdie",
    description: "a native app that does not exit when stdin closes or on SIGTERM",
    script: WONTDIE_BODY.replace(/^ {2}/gm, ""),
  },
  {
    name: "stderr",
    description: "a native app that writes to stderr and then exits",
    script: STDERR_BODY.replace(/^ {2}/gm, ""),
  },
];

add_task(function* setup() {
  yield setupHosts(SCRIPTS);
});

// Test the basic operation of native messaging with a simple
// script that echoes back whatever message is sent to it.
add_task(function* test_happy_path() {
  function background() {
    let port = browser.runtime.connectNative("echo");
    port.onMessage.addListener(msg => {
      browser.test.sendMessage("message", msg);
    });
    browser.test.onMessage.addListener((what, payload) => {
      if (what == "send") {
        if (payload._json) {
          let json = payload._json;
          payload.toJSON = () => json;
          delete payload._json;
        }
        port.postMessage(payload);
      }
    });
    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["nativeMessaging"],
    },
  }, ID);

  yield extension.startup();
  yield extension.awaitMessage("ready");
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
      data: {test: "hello"},
      what: "simple object",
    },
    {
      data: {
        what: "An object with a few properties",
        number: 123,
        bool: true,
        nested: {what: "another object"},
      },
      what: "object with several properties",
    },

    {
      data: {
        ignoreme: true,
        _json: {data: "i have a tojson method"},
      },
      expected: {data: "i have a tojson method"},
      what: "object with toJSON() method",
    },
  ];
  for (let test of tests) {
    extension.sendMessage("send", test.data);
    let response = yield extension.awaitMessage("message");
    let expected = test.expected || test.data;
    deepEqual(response, expected, `Echoed a message of type ${test.what}`);
  }

  let procCount = yield getSubprocessCount();
  equal(procCount, 1, "subprocess is still running");
  let exitPromise = waitForSubprocessExit();
  yield extension.unload();
  yield exitPromise;
});

if (AppConstants.platform == "win") {
  // "relative.echo" has a relative path in the host manifest.
  add_task(function* test_relative_path() {
    function background() {
      let port = browser.runtime.connectNative("relative.echo");
      let MSG = "test relative echo path";
      port.onMessage.addListener(msg => {
        browser.test.assertEq(MSG, msg, "Got expected message back");
        browser.test.sendMessage("done");
      });
      port.postMessage(MSG);
    }

    let extension = ExtensionTestUtils.loadExtension({
      background,
      manifest: {
        permissions: ["nativeMessaging"],
      },
    }, ID);

    yield extension.startup();
    yield extension.awaitMessage("done");

    let procCount = yield getSubprocessCount();
    equal(procCount, 1, "subprocess is still running");
    let exitPromise = waitForSubprocessExit();
    yield extension.unload();
    yield exitPromise;
  });
}

// Test sendNativeMessage()
add_task(function* test_sendNativeMessage() {
  function background() {
    let MSG = {test: "hello world"};

    // Check error handling
    browser.runtime.sendNativeMessage("nonexistent", MSG).then(() => {
      browser.test.fail("sendNativeMessage() to a nonexistent app should have failed");
    }, err => {
      browser.test.succeed("sendNativeMessage() to a nonexistent app failed");
    }).then(() => {
      // Check regular message exchange
      return browser.runtime.sendNativeMessage("echo", MSG);
    }).then(reply => {
      let expected = JSON.stringify(MSG);
      let received = JSON.stringify(reply);
      browser.test.assertEq(expected, received, "Received echoed native message");
      browser.test.sendMessage("finished");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["nativeMessaging"],
    },
  }, ID);

  yield extension.startup();
  yield extension.awaitMessage("finished");

  // With sendNativeMessage(), the subprocess should be disconnected
  // after exchanging a single message.
  yield waitForSubprocessExit();

  yield extension.unload();
});

// Test calling Port.disconnect()
add_task(function* test_disconnect() {
  function background() {
    let port = browser.runtime.connectNative("echo");
    port.onMessage.addListener(msg => {
      browser.test.sendMessage("message", msg);
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
          browser.test.sendMessage("disconnect-result", {success: true});
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
      permissions: ["nativeMessaging"],
    },
  }, ID);

  yield extension.startup();
  yield extension.awaitMessage("ready");

  extension.sendMessage("send", "test");
  let response = yield extension.awaitMessage("message");
  equal(response, "test", "Echoed a string");

  let procCount = yield getSubprocessCount();
  equal(procCount, 1, "subprocess is running");

  extension.sendMessage("disconnect");
  response = yield extension.awaitMessage("disconnect-result");
  equal(response.success, true, "disconnect succeeded");

  do_print("waiting for subprocess to exit");
  yield waitForSubprocessExit();
  procCount = yield getSubprocessCount();
  equal(procCount, 0, "subprocess is no longer running");

  extension.sendMessage("disconnect");
  response = yield extension.awaitMessage("disconnect-result");
  equal(response.success, false, "second call to disconnect failed");
  ok(/already disconnected/.test(response.errmsg), "disconnect error message is reasonable");

  yield extension.unload();
});

// Test the limit on message size for writing
add_task(function* test_write_limit() {
  Services.prefs.setIntPref(PREF_MAX_WRITE, 10);
  function clearPref() {
    Services.prefs.clearUserPref(PREF_MAX_WRITE);
  }
  do_register_cleanup(clearPref);

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
      permissions: ["nativeMessaging"],
    },
  }, ID);

  yield extension.startup();

  let errmsg = yield extension.awaitMessage("result");
  notEqual(errmsg, null, "native postMessage() failed for overly large message");

  yield extension.unload();
  yield waitForSubprocessExit();

  clearPref();
});

// Test the limit on message size for reading
add_task(function* test_read_limit() {
  Services.prefs.setIntPref(PREF_MAX_READ, 10);
  function clearPref() {
    Services.prefs.clearUserPref(PREF_MAX_READ);
  }
  do_register_cleanup(clearPref);

  function background() {
    const PAYLOAD = "0123456789A";
    let port = browser.runtime.connectNative("echo");
    port.onDisconnect.addListener(() => {
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
      permissions: ["nativeMessaging"],
    },
  }, ID);

  yield extension.startup();

  let result = yield extension.awaitMessage("result");
  equal(result, "disconnected", "native port disconnected on receiving large message");

  yield extension.unload();
  yield waitForSubprocessExit();

  clearPref();
});

// Test that an extension without the nativeMessaging permission cannot
// use native messaging.
add_task(function* test_ext_permission() {
  function background() {
    browser.test.assertFalse("connectNative" in chrome.runtime, "chrome.runtime.connectNative does not exist without nativeMessaging permission");
    browser.test.assertFalse("connectNative" in browser.runtime, "browser.runtime.connectNative does not exist without nativeMessaging permission");
    browser.test.assertFalse("sendNativeMessage" in chrome.runtime, "chrome.runtime.sendNativeMessage does not exist without nativeMessaging permission");
    browser.test.assertFalse("sendNativeMessage" in browser.runtime, "browser.runtime.sendNativeMessage does not exist without nativeMessaging permission");
    browser.test.sendMessage("finished");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {},
  });

  yield extension.startup();
  yield extension.awaitMessage("finished");
  yield extension.unload();
});

// Test that an extension that is not listed in allowed_extensions for
// a native application cannot use that application.
add_task(function* test_app_permission() {
  function background() {
    let port = browser.runtime.connectNative("echo");
    port.onDisconnect.addListener(() => {
      browser.test.sendMessage("result", "disconnected");
    });
    port.onMessage.addListener(msg => {
      browser.test.sendMessage("result", "message");
    });
    port.postMessage({test: "test"});
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["nativeMessaging"],
    },
  }, "somethingelse@tests.mozilla.org");

  yield extension.startup();

  let result = yield extension.awaitMessage("result");
  equal(result, "disconnected", "connectNative() failed without native app permission");

  yield extension.unload();

  let procCount = yield getSubprocessCount();
  equal(procCount, 0, "No child process was started");
});

// Test that the command-line arguments and working directory for the
// native application are as expected.
add_task(function* test_child_process() {
  function background() {
    let port = browser.runtime.connectNative("info");
    port.onMessage.addListener(msg => {
      browser.test.sendMessage("result", msg);
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["nativeMessaging"],
    },
  }, ID);

  yield extension.startup();

  let msg = yield extension.awaitMessage("result");
  equal(msg.args.length, 2, "Received one command line argument");
  equal(msg.args[1], getPath("info.json"), "Command line argument is the path to the native host manifest");
  equal(msg.cwd.replace(/^\/private\//, "/"), tmpDir.path,
        "Working directory is the directory containing the native appliation");

  let exitPromise = waitForSubprocessExit();
  yield extension.unload();
  yield exitPromise;
});

// Test that an unresponsive native application still gets killed eventually
add_task(function* test_unresponsive_native_app() {
  // XXX expose GRACEFUL_SHUTDOWN_TIME as a pref and reduce it
  // just for this test?

  function background() {
    let port = browser.runtime.connectNative("wontdie");

    const MSG = "echo me";
    // bounce a message to make sure the process actually starts
    port.onMessage.addListener(msg => {
      browser.test.assertEq(msg, MSG, "Received echoed message");
      browser.test.sendMessage("ready");
    });
    port.postMessage(MSG);
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["nativeMessaging"],
    },
  }, ID);

  yield extension.startup();
  yield extension.awaitMessage("ready");

  let procCount = yield getSubprocessCount();
  equal(procCount, 1, "subprocess is running");

  let exitPromise = waitForSubprocessExit();
  yield extension.unload();
  yield exitPromise;

  procCount = yield getSubprocessCount();
  equal(procCount, 0, "subprocess was succesfully killed");
});

add_task(function* test_stderr() {
  function background() {
    let port = browser.runtime.connectNative("stderr");
    port.onDisconnect.addListener(() => {
      browser.test.sendMessage("finished");
    });
  }

  let {messages} = yield promiseConsoleOutput(function* () {
    let extension = ExtensionTestUtils.loadExtension({
      background,
      manifest: {
        permissions: ["nativeMessaging"],
      },
    }, ID);

    yield extension.startup();
    yield extension.awaitMessage("finished");
    yield extension.unload();

    yield waitForSubprocessExit();
  });

  let lines = STDERR_LINES.map(line => messages.findIndex(msg => msg.message.includes(line)));
  notEqual(lines[0], -1, "Saw first line of stderr output on the console");
  notEqual(lines[1], -1, "Saw second line of stderr output on the console");
  notEqual(lines[0], lines[1], "Stderr output lines are separated in the console");
});
