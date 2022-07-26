/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

const TalosContent = {
  init() {
    addMessageListener("Talos:ForceGC", this);
  },

  receiveMessage(msg) {
    if (msg.name == "Talos:ForceGC") {
      this.forceGC();
    }
  },

  forceGC() {
    content.windowUtils.garbageCollect();
    sendAsyncMessage("Talos:ForceGC:OK");
  },
};

TalosContent.init();
