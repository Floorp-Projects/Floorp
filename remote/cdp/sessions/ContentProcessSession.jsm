/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ContentProcessSession"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ContentProcessDomains:
    "chrome://remote/content/cdp/domains/ContentProcessDomains.jsm",
  ContextObserver: "chrome://remote/content/cdp/observers/ContextObserver.jsm",
  DomainCache: "chrome://remote/content/cdp/domains/DomainCache.jsm",
});

class ContentProcessSession {
  constructor(messageManager, browsingContext, content, docShell) {
    this.messageManager = messageManager;
    this.browsingContext = browsingContext;
    this.content = content;
    this.docShell = docShell;
    // Most children or sibling classes are going to assume that docShell
    // implements the following interface. So do the QI only once from here.
    this.docShell.QueryInterface(Ci.nsIWebNavigation);

    this.domains = new lazy.DomainCache(this, lazy.ContentProcessDomains);
    this.messageManager.addMessageListener("remote:request", this);
    this.messageManager.addMessageListener("remote:destroy", this);
  }

  destructor() {
    this._contextObserver?.destructor();

    this.messageManager.removeMessageListener("remote:request", this);
    this.messageManager.removeMessageListener("remote:destroy", this);
    this.domains.clear();
  }

  get contextObserver() {
    if (!this._contextObserver) {
      this._contextObserver = new lazy.ContextObserver(
        this.docShell.chromeEventHandler
      );
    }
    return this._contextObserver;
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

  async receiveMessage({ name, data }) {
    const { browsingContextId } = data;

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
          const { id, domain, command, params } = data.request;

          const result = await this.domains.execute(domain, command, params);

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
        this.destructor();
        break;
    }
  }
}
