/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ContentProcessSession"];

const {ContentProcessDomains} = ChromeUtils.import("chrome://remote/content/domains/ContentProcessDomains.jsm");
const {Domains} = ChromeUtils.import("chrome://remote/content/domains/Domains.jsm");

class ContentProcessSession {
  constructor(messageManager, browsingContext, content, docShell) {
    this.messageManager = messageManager;
    this.browsingContext = browsingContext;
    this.content = content;
    this.docShell = docShell;

    this.domains = new Domains(this, ContentProcessDomains);
    this.messageManager.addMessageListener("remote:request", this);
    this.messageManager.addMessageListener("remote:destroy", this);
  }

  destroy() {
    this.messageManager.removeMessageListener("remote:request", this);
    this.messageManager.removeMessageListener("remote:destroy", this);
  }

  // Domain event listener

  onEvent(eventName, params) {
    this.messageManager.sendAsyncMessage("remote:event", {
      browsingContextId: this.browsingContext.id,
      event: {
        eventName,
        params,
      },
    });
  }

  // nsIMessageListener

  async receiveMessage({name, data}) {
    const {browsingContextId} = data;

    // We may have more than one tab loaded in the same process,
    // and debug the two at the same time. We want to ensure not
    // mixing up the requests made against two such tabs.
    // Each tab is going to have its own frame script instance
    // and two communication channels are going to be set up via
    // the two message managers.
    if (browsingContextId != this.browsingContext.id) {
      return;
    }

    switch (name) {
    case "remote:request":
      try {
        const {id, domain, method, params} = data.request;

        const inst = this.domains.get(domain);
        const methodFn = inst[method];
        if (!methodFn || typeof methodFn != "function") {
          throw new Error(`Method implementation of ${method} missing`);
        }

        const result = await methodFn.call(inst, params);

        this.messageManager.sendAsyncMessage("remote:result", {
          browsingContextId,
          id,
          result,
        });
      } catch (e) {
        this.messageManager.sendAsyncMessage("remote:error", {
          browsingContextId,
          id: data.request.id,
          error: {
            name: e.name || "exception",
            message: e.message || String(e),
            stack: e.stack,
          },
        });
      }
      break;

    case "remote:destroy":
      this.destroy();
      break;
    }
  }
}
