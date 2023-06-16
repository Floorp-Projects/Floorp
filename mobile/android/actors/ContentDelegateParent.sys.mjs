/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewUtils } from "resource://gre/modules/GeckoViewUtils.sys.mjs";
import { GeckoViewActorParent } from "resource://gre/modules/GeckoViewActorParent.sys.mjs";

const { debug, warn } = GeckoViewUtils.initLogging("ContentDelegateParent");

export class ContentDelegateParent extends GeckoViewActorParent {
  async receiveMessage(aMsg) {
    debug`receiveMessage: ${aMsg.name}`;

    switch (aMsg.name) {
      case "GeckoView:DOMFullscreenExit": {
        this.window.windowUtils.remoteFrameFullscreenReverted();
        return null;
      }

      case "GeckoView:DOMFullscreenRequest": {
        this.window.windowUtils.remoteFrameFullscreenChanged(this.browser);
        return null;
      }
    }

    return super.receiveMessage(aMsg);
  }
}
