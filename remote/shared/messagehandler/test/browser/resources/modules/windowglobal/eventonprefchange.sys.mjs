/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const TEST_PREF = "remote.messagehandler.test.pref";

class EventOnPrefChangeModule extends Module {
  constructor(messageHandler) {
    super(messageHandler);
    Services.prefs.addObserver(TEST_PREF, this.#onPreferenceUpdated);
  }

  destroy() {
    Services.prefs.removeObserver(TEST_PREF, this.#onPreferenceUpdated);
  }

  #onPreferenceUpdated = () => {
    this.emitEvent("preference-changed");
  };

  /**
   * Commands
   */

  ping() {
    // We only use this command to force creating the module.
    return 1;
  }
}

export const eventonprefchange = EventOnPrefChangeModule;
