/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["PromptCollection"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  GeckoViewPrompter: "resource://gre/modules/GeckoViewPrompter.jsm",
});

const { debug, warn } = GeckoViewUtils.initLogging("PromptCollection");

class PromptCollection {
  confirmRepost(browsingContext) {
    const msg = {
      type: "repost",
    };
    const prompter = new lazy.GeckoViewPrompter(browsingContext);
    const result = prompter.showPrompt(msg);
    return !!result?.allow;
  }

  asyncBeforeUnloadCheck(browsingContext) {
    return new Promise(resolve => {
      const msg = {
        type: "beforeUnload",
      };
      const prompter = new lazy.GeckoViewPrompter(browsingContext);
      prompter.asyncShowPrompt(msg, resolve);
    }).then(result => !!result?.allow);
  }

  confirmFolderUpload() {
    // Folder upload is not supported by GeckoView yet, see Bug 1674428.
    return false;
  }
}

PromptCollection.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIPromptCollection",
]);
