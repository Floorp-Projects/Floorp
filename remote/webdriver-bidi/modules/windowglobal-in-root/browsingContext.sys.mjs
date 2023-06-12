/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
});

class BrowsingContextModule extends Module {
  destroy() {}

  interceptEvent(name, payload) {
    if (
      name == "browsingContext.domContentLoaded" ||
      name == "browsingContext.load"
    ) {
      // Resolve browsing context to a Navigable id.
      payload.context = lazy.TabManager.getIdForBrowsingContext(
        payload.context
      );
    }

    return payload;
  }
}

export const browsingContext = BrowsingContextModule;
