/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
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

export const log = LogModule;
