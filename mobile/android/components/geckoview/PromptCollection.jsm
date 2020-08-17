/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["PromptCollection"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  GeckoViewPrompter: "resource://gre/modules/GeckoViewPrompter.jsm",
});

const { debug, warn } = GeckoViewUtils.initLogging("PromptCollection");

class PromptCollection {
  beforeUnloadCheck(browsingContext) {
    const msg = {
      type: "beforeUnload",
    };
    const prompter = new GeckoViewPrompter(browsingContext);
    const result = prompter.showPrompt(msg);
    return !!result?.allow;
  }
}

PromptCollection.prototype.classID = Components.ID(
  "{3e30d2a0-9934-11ea-bb37-0242ac130002}"
);

PromptCollection.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIPromptCollection",
]);
