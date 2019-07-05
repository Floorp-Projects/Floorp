/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* globals AppConstants, FileUtils */
/* exported getSubprocessCount, setupHosts, waitForSubprocessExit */

ChromeUtils.defineModuleGetter(
  this,
  "MockRegistry",
  "resource://testing-common/MockRegistry.jsm"
);
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

let { Subprocess, SubprocessImpl } = ChromeUtils.import(
  "resource://gre/modules/Subprocess.jsm",
  null
);

// It's important that we use a space in this directory name to make sure we
// correctly handle executing batch files with spaces in their path.
let tmpDir = FileUtils.getDir("TmpD", ["Native Messaging"]);
tmpDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

const TYPE_SLUG =
  AppConstants.platform === "linux"
    ? "native-messaging-hosts"
    : "NativeMessagingHosts";
OS.File.makeDir(OS.Path.join(tmpDir.path, TYPE_SLUG));

registerCleanupFunction(() => {
  tmpDir.remove(true);
});

function getPath(filename) {
  return OS.Path.join(tmpDir.path, TYPE_SLUG, filename);
}

const ID = "native@tests.mozilla.org";

async function setupHosts(scripts) {
  const PERMS = { unixMode: 0o755 };

  const env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  const pythonPath = await Subprocess.pathSearch(env.get("PYTHON"));

  async function writeManifest(script, scriptPath, path) {
    let body = `#!${pythonPath} -u\n${script.script}`;

    await OS.File.writeAtomic(scriptPath, body);
    await OS.File.setPermissions(scriptPath, PERMS);

    let manifest = {
      name: script.name,
      description: script.description,
      path,
      type: "stdio",
      allowed_extensions: [ID],
    };

    let manifestPath = getPath(`${script.name}.json`);
    await OS.File.writeAtomic(manifestPath, JSON.stringify(manifest));

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
        await OS.File.writeAtomic(batPath, batBody);

        // Create absolute and relative path versions of the entry.
        for (let [name, path] of [
          [script.name, batPath],
          [`relative.${script.name}`, OS.Path.basename(batPath)],
        ]) {
          script.name = name;
          let manifestPath = await writeManifest(script, scriptPath, path);

          registry.setValue(
            Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
            `${REGKEY}\\${script.name}`,
            "",
            manifestPath
          );
        }
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
