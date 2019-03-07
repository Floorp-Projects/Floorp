/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Session"];

const {ParentProcessDomains} = ChromeUtils.import("chrome://remote/content/domains/ParentProcessDomains.jsm");
const {Domains} = ChromeUtils.import("chrome://remote/content/domains/Domains.jsm");
const {formatError} = ChromeUtils.import("chrome://remote/content/Error.jsm");

/**
 * A session represents exactly one client WebSocket connection.
 *
 * Every new WebSocket connections is associated with one session that
 * deals with despatching incoming command requests to the right
 * target, sending back responses, and propagating events originating
 * from domains.
 */
class Session {
  constructor(connection, target) {
    this.connection = connection;
    this.target = target;

    this.connection.onmessage = this.dispatch.bind(this);

    this.domains = new Domains(this, ParentProcessDomains);
    this.mm.addMessageListener("remote:event", this);
    this.mm.addMessageListener("remote:result", this);
    this.mm.addMessageListener("remote:error", this);

    this.mm.loadFrameScript("chrome://remote/content/frame-script.js", false);
  }

  destructor() {
    this.domains.clear();
    this.connection.onmessage = null;

    this.mm.sendAsyncMessage("remote:destroy", {
      browsingContextId: this.browsingContext.id,
    });

    this.mm.removeMessageListener("remote:event", this);
    this.mm.removeMessageListener("remote:result", this);
    this.mm.removeMessageListener("remote:error", this);
  }

  async dispatch({id, method, params}) {
    try {
      if (typeof id == "undefined") {
        throw new TypeError("Message missing 'id' field");
      }
      if (typeof method == "undefined") {
        throw new TypeError("Message missing 'method' field");
      }

      const [domainName, methodName] = Domains.splitMethod(method);
      if (this.domains.domainSupportsMethod(domainName, methodName)) {
        await this.executeInParent(id, domainName, methodName, params);
      } else {
        this.executeInChild(id, domainName, methodName, params);
      }
    } catch (e) {
      const error = formatError(e, {stack: true});
      this.connection.send({id, error: { message: error }});
    }
  }

  async executeInParent(id, domain, method, params) {
    const inst = this.domains.get(domain);
    const result = await inst[method](params);
    this.connection.send({id, result});
  }

  executeInChild(id, domain, method, params) {
    this.mm.sendAsyncMessage("remote:request", {
      browsingContextId: this.browsingContext.id,
      request: {id, domain, method, params},
    });
  }

  get mm() {
    return this.target.mm;
  }

  get browsingContext() {
    return this.target.browsingContext;
  }

  // Domain event listener

  onEvent(eventName, params) {
    this.connection.send({
      method: eventName,
      params,
    });
  }

  // nsIMessageListener

  receiveMessage({name, data}) {
    const {id, result, event, error} = data;

    switch (name) {
    case "remote:result":
      this.connection.send({id, result});
      break;

    case "remote:event":
      this.connection.send(event);
      break;

    case "remote:error":
      this.connection.send({id, error: { message: formatError(error, {stack: true}) }});
      break;
    }
  }

  toString() {
    return `[object Session ${this.connection.id}]`;
  }
}
