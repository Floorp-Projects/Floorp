/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "MockRegistry",
                                  "resource://testing-common/MockRegistry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

Cu.import("resource://gre/modules/Subprocess.jsm");

const MAX_ROUND_TRIP_TIME_MS = AppConstants.DEBUG || AppConstants.ASAN ? 36 : 18;


let tmpDir = FileUtils.getDir("TmpD", ["NativeMessaging"]);
tmpDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

do_register_cleanup(() => {
  tmpDir.remove(true);
});

function getPath(filename) {
  return OS.Path.join(tmpDir.path, filename);
}

const ID = "native@tests.mozilla.org";

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
  const PERMS = {unixMode: 0o755};

  let pythonPath = yield Subprocess.pathSearch("python2.7").catch(err => {
    return Subprocess.pathSearch("python");
  });

  function* writeManifest(script, scriptPath, path) {
    let body = `#!${pythonPath} -u\n${script.script}`;

    yield OS.File.writeAtomic(scriptPath, body);
    yield OS.File.setPermissions(scriptPath, PERMS);

    let manifest = {
      name: script.name,
      description: script.description,
      path,
      type: "stdio",
      allowed_extensions: [ID],
    };

    let manifestPath = getPath(`${script.name}.json`);
    yield OS.File.writeAtomic(manifestPath, JSON.stringify(manifest));

    return manifestPath;
  }

  switch (AppConstants.platform) {
    case "macosx":
    case "linux":
      let dirProvider = {
        getFile(property) {
          if (property == "XREUserNativeMessaging") {
            return tmpDir.clone();
          } else if (property == "XRESysNativeMessaging") {
            return tmpDir.clone();
          }
          return null;
        },
      };

      Services.dirsvc.registerProvider(dirProvider);
      do_register_cleanup(() => {
        Services.dirsvc.unregisterProvider(dirProvider);
      });

      for (let script of SCRIPTS) {
        let path = getPath(`${script.name}.py`);

        yield writeManifest(script, path, path);
      }
      break;

    case "win":
      const REGKEY = String.raw`Software\Mozilla\NativeMessagingHosts`;

      let registry = new MockRegistry();
      do_register_cleanup(() => {
        registry.shutdown();
      });

      for (let script of SCRIPTS) {
        let batPath = getPath(`${script.name}.bat`);
        let scriptPath = getPath(`${script.name}.py`);

        let batBody = `@ECHO OFF\n${pythonPath} -u ${scriptPath} %*\n`;
        yield OS.File.writeAtomic(batPath, batBody);

        let manifestPath = yield writeManifest(script, scriptPath, batPath);

        registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                          `${REGKEY}\\${script.name}`, "", manifestPath);
      }
      break;

    default:
      ok(false, `Native messaging is not supported on ${AppConstants.platform}`);
  }
});

add_task(function* test_round_trip_perf() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
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
    },
    manifest: {
      permissions: ["nativeMessaging"],
    },
  }, ID);

  yield extension.startup();

  let roundTripTime = yield extension.awaitMessage("result");
  ok(roundTripTime <= MAX_ROUND_TRIP_TIME_MS,
     `Expected round trip time (${roundTripTime}ms) to be less than ${MAX_ROUND_TRIP_TIME_MS}ms`);

  yield extension.unload();
});
