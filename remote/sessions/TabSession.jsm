/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TabSession"];

const {Domains} = ChromeUtils.import("chrome://remote/content/domains/Domains.jsm");
const {Session} = ChromeUtils.import("chrome://remote/content/sessions/Session.jsm");

class TabSession extends Session {
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

      const [domainName, methodName] = Domains.splitMethod(method);
      if (typeof domainName == "undefined" || typeof methodName == "undefined") {
        throw new TypeError("'method' field is incorrect and doesn't define a domain " +
                            "name and method separated by a dot.");
      }
      if (this.domains.has(domainName)) {
        await this.execute(id, domainName, methodName, params);
      } else {
        this.executeInChild(id, domainName, methodName, params);
      }
    } catch (e) {
      this.onError(id, e);
    }
  }

  executeInChild(id, domain, method, params) {
    this.mm.sendAsyncMessage("remote:request", {
      browsingContextId: this.browsingContext.id,
      request: {id, domain, method, params},
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
