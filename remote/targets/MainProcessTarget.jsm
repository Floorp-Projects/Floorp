/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["MainProcessTarget"];

const {Connection} = ChromeUtils.import("chrome://remote/content/Connection.jsm");
const {Session} = ChromeUtils.import("chrome://remote/content/sessions/Session.jsm");
const {WebSocketDebuggerTransport} = ChromeUtils.import("chrome://remote/content/server/WebSocketTransport.jsm");
const {WebSocketServer} = ChromeUtils.import("chrome://remote/content/server/WebSocket.jsm");

/**
 * The main process Target.
 *
 * Matches BrowserDevToolsAgentHost from chromium, and only support a couple of Domains:
 * https://cs.chromium.org/chromium/src/content/browser/devtools/browser_devtools_agent_host.cc?dr=CSs&g=0&l=80-91
 */
class MainProcessTarget {
  /**
   * @param Targets targets
   */
  constructor(targets) {
    this.targets = targets;
    this.sessions = new Map();

    this.type = "main-process";
  }

  get wsDebuggerURL() {
    const RemoteAgent = Cc["@mozilla.org/remote/agent"]
        .getService(Ci.nsISupports).wrappedJSObject;
    const {host, port} = RemoteAgent;
    return `ws://${host}:${port}/devtools/page/${this.id}`;
  }

  toString() {
    return `[object MainProcessTarget]`;
  }

  toJSON() {
    return {
      description: "Main process target",
      devtoolsFrontendUrl: "",
      faviconUrl: "",
      id: "main-process",
      title: "Main process target",
      type: this.type,
      url: "",
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

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI([
      Ci.nsIHttpRequestHandler,
    ]);
  }
}
