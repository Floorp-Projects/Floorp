/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewUtils } from "resource://gre/modules/GeckoViewUtils.sys.mjs";
import { GeckoViewActorParent } from "resource://gre/modules/GeckoViewActorParent.sys.mjs";

const { debug, warn } = GeckoViewUtils.initLogging("ContentDelegateParent");

export class ContentDelegateParent extends GeckoViewActorParent {
  didDestroy() {
    this._didDestroy = true;
  }

  async receiveMessage(aMsg) {
    debug`receiveMessage: ${aMsg.name}`;

    switch (aMsg.name) {
      case "GeckoView:DOMFullscreenExit": {
        if (!this.#hasBeenDestroyed() && !this.#requestOrigin) {
          this.#requestOrigin = this;
        }
        this.window.windowUtils.remoteFrameFullscreenReverted();
        return null;
      }

      case "GeckoView:DOMFullscreenRequest": {
        this.#requestOrigin = this;
        this.window.windowUtils.remoteFrameFullscreenChanged(this.browser);
        return null;
      }
    }

    return super.receiveMessage(aMsg);
  }

  // This is a copy of browser/actors/DOMFullscreenParent.sys.mjs
  get #requestOrigin() {
    const chromeBC = this.browsingContext.topChromeWindow?.browsingContext;
    const requestOrigin = chromeBC?.fullscreenRequestOrigin;
    return requestOrigin && requestOrigin.get();
  }

  // This is a copy of browser/actors/DOMFullscreenParent.sys.mjs
  set #requestOrigin(aActor) {
    const chromeBC = this.browsingContext.topChromeWindow?.browsingContext;
    if (!chromeBC) {
      debug`not able to get browsingContext for chrome window.`;
      return;
    }

    if (aActor) {
      chromeBC.fullscreenRequestOrigin = Cu.getWeakReference(aActor);
      return;
    }
    delete chromeBC.fullscreenRequestOrigin;
  }

  // This is a copy of browser/actors/DOMFullscreenParent.sys.mjs
  #hasBeenDestroyed() {
    if (this._didDestroy) {
      return true;
    }

    // The 'didDestroy' callback is not always getting called.
    // So we can't rely on it here. Instead, we will try to access
    // the browsing context to judge wether the actor has
    // been destroyed or not.
    try {
      return !this.browsingContext;
    } catch {
      return true;
    }
  }
}
