/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
});

export var RemoteSettingsTimer = function () {};
RemoteSettingsTimer.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsITimerCallback"]),
  classID: Components.ID("{5e756573-234a-49ea-bbe4-59ec7a70657d}"),
  contractID: "@mozilla.org/services/settings;1",

  // By default, this timer fires once every 24 hours. See the "services.settings.poll_interval" pref.
  notify(timer) {
    lazy.RemoteSettings.pollChanges({ trigger: "timer" }).catch(e =>
      console.error(e)
    );
  },
};
