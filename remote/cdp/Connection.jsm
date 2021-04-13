/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Connection"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "chrome://remote/content/shared/Log.jsm",
  truncate: "chrome://remote/content/shared/Format.jsm",
  UnknownMethodError: "chrome://remote/content/cdp/Error.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

XPCOMUtils.defineLazyServiceGetter(
  this,
  "UUIDGen",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator"
);

class Connection {
  /**
   * @param WebSocketTransport transport
   * @param httpd.js's Connection httpdConnection
   */
  constructor(transport, httpdConnection) {
    this.id = UUIDGen.generateUUID().toString();
    this.transport = transport;
    this.httpdConnection = httpdConnection;

    this.transport.hooks = this;
    this.transport.ready();

    this.defaultSession = null;
    this.sessions = new Map();
  }

  /**
   * Register a new Session to forward the messages to.
   * Session without any `id` attribute will be considered to be the
   * default one, to which messages without `sessionId` attribute are
   * forwarded to. Only one such session can be registered.
   *
   * @param Session session
   */
  registerSession(session) {
    if (!session.id) {
      if (this.defaultSession) {
        throw new Error(
          "Default session is already set on Connection," +
            "can't register another one."
        );
      }
      this.defaultSession = session;
    }
    this.sessions.set(session.id, session);
  }

  send(body) {
    const payload = JSON.stringify(body, null, Log.verbose ? "\t" : null);
    logger.trace(truncate`<-(connection ${this.id}) ${payload}`);
    this.transport.send(JSON.parse(payload));
  }

  /**
   * Send an error back to the client.
   *
   * @param Number id
   *        Id of the packet which lead to an error.
   * @param Error e
   *        Error object with `message` and `stack` attributes.
   * @param Number sessionId (Optional)
   *        Id of the session used to send this packet.
   *        This will be null if that was the default session.
   */
  onError(id, e, sessionId) {
    const error = {
      message: e.message,
      data: e.stack,
    };
    this.send({ id, sessionId, error });
  }

  /**
   * Send the result of a call to a Domain's function.
   *
   * @param Number id
   *        The request id being sent by the client to call the domain's method.
   * @param Object result
   *        A JSON-serializable value which is the actual result.
   * @param Number sessionId
   *        The sessionId from which this packet is emitted.
   *        This will be undefined for the default session.
   */
  onResult(id, result, sessionId) {
    this.sendResult(id, result, sessionId);

    // When a client attaches to a secondary target via
    // `Target.attachToTarget`, and it executes a command via
    // `Target.sendMessageToTarget`, we should emit an event back with the
    // result including the `sessionId` attribute of this secondary target's
    // session. `Target.attachToTarget` creates the secondary session and
    // returns the session ID.
    if (sessionId) {
      this.sendEvent("Target.receivedMessageFromTarget", {
        sessionId,
        // receivedMessageFromTarget is expected to send a raw CDP packet
        // in the `message` property and it to be already serialized to a
        // string
        message: JSON.stringify({
          id,
          result,
        }),
      });
    }
  }

  sendResult(id, result, sessionId) {
    this.send({
      sessionId, // this will be undefined for the default session
      id,
      result: typeof result != "undefined" ? result : {},
    });
  }

  /**
   * Send an event coming from a Domain to the CDP client.
   *
   * @param String method
   *        The event name. This is composed by a domain name,
   *        a dot character followed by the event name.
   *        e.g. `Target.targetCreated`
   * @param Object params
   *        A JSON-serializable value which is the payload
   *        associated with this event.
   * @param Number sessionId
   *        The sessionId from which this packet is emitted.
   *        This will be undefined for the default session.
   */
  onEvent(method, params, sessionId) {
    this.sendEvent(method, params, sessionId);

    // When a client attaches to a secondary target via
    // `Target.attachToTarget`, we should emit an event back with the
    // result including the `sessionId` attribute of this secondary target's
    // session. `Target.attachToTarget` creates the secondary session and
    // returns the session ID.
    if (sessionId) {
      this.sendEvent("Target.receivedMessageFromTarget", {
        sessionId,
        message: JSON.stringify({
          method,
          params,
        }),
      });
    }
  }

  sendEvent(method, params, sessionId) {
    this.send({
      sessionId, // this will be undefined for the default session
      method,
      params,
    });
  }

  // transport hooks

  /**
   * Receive a packet from the WebSocket layer.
   * This packet is sent by a CDP client and is meant to execute
   * a particular function on a given Domain.
   *
   * @param Object packet
   *        JSON-serializable object sent by the client
   */
  async onPacket(packet) {
    logger.trace(`(connection ${this.id})-> ${JSON.stringify(packet)}`);

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
      const { domain, command } = Connection.splitMethod(method);

      // If a `sessionId` field is passed, retrieve the session to which we
      // should forward this packet. Otherwise send it to the default session.
      let session;
      if (!sessionId) {
        if (!this.defaultSession) {
          throw new Error(`Connection is missing a default Session.`);
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
        throw new UnknownMethodError(command);
      }

      // Finally, instruct the targeted session to execute the command
      const result = await session.execute(id, domain, command, params);
      this.onResult(id, result, sessionId);
    } catch (e) {
      this.onError(packet.id, e, packet.sessionId);
    }
  }

  /**
   * Interpret a given CDP packet for a given Session.
   *
   * @param String sessionId
   *               ID of the session for which we should execute a command.
   * @param String message
   *               JSON payload of the CDP packet stringified to a string.
   *               The CDP packet is about executing a Domain's function.
   */
  sendMessageToTarget(sessionId, message) {
    const session = this.sessions.get(sessionId);
    if (!session) {
      throw new Error(`Session '${sessionId}' doesn't exists.`);
    }
    // `message` is received from `Target.sendMessageToTarget` where the
    // message attribute is a stringify JSON payload which represent a CDP
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
   * Instruct the connection to close.
   * This will ask the transport to shutdown the WebSocket connection
   * and destroy all active sessions.
   */
  close() {
    this.transport.close();

    // In addition to the WebSocket transport, we also have to close the Connection
    // used internaly within httpd.js. Otherwise the server doesn't shut down correctly
    // and keep these Connection instances alive.
    this.httpdConnection.close();
  }

  /**
   * This is called by the `transport` when the connection is closed.
   * Cleanup all the registered sessions.
   */
  onClosed(status) {
    for (const session of this.sessions.values()) {
      session.destructor();
    }
    this.sessions.clear();
  }

  /**
   * Splits a method, e.g. "Browser.getVersion",
   * into domain ("Browser") and command ("getVersion") components.
   */
  static splitMethod(s) {
    const ss = s.split(".");
    if (ss.length != 2 || ss[0].length == 0 || ss[1].length == 0) {
      throw new TypeError(`Invalid method format: "${s}"`);
    }
    return {
      domain: ss[0],
      command: ss[1],
    };
  }

  toString() {
    return `[object Connection ${this.id}]`;
  }
}
