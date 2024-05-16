/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewActorParent } from "resource://gre/modules/GeckoViewActorParent.sys.mjs";

export class GeckoViewPrintDelegateParent extends GeckoViewActorParent {
  constructor() {
    super();
    this._browserStaticClone = null;
  }

  set browserStaticClone(staticClone) {
    this._browserStaticClone = staticClone;
  }

  get browserStaticClone() {
    return this._browserStaticClone;
  }

  clearStaticClone() {
    // Removes static browser element from DOM that was made for window.print
    this.browserStaticClone?.remove();
    this.browserStaticClone = null;
  }

  printRequest() {
    if (this.browserStaticClone != null) {
      this.eventDispatcher.sendRequest({
        type: "GeckoView:DotPrintRequest",
        canonicalBrowsingContextId: this.browserStaticClone.browsingContext.id,
      });
    }
  }
}
