/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "TargetListHandler",
  "ProtocolHandler",
];

const {Log} = ChromeUtils.import("chrome://remote/content/Log.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Protocol} = ChromeUtils.import("chrome://remote/content/Protocol.jsm");

XPCOMUtils.defineLazyGetter(this, "log", Log.get);

class Handler {
  register(server) {
    server.registerPathHandler(this.path, this.rawHandle);
  }

  rawHandle(request, response) {
    log.trace(`(${request._scheme})-> ${request._method} ${request._path}`);

    try {
      this.handle(request, response);
    } catch (e) {
      log.warn(e);
    }
  }
}

class JSONHandler extends Handler {
  register(server) {
    server.registerPathHandler(this.path, (req, resp) => {
      this.rawHandle(req, new JSONWriter(resp));
    });
  }
}

this.TargetListHandler = class extends JSONHandler {
  constructor(targets) {
    super();
    this.targets = targets;
  }

  get path() {
    return "/json/list";
  }

  handle(request, response) {
    response.write([...this.targets]);
  }
};

this.ProtocolHandler = class extends JSONHandler {
  get path() {
    return "/json/protocol";
  }

  handle(request, response) {
    response.write(Protocol.Description);
  }
};

/**
 * Wraps an httpd.js response and serialises anything passed to
 * write() to JSON.
 */
class JSONWriter {
  constructor(response) {
    this._response = response;
  }

  /** Filters out null and empty strings. */
  _replacer(key, value) {
    if (value === null || (typeof value == "string" && value.length == 0)) {
      return undefined;
    }
    return value;
  }

  write(data) {
    try {
      const json = JSON.stringify(data, this._replacer, "\t");
      this._response.write(json);
    } catch (e) {
      log.error(`Unable to serialise JSON: ${e.message}`, e);
      this._response.write("");
    }
  }
}
