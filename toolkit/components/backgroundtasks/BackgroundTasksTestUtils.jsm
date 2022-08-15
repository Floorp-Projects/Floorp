/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

var EXPORTED_SYMBOLS = ["BackgroundTasksTestUtils"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Subprocess } = ChromeUtils.import(
  "resource://gre/modules/Subprocess.jsm"
);

function getFirefoxExecutableFilename() {
  if (AppConstants.platform === "win") {
    return AppConstants.MOZ_APP_NAME + ".exe";
  }
  if (AppConstants.platform == "linux") {
    return AppConstants.MOZ_APP_NAME + "-bin";
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

var BackgroundTasksTestUtils = {
  init(scope) {
    this.testScope = scope;
  },

  async do_backgroundtask(
    task,
    options = { extraArgs: [], extraEnv: {}, onStdoutLine: null }
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
    const { Assert } = this.testScope;
    Assert.ok(!!uri, "resource://testing-common is not substituted");

    // The equivalent of _TESTING_MODULES_DIR in xpcshell.
    options.extraEnv.XPCSHELL_TESTING_MODULES_URI = uri.spec;

    // Now we can actually invoke the process.
    console.info(`launching background task`, {
      command,
      args,
      extraEnv: options.extraEnv,
    });
    let { proc, readPromise } = await Subprocess.call({
      command,
      arguments: args,
      environment: options.extraEnv,
      environmentAppend: true,
      stderr: "stdout",
    }).then(p => {
      p.stdin.close().catch(() => {
        // It's possible that the process exists before we close stdin.
        // In that case, we should ignore the errors.
      });
      const dumpPipe = async pipe => {
        // We must assemble all of the string fragments from stdout.
        let leftover = "";
        let data = await pipe.readString();
        while (data) {
          data = leftover + data;
          // When the string is empty and the separator is not empty,
          // split() returns an array containing one empty string,
          // rather than an empty array, i.e., we always have
          // `lines.length > 0`.
          let lines = data.split(/\r\n|\r|\n/);
          for (let line of lines.slice(0, -1)) {
            dump(`${p.pid}> ${line}\n`);
            if (options.onStdoutLine) {
              options.onStdoutLine(line, p);
            }
          }
          leftover = lines[lines.length - 1];
          data = await pipe.readString();
        }

        if (leftover.length) {
          dump(`${p.pid}> ${leftover}\n`);
          if (options.onStdoutLine) {
            options.onStdoutLine(leftover, p);
          }
        }
      };
      let readPromise = dumpPipe(p.stdout);

      return { proc: p, readPromise };
    });

    let { exitCode } = await proc.wait();
    try {
      // Read from the output pipe.
      await readPromise;
    } catch (e) {
      if (e.message !== "File closed") {
        throw e;
      }
    }

    return exitCode;
  },

  // Setup that allows to use the profile service in xpcshell tests,
  // lifted from `toolkit/profile/xpcshell/head.js`.
  setupProfileService() {
    let gProfD = this.testScope.do_get_profile();
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
  },
};
