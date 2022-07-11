/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is an XPCOM service-ified copy of ../devtools/shared/transport/websocket-transport.js.

"use strict";

var EXPORTED_SYMBOLS = ["WebSocketTransport"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",
});

function WebSocketTransport(socket) {
  lazy.EventEmitter.decorate(this);

  this.active = false;
  this.hooks = null;
  this.socket = socket;
}

WebSocketTransport.prototype = {
  ready() {
    if (this.active) {
      return;
    }

    this.socket.addEventListener("message", this);
    this.socket.addEventListener("close", this);

    this.active = true;
  },

  send(object) {
    this.emit("send", object);
    if (this.socket) {
      this.socket.send(JSON.stringify(object));
    }
  },

  startBulkSend() {
    throw new Error("Bulk send is not supported by WebSocket transport");
  },

  close() {
    if (!this.socket) {
      return;
    }
    this.emit("close");
    this.active = false;

    this.socket.removeEventListener("message", this);
    this.socket.removeEventListener("close", this);
    this.socket.close();
    this.socket = null;

    if (this.hooks) {
      this.hooks.onClosed();
      this.hooks = null;
    }
  },

  handleEvent(event) {
    switch (event.type) {
      case "message":
        this.onMessage(event);
        break;
      case "close":
        this.close();
        break;
    }
  },

  onMessage({ data }) {
    if (typeof data !== "string") {
      throw new Error(
        "Binary messages are not supported by WebSocket transport"
      );
    }

    const object = JSON.parse(data);
    this.emit("packet", object);
    if (this.hooks) {
      this.hooks.onPacket(object);
    }
  },
};
