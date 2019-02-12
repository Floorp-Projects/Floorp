/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Connection"];

const {Log} = ChromeUtils.import("chrome://remote/content/Log.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "log", Log.get);

class Connection {
  constructor(connID, transport, socketListener) {
    this.id = connID;
    this.transport = transport;
    this.socketListener = socketListener;

    this.transport.hooks = this;
    this.onmessage = () => {};
  }

  send(message) {
    log.trace(`<-(connection ${this.id}) ${JSON.stringify(message)}`);
    // TODO(ato): Check return types
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
    // TODO(ato): what if params is falsy?
    const params = data.params || {};

    // TODO(ato): Do protocol validation (Protocol.jsm)

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

  onClosed(status) {}

  toString() {
    return `[object Connection ${this.id}]`;
  }
};
