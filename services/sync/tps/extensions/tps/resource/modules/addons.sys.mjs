/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AddonManager } from "resource://gre/modules/AddonManager.sys.mjs";
import { AddonUtils } from "resource://services-sync/addonutils.sys.mjs";
import { Logger } from "resource://tps/logger.sys.mjs";

export const STATE_ENABLED = 1;
export const STATE_DISABLED = 2;

export function Addon(TPS, id) {
  this.TPS = TPS;
  this.id = id;
}

Addon.prototype = {
  addon: null,

  async uninstall() {
    // find our addon locally
    let addon = await AddonManager.getAddonByID(this.id);
    Logger.AssertTrue(
      !!addon,
      "could not find addon " + this.id + " to uninstall"
    );
    await AddonUtils.uninstallAddon(addon);
  },

  async find(state) {
    let addon = await AddonManager.getAddonByID(this.id);

    if (!addon) {
      Logger.logInfo("Could not find add-on with ID: " + this.id);
      return false;
    }

    this.addon = addon;

    Logger.logInfo(
      "add-on found: " + addon.id + ", enabled: " + !addon.userDisabled
    );
    if (state == STATE_ENABLED) {
      Logger.AssertFalse(addon.userDisabled, "add-on is disabled: " + addon.id);
      return true;
    } else if (state == STATE_DISABLED) {
      Logger.AssertTrue(addon.userDisabled, "add-on is enabled: " + addon.id);
      return true;
    } else if (state) {
      throw new Error("Don't know how to handle state: " + state);
    } else {
      // No state, so just checking that it exists.
      return true;
    }
  },

  async install() {
    // For Install, the id parameter initially passed is really the filename
    // for the addon's install .xml; we'll read the actual id from the .xml.

    const result = await AddonUtils.installAddons([
      { id: this.id, requireSecureURI: false },
    ]);

    Logger.AssertEqual(
      1,
      result.installedIDs.length,
      "Exactly 1 add-on was installed."
    );
    Logger.AssertEqual(
      this.id,
      result.installedIDs[0],
      "Add-on was installed successfully: " + this.id
    );
  },

  async setEnabled(flag) {
    Logger.AssertTrue(await this.find(), "Add-on is available.");

    let userDisabled;
    if (flag == STATE_ENABLED) {
      userDisabled = false;
    } else if (flag == STATE_DISABLED) {
      userDisabled = true;
    } else {
      throw new Error("Unknown flag to setEnabled: " + flag);
    }

    AddonUtils.updateUserDisabled(this.addon, userDisabled);

    return true;
  },
};
