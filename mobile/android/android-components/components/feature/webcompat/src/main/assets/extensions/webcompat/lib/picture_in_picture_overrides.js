/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser, module */

class PictureInPictureOverrides {
  constructor(availableOverrides) {
    this.pref = "enable_picture_in_picture_overrides";
    this._prefEnabledOverrides = new Set();
    this._availableOverrides = availableOverrides;
    this.policies = browser.pictureInPictureChild.getPolicies();
  }

  async _checkGlobalPref() {
    await browser.aboutConfigPrefs.getPref(this.pref).then(value => {
      if (value === false) {
        this._enabled = false;
      } else {
        if (value === undefined) {
          browser.aboutConfigPrefs.setPref(this.pref, true);
        }
        this._enabled = true;
      }
    });
  }

  async _checkSpecificOverridePref(id, pref) {
    const isDisabled = await browser.aboutConfigPrefs.getPref(pref);
    if (isDisabled === true) {
      this._prefEnabledOverrides.delete(id);
    } else {
      this._prefEnabledOverrides.add(id);
    }
  }

  bootup() {
    const checkGlobal = async () => {
      await this._checkGlobalPref();
      this._onAvailableOverridesChanged();
    };
    browser.aboutConfigPrefs.onPrefChange.addListener(checkGlobal, this.pref);

    const bootupPrefCheckPromises = [this._checkGlobalPref()];

    for (const id of Object.keys(this._availableOverrides)) {
      const pref = `disabled_picture_in_picture_overrides.${id}`;
      const checkSingle = async () => {
        await this._checkSpecificOverridePref(id, pref);
        this._onAvailableOverridesChanged();
      };
      browser.aboutConfigPrefs.onPrefChange.addListener(checkSingle, pref);
      bootupPrefCheckPromises.push(this._checkSpecificOverridePref(id, pref));
    }

    Promise.all(bootupPrefCheckPromises).then(() => {
      this._onAvailableOverridesChanged();
    });
  }

  async _onAvailableOverridesChanged() {
    const policies = await this.policies;
    let enabledOverrides = {};
    for (const [id, override] of Object.entries(this._availableOverrides)) {
      const enabled = this._enabled && this._prefEnabledOverrides.has(id);
      for (const [url, policy] of Object.entries(override)) {
        enabledOverrides[url] = enabled ? policy : policies.DEFAULT;
      }
    }
    browser.pictureInPictureParent.setOverrides(enabledOverrides);
  }
}
