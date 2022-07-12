/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["log"];

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

class LogModule extends Module {
  destroy() {}

  interceptEvent(name, payload) {
    if (name == "log.entryAdded") {
      // Resolve browsing context to a TabManager id.
      payload.source.context = lazy.TabManager.getIdForBrowsingContext(
        payload.source.context
      );
    }

    return payload;
  }
}

const log = LogModule;
