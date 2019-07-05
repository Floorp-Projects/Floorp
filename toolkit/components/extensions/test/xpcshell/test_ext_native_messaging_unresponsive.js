/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

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

const SCRIPTS = [
  {
    name: "wontdie",
    description:
      "a native app that does not exit when stdin closes or on SIGTERM",
    script: WONTDIE_BODY.replace(/^ {2}/gm, ""),
  },
];

add_task(async function setup() {
  await setupHosts(SCRIPTS);
});

// Test that an unresponsive native application still gets killed eventually
add_task(async function test_unresponsive_native_app() {
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
      applications: { gecko: { id: ID } },
      permissions: ["nativeMessaging"],
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  let procCount = await getSubprocessCount();
  equal(procCount, 1, "subprocess is running");

  let exitPromise = waitForSubprocessExit();
  await extension.unload();
  await exitPromise;

  procCount = await getSubprocessCount();
  equal(procCount, 0, "subprocess was successfully killed");
});
