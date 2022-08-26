/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["CDPConnection", "splitMethod"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const { WebSocketConnection } = ChromeUtils.import(
  "chrome://remote/content/shared/WebSocketConnection.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  Log: "chrome://remote/content/shared/Log.jsm",
  truncate: "chrome://remote/content/shared/Format.jsm",
  UnknownMethodError: "chrome://remote/content/cdp/Error.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.CDP)
);

class CDPConnection extends WebSocketConnection {
  /**
   * @param {WebSocket} webSocket
   *     The WebSocket server connection to wrap.
   * @param {Connection} httpdConnection
   *     Reference to the httpd.js's connection needed for clean-up.
   */
  constructor(webSocket, httpdConnection) {
    super(webSocket, httpdConnection);

    this.sessions = new Map();
    this.defaultSession = null;
  }

  /**
   * Register a new Session to forward the messages to.
   *
   * A session without any `id` attribute will be considered to be the
   * default one, to which messages without `sessionId` attribute are
   * forwarded to. Only one such session can be registered.
   *
   * @param {Session} session
   *     The session to register.
   */
  registerSession(session) {
    // CDP is not compatible with Fission by default, check the appropriate
    // preferences are set to ensure compatibility.
    if (
      Services.prefs.getIntPref("fission.webContentIsolationStrategy") !== 0 ||
      Services.prefs.getBoolPref("fission.bfcacheInParent")
    ) {
      lazy.logger.error(
        `Invalid browser preferences for CDP. Set "fission.webContentIsolationStrategy"` +
          `to 0 and "fission.bfcacheInParent" to false before Firefox starts.`
      );
    }

    if (!session.id) {
      if (this.defaultSession) {
        throw new Error(
          "Default session is already set on Connection, " +
            "can't register another one."
        );
      }
      this.defaultSession = session;
    }

    this.sessions.set(session.id, session);
  }

  /**
   * Send the JSON-serializable object to the CDP client.
   *
   * @param {Object} data
   *     The object to be sent.
   */
  send(data) {
    const payload = JSON.stringify(data, null, lazy.Log.verbose ? "\t" : null);
    lazy.logger.trace(
      lazy.truncate`${this.constructor.name} ${this.id} <- ${payload}`
    );

    super.send(data);
  }

  /**
   * Send an error back to the CDP client.
   *
   * @param {Number} id
   *     Id of the packet which lead to an error.
   * @param {Error} err
   *     Error object with `message` and `stack` attributes.
   * @param {String=} sessionId
   *     Id of the session used to send this packet. Falls back to the
   *     default session if not specified.
   */
  sendError(id, err, sessionId) {
    const error = {
      message: err.message,
      data: err.stack,
    };

    this.send({ id, error, sessionId });
  }

  /**
   * Send an event coming from a Domain to the CDP client.
   *
   * @param {String} method
   *     The event name. This is composed by a domain name, a dot character
   *     followed by the event name, e.g. `Target.targetCreated`.
   * @param {Object} params
   *     A JSON-serializable object, which is the payload of this event.
   * @param {Sting=} sessionId
   *     The sessionId from which this packet is emitted. Falls back to the
   *     default session if not specified.
   */
  sendEvent(method, params, sessionId) {
    this.send({ method, params, sessionId });

    // When a client attaches to a secondary target via
    // `Target.attachToTarget`, we should emit an event back with the
    // result including the `sessionId` attribute of this secondary target's
    // session. `Target.attachToTarget` creates the secondary session and
    // returns the session ID.
    if (sessionId) {
      // receivedMessageFromTarget is expected to send a raw CDP packet
      // in the `message` property and it to be already serialized to a
      // string
      this.send({
        method: "Target.receivedMessageFromTarget",
        params: { sessionId, message: JSON.stringify({ method, params }) },
      });
    }
  }

  /**
   * Interpret a given CDP packet for a given Session.
   *
   * @param {String} sessionId
   *     ID of the session for which we should execute a command.
   * @param {String} message
   *     The stringified JSON payload of the CDP packet, which is about
   *     executing a Domain's function.
   */
  sendMessageToTarget(sessionId, message) {
    const session = this.sessions.get(sessionId);
    if (!session) {
      throw new Error(`Session '${sessionId}' doesn't exist.`);
    }
    // `message` is received from `Target.sendMessageToTarget` where the
    // message attribute is a stringified JSON payload which represent a CDP
    // packet.
    const packet = JSON.parse(message);

    // The CDP packet sent by the client shouldn't have a sessionId attribute
    // as it is passed as another argument of `Target.sendMessageToTarget`.
    // Set it here in order to reuse the codepath of flatten session, where
    // the client sends CDP packets with a `sessionId` attribute instead
    // of going through the old and probably deprecated
    // `Target.sendMessageToTarget` API.
    packet.sessionId = sessionId;
    this.onPacket(packet);
  }

  /**
   * Send the result of a call to a Domain's function back to the CDP client.
   *
   * @param {Number} id
   *     The request id being sent by the client to call the domain's method.
   * @param {Object} result
   *     A JSON-serializable object, which is the actual result.
   * @param {String=} sessionId
   *     The sessionId from which this packet is emitted. Falls back to the
   *     default session if not specified.
   */
  sendResult(id, result, sessionId) {
    result = typeof result != "undefined" ? result : {};
    this.send({ id, result, sessionId });

    // When a client attaches to a secondary target via
    // `Target.attachToTarget`, and it executes a command via
    // `Target.sendMessageToTarget`, we should emit an event back with the
    // result including the `sessionId` attribute of this secondary target's
    // session. `Target.attachToTarget` creates the secondary session and
    // returns the session ID.
    if (sessionId) {
      // receivedMessageFromTarget is expected to send a raw CDP packet
      // in the `message` property and it to be already serialized to a
      // string
      this.send({
        method: "Target.receivedMessageFromTarget",
        params: { sessionId, message: JSON.stringify({ id, result }) },
      });
    }
  }

  // Transport hooks

  /**
   * Called by the `transport` when the connection is closed.
   */
  onClosed() {
    // Cleanup all the registered sessions.
    for (const session of this.sessions.values()) {
      session.destructor();
    }
    this.sessions.clear();

    super.onClosed();
  }

  /**
   * Receive a packet from the WebSocket layer.
   *
   * This packet is sent by a CDP client and is meant to execute
   * a particular function on a given Domain.
   *
   * @param {Object} packet
   *        JSON-serializable object sent by the client.
   */
  async onPacket(packet) {
    const payload = JSON.stringify(
      packet,
      null,
      lazy.Log.verbose ? "\t" : null
    );
    lazy.logger.trace(
      lazy.truncate`${this.constructor.name} ${this.id} -> ${payload}`
    );

    try {
      const { id, method, params, sessionId } = packet;

      // First check for mandatory field in the packets
      if (typeof id == "undefined") {
        throw new TypeError("Message missing 'id' field");
      }
      if (typeof method == "undefined") {
        throw new TypeError("Message missing 'method' field");
      }

      // Extract the domain name and the method name out of `method` attribute
      const { domain, command } = splitMethod(method);

      // If a `sessionId` field is passed, retrieve the session to which we
      // should forward this packet. Otherwise send it to the default session.
      let session;
      if (!sessionId) {
        if (!this.defaultSession) {
          throw new Error("Connection is missing a default Session.");
        }
        session = this.defaultSession;
      } else {
        session = this.sessions.get(sessionId);
        if (!session) {
          throw new Error(`Session '${sessionId}' doesn't exists.`);
        }
      }

      // Bug 1600317 - Workaround to deny internal methods to be called
      if (command.startsWith("_")) {
        throw new lazy.UnknownMethodError(command);
      }

      // Finally, instruct the targeted session to execute the command
      const result = await session.execute(id, domain, command, params);
      this.sendResult(id, result, sessionId);
    } catch (e) {
      this.sendError(packet.id, e, packet.sessionId);
    }
  }
}

/**
 * Splits a CDP method into domain and command components.
 *
 * @param {String} method
 *     Name of the method to split, e.g. "Browser.getVersion".
 *
 * @returns {Object<String, String>}
 *     Object with the domain ("Browser") and command ("getVersion")
 *     as properties.
 */
function splitMethod(method) {
  const parts = method.split(".");

  if (parts.length != 2 || !parts[0].length || !parts[1].length) {
    throw new TypeError(`Invalid method format: '${method}'`);
  }

  return {
    domain: parts[0],
    command: parts[1],
  };
}
