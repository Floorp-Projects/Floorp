/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Session"];

const {Domain} = ChromeUtils.import("chrome://remote/content/Domain.jsm");
const {formatError} = ChromeUtils.import("chrome://remote/content/Error.jsm");

class Session {
  constructor(connection, target) {
    this.connection = connection;
    this.target = target;

    this.browsingContext = target.browser.browsingContext;
    this.messageManager = target.browser.messageManager;
    this.messageManager.loadFrameScript("chrome://remote/content/frame-script.js", false);
    this.messageManager.addMessageListener("remote-protocol:event", this);
    this.messageManager.addMessageListener("remote-protocol:result", this);
    this.messageManager.addMessageListener("remote-protocol:error", this);

    this.connection.onmessage = this.dispatch.bind(this);
  }

  destructor() {
    this.connection.onmessage = null;

    this.messageManager.sendAsyncMessage("remote-protocol:destroy", {
      browsingContextId: this.browsingContext.id,
    });
    this.messageManager.removeMessageListener("remote-protocol:event", this);
    this.messageManager.removeMessageListener("remote-protocol:result", this);
    this.messageManager.removeMessageListener("remote-protocol:error", this);
  }

  async dispatch({id, method, params}) {
    try {
      if (typeof id == "undefined") {
        throw new TypeError("Message missing 'id' field");
      }
      if (typeof method == "undefined") {
        throw new TypeError("Message missing 'method' field");
      }

      const [domainName, methodName] = split(method, ".", 1);
      const domain = Domain[domainName];
      if (!domain) {
        throw new TypeError("No such domain: " + domainName);
      }

      this.messageManager.sendAsyncMessage("remote-protocol:request", {
        browsingContextId: this.browsingContext.id,
        request: {id, domainName, methodName, params},
      });
    } catch (e) {
      const error = formatError(e, {stack: true});
      this.connection.send({id, error});
    }
  }

  receiveMessage({name, data}) {
    const {id, result, event, error} = data;

    switch (name) {
    case "remote-protocol:result":
      this.connection.send({id, result});
      break;

    case "remote-protocol:event":
      this.connection.send(event);
      break;

    case "remote-protocol:error":
      this.connection.send({id, error: formatError(error, {stack: true})});
      break;
    }
  }
}

/**
 * Split s by sep, returning list of substrings.
 * If max is given, at most max splits are done.
 * If max is 0, there is no limit on the number of splits.
 */
function split(s, sep, max = 0) {
  if (typeof s != "string" ||
      typeof sep != "string" ||
      typeof max != "number") {
    throw new TypeError();
  }
  if (!Number.isInteger(max) || max < 0) {
    throw new RangeError();
  }

  const rv = [];
  let i = 0;

  while (rv.length < max) {
    const si = s.indexOf(sep, i);
    if (!si) {
      break;
    }

    rv.push(s.substring(i, si));
    i = si + sep.length;
  }

  rv.push(s.substring(i));
  return rv;
}
