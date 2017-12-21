"use strict";

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/AsyncShutdown.jsm");
Cu.import("resource://gre/modules/ExtensionCommon.jsm");
Cu.import("resource://gre/modules/NativeManifests.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/Schemas.jsm");
Cu.import("resource://gre/modules/Services.jsm");
const {Subprocess, SubprocessImpl} = Cu.import("resource://gre/modules/Subprocess.jsm", {});
Cu.import("resource://gre/modules/NativeMessaging.jsm");
Cu.import("resource://gre/modules/osfile.jsm");

let registry = null;
if (AppConstants.platform == "win") {
  Cu.import("resource://testing-common/MockRegistry.jsm");
  registry = new MockRegistry();
  registerCleanupFunction(() => {
    registry.shutdown();
  });
}

const REGPATH = "Software\\Mozilla\\NativeMessagingHosts";

const BASE_SCHEMA = "chrome://extensions/content/schemas/manifest.json";

const TYPE_SLUG = AppConstants.platform === "linux" ? "native-messaging-hosts" : "NativeMessagingHosts";

let dir = FileUtils.getDir("TmpD", ["NativeManifests"]);
dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

let userDir = dir.clone();
userDir.append("user");
userDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

let globalDir = dir.clone();
globalDir.append("global");
globalDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

OS.File.makeDir(OS.Path.join(userDir.path, TYPE_SLUG));
OS.File.makeDir(OS.Path.join(globalDir.path, TYPE_SLUG));

let dirProvider = {
  getFile(property) {
    if (property == "XREUserNativeManifests") {
      return userDir.clone();
    } else if (property == "XRESysNativeManifests") {
      return globalDir.clone();
    }
    return null;
  },
};

Services.dirsvc.registerProvider(dirProvider);

registerCleanupFunction(() => {
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
add_task(async function setup() {
  await Schemas.load(BASE_SCHEMA);

  const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  try {
    PYTHON = await Subprocess.pathSearch(env.get("PYTHON"));
  } catch (e) {
    notEqual(PYTHON, null, `Can't find a suitable python interpreter ${e.message}`);
  }
});

let global = this;

// Test of NativeManifests.lookupApplication() begin here...
let context = {
  extension: {
    id: "extension@tests.mozilla.org",
  },
  url: null,
  jsonStringify(...args) { return JSON.stringify(...args); },
  cloneScope: global,
  logError() {},
  preprocessors: {},
  callOnClose: () => {},
  forgetOnClose: () => {},
};

class MockContext extends ExtensionCommon.BaseContext {
  constructor(extensionId) {
    let fakeExtension = {id: extensionId};
    super("testEnv", fakeExtension);
    this.sandbox = Cu.Sandbox(global);
  }

  get cloneScope() {
    return global;
  }

  get principal() {
    return Cu.getObjectPrincipal(this.sandbox);
  }
}

let templateManifest = {
  name: "test",
  description: "this is only a test",
  path: "/bin/cat",
  type: "stdio",
  allowed_extensions: ["extension@tests.mozilla.org"],
};

function lookupApplication(app, ctx) {
  return NativeManifests.lookupManifest("stdio", app, ctx);
}

add_task(async function test_nonexistent_manifest() {
  let result = await lookupApplication("test", context);
  equal(result, null, "lookupApplication returns null for non-existent application");
});

const USER_TEST_JSON = OS.Path.join(userDir.path, TYPE_SLUG, "test.json");

add_task(async function test_good_manifest() {
  await writeManifest(USER_TEST_JSON, templateManifest);
  if (registry) {
    registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                      `${REGPATH}\\test`, "", USER_TEST_JSON);
  }

  let result = await lookupApplication("test", context);
  notEqual(result, null, "lookupApplication finds a good manifest");
  equal(result.path, USER_TEST_JSON, "lookupApplication returns the correct path");
  deepEqual(result.manifest, templateManifest, "lookupApplication returns the manifest contents");
});

add_task(async function test_invalid_json() {
  await writeManifest(USER_TEST_JSON, "this is not valid json");
  let result = await lookupApplication("test", context);
  equal(result, null, "lookupApplication ignores bad json");
});

add_task(async function test_invalid_name() {
  let manifest = Object.assign({}, templateManifest);
  manifest.name = "../test";
  await writeManifest(USER_TEST_JSON, manifest);
  let result = await lookupApplication("test", context);
  equal(result, null, "lookupApplication ignores an invalid name");
});

add_task(async function test_name_mismatch() {
  let manifest = Object.assign({}, templateManifest);
  manifest.name = "not test";
  await writeManifest(USER_TEST_JSON, manifest);
  let result = await lookupApplication("test", context);
  let what = (AppConstants.platform == "win") ? "registry key" : "json filename";
  equal(result, null, `lookupApplication ignores mistmatch between ${what} and name property`);
});

add_task(async function test_missing_props() {
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

    await writeManifest(USER_TEST_JSON, manifest);
    let result = await lookupApplication("test", context);
    equal(result, null, `lookupApplication ignores missing ${prop}`);
  }
});

