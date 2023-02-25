/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* globals AppConstants, FileUtils */
/* exported getSubprocessCount, setupHosts, waitForSubprocessExit */

ChromeUtils.defineESModuleGetters(this, {
  MockRegistry: "resource://testing-common/MockRegistry.sys.mjs",
});
if (AppConstants.platform == "win") {
  ChromeUtils.defineESModuleGetters(this, {
    SubprocessImpl: "resource://gre/modules/subprocess/subprocess_win.sys.mjs",
  });
} else {
  ChromeUtils.defineESModuleGetters(this, {
    SubprocessImpl: "resource://gre/modules/subprocess/subprocess_unix.sys.mjs",
  });
}

const { Subprocess } = ChromeUtils.importESModule(
  "resource://gre/modules/Subprocess.sys.mjs"
);

// It's important that we use a space in this directory name to make sure we
// correctly handle executing batch files with spaces in their path.
let tmpDir = FileUtils.getDir("TmpD", ["Native Messaging"]);
tmpDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

const TYPE_SLUG =
  AppConstants.platform === "linux"
    ? "native-messaging-hosts"
    : "NativeMessagingHosts";

add_setup(async function setup() {
  await IOUtils.makeDirectory(PathUtils.join(tmpDir.path, TYPE_SLUG));
});

registerCleanupFunction(async () => {
  await IOUtils.remove(tmpDir.path, { recursive: true });
});

function getPath(filename) {
  return PathUtils.join(tmpDir.path, TYPE_SLUG, filename);
}

const ID = "native@tests.mozilla.org";

async function setupHosts(scripts) {
  const pythonPath = await Subprocess.pathSearch(Services.env.get("PYTHON"));

  async function writeManifest(script, scriptPath, path) {
    let body = `#!${pythonPath} -u\n${script.script}`;

    await IOUtils.writeUTF8(scriptPath, body);
    await IOUtils.setPermissions(scriptPath, 0o755);

    let manifest = {
      name: script.name,
      description: script.description,
      path,
      type: "stdio",
      allowed_extensions: [ID],
    };

    // Optionally, allow the test to change the manifest before writing.
    script._hookModifyManifest?.(manifest);

    let manifestPath = getPath(`${script.name}.json`);
    await IOUtils.writeJSON(manifestPath, manifest);

    return manifestPath;
  }

  switch (AppConstants.platform) {
    case "macosx":
    case "linux":
      let dirProvider = {
        getFile(property) {
          if (property == "XREUserNativeManifests") {
            return tmpDir.clone();
          } else if (property == "XRESysNativeManifests") {
            return tmpDir.clone();
          }
          return null;
        },
      };

      Services.dirsvc.registerProvider(dirProvider);
      registerCleanupFunction(() => {
        Services.dirsvc.unregisterProvider(dirProvider);
      });

      for (let script of scripts) {
        let path = getPath(`${script.name}.py`);

        await writeManifest(script, path, path);
      }
      break;

    case "win":
      const REGKEY = String.raw`Software\Mozilla\NativeMessagingHosts`;

      let registry = new MockRegistry();
      registerCleanupFunction(() => {
        registry.shutdown();
      });

      for (let script of scripts) {
        let { scriptExtension = "bat" } = script;

        // It's important that we use a space in this filename. See directory
        // name comment above.
        let batPath = getPath(`batch ${script.name}.${scriptExtension}`);
        let scriptPath = getPath(`${script.name}.py`);

        let batBody = `@ECHO OFF\n${pythonPath} -u "${scriptPath}" %*\n`;
        await IOUtils.writeUTF8(batPath, batBody);

        let manifestPath = await writeManifest(script, scriptPath, batPath);

        registry.setValue(
          Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
          `${REGKEY}\\${script.name}`,
          "",
          manifestPath
        );
      }
      break;

    default:
      ok(
        false,
        `Native messaging is not supported on ${AppConstants.platform}`
      );
  }
}

function getSubprocessCount() {
  return SubprocessImpl.Process.getWorker()
    .call("getProcesses", [])
    .then(result => result.size);
}
function waitForSubprocessExit() {
  return SubprocessImpl.Process.getWorker()
    .call("waitForNoProcesses", [])
    .then(() => {
      // Return to the main event loop to give IO handlers enough time to consume
      // their remaining buffered input.
      return new Promise(resolve => setTimeout(resolve, 0));
    });
}
