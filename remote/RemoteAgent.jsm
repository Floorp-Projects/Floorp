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
  JSONHandler: "chrome://remote/content/JSONHandler.jsm",
  Log: "chrome://remote/content/Log.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
  RecommendedPreferences: "chrome://remote/content/RecommendedPreferences.jsm",
  TargetList: "chrome://remote/content/targets/TargetList.jsm",
});
XPCOMUtils.defineLazyGetter(this, "log", Log.get);

const ENABLED = "remote.enabled";
const FORCE_LOCAL = "remote.force-local";

const LOOPBACKS = ["localhost", "127.0.0.1", "[::1]"];

class RemoteAgentClass {
  get listening() {
    return !!this.server && !this.server.isStopped();
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

    if (this.listening) {
      return Promise.resolve();
    }

    Preferences.set(RecommendedPreferences);

    this.server = new HttpServer();
    this.server.registerPrefixHandler("/json/", new JSONHandler(this));

    this.targets = new TargetList();
    this.targets.on("target-created", (eventName, target) => {
      this.server.registerPathHandler(target.path, target);
    });
    this.targets.on("target-destroyed", (eventName, target) => {
      this.server.registerPathHandler(target.path, null);
    });

    return this.asyncListen(host, port);
  }

  async asyncListen(host, port) {
    try {
      await this.targets.watchForTargets();

      // Immediatly instantiate the main process target in order
      // to be accessible via HTTP endpoint on startup
      const mainTarget = this.targets.getMainProcessTarget();

      this.server._start(port, host);
      Services.obs.notifyObservers(
        null,
        "remote-listening",
        mainTarget.wsDebuggerURL
      );
    } catch (e) {
      await this.close();
      log.error(`Unable to start remote agent: ${e.message}`, e);
    }
  }

  close() {
    try {
      // if called early at startup, preferences may not be available
      try {
        Preferences.reset(Object.keys(RecommendedPreferences));
      } catch (e) {}

      // destroy targets before stopping server,
      // otherwise the HTTP will fail to stop
      if (this.targets) {
        this.targets.destructor();
      }

      if (this.listening) {
        return this.server.stop();
      }
    } catch (e) {
      // this function must never fail
      log.error("unable to stop listener", e);
    } finally {
      this.server = null;
      this.targets = null;
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
    return this.server.identity.primaryHost;
  }

  get port() {
    if (!this.server) {
      return null;
    }
    return this.server.identity.primaryPort;
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
