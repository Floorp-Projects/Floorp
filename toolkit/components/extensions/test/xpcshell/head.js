"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

/* exported createHttpServer, promiseConsoleOutput, cleanupDir */

Components.utils.import("resource://gre/modules/AppConstants.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Timer.jsm");
Components.utils.import("resource://testing-common/AddonTestUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ContentTask",
                                  "resource://testing-common/ContentTask.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Extension",
                                  "resource://gre/modules/Extension.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionData",
                                  "resource://gre/modules/Extension.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionTestUtils",
                                  "resource://testing-common/ExtensionXPCShellUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "HttpServer",
                                  "resource://testing-common/httpd.js");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

ExtensionTestUtils.init(this);

/**
 * Creates a new HttpServer for testing, and begins listening on the
 * specified port. Automatically shuts down the server when the test
 * unit ends.
 *
 * @param {integer} [port]
 *        The port to listen on. If omitted, listen on a random
 *        port. The latter is the preferred behavior.
 *
 * @returns {HttpServer}
 */
function createHttpServer(port = -1) {
  let server = new HttpServer();
  server.start(port);

  do_register_cleanup(() => {
    return new Promise(resolve => {
      server.stop(resolve);
    });
  });

  return server;
}

if (AppConstants.platform === "android") {
  Services.io.offline = true;
}

var promiseConsoleOutput = async function(task) {
  const DONE = `=== console listener ${Math.random()} done ===`;

  let listener;
  let messages = [];
  let awaitListener = new Promise(resolve => {
    listener = msg => {
      if (msg == DONE) {
        resolve();
      } else {
        void (msg instanceof Ci.nsIConsoleMessage);
        messages.push(msg);
      }
    };
  });

  Services.console.registerListener(listener);
  try {
    let result = await task();

    Services.console.logStringMessage(DONE);
    await awaitListener;

    return {messages, result};
  } finally {
    Services.console.unregisterListener(listener);
  }
};

// Attempt to remove a directory.  If the Windows OS is still using the
// file sometimes remove() will fail.  So try repeatedly until we can
// remove it or we give up.
function cleanupDir(dir) {
  let count = 0;
  return new Promise((resolve, reject) => {
    function tryToRemoveDir() {
      count += 1;
      try {
        dir.remove(true);
      } catch (e) {
        // ignore
      }
      if (!dir.exists()) {
        return resolve();
      }
      if (count >= 25) {
        return reject(`Failed to cleanup directory: ${dir}`);
      }
      setTimeout(tryToRemoveDir, 100);
    }
    tryToRemoveDir();
  });
}
