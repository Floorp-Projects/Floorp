/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is an XPCOM service-ified copy of ../devtools/shared/transport/websocket-transport.js.

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  EventEmitter: "resource://gre/modules/EventEmitter.sys.mjs",
});

export function WebSocketTransport(socket) {
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

  /**
   * Force closing the active connection and WebSocket.
   */
  close() {
    if (!this.socket) {
      return;
    }
    this.emit("close");

    if (this.hooks) {
      this.hooks.onConnectionClose();
    }

    this.active = false;

    this.socket.removeEventListener("message", this);

    // Remove the listener that is used when the connection
    // is closed because we already emitted the 'close' event.
    // Instead listen for the closing of the WebSocket.
    this.socket.removeEventListener("close", this);
    this.socket.addEventListener("close", this.onSocketClose.bind(this));

    // Close socket with code `1000` for a normal closure.
    this.socket.close(1000);
  },

  /**
   * Callback for socket on close event,
   * it is used in case websocket was closed by the client.
   */
  onClose() {
    if (!this.socket) {
      return;
    }
    this.emit("close");

    if (this.hooks) {
      this.hooks.onConnectionClose();
      this.hooks.onSocketClose();
      this.hooks = null;
    }

    this.active = false;

    this.socket.removeEventListener("message", this);
    this.socket.removeEventListener("close", this);
    this.socket = null;
  },

  /**
   * Callback which is called when we can be sure that websocket is closed.
   */
  onSocketClose() {
    this.socket = null;

    if (this.hooks) {
      this.hooks.onSocketClose();
      this.hooks = null;
    }
  },

  handleEvent(event) {
    switch (event.type) {
      case "message":
        this.onMessage(event);
        break;
      case "close":
        this.onClose();
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
