/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["RemoteAgent", "RemoteAgentFactory"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  FatalError: "chrome://remote/content/Error.jsm",
  HttpServer: "chrome://remote/content/server/HTTPD.jsm",
  JSONHandler: "chrome://remote/content/JSONHandler.jsm",
  Log: "chrome://remote/content/Log.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  Observer: "chrome://remote/content/Observer.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
  RecommendedPreferences: "chrome://remote/content/RecommendedPreferences.jsm",
  Targets: "chrome://remote/content/targets/Targets.jsm",
});
XPCOMUtils.defineLazyGetter(this, "log", Log.get);

const ENABLED = "remote.enabled";
const FORCE_LOCAL = "remote.force-local";

const DEFAULT_HOST = "localhost";
const DEFAULT_PORT = 9222;
const LOOPBACKS = ["localhost", "127.0.0.1", "[::1]"];

class RemoteAgentClass {
  async init() {
    if (!Preferences.get(ENABLED, false)) {
      throw new Error("Remote agent is disabled by its preference");
    }
    if (Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
      throw new Error(
        "Remote agent can only be instantiated from the parent process"
      );
    }

    if (this.server) {
      return;
    }

    this.server = new HttpServer();
    this.targets = new Targets();

    // Register the static HTTP endpoints like /json/version or /json/list
    this.server.registerPrefixHandler("/json/", new JSONHandler(this));

    // Register the dynamic HTTP endpoint of each target.
    // These are WebSocket URL where the HTTP request will be morphed
    // into a WebSocket connectiong after an handshake.
    this.targets.on("target-created", (eventName, target) => {
      if (!target.path) {
        throw new Error(`Target is missing 'path' attribute: ${target}`);
      }
      this.server.registerPathHandler(target.path, target);
    });
    this.targets.on("target-destroyed", (eventName, target) => {
      // TODO: This removes the entry added by registerPathHandler, should rather expose
      // an unregisterPathHandler method on nsHttpServer.
      delete this.server._handler._overridePaths[target.path];
    });
  }

  get listening() {
    return !!this.server && !this.server._socketClosed;
  }

  async listen(address) {
    if (!(address instanceof Ci.nsIURI)) {
      throw new TypeError(`Expected nsIURI: ${address}`);
    }

    let { host, port } = address;
    if (Preferences.get(FORCE_LOCAL) && !LOOPBACKS.includes(host)) {
      throw new Error("Restricted to loopback devices");
    }

    // nsIServerSocket uses -1 for atomic port allocation
    if (port === 0) {
      port = -1;
    }

    if (this.listening) {
      return;
    }

    await this.init();

    // Start watching for targets *after* registering the target listeners
    // as this will fire event for already-existing targets.
    await this.targets.watchForTargets();

    try {
      // Immediatly instantiate the main process target in order
      // to be accessible via HTTP endpoint on startup
      const mainTarget = this.targets.getMainProcessTarget();

      this.server._start(port, host);
      dump(`DevTools listening on ${mainTarget.wsDebuggerURL}\n`);
    } catch (e) {
      throw new Error(`Unable to start remote agent: ${e.message}`, e);
    }

    Preferences.set(RecommendedPreferences);
  }

  async close() {
    if (this.listening) {
      try {
        // Destroy all the targets first in order to ensure closing all pending
        // connections first. Otherwise Httpd's stop is not going to resolve.
        this.targets.destructor();

        await this.server.stop();

        Preferences.reset(Object.keys(RecommendedPreferences));
      } catch (e) {
        throw new Error(`Unable to stop agent: ${e.message}`, e);
      }

      log.info("Stopped listening");
    }
  }

  get scheme() {
    if (!this.server) {
      return null;
    }
    return this.server.identity.primaryScheme;
  }

  get host() {
    if (!this.server) {
      return null;
    }
    return this.server.identity.primaryHost;
  }

  get port() {
    if (!this.server) {
      return null;
    }
    return this.server.identity.primaryPort;
  }

  // nsICommandLineHandler

  async handle(cmdLine) {
    function flag(name) {
      const caseSensitive = true;
      try {
        return cmdLine.handleFlagWithParam(name, caseSensitive);
      } catch (e) {
        return cmdLine.handleFlag(name, caseSensitive);
      }
    }

    const remoteDebugger = flag("remote-debugger");
    const remoteDebuggingPort = flag("remote-debugging-port");

    if (remoteDebugger && remoteDebuggingPort) {
      log.fatal(
        "Conflicting flags --remote-debugger and --remote-debugging-port"
      );
      cmdLine.preventDefault = true;
      return;
    }

    if (!remoteDebugger && !remoteDebuggingPort) {
      return;
    }

    let host, port;
    if (typeof remoteDebugger == "string") {
      [host, port] = remoteDebugger.split(":");
    } else if (typeof remoteDebuggingPort == "string") {
      port = remoteDebuggingPort;
    }

    let addr;
    try {
      addr = NetUtil.newURI(
        `http://${host || DEFAULT_HOST}:${port || DEFAULT_PORT}/`
      );
    } catch (e) {
      log.fatal(
        `Expected address syntax [<host>]:<port>: ${remoteDebugger ||
          remoteDebuggingPort}`
      );
      cmdLine.preventDefault = true;
      return;
    }

    await Observer.once("sessionstore-windows-restored");

    try {
      this.listen(addr);
    } catch (e) {
      this.close();
      throw new FatalError(
        `Unable to start remote agent on ${addr.spec}: ${e.message}`,
        e
      );
    }
  }

  get helpInfo() {
    return (
      "  --remote-debugger [<host>][:<port>]\n" +
      "  --remote-debugging-port <port> Start the Firefox remote agent, which is \n" +
      "                     a low-level debugging interface based on the CDP protocol.\n" +
      "                     Defaults to listen on localhost:9222.\n"
    );
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI([Ci.nsICommandLineHandler]);
  }
}

var RemoteAgent = new RemoteAgentClass();

// This is used by the XPCOM codepath which expects a constructor
var RemoteAgentFactory = function() {
  return RemoteAgent;
};
