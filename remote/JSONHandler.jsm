/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["JSONHandler"];

const { HTTP_404, HTTP_505 } = ChromeUtils.import(
  "chrome://remote/content/server/HTTPD.jsm"
);
const { Log } = ChromeUtils.import("chrome://remote/content/Log.jsm");
const { Protocol } = ChromeUtils.import("chrome://remote/content/Protocol.jsm");
const { RemoteAgentError } = ChromeUtils.import(
  "chrome://remote/content/Error.jsm"
);

class JSONHandler {
  constructor(agent) {
    this.agent = agent;
    this.routes = {
      "/json/version": this.getVersion.bind(this),
      "/json/protocol": this.getProtocol.bind(this),
      "/json/list": this.getTargetList.bind(this),
    };
  }

  getVersion() {
    const mainProcessTarget = this.agent.targets.getMainProcessTarget();
    return {
      Browser: "Firefox",
      "Protocol-Version": "1.0",
      "User-Agent": "Mozilla",
      "V8-Version": "1.0",
      "WebKit-Version": "1.0",
      webSocketDebuggerUrl: mainProcessTarget.toJSON().webSocketDebuggerUrl,
    };
  }

  getProtocol() {
    return Protocol.Description;
  }

  getTargetList() {
    return [...this.agent.targets];
  }

  // nsIHttpRequestHandler

  handle(request, response) {
    if (request.method != "GET") {
      throw HTTP_404;
    }

    if (!(request.path in this.routes)) {
      throw HTTP_404;
    }

    try {
      const body = this.routes[request.path]();
      const payload = JSON.stringify(
        body,
        sanitise,
        Log.verbose ? "\t" : undefined
      );

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json");
      response.write(payload);
    } catch (e) {
      new RemoteAgentError(e).notify();
      throw HTTP_505;
    }
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI([Ci.nsIHttpRequestHandler]);
  }
}

// Filters out null and empty strings
function sanitise(key, value) {
  if (value === null || (typeof value == "string" && value.length == 0)) {
    return undefined;
  }
  return value;
}
