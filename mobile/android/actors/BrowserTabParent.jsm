/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["BrowserTabParent"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

const { debug, warn } = GeckoViewUtils.initLogging("BrowserTabParent");

class BrowserTabParent extends JSWindowActorParent {
  async receiveMessage(aMsg) {
    debug`receiveMessage: ${aMsg.name} ${aMsg}`;

    switch (aMsg.name) {
      case "Browser:LoadURI": {
        const browser = this.browsingContext.top.embedderElement;
        const window = browser.ownerGlobal;
        if (!window || !window.moduleManager) {
          debug`No window`;
          return;
        }

        const { loadOptions, historyIndex } = aMsg.data;
        const { uri } = loadOptions;
        const { moduleManager } = window;

        moduleManager.updateRemoteAndNavigate(uri, loadOptions, historyIndex);
      }
    }
  }
}
