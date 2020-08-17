/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ContentDelegateParent"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

const { debug, warn } = GeckoViewUtils.initLogging("ContentDelegateParent");

class ContentDelegateParent extends JSWindowActorParent {
  async receiveMessage(aMsg) {
    debug`receiveMessage: ${aMsg.name} ${aMsg}`;

    const browser = this.browsingContext.top.embedderElement;
    const window = browser.ownerGlobal;

    switch (aMsg.name) {
      case "GeckoView:DOMFullscreenExit": {
        window.windowUtils.remoteFrameFullscreenReverted();
        break;
      }

      case "GeckoView:DOMFullscreenRequest": {
        window.windowUtils.remoteFrameFullscreenChanged(browser);
        break;
      }
    }
  }
}
