/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "MockRegistry",
                                  "resource://testing-common/MockRegistry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

Cu.import("resource://gre/modules/Subprocess.jsm");

const MAX_ROUND_TRIP_TIME_MS = AppConstants.DEBUG || AppConstants.ASAN ? 36 : 18;
const MAX_RETRIES = 5;


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

const SCRIPTS = [
  {
    name: "echo",
    description: "A native app that echoes back messages it receives",
    script: ECHO_BODY.replace(/^ {2}/gm, ""),
  },
];

add_task(function* setup() {
  yield setupHosts(SCRIPTS);
});

add_task(function* test_round_trip_perf() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.onMessage.addListener(msg => {
        if (msg != "run-tests") {
          return;
        }

        let port = browser.runtime.connectNative("echo");

        function next() {
          port.postMessage({
            "Lorem": {
              "ipsum": {
                "dolor": [
                  "sit amet",
                  "consectetur adipiscing elit",
                  "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.",
                ],
                "Ut enim": [
                  "ad minim veniam",
                  "quis nostrud exercitation ullamco",
                  "laboris nisi ut aliquip ex ea commodo consequat.",
                ],
                "Duis": [
                  "aute irure dolor in reprehenderit in",
                  "voluptate velit esse cillum dolore eu",
                  "fugiat nulla pariatur.",
                ],
                "Excepteur": [
                  "sint occaecat cupidatat non proident",
                  "sunt in culpa qui officia deserunt",
                  "mollit anim id est laborum.",
                ],
              },
            },
          });
        }

        const COUNT = 1000;
        let now;
        function finish() {
          let roundTripTime = (Date.now() - now) / COUNT;

          port.disconnect();
          browser.test.sendMessage("result", roundTripTime);
        }

        let count = 0;
        port.onMessage.addListener(msg => {
          if (count == 0) {
            // Skip the first round, since it includes the time it takes
            // the app to start up.
            now = Date.now();
          }

          if (count++ <= COUNT) {
            next();
          } else {
            finish();
          }
        });

        next();
      });
    },
    manifest: {
      permissions: ["nativeMessaging"],
    },
  }, ID);

  yield extension.startup();

  let roundTripTime = Infinity;
  for (let i = 0; i < MAX_RETRIES && roundTripTime > MAX_ROUND_TRIP_TIME_MS; i++) {
    extension.sendMessage("run-tests");
    roundTripTime = yield extension.awaitMessage("result");
  }

  yield extension.unload();

  ok(roundTripTime <= MAX_ROUND_TRIP_TIME_MS,
     `Expected round trip time (${roundTripTime}ms) to be less than ${MAX_ROUND_TRIP_TIME_MS}ms`);
});
