/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    prefix: "UserCharacteristicsPage",
    maxLogLevelPref: "toolkit.telemetry.user_characteristics_ping.logLevel",
  });
});

export class UserCharacteristicsChild extends JSWindowActorChild {
  /**
   * A placeholder for the collected data.
   *
   * @typedef {Object} userDataDetails
   *   @property {string} debug - The debug messages.
   *   @property {Object} output - The user characteristics data.
   */
  userDataDetails;

  collectingDelay = 1000; // Collecting delay for 1000 ms.

  // Please add data collection here if the collection requires privilege
  // access, such as accessing ChromeOnly functions. The function is called
  // right after the content page finishes.
  async collectUserCharacteristicsData() {
    lazy.console.debug("Calling collectUserCharacteristicsData()");
  }

  // This function is similar to the above function, but we call this function
  // with a delay after the content page finished. This function is for data
  // that requires loading time.
  async collectUserCharacteristicsDataWithDelay() {
    lazy.console.debug("Calling collectUserCharacteristicsDataWithDelay()");
  }

  async handleEvent(event) {
    lazy.console.debug("Got ", event.type);
    switch (event.type) {
      case "UserCharacteristicsDataDone":
        this.userDataDetails = event.detail;

        await this.collectUserCharacteristicsData();

        await new Promise(resolve => {
          lazy.setTimeout(resolve, this.collectingDelay);
        });

        await this.collectUserCharacteristicsDataWithDelay();

        lazy.console.debug("creating IdleDispatch");
        ChromeUtils.idleDispatch(() => {
          lazy.console.debug("sending PageReady");
          this.sendAsyncMessage(
            "UserCharacteristics::PageReady",
            this.userDataDetails
          );
        });
        break;
    }
  }
}
