/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewMedia"];

ChromeUtils.import("resource://gre/modules/GeckoViewModule.jsm");

class GeckoViewMedia extends GeckoViewModule {
  onEnable() {
    this.registerListener([
      "GeckoView:MediaObserve",
      "GeckoView:MediaUnobserve",
      "GeckoView:MediaPlay",
      "GeckoView:MediaPause",
      "GeckoView:MediaSeek",
      "GeckoView:MediaSetVolume",
      "GeckoView:MediaSetMuted",
      "GeckoView:MediaSetPlaybackRate",
    ]);
  }

  onDisable() {
    this.unregisterListener();
  }

  onEvent(aEvent, aData, aCallback) {
    debug `onEvent: event=${aEvent}, data=${aData}`;
    this.messageManager.sendAsyncMessage(aEvent, aData);
  }
}
