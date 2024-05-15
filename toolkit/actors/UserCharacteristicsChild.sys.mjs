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

    await this.populateGamepadsInfo();
  }

  async populateGamepadsInfo() {
    lazy.console.debug("Calling populateGamepadsInfo()");
    let gamepads = await this.contentWindow.navigator.requestAllGamepads();

    lazy.console.debug(`Found ${gamepads.length} gamepads`);

    let gamepadsInfo = [];

    for (const gamepad of gamepads) {
      // We use an array to represent a gamepad device because it uses less size
      // then an object when convert to a JSON string. So, we can fit the string
      // into a Glean string which has a 100 size limitation.
      let data = [];
      data.push(gamepad.id);
      data.push(gamepad?.hand ?? "");
      data.push(gamepad.buttons.length);
      data.push(gamepad.axes?.length ?? 0);
      data.push(gamepad.hapticActuators?.length ?? 0);
      data.push(gamepad.lightIndicators?.length ?? 0);
      data.push(gamepad.touchEvents?.length ?? 0);

      gamepadsInfo.push(JSON.stringify(data));
    }

    lazy.console.debug(`Reporting gamepad: ${gamepadsInfo}`);
    this.userDataDetails.output.gamepads = gamepadsInfo;
  }

  async handleEvent(event) {
    lazy.console.debug("Got ", event.type);
    switch (event.type) {
      case "UserCharacteristicsDataDone":
        // Clone the data so we can modify it. Otherwise, we cannot change it
        // because it's behind Xray wrapper.
        this.userDataDetails = structuredClone(event.detail);

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
