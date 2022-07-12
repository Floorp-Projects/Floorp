/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewSelectionAction"];

const { GeckoViewModule } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewModule.jsm"
);

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

class GeckoViewSelectionAction extends GeckoViewModule {
  onEnable() {
    debug`onEnable`;
    this.registerListener(["GeckoView:ExecuteSelectionAction"]);
  }

  onDisable() {
    debug`onDisable`;
    this.unregisterListener();
  }

  get actor() {
    return this.getActor("SelectionActionDelegate");
  }

  // Bundle event handler.
  onEvent(aEvent, aData, aCallback) {
    debug`onEvent: ${aEvent}`;

    switch (aEvent) {
      case "GeckoView:ExecuteSelectionAction": {
        this.actor.executeSelectionAction(aData);
      }
    }
  }
}

const { debug, warn } = GeckoViewSelectionAction.initLogging(
  "GeckoViewSelectionAction"
);
