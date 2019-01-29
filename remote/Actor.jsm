/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "MessageChannelActorChild",
  "RemoteAgentActorChild",
];

const {ActorChild} = ChromeUtils.import("resource://gre/modules/ActorChild.jsm");
const {Log} = ChromeUtils.import("chrome://remote/content/Log.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "log", Log.get);

// TODO(ato):
// This used to have more stuff on it, but now only really does logging,
// and I'm sure there's a better way to get the message manager logs.
this.RemoteAgentActorChild = class extends ActorChild {
  get browsingContext() {
    return this.docShell.browsingContext;
  }

  sendAsyncMessage(name, data = {}) {
    log.trace(`<--(message ${name}) ${JSON.stringify(data)}`);
    super.sendAsyncMessage(name, data);
  }

  receiveMessage(name, data) {
    log.trace(`(message ${name})--> ${JSON.stringify(data)}`);
    super.receiveMessage(name, data);
  }
};

// TODO(ato): Move to MessageChannel.jsm?
// TODO(ato): This can eventually be replaced by ActorChild and IPDL generation
// TODO(ato): Can we find a shorter name?
this.MessageChannelActorChild = class extends RemoteAgentActorChild {
  constructor(despatcher) {
    super(despatcher);
    this.name = `RemoteAgent:${this.constructor.name}`;
  }

  emit(eventName, params = {}) {
    this.send({eventName, params});
  }

  send(message = {}) {
    this.sendAsyncMessage(this.name, message);
  }

  // nsIMessageListener

  async receiveMessage({name, data}) {
    const {id, methodName, params} = data;

    try {
      const func = this[methodName];
      if (!func) {
        throw new Error("Unknown method: " + methodName);
      }

      const result = await func.call(this, params);
      this.send({id, result});
    } catch ({message, stack}) {
      const error = `${message}\n${stack}`;
      this.send({id, error});
    }
  }
};
