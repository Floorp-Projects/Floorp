"use strict";

/* global OS, HostManifestManager, NativeApp */
Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/AsyncShutdown.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/Schemas.jsm");
Cu.import("resource://gre/modules/Services.jsm");
const {Subprocess, SubprocessImpl} = Cu.import("resource://gre/modules/Subprocess.jsm");
Cu.import("resource://gre/modules/NativeMessaging.jsm");
Cu.import("resource://gre/modules/osfile.jsm");

let registry = null;
if (AppConstants.platform == "win") {
  Cu.import("resource://testing-common/MockRegistry.jsm");
  registry = new MockRegistry();
  do_register_cleanup(() => {
    registry.shutdown();
  });
}

const REGKEY = "Software\\Mozilla\\NativeMessagingHosts";

const BASE_SCHEMA = "chrome://extensions/content/schemas/manifest.json";

let dir = FileUtils.getDir("TmpD", ["NativeMessaging"]);
dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

let userDir = dir.clone();
userDir.append("user");
userDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

let globalDir = dir.clone();
globalDir.append("global");
globalDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

let dirProvider = {
  getFile(property) {
    if (property == "XREUserNativeMessaging") {
      return userDir.clone();
    } else if (property == "XRESysNativeMessaging") {
      return globalDir.clone();
    }
    return null;
  },
};

Services.dirsvc.registerProvider(dirProvider);

do_register_cleanup(() => {
  Services.dirsvc.unregisterProvider(dirProvider);
  dir.remove(true);
});

function writeManifest(path, manifest) {
  if (typeof manifest != "string") {
    manifest = JSON.stringify(manifest);
  }
  return OS.File.writeAtomic(path, manifest);
}

let PYTHON;
add_task(function* setup() {
  yield Schemas.load(BASE_SCHEMA);

  PYTHON = yield Subprocess.pathSearch("python2.7");
  if (PYTHON == null) {
    PYTHON = yield Subprocess.pathSearch("python");
  }
  notEqual(PYTHON, null, "Found a suitable python interpreter");
});

// Test of HostManifestManager.lookupApplication() begin here...
let context = {
  url: null,
  logError() {},
  preprocessors: {},
  callOnClose: () => {},
  forgetOnClose: () => {},
};

let templateManifest = {
  name: "test",
  description: "this is only a test",
  path: "/bin/cat",
  type: "stdio",
  allowed_extensions: ["extension@tests.mozilla.org"],
};

add_task(function* test_nonexistent_manifest() {
  let result = yield HostManifestManager.lookupApplication("test", context);
  equal(result, null, "lookupApplication returns null for non-existent application");
});

const USER_TEST_JSON = OS.Path.join(userDir.path, "test.json");

add_task(function* test_good_manifest() {
  yield writeManifest(USER_TEST_JSON, templateManifest);
  if (registry) {
    registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                      REGKEY, "test", USER_TEST_JSON);
  }

  let result = yield HostManifestManager.lookupApplication("test", context);
  notEqual(result, null, "lookupApplication finds a good manifest");
  equal(result.path, USER_TEST_JSON, "lookupApplication returns the correct path");
  deepEqual(result.manifest, templateManifest, "lookupApplication returns the manifest contents");
});

add_task(function* test_invalid_json() {
  yield writeManifest(USER_TEST_JSON, "this is not valid json");
  let result = yield HostManifestManager.lookupApplication("test", context);
  equal(result, null, "lookupApplication ignores bad json");
});

add_task(function* test_invalid_name() {
  let manifest = Object.assign({}, templateManifest);
  manifest.name = "../test";
  yield writeManifest(USER_TEST_JSON, manifest);
  let result = yield HostManifestManager.lookupApplication("test", context);
  equal(result, null, "lookupApplication ignores an invalid name");
});

add_task(function* test_name_mismatch() {
  let manifest = Object.assign({}, templateManifest);
  manifest.name = "not test";
  yield writeManifest(USER_TEST_JSON, manifest);
  let result = yield HostManifestManager.lookupApplication("test", context);
  let what = (AppConstants.platform == "win") ? "registry key" : "json filename";
  equal(result, null, `lookupApplication ignores mistmatch between ${what} and name property`);
});

add_task(function* test_missing_props() {
  const PROPS = [
    "name",
    "description",
    "path",
    "type",
    "allowed_extensions",
  ];
  for (let prop of PROPS) {
    let manifest = Object.assign({}, templateManifest);
    delete manifest[prop];

    yield writeManifest(USER_TEST_JSON, manifest);
    let result = yield HostManifestManager.lookupApplication("test", context);
    equal(result, null, `lookupApplication ignores missing ${prop}`);
  }
});

add_task(function* test_invalid_type() {
  let manifest = Object.assign({}, templateManifest);
  manifest.type = "bogus";
  yield writeManifest(USER_TEST_JSON, manifest);
  let result = yield HostManifestManager.lookupApplication("test", context);
  equal(result, null, "lookupApplication ignores invalid type");
});

