/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["RemoteAgent", "RemoteAgentFactory"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Preferences: "resource://gre/modules/Preferences.jsm",
  Services: "resource://gre/modules/Services.jsm",

  CDP: "chrome://remote/content/cdp/CDP.jsm",
  HttpServer: "chrome://remote/content/server/HTTPD.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  WebDriverBiDi: "chrome://remote/content/webdriver-bidi/WebDriverBiDi.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

XPCOMUtils.defineLazyGetter(this, "activeProtocols", () => {
  const protocols = Services.prefs.getIntPref("remote.active-protocols");
  if (protocols < 1 || protocols > 3) {
    throw Error(`Invalid remote protocol identifier: ${protocols}`);
  }

  return protocols;
});

const WEBDRIVER_BIDI_ACTIVE = 0x1;
const CDP_ACTIVE = 0x2;

const DEFAULT_PORT = 9222;
// By default force local connections only
const LOOPBACKS = ["localhost", "127.0.0.1", "[::1]"];
const PREF_FORCE_LOCAL = "remote.force-local";

class RemoteAgentClass {
  constructor() {
    this.classID = Components.ID("{8f685a9d-8181-46d6-a71d-869289099c6d}");
    this.helpInfo = `  --remote-debugging-port [<port>] Start the Firefox remote agent,
                     which is a low-level debugging interface based on the
                     CDP protocol. Defaults to listen on localhost:9222.\n`;

    this._enabled = false;
    this._port = DEFAULT_PORT;
    this._server = null;

    this._cdp = null;
    this._webDriverBiDi = null;
  }

  get cdp() {
    return this._cdp;
  }

  get debuggerAddress() {
    if (!this.server) {
      return "";
    }

    return `${this.host}:${this.port}`;
  }

  get enabled() {
    return this._enabled;
  }

  get host() {
    // Bug 1675471: When using the nsIRemoteAgent interface the HTTPd server's
    // primary identity ("this.server.identity.primaryHost") is lazily set.
    return this.server?._host;
  }

  get listening() {
    return !!this.server && !this.server.isStopped();
  }

  get port() {
    // Bug 1675471: When using the nsIRemoteAgent interface the HTTPd server's
    // primary identity ("this.server.identity.primaryPort") is lazily set.
    return this.server?._port;
  }

  get scheme() {
    return this.server?.identity.primaryScheme;
  }

  get server() {
    return this._server;
  }

  get webDriverBiDi() {
    return this._webDriverBiDi;
  }

  handle(cmdLine) {
    // remote-debugging-port has to be consumed in nsICommandLineHandler:handle
    // to avoid issues on macos. See Marionette.jsm::handle() for more details.
    // TODO: remove after Bug 1724251 is fixed.
    try {
      cmdLine.handleFlagWithParam("remote-debugging-port", false);
    } catch (e) {
      cmdLine.handleFlag("remote-debugging-port", false);
    }
  }

  async listen(url) {
    if (Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
      throw Components.Exception(
        "May only be instantiated in parent process",
        Cr.NS_ERROR_LAUNCHED_CHILD_PROCESS
      );
    }

    if (this.listening) {
      return;
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

    try {
      this._server = new HttpServer();
      this.server._start(port, host);

      Services.obs.notifyObservers(null, "remote-listening", true);

      await this.cdp?.start();
      await this.webDriverBiDi?.start();
    } catch (e) {
      await this.close();
      logger.error(`Unable to start remote agent: ${e.message}`, e);
    }
  }

  async close() {
    if (!this.listening) {
      return;
    }

    try {
      // Stop the CDP support before stopping the server.
      // Otherwise the HTTP server will fail to stop.
      await this.cdp?.stop();
      await this.webDriverBiDi?.stop();

      await this.server.stop();
      this._server = null;
      Services.obs.notifyObservers(null, "remote-listening");
    } catch (e) {
      // this function must never fail
      logger.error("unable to stop listener", e);
    }
  }

  /**
   * Handle the --remote-debugging-port command line argument.
   *
   * @param {nsICommandLine} cmdLine
   *     Instance of the command line interface.
   *
   * @return {boolean}
   *     Return `true` if the command line argument has been found.
   */
  handleRemoteDebuggingPortFlag(cmdLine) {
    let enabled = false;

    try {
      // Catch cases when the argument, and a port have been specified.
      const port = cmdLine.handleFlagWithParam("remote-debugging-port", false);
      if (port !== null) {
        enabled = true;

        // In case of an invalid port keep the default port
        const parsed = Number(port);
        if (!isNaN(parsed)) {
          this._port = parsed;
        }
      }
    } catch (e) {
      // If no port has been given check for the existence of the argument.
      enabled = cmdLine.handleFlag("remote-debugging-port", false);
    }

    return enabled;
  }

  async observe(subject, topic) {
    if (this.enabled) {
      logger.trace(`Received observer notification ${topic}`);
    }

    switch (topic) {
      case "profile-after-change":
        Services.obs.addObserver(this, "command-line-startup");
        break;

      case "command-line-startup":
        Services.obs.removeObserver(this, topic);
        this._enabled = this.handleRemoteDebuggingPortFlag(subject);

        if (this.enabled) {
          Services.obs.addObserver(this, "remote-startup-requested");
        }

        // Ideally we should only enable the Remote Agent when the command
        // line argument has been specified. But to allow Browser Chrome tests
        // to run the Remote Agent and the supported protocols also need to be
        // initialized.

        // Listen for application shutdown to also shutdown the Remote Agent
        // and a possible running instance of httpd.js.
        Services.obs.addObserver(this, "quit-application");

        // With Bug 1717899 we will extend the lifetime of the Remote Agent to
        // the whole Firefox session, which will be identical to Marionette. For
        // now prevent logging if the component is not enabled during startup.
        if (
          (activeProtocols & WEBDRIVER_BIDI_ACTIVE) ===
          WEBDRIVER_BIDI_ACTIVE
        ) {
          this._webDriverBiDi = new WebDriverBiDi(this);
          if (this.enabled) {
            logger.debug("WebDriver BiDi enabled");
          }
        }

        if ((activeProtocols & CDP_ACTIVE) === CDP_ACTIVE) {
          this._cdp = new CDP(this);
          if (this.enabled) {
            logger.debug("CDP enabled");
          }
        }

        break;

      case "remote-startup-requested":
        Services.obs.removeObserver(this, topic);

        try {
          let address = Services.io.newURI(`http://localhost:${this._port}`);
          await this.listen(address);
        } catch (e) {
          throw Error(`Unable to start remote agent: ${e}`);
        }

        break;

      case "quit-application":
        Services.obs.removeObserver(this, "quit-application");

        this.close();
        break;
    }
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI([
      "nsICommandLineHandler",
      "nsIObserver",
      "nsIRemoteAgent",
    ]);
  }
}

var RemoteAgent = new RemoteAgentClass();

// This is used by the XPCOM codepath which expects a constructor
var RemoteAgentFactory = function() {
  return RemoteAgent;
};
