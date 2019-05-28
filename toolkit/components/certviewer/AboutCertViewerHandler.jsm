/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutCertViewerHandler"];

const { RemotePages } = ChromeUtils.import("resource://gre/modules/remotepagemanager/RemotePageManagerParent.jsm");

var AboutCertViewerHandler = {
  _inited: false,
  _topics: [
    "getCertificate",
  ],

  init() {
    this.pageListener = new RemotePages("about:certificate");
    for (let topic of this._topics) {
      this.pageListener.addMessageListener(topic, this.receiveMessage);
    }
    this._inited = true;
  },

  uninit() {
    if (!this._inited) {
      return;
    }
    for (let topic of this._topics) {
      this.pageListener.removeMessageListener(topic, this.receiveMessage);
    }
    this.pageListener.destroy();
  },

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "getCertificate": {
        break;
      }
    }
  },
};

