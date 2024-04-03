/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    prefix: "UserCharacteristicsPage",
    maxLogLevelPref: "toolkit.telemetry.user_characteristics_ping.logLevel",
  });
});

export class UserCharacteristicsChild extends JSWindowActorChild {
  handleEvent(event) {
    lazy.console.debug("Got ", event.type);
    switch (event.type) {
      case "UserCharacteristicsDataDone":
        lazy.console.debug("creating IdleDispatch");
        ChromeUtils.idleDispatch(() => {
          lazy.console.debug("sending PageReady");
          this.sendAsyncMessage("UserCharacteristics::PageReady", event.detail);
        });
        break;
    }
  }
}
