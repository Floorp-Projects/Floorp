/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TabSession"];

const {Domains} = ChromeUtils.import("chrome://remote/content/domains/Domains.jsm");
const {Session} = ChromeUtils.import("chrome://remote/content/sessions/Session.jsm");

/**
 * A session to communicate with a given tab
 */
class TabSession extends Session {
  /**
   * @param Connection connection
   *        The connection used to communicate with the server.
   * @param TabTarget target
   *        The tab target to which this session communicates with.
   * @param Number id (optional)
   *        If this session isn't the default one used for the HTTP endpoint we
   *        connected to, the session requires an id to distinguish it from the default
   *        one. This id is used to filter our request, responses and events between
   *        all active sessions.
   * @param Session parentSession (optional)
   *        If this isn't the default session, optional hand over a session to which
   *        we will forward all request responses and events via
   *        `Target.receivedMessageFromTarget` events.
   */
  constructor(connection, target, id, parentSession) {
    super(connection, target, id);
    this.parentSession = parentSession;

    this.mm.addMessageListener("remote:event", this);
    this.mm.addMessageListener("remote:result", this);
    this.mm.addMessageListener("remote:error", this);

    this.mm.loadFrameScript("chrome://remote/content/sessions/frame-script.js", false);
  }

  destructor() {
    super.destructor();

    this.mm.sendAsyncMessage("remote:destroy", {
      browsingContextId: this.browsingContext.id,
    });

    this.mm.removeMessageListener("remote:event", this);
    this.mm.removeMessageListener("remote:result", this);
    this.mm.removeMessageListener("remote:error", this);
  }

  async onMessage({id, method, params}) {
    try {
      if (typeof id == "undefined") {
        throw new TypeError("Message missing 'id' field");
      }
      if (typeof method == "undefined") {
        throw new TypeError("Message missing 'method' field");
      }

      const {domain, command} = Domains.splitMethod(method);
      if (this.domains.has(domain)) {
        await this.execute(id, domain, command, params);
      } else {
        this.executeInChild(id, domain, command, params);
      }
    } catch (e) {
      this.onError(id, e);
    }
  }

  executeInChild(id, domain, command, params) {
    this.mm.sendAsyncMessage("remote:request", {
      browsingContextId: this.browsingContext.id,
      request: {id, domain, command, params},
    });
  }

  onResult(id, result) {
    super.onResult(id, result);

    // When `Target.sendMessageToTarget` is used, we should forward the responses
    // to the parent session from which we called `sendMessageToTarget`.
    if (this.parentSession) {
      this.parentSession.onEvent("Target.receivedMessageFromTarget", {
        sessionId: this.id,
        message: JSON.stringify({ id, result }),
      });
    }
  }

  onEvent(eventName, params) {
    super.onEvent(eventName, params);

    // When `Target.sendMessageToTarget` is used, we should forward the responses
    // to the parent session from which we called `sendMessageToTarget`.
    if (this.parentSession) {
      this.parentSession.onEvent("Target.receivedMessageFromTarget", {
        sessionId: this.id,
        message: JSON.stringify({
          method: eventName,
          params,
        }),
      });
    }
  }

  get mm() {
    return this.target.mm;
  }

  get browsingContext() {
    return this.target.browsingContext;
  }

  // nsIMessageListener

  receiveMessage({name, data}) {
    const {id, result, event, error} = data;

    switch (name) {
    case "remote:result":
      this.onResult(id, result);
      break;

    case "remote:event":
      this.onEvent(event.eventName, event.params);
      break;

    case "remote:error":
      this.onError(id, error);
      break;
    }
  }
}
