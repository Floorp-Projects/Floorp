/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    prefix: "UserCharacteristicsPage",
    maxLogLevelPref: "toolkit.telemetry.user_characteristics_ping.logLevel",
  });
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "UserCharacteristicsPageService",
  "@mozilla.org/user-characteristics-page;1",
  "nsIUserCharacteristicsPageService"
);

export class UserCharacteristicsParent extends JSWindowActorParent {
  receiveMessage(aMessage) {
    lazy.console.debug("Got ", aMessage.name);
    if (aMessage.name == "UserCharacteristics::PageReady") {
      lazy.console.debug("Got pageReady");
      lazy.UserCharacteristicsPageService.pageLoaded(
        this.browsingContext,
        aMessage.data
      );
    }
  }
}