add_task(function* test_no_allowed_extensions() {
  let manifest = Object.assign({}, templateManifest);
  manifest.allowed_extensions = [];
  yield writeManifest(USER_TEST_JSON, manifest);
  let result = yield HostManifestManager.lookupApplication("test", context);
  equal(result, null, "lookupApplication ignores manifest with no allowed_extensions");
});

const GLOBAL_TEST_JSON = OS.Path.join(globalDir.path, "test.json");
let globalManifest = Object.assign({}, templateManifest);
globalManifest.description = "This manifest is from the systemwide directory";

add_task(function* good_manifest_system_dir() {
  yield OS.File.remove(USER_TEST_JSON);
  yield writeManifest(GLOBAL_TEST_JSON, globalManifest);
  if (registry) {
    registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                      REGKEY, "test", null);
    registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                      REGKEY, "test", GLOBAL_TEST_JSON);
  }

  let where = (AppConstants.platform == "win") ? "registry location" : "directory";
  let result = yield HostManifestManager.lookupApplication("test", context);
  notEqual(result, null, `lookupApplication finds a manifest in the system-wide ${where}`);
  equal(result.path, GLOBAL_TEST_JSON, `lookupApplication returns path in the system-wide ${where}`);
  deepEqual(result.manifest, globalManifest, `lookupApplication returns manifest contents from the system-wide ${where}`);
});

add_task(function* test_user_dir_precedence() {
  yield writeManifest(USER_TEST_JSON, templateManifest);
  if (registry) {
    registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                      REGKEY, "test", USER_TEST_JSON);
  }
  // global test.json and LOCAL_MACHINE registry key on windows are
  // still present from the previous test

  let result = yield HostManifestManager.lookupApplication("test", context);
  notEqual(result, null, "lookupApplication finds a manifest when entries exist in both user-specific and system-wide locations");
  equal(result.path, USER_TEST_JSON, "lookupApplication returns the user-specific path when user-specific and system-wide entries both exist");
  deepEqual(result.manifest, templateManifest, "lookupApplication returns user-specific manifest contents with user-specific and system-wide entries both exist");
});

// Test shutdown handling in NativeApp
add_task(function* test_native_app_shutdown() {
  const SCRIPT = String.raw`
import signal
import struct
import sys

signal.signal(signal.SIGTERM, signal.SIG_IGN)

while True:
    rawlen = sys.stdin.read(4)
    if len(rawlen) == 0:
        signal.pause()
    msglen = struct.unpack('@I', rawlen)[0]
    msg = sys.stdin.read(msglen)

    sys.stdout.write(struct.pack('@I', msglen))
    sys.stdout.write(msg)
  `;

  let scriptPath = OS.Path.join(userDir.path, "wontdie.py");
  let manifestPath = OS.Path.join(userDir.path, "wontdie.json");

  const ID = "native@tests.mozilla.org";
  let manifest = {
    name: "wontdie",
    description: "test async shutdown of native apps",
    type: "stdio",
    allowed_extensions: [ID],
  };

  if (AppConstants.platform == "win") {
    yield OS.File.writeAtomic(scriptPath, SCRIPT);

    let batPath = OS.Path.join(userDir.path, "wontdie.bat");
    let batBody = `@ECHO OFF\n${PYTHON} -u ${scriptPath} %*\n`;
    yield OS.File.writeAtomic(batPath, batBody);
    yield OS.File.setPermissions(batPath, {unixMode: 0o755});

    manifest.path = batPath;
    yield writeManifest(manifestPath, manifest);

    registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                      REGKEY, "wontdie", manifestPath);
  } else {
    yield OS.File.writeAtomic(scriptPath, `#!${PYTHON} -u\n${SCRIPT}`);
    yield OS.File.setPermissions(scriptPath, {unixMode: 0o755});
    manifest.path = scriptPath;
    yield writeManifest(manifestPath, manifest);
  }

  let extension = {id: ID};
  let app = new NativeApp(extension, context, "wontdie");

  // send a message and wait for the reply to make sure the app is running
  let MSG = "test";
  let recvPromise = new Promise(resolve => {
    let listener = (what, msg) => {
      equal(msg, MSG, "Received test message");
      app.off("message", listener);
      resolve();
    };
    app.on("message", listener);
  });

  app.send(MSG);
  yield recvPromise;

  app._cleanup();

  do_print("waiting for async shutdown");
  Services.prefs.setBoolPref("toolkit.asyncshutdown.testing", true);
  AsyncShutdown.profileBeforeChange._trigger();
  Services.prefs.clearUserPref("toolkit.asyncshutdown.testing");

  let procs = yield SubprocessImpl.Process.getWorker().call("getProcesses", []);
  equal(procs.size, 0, "native process exited");
});
