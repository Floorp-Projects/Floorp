/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewUtils } from "resource://gre/modules/GeckoViewUtils.sys.mjs";

export class GeckoViewActorParent extends JSWindowActorParent {
  static initLogging(aModuleName) {
    const tag = aModuleName.replace("GeckoView", "");
    return GeckoViewUtils.initLogging(tag);
  }

  get browser() {
    return this.browsingContext.top.embedderElement;
  }

  get window() {
    const { browsingContext } = this;
    // If this is a chrome actor, the chrome window will be at
    // browsingContext.window.
    if (!browsingContext.isContent && browsingContext.window) {
      return browsingContext.window;
    }
    return this.browser?.ownerGlobal;
  }

  get eventDispatcher() {
    return this.window?.moduleManager.eventDispatcher;
  }

  receiveMessage(aMessage) {
    if (!this.window) {
      // If we have no window, it means that this browsingContext has been
      // destroyed already and there's nothing to do here for us.
      debug`receiveMessage window destroyed ${aMessage.name} ${aMessage.data?.type}`;
      return null;
    }

    switch (aMessage.name) {
      case "DispatcherMessage":
        return this.eventDispatcher.sendRequest(aMessage.data);
      case "DispatcherQuery":
        return this.eventDispatcher.sendRequestForResult(aMessage.data);
    }

    // By default messages are forwarded to the module.
    return this.window.moduleManager.onMessageFromActor(this.name, aMessage);
  }
}

const { debug, warn } = GeckoViewUtils.initLogging("GeckoViewActorParent");
