/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["JSONHandler"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  HTTP_404: "chrome://remote/content/server/HTTPD.jsm",
  HTTP_505: "chrome://remote/content/server/HTTPD.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  Protocol: "chrome://remote/content/cdp/Protocol.jsm",
  RemoteAgentError: "chrome://remote/content/cdp/Error.jsm",
});

class JSONHandler {
  constructor(cdp) {
    this.cdp = cdp;
    this.routes = {
      "/json/version": this.getVersion.bind(this),
      "/json/protocol": this.getProtocol.bind(this),
      "/json/list": this.getTargetList.bind(this),
    };
  }

  getVersion() {
    const mainProcessTarget = this.cdp.targetList.getMainProcessTarget();

    const { userAgent } = Cc[
      "@mozilla.org/network/protocol;1?name=http"
    ].getService(Ci.nsIHttpProtocolHandler);

    return {
      Browser: `${Services.appinfo.name}/${Services.appinfo.version}`,
      "Protocol-Version": "1.0",
      "User-Agent": userAgent,
      "V8-Version": "1.0",
      "WebKit-Version": "1.0",
      webSocketDebuggerUrl: mainProcessTarget.toJSON().webSocketDebuggerUrl,
    };
  }

  getProtocol() {
    return lazy.Protocol.Description;
  }

  getTargetList() {
    return [...this.cdp.targetList];
  }

  // nsIHttpRequestHandler

  handle(request, response) {
    if (request.method != "GET") {
      throw lazy.HTTP_404;
    }

    if (!(request.path in this.routes)) {
      throw lazy.HTTP_404;
    }

    try {
      const body = this.routes[request.path]();
      const payload = JSON.stringify(
        body,
        null,
        lazy.Log.verbose ? "\t" : null
      );

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json");
      response.write(payload);
    } catch (e) {
      new lazy.RemoteAgentError(e).notify();
      throw lazy.HTTP_505;
    }
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI(["nsIHttpRequestHandler"]);
  }
}
