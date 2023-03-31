/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewPrintDelegateParent"];

const { GeckoViewActorParent } = ChromeUtils.importESModule(
  "resource://gre/modules/GeckoViewActorParent.sys.mjs"
);

class GeckoViewPrintDelegateParent extends GeckoViewActorParent {
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

  telemetryDotPrintRequested() {
    Glean.dotprint.requested.add(1);
  }

  printRequest() {
    if (this.browserStaticClone != null) {
      this.telemetryDotPrintRequested();
      this.eventDispatcher.sendRequest({
        type: "GeckoView:DotPrintRequest",
        canonicalBrowsingContextId: this.browserStaticClone.browsingContext.id,
      });
    }
  }
}
