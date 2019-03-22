/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TabTarget"];

const {Target} = ChromeUtils.import("chrome://remote/content/targets/Target.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {TabSession} = ChromeUtils.import("chrome://remote/content/sessions/TabSession.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {RemoteAgent} = ChromeUtils.import("chrome://remote/content/RemoteAgent.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "Favicons",
    "@mozilla.org/browser/favicon-service;1", "nsIFaviconService");

/**
 * Target for a local tab or a remoted frame.
 */
class TabTarget extends Target {
  /**
   * @param Targets targets
   * @param BrowserElement browser
   */
  constructor(targets, browser) {
    super(targets, TabSession);

    this.browser = browser;

    // Define the HTTP path to query this target
    this.path = `/devtools/page/${this.id}`;
  }

  connect() {
    Services.obs.addObserver(this, "message-manager-disconnect");
  }

  disconnect() {
    Services.obs.removeObserver(this, "message-manager-disconnect");
    super.disconnect();
  }

  get id() {
    return this.browsingContext.id;
  }

  get browsingContext() {
    return this.browser.browsingContext;
  }

  get mm() {
    return this.browser.messageManager;
  }

  get window() {
    return this.browser.ownerGlobal;
  }

  /**
   * Determines if the content browser remains attached
   * to its parent chrome window.
   *
   * We determine this by checking if the <browser> element
   * is still attached to the DOM.
   *
   * @return {boolean}
   *     True if target's browser is still attached,
   *     false if it has been disconnected.
   */
  get closed() {
    return !this.browser || !this.browser.isConnected;
  }

  get description() {
    return "";
  }

  get frontendURL() {
    return null;
  }

  /** @return {Promise.<String=>} */
  get faviconUrl() {
    return new Promise((resolve, reject) => {
      Favicons.getFaviconURLForPage(this.browser.currentURI, url => {
        if (url) {
          resolve(url.spec);
        } else {
          resolve(null);
        }
      });
    });
  }

  get type() {
    return "page";
  }

  get url() {
    return this.browser.currentURI.spec;
  }

  get wsDebuggerURL() {
    const {host, port} = RemoteAgent;
    return `ws://${host}:${port}${this.path}`;
  }

  toString() {
    return `[object Target ${this.id}]`;
  }

  toJSON() {
    return {
      description: this.description,
      devtoolsFrontendUrl: this.frontendURL,
      // TODO(ato): toJSON cannot be marked async )-:
      faviconUrl: "",
      id: this.id,
      title: this.title,
      type: this.type,
      url: this.url,
      webSocketDebuggerUrl: this.wsDebuggerURL,
    };
  }

  // nsIObserver

  observe(subject, topic, data) {
    if (subject === this.mm && subject == "message-manager-disconnect") {
      // disconnect debugging target if <browser> is disconnected,
      // otherwise this is just a host process change
      if (this.closed) {
        this.disconnect();
      }
    }
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI([
      Ci.nsIHttpRequestHandler,
      Ci.nsIObserver,
    ]);
  }
}
