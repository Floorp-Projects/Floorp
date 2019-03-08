/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Target"];

const {Connection} = ChromeUtils.import("chrome://remote/content/Connection.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {Session} = ChromeUtils.import("chrome://remote/content/Session.jsm");
const {WebSocketDebuggerTransport} = ChromeUtils.import("chrome://remote/content/server/WebSocketTransport.jsm");
const {WebSocketServer} = ChromeUtils.import("chrome://remote/content/server/WebSocket.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "Favicons",
    "@mozilla.org/browser/favicon-service;1", "nsIFaviconService");

/**
 * A debugging target.
 *
 * Targets can be a document (page), an OOP frame, a background
 * document, or a worker.  They can all run in dedicated process or frame.
 */
class Target {
  constructor(browser) {
    this.browser = browser;
    this.sessions = new Map();
  }

  connect() {
    Services.obs.addObserver(this, "message-manager-disconnect");
  }

  disconnect() {
    Services.obs.removeObserver(this, "message-manager-disconnect");
    // TODO(ato): Disconnect existing client sockets
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
    const RemoteAgent = Cc["@mozilla.org/remote/agent"]
        .getService(Ci.nsISupports).wrappedJSObject;
    const {host, port} = RemoteAgent;
    return `ws://${host}:${port}/devtools/page/${this.id}`;
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

  // nsIHttpRequestHandler

  async handle(request, response) {
    const so = await WebSocketServer.upgrade(request, response);
    const transport = new WebSocketDebuggerTransport(so);
    const conn = new Connection(transport);
    this.sessions.set(conn, new Session(conn, this));
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
