/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

const EXPORTED_SYMBOLS = ["GeckoViewActorParent"];

class GeckoViewActorParent extends JSWindowActorParent {
  static initLogging(aModuleName) {
    const tag = aModuleName.replace("GeckoView", "");
    return GeckoViewUtils.initLogging(tag);
  }

  get browser() {
    return this.browsingContext.top.embedderElement;
  }

  get window() {
    return this.browser.ownerGlobal;
  }

  get eventDispatcher() {
    return this.window.moduleManager.eventDispatcher;
  }

  receiveMessage(aMessage) {
    // By default messages are forwarded to the module.
    this.window.moduleManager.onMessageFromActor(this.name, aMessage);
  }
}
