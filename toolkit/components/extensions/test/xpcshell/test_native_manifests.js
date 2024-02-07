"use strict";

const { AsyncShutdown } = ChromeUtils.importESModule(
  "resource://gre/modules/AsyncShutdown.sys.mjs"
);
const { NativeManifests } = ChromeUtils.importESModule(
  "resource://gre/modules/NativeManifests.sys.mjs"
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs"
);
const { Schemas } = ChromeUtils.importESModule(
  "resource://gre/modules/Schemas.sys.mjs"
);
const { Subprocess } = ChromeUtils.importESModule(
  "resource://gre/modules/Subprocess.sys.mjs"
);
const { NativeApp } = ChromeUtils.importESModule(
  "resource://gre/modules/NativeMessaging.sys.mjs"
);

let registry = null;
if (AppConstants.platform == "win") {
  var { MockRegistry } = ChromeUtils.importESModule(
    "resource://testing-common/MockRegistry.sys.mjs"
  );
  registry = new MockRegistry();
  registerCleanupFunction(() => {
    registry.shutdown();
  });
  ChromeUtils.defineESModuleGetters(this, {
    SubprocessImpl: "resource://gre/modules/subprocess/subprocess_win.sys.mjs",
  });
} else {
  ChromeUtils.defineESModuleGetters(this, {
    SubprocessImpl: "resource://gre/modules/subprocess/subprocess_unix.sys.mjs",
  });
}

const REGPATH = "Software\\Mozilla\\NativeMessagingHosts";

const BASE_SCHEMA = "chrome://extensions/content/schemas/manifest.json";

const TYPE_SLUG =
  AppConstants.platform === "linux"
    ? "native-messaging-hosts"
    : "NativeMessagingHosts";

let dir = FileUtils.getDir("TmpD", ["NativeManifests"]);
dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

let userDir = dir.clone();
userDir.append("user");
userDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

let globalDir = dir.clone();
globalDir.append("global");
globalDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

add_setup(async function setup() {
  await IOUtils.makeDirectory(PathUtils.join(userDir.path, TYPE_SLUG));
  await IOUtils.makeDirectory(PathUtils.join(globalDir.path, TYPE_SLUG));
});

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
  return IOUtils.writeUTF8(path, manifest);
}

let PYTHON;
add_task(async function setup() {
  await Schemas.load(BASE_SCHEMA);

  try {
    PYTHON = await Subprocess.pathSearch(Services.env.get("PYTHON"));
  } catch (e) {
    notEqual(
      PYTHON,
      null,
      `Can't find a suitable python interpreter ${e.message}`
    );
  }
});

let global = this;

// Test of NativeManifests.lookupApplication() begin here...
let context = {
  extension: {
    id: "extension@tests.mozilla.org",
  },
  manifestVersion: 2,
  envType: "addon_parent",
  url: null,
  jsonStringify(...args) {
    return JSON.stringify(...args);
  },
  cloneScope: global,
  logError() {},
  preprocessors: {},
  callOnClose: () => {},
  forgetOnClose: () => {},
};

class MockContext extends ExtensionCommon.BaseContext {
  constructor(extensionId) {
    let fakeExtension = { id: extensionId, manifestVersion: 2 };
    super("addon_parent", fakeExtension);
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
  equal(
    result,
    null,
    "lookupApplication returns null for non-existent application"
  );
});

const USER_TEST_JSON = PathUtils.join(userDir.path, TYPE_SLUG, "test.json");

add_task(async function test_nonexistent_manifest_with_registry_entry() {
  if (registry) {
    registry.setValue(
      Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
      `${REGPATH}\\test`,
      "",
      USER_TEST_JSON
    );
  }

  await IOUtils.remove(USER_TEST_JSON);
  let { messages, result } = await promiseConsoleOutput(() =>
    lookupApplication("test", context)
  );
  equal(
    result,
    null,
    "lookupApplication returns null for non-existent manifest"
  );

  let noSuchFileErrors = messages.filter(logMessage =>
    logMessage.message.includes(
      "file is referenced in the registry but does not exist"
    )
  );

  if (registry) {
    equal(
      noSuchFileErrors.length,
      1,
      "lookupApplication logs a non-existent manifest file pointed to by the registry"
    );
  } else {
    equal(
      noSuchFileErrors.length,
      0,
      "lookupApplication does not log about registry on non-windows platforms"
    );
  }
});

