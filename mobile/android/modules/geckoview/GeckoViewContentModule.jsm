/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["GeckoViewContentModule"];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

var dump = Cu.import("resource://gre/modules/AndroidLog.jsm", {})
           .AndroidLog.d.bind(null, "ViewContentModule");

function debug(aMsg) {
  // dump(aMsg);
}

class GeckoViewContentModule {
  constructor(aModuleName, aMessageManager) {
    this.moduleName = aModuleName;
    this.messageManager = aMessageManager;

    this.messageManager.addMessageListener(
      "GeckoView:Register",
      aMsg => {
        if (aMsg.data.module == this.moduleName) {
          this.register();
        }
      }
    );
    this.messageManager.addMessageListener(
      "GeckoView:Unregister",
      aMsg => {
        if (aMsg.data.module == this.moduleName) {
          this.unregister();
        }
      }
    );

    this.init();
  }

  init() {}
  register() {}
  unregister() {}
}
