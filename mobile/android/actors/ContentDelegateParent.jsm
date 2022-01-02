/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ContentDelegateParent"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

const { GeckoViewActorParent } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorParent.jsm"
);

const { debug, warn } = GeckoViewUtils.initLogging("ContentDelegateParent");

class ContentDelegateParent extends GeckoViewActorParent {
  async receiveMessage(aMsg) {
    debug`receiveMessage: ${aMsg.name} ${aMsg}`;

    switch (aMsg.name) {
      case "GeckoView:DOMFullscreenExit": {
        this.window.windowUtils.remoteFrameFullscreenReverted();
        break;
      }

      case "GeckoView:DOMFullscreenRequest": {
        this.window.windowUtils.remoteFrameFullscreenChanged(this.browser);
        break;
      }
    }
  }
}
