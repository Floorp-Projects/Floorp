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
  constructor(connection, target, id) {
    this.connection = connection;
    this.target = target;
    this.id = id;

    this.destructor = this.destructor.bind(this);

    this.connection.registerSession(this);
    this.connection.transport.on("close", this.destructor);

    this.domains = new Domains(this, ParentProcessDomains);
  }

  destructor() {
    this.domains.clear();
  }

  async onMessage({id, method, params}) {
    try {
      if (typeof id == "undefined") {
        throw new TypeError("Message missing 'id' field");
      }
      if (typeof method == "undefined") {
        throw new TypeError("Message missing 'method' field");
      }

      const [domainName, methodName] = Domains.splitMethod(method);
      await this.execute(id, domainName, methodName, params);
    } catch (e) {
      this.onError(id, e);
    }
  }

  async execute(id, domain, method, params) {
    const inst = this.domains.get(domain);
    const result = await inst[method](params);
    this.onResult(id, result);
  }

  onResult(id, result) {
    this.connection.send({
      id,
      sessionId: this.id,
      result,
    });
  }

  onError(id, error) {
    this.connection.send({
      id,
      sessionId: this.id,
      error: {
        message: formatError(error, {stack: true}),
      },
    });
  }

  // Domain event listener

  onEvent(eventName, params) {
    this.connection.send({
      sessionId: this.id,
      method: eventName,
      params,
    });
  }

  toString() {
    return `[object ${this.constructor.name} ${this.connection.id}]`;
  }
}