add_task(async function test_good_manifest() {
  await writeManifest(USER_TEST_JSON, templateManifest);
  if (registry) {
    registry.setValue(
      Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
      `${REGPATH}\\test`,
      "",
      USER_TEST_JSON
    );
  }

  let result = await lookupApplication("test", context);
  notEqual(result, null, "lookupApplication finds a good manifest");
  equal(
    result.path,
    USER_TEST_JSON,
    "lookupApplication returns the correct path"
  );
  deepEqual(
    result.manifest,
    templateManifest,
    "lookupApplication returns the manifest contents"
  );
});

add_task(
  { skip_if: () => AppConstants.platform != "win" },
  async function test_forward_slashes_instead_of_backslashes_in_registry() {
    Assert.ok(USER_TEST_JSON.includes("\\"), `Path has \\: ${USER_TEST_JSON}`);
    const manifest = { ...templateManifest, name: "testslash" };
    await writeManifest(USER_TEST_JSON, manifest);
    registry.setValue(
      Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
      `${REGPATH}\\testslash`,
      "",
      USER_TEST_JSON.replaceAll("\\", "/")
    );

    let result = await lookupApplication("testslash", context);
    notEqual(result, null, "lookupApplication finds the manifest despite /");
    equal(
      result.path,
      USER_TEST_JSON,
      "lookupApplication returns the correct path with platform-native slash"
    );
    // Side note: manifest.path does not contain a platform-native path,
    // but it is normalized when used in NativeMessaging.jsm.
    deepEqual(
      result.manifest,
      manifest,
      "lookupApplication returns the manifest contents"
    );
  }
);

add_task(async function test_manifest_with_utf8_byte_order_mark() {
  const manifest = { ...templateManifest, description: "had BOM at start" };
  const manifestString = JSON.stringify(manifest);

  // "123" to have a placeholder where we'll fill in the 3 BOM bytes.
  const manifestBytes = new TextEncoder().encode("123" + manifestString);
  manifestBytes.set([0xef, 0xbb, 0xbf]);

  // Sanity check: verify that the bytes prepended above have the special
  // meaning of being a UTF-8 BOM. That is, when parsed as UTF-8, the bytes can
  // be removed without loss of meaning.
  equal(
    new TextDecoder().decode(manifestBytes),
    manifestString,
    "Sanity check: input bytes has UTF-8 BOM that is ordinarily stripped"
  );

  await IOUtils.write(USER_TEST_JSON, manifestBytes);
  if (registry) {
    registry.setValue(
      Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
      `${REGPATH}\\test`,
      "",
      USER_TEST_JSON
    );
  }
  let result = await lookupApplication("test", context);
  notEqual(result, null, "lookupApplication finds a good manifest despite BOM");
  deepEqual(
    result.manifest,
    manifest,
    "lookupApplication returns the manifest contents"
  );
});

add_task(async function test_manifest_with_invalid_utf_8() {
  const manifest = { ...templateManifest, description: "bad bytes" };
  const manifestString = JSON.stringify(manifest);
  const manifestBytes = Uint8Array.from(manifestString, c => c.charCodeAt(0));
  // manifestString ends with `bad bytes"}`. Replace the `s` with a bad byte:
  manifestBytes.set([0xff], manifestBytes.byteLength - 3);

  await IOUtils.write(USER_TEST_JSON, manifestBytes);
  if (registry) {
    registry.setValue(
      Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
      `${REGPATH}\\test`,
      "",
      USER_TEST_JSON
    );
  }
  let { messages, result } = await promiseConsoleOutput(() =>
    lookupApplication("test", context)
  );
  equal(result, null, "lookupApplication should reject file with invalid UTF8");
  let errorPattern =
    /NotReadableError: Could not read file.* because it is not UTF-8 encoded/;
  let utf8Errors = messages.filter(({ message }) => errorPattern.test(message));
  equal(utf8Errors.length, 1, "lookupApplication logs error about UTF-8");
});

add_task(async function test_invalid_json() {
  await writeManifest(USER_TEST_JSON, "this is not valid json");
  let { messages, result } = await promiseConsoleOutput(() =>
    lookupApplication("test", context)
  );
  equal(result, null, "lookupApplication ignores bad json");
  let errorPattern = /Error parsing native manifest .*test.json: JSON\.parse:/;
  let jsonErrors = messages.filter(({ message }) => errorPattern.test(message));
  equal(jsonErrors.length, 1, "lookupApplication logs JSON error");
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
  let what = AppConstants.platform == "win" ? "registry key" : "json filename";
  equal(
    result,
    null,
    `lookupApplication ignores mistmatch between ${what} and name property`
  );
});