add_task(async function test_invalid_type() {
  let manifest = Object.assign({}, templateManifest);
  manifest.type = "bogus";
  await writeManifest(USER_TEST_JSON, manifest);
  let result = await lookupApplication("test", context);
  equal(result, null, "lookupApplication ignores invalid type");
});

add_task(async function test_no_allowed_extensions() {
  let manifest = Object.assign({}, templateManifest);
  manifest.allowed_extensions = [];
  await writeManifest(USER_TEST_JSON, manifest);
  let result = await lookupApplication("test", context);
  equal(result, null, "lookupApplication ignores manifest with no allowed_extensions");
});

const GLOBAL_TEST_JSON = OS.Path.join(globalDir.path, TYPE_SLUG, "test.json");
let globalManifest = Object.assign({}, templateManifest);
globalManifest.description = "This manifest is from the systemwide directory";

add_task(async function good_manifest_system_dir() {
  await OS.File.remove(USER_TEST_JSON);
  await writeManifest(GLOBAL_TEST_JSON, globalManifest);
  if (registry) {
    registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                      `${REGPATH}\\test`, "", null);
    registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                      `${REGPATH}\\test`, "", GLOBAL_TEST_JSON);
  }

  let where = (AppConstants.platform == "win") ? "registry location" : "directory";
  let result = await lookupApplication("test", context);
  notEqual(result, null, `lookupApplication finds a manifest in the system-wide ${where}`);
  equal(result.path, GLOBAL_TEST_JSON, `lookupApplication returns path in the system-wide ${where}`);
  deepEqual(result.manifest, globalManifest, `lookupApplication returns manifest contents from the system-wide ${where}`);
});

add_task(async function test_user_dir_precedence() {
  await writeManifest(USER_TEST_JSON, templateManifest);
  if (registry) {
    registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                      `${REGPATH}\\test`, "", USER_TEST_JSON);
  }
  // global test.json and LOCAL_MACHINE registry key on windows are
  // still present from the previous test

  let result = await lookupApplication("test", context);
  notEqual(result, null, "lookupApplication finds a manifest when entries exist in both user-specific and system-wide locations");
  equal(result.path, USER_TEST_JSON, "lookupApplication returns the user-specific path when user-specific and system-wide entries both exist");
  deepEqual(result.manifest, templateManifest, "lookupApplication returns user-specific manifest contents with user-specific and system-wide entries both exist");
});

// Test shutdown handling in NativeApp
add_task(async function test_native_app_shutdown() {
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

  let scriptPath = OS.Path.join(userDir.path, TYPE_SLUG, "wontdie.py");
  let manifestPath = OS.Path.join(userDir.path, TYPE_SLUG, "wontdie.json");

  const ID = "native@tests.mozilla.org";
  let manifest = {
    name: "wontdie",
    description: "test async shutdown of native apps",
    type: "stdio",
    allowed_extensions: [ID],
  };

  if (AppConstants.platform == "win") {
    await OS.File.writeAtomic(scriptPath, SCRIPT);

    let batPath = OS.Path.join(userDir.path, TYPE_SLUG, "wontdie.bat");
    let batBody = `@ECHO OFF\n${PYTHON} -u "${scriptPath}" %*\n`;
    await OS.File.writeAtomic(batPath, batBody);
    await OS.File.setPermissions(batPath, {unixMode: 0o755});

    manifest.path = batPath;
    await writeManifest(manifestPath, manifest);

    registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                      `${REGPATH}\\wontdie`, "", manifestPath);
  } else {
    await OS.File.writeAtomic(scriptPath, `#!${PYTHON} -u\n${SCRIPT}`);
    await OS.File.setPermissions(scriptPath, {unixMode: 0o755});
    manifest.path = scriptPath;
    await writeManifest(manifestPath, manifest);
  }

  let mockContext = new MockContext(ID);
  let app = new NativeApp(mockContext, "wontdie");

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

  let buffer = NativeApp.encodeMessage(mockContext, MSG);
  app.send(new StructuredCloneHolder(buffer));
  await recvPromise;

  app._cleanup();

  info("waiting for async shutdown");
  Services.prefs.setBoolPref("toolkit.asyncshutdown.testing", true);
  AsyncShutdown.profileBeforeChange._trigger();
  Services.prefs.clearUserPref("toolkit.asyncshutdown.testing");

  let procs = await SubprocessImpl.Process.getWorker().call("getProcesses", []);
  equal(procs.size, 0, "native process exited");
});
