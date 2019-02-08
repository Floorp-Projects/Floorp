/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  ActorManagerParent: "resource://gre/modules/ActorManagerParent.jsm",
  ConsoleServiceObserver: "chrome://remote/content/ConsoleServiceObserver.jsm",
  HttpServer: "chrome://remote/content/server/HTTPD.jsm",
  Log: "chrome://remote/content/Log.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  Observer: "chrome://remote/content/Observer.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
  ProtocolHandler: "chrome://remote/content/Handler.jsm",
  RecommendedPreferences: "chrome://remote/content/Prefs.jsm",
  TabObserver: "chrome://remote/content/WindowManager.jsm",
  Target: "chrome://remote/content/Target.jsm",
  TargetListHandler: "chrome://remote/content/Handler.jsm",
});
XPCOMUtils.defineLazyGetter(this, "log", Log.get);

const ENABLED = "remote.enabled";
const FORCE_LOCAL = "remote.force-local";
const HTTPD = "remote.httpd";
const SCHEME = `${HTTPD}.scheme`;
const HOST = `${HTTPD}.host`;
const PORT = `${HTTPD}.port`;

const DEFAULT_HOST = "localhost";
const DEFAULT_PORT = 9222;
const LOOPBACKS = ["localhost", "127.0.0.1", "::1"];

const ACTORS = {
  DOM: {
    child: {
      module: "chrome://remote/content/actor/DOMChild.jsm",
      events: {
        DOMContentLoaded: {},
        DOMWindowCreated: {once: true},
        pageshow: {mozSystemGroup: true},
        pagehide: {mozSystemGroup: true},
      },
    },
  },

  Log: {
    child: {
      module: "chrome://remote/content/actor/LogChild.jsm",
      observers: [
        "console-api-log-event",
      ],
    },
  },
};

class ParentRemoteAgent {
  constructor() {
    this.server = null;
    this.targets = new Targets();

    // TODO(ato): We need a way to dynamically load actors,
    // otherwise these actors may risk being instantiated
    // without the remote agent running!
    ActorManagerParent.addActors(ACTORS);
    ActorManagerParent.flush();

    this.consoleService = new ConsoleServiceObserver();

    this.tabs = new TabObserver({registerExisting: true});
    this.tabs.on("open", tab => this.targets.connect(tab.linkedBrowser));
    this.tabs.on("close", tab => this.targets.disconnect(tab.linkedBrowser));
  }

  // nsIRemoteAgent

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

    this.server = new HttpServer();
    const targetList = new TargetListHandler(this.targets);
    const protocol = new ProtocolHandler();
    targetList.register(this.server);
    protocol.register(this.server);

    try {
      this.server._start(port, host);
      log.info(`Remote debugging agent listening on ${this.scheme}://${this.host}:${this.port}/`);
    } catch (e) {
      throw new Error(`Unable to start agent on ${port}: ${e.message}`, e);
    }

    Preferences.set(RecommendedPreferences);
    Preferences.set(SCHEME, this.scheme);
    Preferences.set(HOST, this.host);
    Preferences.set(PORT, this.port);
  }

  async close() {
    if (this.listening) {
      try {
        await this.server.stop();

        Preferences.resetBranch(HTTPD);
        Preferences.reset(Object.keys(RecommendedPreferences));

        this.consoleService.stop();
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
    let flag;
    try {
      flag = cmdLine.handleFlagWithParam("debug", false);
    } catch (e) {
      flag = cmdLine.handleFlag("debug", false);
    }

    if (!flag) {
      return;
    }

    let host, port;
    if (typeof flag == "string") {
      [host, port] = flag.split(":");
    }

    let addr;
    try {
      addr = NetUtil.newURI(`http://${host || DEFAULT_HOST}:${port || DEFAULT_PORT}/`);
    } catch (e) {
      log.fatal(`Expected address syntax [<host>]:<port>: ${flag}`);
      cmdLine.preventDefault = true;
      return;
    }

    await Observer.once("sessionstore-windows-restored");
    await this.tabs.start();

    this.consoleService.start();

    try {
      this.listen(addr);
    } catch ({message}) {
      this.close();
      log.fatal(`Unable to start remote agent on ${addr.spec}: ${message}`);
      cmdLine.preventDefault = true;
    }
  }

  get helpInfo() {
    return "  --debug [<host>][:<port>] Start the Firefox remote agent, which is a low-level\n" +
           "                     debugging interface based on the CDP protocol.  Defaults to\n" +
           "                     listen on port 9222.\n";
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI([
      Ci.nsICommandLineHandler,
      Ci.nsIRemoteAgent,
    ]);
  }
}

class Targets {
  constructor() {
    // browser context ID -> Target<XULElement>
    this._targets = new Map();
  }

  /** @param {XULElement|Target} subject */
  connect(subject) {
    let target = subject;
    if (!(subject instanceof Target)) {
      target = new Target(subject);
    }

    target.connect();
    this._targets.set(target.id, target);
  }

  /** @param {XULElement|Target} subject */
  disconnect(subject) {
    let target = subject;
    if (!(subject instanceof Target)) {
      target = this._targets.get(subject.browsingContext.id);
    }

    if (target) {
      target.disconnect();
      this._targets.delete(target.id);
    }
  }

  clear() {
    for (const target of this) {
      this.disconnect(target);
    }
  }

  get size() {
    return this._targets.size;
  }

  * [Symbol.iterator]() {
    for (const target of this._targets.values()) {
      yield target;
    }
  }

  toJSON() {
    return [...this];
  }

  toString() {
    return `[object Targets ${this.size}]`;
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
