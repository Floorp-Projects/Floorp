/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewPermission"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

class GeckoViewPermission {
  constructor() {
    this.wrappedJSObject = this;
  }

  async prompt(aRequest) {
    const window = aRequest.window
      ? aRequest.window
      : aRequest.element.ownerGlobal;

    const actor = window.windowGlobalChild.getActor("GeckoViewPermission");
    const result = await actor.promptPermission(aRequest);
    if (!result.allow) {
      aRequest.cancel();
    } else {
      // Note: permission could be undefined, that's what aRequest expects.
      const { permission } = result;
      aRequest.allow(permission);
    }
  }
}

GeckoViewPermission.prototype.classID = Components.ID(
  "{42f3c238-e8e8-4015-9ca2-148723a8afcf}"
);
GeckoViewPermission.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIContentPermissionPrompt",
]);

const { debug, warn } = GeckoViewUtils.initLogging("GeckoViewPermission");
