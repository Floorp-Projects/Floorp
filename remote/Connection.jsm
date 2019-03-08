/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Connection"];

const {Log} = ChromeUtils.import("chrome://remote/content/Log.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "log", Log.get);
XPCOMUtils.defineLazyServiceGetter(this, "UUIDGen", "@mozilla.org/uuid-generator;1", "nsIUUIDGenerator");

class Connection {
  constructor(transport) {
    this.id = UUIDGen.generateUUID().toString();
    this.transport = transport;

    this.transport.hooks = this;
    this.onmessage = () => {};

    this.transport.ready();
  }

  send(message) {
    log.trace(`<-(connection ${this.id}) ${JSON.stringify(message)}`);
    this.transport.send(message);
  }

  error(id, e) {
    const error = {
      message: e.message,
      data: e.stack,
    };
    this.send({id, error});
  }

  deserialize(data) {
    const id = data.id;
    const method = data.method;
    const params = data.params || {};
    return {id, method, params};
  }

  // transport hooks

  onPacket(packet) {
    log.trace(`(connection ${this.id})-> ${JSON.stringify(packet)}`);

    let message = {id: null};
    try {
      message = this.deserialize(packet);
      this.onmessage.call(null, message);
    } catch (e) {
      log.warn(e);
      this.error(message.id, e);
    }
  }

  close() {
    this.transport.close();
  }

  onClosed(status) {}

  toString() {
    return `[object Connection ${this.id}]`;
  }
}
