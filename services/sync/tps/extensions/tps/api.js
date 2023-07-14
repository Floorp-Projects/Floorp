/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs"
);

/* globals ExtensionAPI, Services, XPCOMUtils */

XPCOMUtils.defineLazyServiceGetter(
  this,
  "resProto",
  "@mozilla.org/network/protocol;1?name=resource",
  "nsISubstitutingProtocolHandler"
);

async function tpsStartup() {
  try {
    var { TPS } = ChromeUtils.importESModule("resource://tps/tps.sys.mjs");
    let { goQuitApplication } = ChromeUtils.importESModule(
      "resource://tps/quit.sys.mjs"
    );
    TPS.goQuitApplication = goQuitApplication;

    let testFile = Services.prefs.getStringPref("testing.tps.testFile", "");
    let testPhase = Services.prefs.getStringPref("testing.tps.testPhase", "");
    if (!testFile || !testPhase) {
      // Note: this quits.
      TPS.DumpError(
        "TPS no longer takes arguments from the command line. " +
          "instead you need to pass preferences  `testing.tps.{testFile,testPhase}` " +
          "and optionally `testing.tps.{logFile,ignoreUnusedEngines}`.\n"
      );
    }

    let logFile = Services.prefs.getStringPref("testing.tps.logFile", "");
    let ignoreUnusedEngines = Services.prefs.getBoolPref(
      "testing.tps.ignoreUnusedEngines",
      false
    );
    let options = { ignoreUnusedEngines };
    let testFileUri = Services.io.newFileURI(new FileUtils.File(testFile)).spec;

    try {
      await TPS.RunTestPhase(testFileUri, testPhase, logFile, options);
    } catch (err) {
      TPS.DumpError("TestPhase failed", err);
    }
  } catch (e) {
    if (typeof TPS != "undefined") {
      // Note: This calls quit() under the hood
      TPS.DumpError("Test initialization failed", e);
    }
    dump(`TPS test initialization failed: ${e} - ${e.stack}\n`);
    // Try and quit right away, no reason to wait around for python
    // to kill us if initialization failed.
    Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
  }
}

this.tps = class extends ExtensionAPI {
  onStartup() {
    resProto.setSubstitution(
      "tps",
      Services.io.newURI("resource/", null, this.extension.rootURI)
    );
    /* Ignore the platform's online/offline status while running tests. */
    Services.io.manageOfflineStatus = false;
    Services.io.offline = false;
    tpsStartup();
  }

  onShutdown() {
    resProto.setSubstitution("tps", null);
  }
};
