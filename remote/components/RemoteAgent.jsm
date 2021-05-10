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
  HttpServer: "chrome://remote/content/server/HTTPD.jsm",
  JSONHandler: "chrome://remote/content/cdp/JSONHandler.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
  RecommendedPreferences:
    "chrome://remote/content/cdp/RecommendedPreferences.jsm",
  TargetList: "chrome://remote/content/cdp/targets/TargetList.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

const ENABLED = "remote.enabled";
const FORCE_LOCAL = "remote.force-local";

const LOOPBACKS = ["localhost", "127.0.0.1", "[::1]"];

class RemoteAgentClass {
  constructor() {
    this.alteredPrefs = new Set();
  }

  get listening() {
    return !!this.server && !this.server.isStopped();
  }

  get debuggerAddress() {
    if (!this.server) {
      return "";
    }

    return `${this.host}:${this.port}`;
  }

  listen(url) {
    if (!Preferences.get(ENABLED, false)) {
      throw Components.Exception(
        "Disabled by preference",
        Cr.NS_ERROR_NOT_AVAILABLE
      );
    }
    if (Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
      throw Components.Exception(
        "May only be instantiated in parent process",
        Cr.NS_ERROR_LAUNCHED_CHILD_PROCESS
      );
    }

    if (this.listening) {
      return Promise.resolve();
    }

    if (!(url instanceof Ci.nsIURI)) {
      url = Services.io.newURI(url);
    }

    let { host, port } = url;
    if (Preferences.get(FORCE_LOCAL) && !LOOPBACKS.includes(host)) {
      throw Components.Exception(
        "Restricted to loopback devices",
        Cr.NS_ERROR_ILLEGAL_VALUE
      );
    }

    // nsIServerSocket uses -1 for atomic port allocation
    if (port === 0) {
      port = -1;
    }

    for (let [k, v] of RecommendedPreferences) {
      if (!Preferences.isSet(k)) {
        logger.debug(`Setting recommended pref ${k} to ${v}`);
        Preferences.set(k, v);
        this.alteredPrefs.add(k);
      }
    }

    this.server = new HttpServer();
    this.server.registerPrefixHandler("/json/", new JSONHandler(this));

    this.targetList = new TargetList();
    this.targetList.on("target-created", (eventName, target) => {
      this.server.registerPathHandler(target.path, target);
    });
    this.targetList.on("target-destroyed", (eventName, target) => {
      this.server.registerPathHandler(target.path, null);
    });

    return this.asyncListen(host, port);
  }

  async asyncListen(host, port) {
    try {
      await this.targetList.watchForTargets();

      // Immediatly instantiate the main process target in order
      // to be accessible via HTTP endpoint on startup
      const mainTarget = this.targetList.getMainProcessTarget();

      this.server._start(port, host);
      Services.obs.notifyObservers(
        null,
        "remote-listening",
        `DevTools listening on ${mainTarget.wsDebuggerURL}`
      );
    } catch (e) {
      await this.close();
      logger.error(`Unable to start remote agent: ${e.message}`, e);
    }
  }

  close() {
    try {
      for (let k of this.alteredPrefs) {
        logger.debug(`Resetting recommended pref ${k}`);
        Preferences.reset(k);
      }
      this.alteredPrefs.clear();

      // destroy targetList before stopping server,
      // otherwise the HTTP will fail to stop
      if (this.targetList) {
        this.targetList.destructor();
      }

      if (this.listening) {
        return this.server.stop();
      }
    } catch (e) {
      // this function must never fail
      logger.error("unable to stop listener", e);
    } finally {
      this.server = null;
      this.targetList = null;
    }

    return Promise.resolve();
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

    // Bug 1675471: When using the nsIRemoteAgent interface the HTTPd server's
    // primary identity ("this.server.identity.primaryHost") is lazily set.
    return this.server._host;
  }

  get port() {
    if (!this.server) {
      return null;
    }

    // Bug 1675471: When using the nsIRemoteAgent interface the HTTPd server's
    // primary identity ("this.server.identity.primaryPort") is lazily set.
    return this.server._port;
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI(["nsIRemoteAgent"]);
  }
}

var RemoteAgent = new RemoteAgentClass();

// This is used by the XPCOM codepath which expects a constructor
var RemoteAgentFactory = function() {
  return RemoteAgent;
};
