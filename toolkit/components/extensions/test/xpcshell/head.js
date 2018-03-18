"use strict";

/* exported createHttpServer, promiseConsoleOutput, cleanupDir, clearCache, testEnv */

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");
ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  ContentTask: "resource://testing-common/ContentTask.jsm",
  Extension: "resource://gre/modules/Extension.jsm",
  ExtensionData: "resource://gre/modules/Extension.jsm",
  ExtensionParent: "resource://gre/modules/ExtensionParent.jsm",
  ExtensionTestUtils: "resource://testing-common/ExtensionXPCShellUtils.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  HttpServer: "resource://testing-common/httpd.js",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  PromiseTestUtils: "resource://testing-common/PromiseTestUtils.jsm",
  Schemas: "resource://gre/modules/Schemas.jsm",
});

XPCOMUtils.defineLazyServiceGetter(this, "proxyService",
                                   "@mozilla.org/network/protocol-proxy-service;1",
                                   "nsIProtocolProxyService");

// These values may be changed in later head files and tested in check_remote
// below.
Services.prefs.setBoolPref("extensions.webextensions.remote", false);
const testEnv = {
  expectRemote: false,
};

add_task(function check_remote() {
  Assert.equal(WebExtensionPolicy.useRemoteWebExtensions, testEnv.expectRemote, "useRemoteWebExtensions matches");
  Assert.equal(WebExtensionPolicy.isExtensionProcess, !testEnv.expectRemote, "testing from extension process");
});

ExtensionTestUtils.init(this);

/**
 * Creates a new HttpServer for testing, and begins listening on the
 * specified port. Automatically shuts down the server when the test
 * unit ends.
 *
 * @param {object} [options = {}]
 * @param {integer} [options.port = -1]
 *        The port to listen on. If omitted, listen on a random
 *        port. The latter is the preferred behavior.
 * @param {sequence<string>?} [options.hosts = null]
 *        A set of hosts to accept connections to. Support for this is
 *        implemented using a proxy filter.
 *
 * @returns {HttpServer}
 */
function createHttpServer({port = -1, hosts} = {}) {
  let server = new HttpServer();
  server.start(port);

  if (hosts) {
    hosts = new Set(hosts);
    const serverHost = "localhost";
    const serverPort = server.identity.primaryPort;

    for (let host of hosts) {
      server.identity.add("http", host, 80);
    }

    const proxyFilter = {
      proxyInfo: proxyService.newProxyInfo("http", serverHost, serverPort, 0, 4096, null),

      applyFilter(service, channel, defaultProxyInfo, callback) {
        if (hosts.has(channel.URI.host)) {
          callback.onProxyFilterResult(this.proxyInfo);
        } else {
          callback.onProxyFilterResult(defaultProxyInfo);
        }
      },
    };

    proxyService.registerChannelFilter(proxyFilter, 0);
    registerCleanupFunction(() => {
      proxyService.unregisterChannelFilter(proxyFilter);
    });
  }

  registerCleanupFunction(() => {
    return new Promise(resolve => {
      server.stop(resolve);
    });
  });

  return server;
}

if (AppConstants.platform === "android") {
  Services.io.offline = true;
}

/**
 * Clears the HTTP and content image caches.
 */
function clearCache() {
  Services.cache2.clear();

  let imageCache = Cc["@mozilla.org/image/tools;1"]
      .getService(Ci.imgITools)
      .getImgCacheForDocument(null);
  imageCache.clearCache(false);
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
        void (msg instanceof Ci.nsIScriptError);
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
