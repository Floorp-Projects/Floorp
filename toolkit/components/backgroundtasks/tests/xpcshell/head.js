/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { Subprocess } = ChromeUtils.import(
  "resource://gre/modules/Subprocess.jsm"
);

function getFirefoxExecutableFilename() {
  if (AppConstants.platform === "win") {
    return AppConstants.MOZ_APP_NAME + ".exe";
  }
  return AppConstants.MOZ_APP_NAME;
}

// Returns a nsIFile to the firefox.exe (really, application) executable file.
function getFirefoxExecutableFile() {
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file = Services.dirsvc.get("GreBinD", Ci.nsIFile);

  file.append(getFirefoxExecutableFilename());
  return file;
}

async function do_backgroundtask(
  task,
  options = { extraArgs: [], extraEnv: {} }
) {
  options = Object.assign({}, options);
  options.extraArgs = options.extraArgs || [];
  options.extraEnv = options.extraEnv || {};

  let command = getFirefoxExecutableFile().path;
  let args = ["--backgroundtask", task];
  args.push(...options.extraArgs);

  // Ensure `resource://testing-common` gets mapped.
  let protocolHandler = Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);

  let uri = protocolHandler.getSubstitution("testing-common");
  Assert.ok(uri, "resource://testing-common is not substituted");

  // The equivalent of _TESTING_MODULES_DIR in xpcshell.
  options.extraEnv.XPCSHELL_TESTING_MODULES_URI = uri.spec;

  // Now we can actually invoke the process.
  info(
    `launching child process ${command} with args: ${args} and extra environment: ${JSON.stringify(
      options.extraEnv
    )}`
  );
  let proc = await Subprocess.call({
    command,
    arguments: args,
    environment: options.extraEnv,
    environmentAppend: true,
    stderr: "stdout",
  }).then(p => {
    p.stdin.close();
    const dumpPipe = async pipe => {
      let data = await pipe.readString();
      while (data) {
        for (let line of data.split(/\r\n|\r|\n/).slice(0, -1)) {
          dump("> " + line + "\n");
        }
        data = await pipe.readString();
      }
    };
    dumpPipe(p.stdout);

    return p;
  });

  let { exitCode } = await proc.wait();
  return exitCode;
}

// Setup that allows to use the profile service, lifted from
// `toolkit/profile/xpcshell/head.js`.
function setupProfileService() {
  let gProfD = do_get_profile();
  let gDataHome = gProfD.clone();
  gDataHome.append("data");
  gDataHome.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  let gDataHomeLocal = gProfD.clone();
  gDataHomeLocal.append("local");
  gDataHomeLocal.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o755);

  let xreDirProvider = Cc["@mozilla.org/xre/directory-provider;1"].getService(
    Ci.nsIXREDirProvider
  );
  xreDirProvider.setUserDataDirectory(gDataHome, false);
  xreDirProvider.setUserDataDirectory(gDataHomeLocal, true);
}
