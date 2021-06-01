/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["RemoteAgent", "RemoteAgentFactory"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",

  CDP: "chrome://remote/content/cdp/CDP.jsm",
  HttpServer: "chrome://remote/content/server/HTTPD.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
  RecommendedPreferences:
    "chrome://remote/content/cdp/RecommendedPreferences.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

const PREF_ACTIVE_PROTOCOLS = "remote.active-protocols";
const PREF_FORCE_LOCAL = "remote.force-local";

// const BIDI_ACTIVE = 0x1;
const CDP_ACTIVE = 0x2;

const LOOPBACKS = ["localhost", "127.0.0.1", "[::1]"];

class RemoteAgentClass {
  constructor() {
    this.cdp = null;
    this.server = null;

    this.alteredPrefs = new Set();

    const protocols = Services.prefs.getIntPref(PREF_ACTIVE_PROTOCOLS);
    if (protocols < 1 || protocols > 3) {
      throw Error(`Invalid remote protocol identifier: ${protocols}`);
    }
    this.activeProtocols = protocols;
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
    if (Preferences.get(PREF_FORCE_LOCAL) && !LOOPBACKS.includes(host)) {
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

    if ((this.activeProtocols & CDP_ACTIVE) === CDP_ACTIVE) {
      this.cdp = new CDP(this.server);
    }

    return this.asyncListen(host, port);
  }

  async asyncListen(host, port) {
    try {
      this.server._start(port, host);

      await this.cdp?.start();
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

      // Stop the CDP support before stopping the server.
      // Otherwise the HTTP server will fail to stop.
      this.cdp?.stop();

      if (this.listening) {
        return this.server.stop();
      }
    } catch (e) {
      // this function must never fail
      logger.error("unable to stop listener", e);
    } finally {
      this.cdp = null;
      this.server = null;
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
