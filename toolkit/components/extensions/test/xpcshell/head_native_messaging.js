/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* globals AppConstants, FileUtils */
/* exported getSubprocessCount, setupHosts, waitForSubprocessExit */

XPCOMUtils.defineLazyModuleGetter(this, "MockRegistry",
                                  "resource://testing-common/MockRegistry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

let {Subprocess, SubprocessImpl} = Cu.import("resource://gre/modules/Subprocess.jsm");


let tmpDir = FileUtils.getDir("TmpD", ["NativeMessaging"]);
tmpDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

do_register_cleanup(() => {
  tmpDir.remove(true);
});

function getPath(filename) {
  return OS.Path.join(tmpDir.path, filename);
}

const ID = "native@tests.mozilla.org";


function* setupHosts(scripts) {
  const PERMS = {unixMode: 0o755};

  const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  const pythonPath = yield Subprocess.pathSearch(env.get("PYTHON"));

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

      for (let script of scripts) {
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

      for (let script of scripts) {
        let batPath = getPath(`${script.name}.bat`);
        let scriptPath = getPath(`${script.name}.py`);

        let batBody = `@ECHO OFF\n${pythonPath} -u ${scriptPath} %*\n`;
        yield OS.File.writeAtomic(batPath, batBody);

        // Create absolute and relative path versions of the entry.
        for (let [name, path] of [[script.name, batPath],
                                  [`relative.${script.name}`, OS.Path.basename(batPath)]]) {
          script.name = name;
          let manifestPath = yield writeManifest(script, scriptPath, path);

          registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                            `${REGKEY}\\${script.name}`, "", manifestPath);
        }
      }
      break;

    default:
      ok(false, `Native messaging is not supported on ${AppConstants.platform}`);
  }
}


function getSubprocessCount() {
  return SubprocessImpl.Process.getWorker().call("getProcesses", [])
                       .then(result => result.size);
}
function waitForSubprocessExit() {
  return SubprocessImpl.Process.getWorker().call("waitForNoProcesses", []);
}
