/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  FatalError: "chrome://remote/content/Error.jsm",
  HttpServer: "chrome://remote/content/server/HTTPD.jsm",
  JSONHandler: "chrome://remote/content/JSONHandler.jsm",
  Log: "chrome://remote/content/Log.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  Observer: "chrome://remote/content/Observer.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
  RecommendedPreferences: "chrome://remote/content/RecommendedPreferences.jsm",
  TabObserver: "chrome://remote/content/WindowManager.jsm",
  Targets: "chrome://remote/content/targets/Targets.jsm",
});
XPCOMUtils.defineLazyGetter(this, "log", Log.get);

const ENABLED = "remote.enabled";
const FORCE_LOCAL = "remote.force-local";

const DEFAULT_HOST = "localhost";
const DEFAULT_PORT = 9222;
const LOOPBACKS = ["localhost", "127.0.0.1", "[::1]"];

class ParentRemoteAgent {
  constructor() {
    this.server = new HttpServer();
    this.targets = new Targets();

    this.server.registerPrefixHandler("/json/", new JSONHandler(this));

    this.tabs = new TabObserver({registerExisting: true});
    this.tabs.on("open", (eventName, tab) => {
      this.targets.connect(tab.linkedBrowser);
    });
    this.tabs.on("close", (eventName, tab) => {
      this.targets.disconnect(tab.linkedBrowser);
    });

    this.targets.on("connect", (eventName, target) => {
      if (!target.path) {
        throw new Error(`Target is missing 'path' attribute: ${target}`);
      }
      this.server.registerPathHandler(target.path, target);
    });
    this.targets.on("disconnect", (eventName, target) => {
      // TODO(ato): removing a handler is currently not possible
    });

    // This allows getting access to the underlying JS object
    // of the @mozilla.org/remote/agent XPCOM components
    this.wrappedJSObject = this;
  }

  get listening() {
    return !!this.server && !this.server._socketClosed;
  }

  listen(address) {
    if (!(address instanceof Ci.nsIURI)) {
      throw new TypeError(`Expected nsIURI: ${address}`);
    }

    let {host, port} = address;
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

    try {
      this.server._start(port, host);
      log.info(`Remote debugging agent listening on ${this.scheme}://${this.host}:${this.port}/`);
    } catch (e) {
      throw new Error(`Unable to start remote agent: ${e.message}`, e);
    }

    Preferences.set(RecommendedPreferences);
  }

  async close() {
    if (this.listening) {
      try {
        await this.server.stop();

        Preferences.reset(Object.keys(RecommendedPreferences));

        this.tabs.stop();
        this.targets.clear();
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
      log.fatal("Conflicting flags --remote-debugger and --remote-debugging-port");
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
      addr = NetUtil.newURI(`http://${host || DEFAULT_HOST}:${port || DEFAULT_PORT}/`);
    } catch (e) {
      log.fatal(`Expected address syntax [<host>]:<port>: ${remoteDebugger || remoteDebuggingPort}`);
      cmdLine.preventDefault = true;
      return;
    }

    await Observer.once("sessionstore-windows-restored");
    await this.tabs.start();

    try {
      this.listen(addr);
    } catch (e) {
      this.close();
      throw new FatalError(`Unable to start remote agent on ${addr.spec}: ${e.message}`, e);
     }
  }

  get helpInfo() {
    return "  --remote-debugger [<host>][:<port>]\n" +
           "  --remote-debugging-port <port> Start the Firefox remote agent, which is \n" +
           "                     a low-level debugging interface based on the CDP protocol.\n" +
           "                     Defaults to listen on localhost:9222.\n";
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI([Ci.nsICommandLineHandler]);
  }
}

const RemoteAgentFactory = {
  instance_: null,

  createInstance(outer, iid) {
    if (outer) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    if (!Preferences.get(ENABLED, false)) {
      return null;
    }

    if (Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT) {
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    }

    if (!this.instance_) {
      this.instance_ = new ParentRemoteAgent();
    }
    return this.instance_.QueryInterface(iid);
  },
};

function RemoteAgent() {}

RemoteAgent.prototype = {
    classDescription: "Remote Agent",
    classID: Components.ID("{8f685a9d-8181-46d6-a71d-869289099c6d}"),
    contractID: "@mozilla.org/remote/agent",
    _xpcom_factory: RemoteAgentFactory,  /* eslint-disable-line */
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RemoteAgent]);
