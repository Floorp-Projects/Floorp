/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["browsingContext"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const { Module } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/Module.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  TabManager: "chrome://remote/content/shared/TabManager.jsm",
});

class BrowsingContextModule extends Module {
  destroy() {}

  interceptEvent(name, payload) {
    if (name == "browsingContext.load") {
      // Resolve browsing context to a TabManager id.
      payload.context = lazy.TabManager.getIdForBrowsingContext(
        payload.context
      );
    }

    return payload;
  }
}

const browsingContext = BrowsingContextModule;
