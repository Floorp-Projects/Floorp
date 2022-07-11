/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["SelectionActionDelegateParent"];

const { GeckoViewActorParent } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorParent.jsm"
);

class SelectionActionDelegateParent extends GeckoViewActorParent {
  respondTo = null;
  actionId = null;

  get rootActor() {
    return this.browsingContext.top.currentWindowGlobal.getActor(
      "SelectionActionDelegate"
    );
  }

  receiveMessage(aMessage) {
    const { data, name } = aMessage;
    switch (name) {
      case "ShowSelectionAction": {
        this.rootActor.showSelectionAction(this, data);
        break;
      }

      case "HideSelectionAction": {
        this.rootActor.hideSelectionAction(this, data.reason);
        break;
      }

      default: {
        super.receiveMessage(aMessage);
      }
    }
  }

  hideSelectionAction(aRespondTo, reason) {
    // Mark previous actions as stale. Don't do this for "invisibleselection"
    // or "scroll" because previous actions should still be valid even after
    // these events occur.
    if (reason !== "invisibleselection" && reason !== "scroll") {
      this.actionId = null;
    }

    this.eventDispatcher?.sendRequest({
      type: "GeckoView:HideSelectionAction",
      reason,
    });
  }

  showSelectionAction(aRespondTo, aData) {
    this.actionId = Services.uuid.generateUUID().toString();
    this.respondTo = aRespondTo;

    this.eventDispatcher?.sendRequest({
      type: "GeckoView:ShowSelectionAction",
      actionId: this.actionId,
      ...aData,
    });
  }

  executeSelectionAction(aData) {
    if (this.actionId === null || aData.actionId != this.actionId) {
      warn`Stale response ${aData.id} ${aData.actionId}`;
      return;
    }
    this.respondTo.sendAsyncMessage("ExecuteSelectionAction", aData);
  }
}

const { debug, warn } = SelectionActionDelegateParent.initLogging(
  "SelectionActionDelegate"
);
