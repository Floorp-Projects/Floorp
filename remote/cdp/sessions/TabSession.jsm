/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TabSession"];

const { Session } = ChromeUtils.import(
  "chrome://remote/content/cdp/sessions/Session.jsm"
);

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
   *        For now, this is only passed by `Target.attachToTarget()`.
   *        Otherwise it will be undefined when you are connecting directly to
   *        a given Tab. i.e. connect directly to the WebSocket URL provided by
   *        /json/list HTTP endpoint.
   */
  constructor(connection, target, id) {
    super(connection, target, id);

    // Request id => { resolve, reject }
    this.requestPromises = new Map();

    this.mm.addMessageListener("remote:event", this);
    this.mm.addMessageListener("remote:result", this);
    this.mm.addMessageListener("remote:error", this);

    this.mm.loadFrameScript(
      "chrome://remote/content/cdp/sessions/frame-script.js",
      false
    );
  }

  destructor() {
    super.destructor();

    this.requestPromises.clear();

    this.mm.sendAsyncMessage("remote:destroy", {
      browsingContextId: this.browsingContext.id,
    });

    this.mm.removeMessageListener("remote:event", this);
    this.mm.removeMessageListener("remote:result", this);
    this.mm.removeMessageListener("remote:error", this);
  }

  execute(id, domain, command, params) {
    // Check if the domain and command is implemented in the parent
    // and execute it there. Otherwise forward the command to the content process
    // in order to try to execute it in the content process.
    if (this.domains.domainSupportsMethod(domain, command)) {
      return super.execute(id, domain, command, params);
    }
    return this.executeInChild(id, domain, command, params);
  }

  executeInChild(id, domain, command, params) {
    return new Promise((resolve, reject) => {
      // Save the promise's resolution and rejection handler in order to later
      // resolve this promise once we receive the reply back from the content process.
      this.requestPromises.set(id, { resolve, reject });

      this.mm.sendAsyncMessage("remote:request", {
        browsingContextId: this.browsingContext.id,
        request: { id, domain, command, params },
      });
    });
  }

  get mm() {
    return this.target.mm;
  }

  get browsingContext() {
    return this.target.browsingContext;
  }

  // nsIMessageListener

  receiveMessage({ name, data }) {
    const { id, result, event, error } = data;

    switch (name) {
      case "remote:result":
        const { resolve } = this.requestPromises.get(id);
        resolve(result);
        this.requestPromises.delete(id);
        break;

      case "remote:event":
        this.connection.sendEvent(event.eventName, event.params, this.id);
        break;

      case "remote:error":
        const { reject } = this.requestPromises.get(id);
        reject(error);
        this.requestPromises.delete(id);
        break;
    }
  }
}
