/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["WebDriverNewSessionHandler"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  WebDriverBiDiConnection:
    "chrome://remote/content/webdriver-bidi/WebDriverBiDiConnection.jsm",
  WebSocketHandshake: "chrome://remote/content/server/WebSocketHandshake.jsm",
});

/**
 * httpd.js JSON handler for direct BiDi connections.
 */
class WebDriverNewSessionHandler {
  /**
   * Construct a new JSON handler.
   *
   * @param {WebDriverBiDi} webDriverBiDi
   *     Reference to the WebDriver BiDi protocol implementation.
   */
  constructor(webDriverBiDi) {
    this.webDriverBiDi = webDriverBiDi;
  }

  // nsIHttpRequestHandler

  /**
   * Handle new direct WebSocket connection requests.
   *
   * WebSocket clients not using the WebDriver BiDi opt-in mechanism via the
   * WebDriver HTTP implementation will attempt to directly connect at
   * `/session`.  Hereby a WebSocket upgrade will automatically be performed.
   *
   * @param {Request} request
   *     HTTP request (httpd.js)
   * @param {Response} response
   *     Response to an HTTP request (httpd.js)
   */
  async handle(request, response) {
    const webSocket = await WebSocketHandshake.upgrade(request, response);
    const conn = new WebDriverBiDiConnection(webSocket, response._connection);

    this.webDriverBiDi.addSessionlessConnection(conn);
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI(["nsIHttpRequestHandler"]);
  }
}