add_task(async function test_missing_props() {
  const PROPS = ["name", "description", "path", "type", "allowed_extensions"];
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
  equal(
    result,
    null,
    "lookupApplication ignores manifest with no allowed_extensions"
  );
});

const GLOBAL_TEST_JSON = PathUtils.join(globalDir.path, TYPE_SLUG, "test.json");
let globalManifest = Object.assign({}, templateManifest);
globalManifest.description = "This manifest is from the systemwide directory";

add_task(async function good_manifest_system_dir() {
  await IOUtils.remove(USER_TEST_JSON);
  await writeManifest(GLOBAL_TEST_JSON, globalManifest);
  if (registry) {
    registry.setValue(
      Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
      `${REGPATH}\\test`,
      "",
      null
    );
    registry.setValue(
      Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
      `${REGPATH}\\test`,
      "",
      GLOBAL_TEST_JSON
    );
  }

  let where =
    AppConstants.platform == "win" ? "registry location" : "directory";
  let result = await lookupApplication("test", context);
  notEqual(
    result,
    null,
    `lookupApplication finds a manifest in the system-wide ${where}`
  );
  equal(
    result.path,
    GLOBAL_TEST_JSON,
    `lookupApplication returns path in the system-wide ${where}`
  );
  deepEqual(
    result.manifest,
    globalManifest,
    `lookupApplication returns manifest contents from the system-wide ${where}`
  );
});

add_task(async function test_user_dir_precedence() {
  await writeManifest(USER_TEST_JSON, templateManifest);
  if (registry) {
    registry.setValue(
      Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
      `${REGPATH}\\test`,
      "",
      USER_TEST_JSON
    );
  }
  // global test.json and LOCAL_MACHINE registry key on windows are
  // still present from the previous test

  let result = await lookupApplication("test", context);
  notEqual(
    result,
    null,
    "lookupApplication finds a manifest when entries exist in both user-specific and system-wide locations"
  );
  equal(
    result.path,
    USER_TEST_JSON,
    "lookupApplication returns the user-specific path when user-specific and system-wide entries both exist"
  );
  deepEqual(
    result.manifest,
    templateManifest,
    "lookupApplication returns user-specific manifest contents with user-specific and system-wide entries both exist"
  );
});

// Test shutdown handling in NativeApp
add_task(async function test_native_app_shutdown() {
  const SCRIPT = String.raw`
import signal
import struct
import sys

signal.signal(signal.SIGTERM, signal.SIG_IGN)

stdin = getattr(sys.stdin, 'buffer', sys.stdin)
stdout = getattr(sys.stdout, 'buffer', sys.stdout)

while True:
    rawlen = stdin.read(4)
    if len(rawlen) == 0:
        signal.pause()
    msglen = struct.unpack('@I', rawlen)[0]
    msg = stdin.read(msglen)

    stdout.write(struct.pack('@I', msglen))
    stdout.write(msg)
`;

  let scriptPath = PathUtils.join(userDir.path, TYPE_SLUG, "wontdie.py");
  let manifestPath = PathUtils.join(userDir.path, TYPE_SLUG, "wontdie.json");

  const ID = "native@tests.mozilla.org";
  let manifest = {
    name: "wontdie",
    description: "test async shutdown of native apps",
    type: "stdio",
    allowed_extensions: [ID],
  };

  if (AppConstants.platform == "win") {
    await IOUtils.writeUTF8(scriptPath, SCRIPT);

    let batPath = PathUtils.join(userDir.path, TYPE_SLUG, "wontdie.bat");
    let batBody = `@ECHO OFF\n${PYTHON} -u "${scriptPath}" %*\n`;
    await IOUtils.writeUTF8(batPath, batBody);
    await IOUtils.setPermissions(batPath, 0o755);

    manifest.path = batPath;
    await writeManifest(manifestPath, manifest);

    registry.setValue(
      Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
      `${REGPATH}\\wontdie`,
      "",
      manifestPath
    );
  } else {
    await IOUtils.writeUTF8(scriptPath, `#!${PYTHON} -u\n${SCRIPT}`);
    await IOUtils.setPermissions(scriptPath, 0o755);
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
  app.send(new StructuredCloneHolder("", null, buffer));
  await recvPromise;

  app._cleanup();

  info("waiting for async shutdown");
  Services.prefs.setBoolPref("toolkit.asyncshutdown.testing", true);
  AsyncShutdown.profileBeforeChange._trigger();
  Services.prefs.clearUserPref("toolkit.asyncshutdown.testing");

  let procs = await SubprocessImpl.Process.getWorker().call("getProcesses", []);
  equal(procs.size, 0, "native process exited");
});
