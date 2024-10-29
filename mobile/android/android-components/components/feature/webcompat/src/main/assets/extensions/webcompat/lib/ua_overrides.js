/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser, module */

class UAOverrides {
  constructor(availableOverrides) {
    this.OVERRIDE_PREF = "perform_ua_overrides";

    this._overridesEnabled = true;

    this._availableOverrides = availableOverrides;
    this._activeListeners = new Map();
  }

  bindAboutCompatBroker(broker) {
    this._aboutCompatBroker = broker;
  }

  bootup() {
    browser.aboutConfigPrefs.onPrefChange.addListener(() => {
      this.checkOverridePref();
    }, this.OVERRIDE_PREF);
    this.checkOverridePref();
  }

  checkOverridePref() {
    browser.aboutConfigPrefs.getPref(this.OVERRIDE_PREF).then(value => {
      if (value === undefined) {
        browser.aboutConfigPrefs.setPref(this.OVERRIDE_PREF, true);
      } else if (value === false) {
        this.unregisterUAOverrides();
      } else {
        this.registerUAOverrides();
      }
    });
  }

  getAvailableOverrides() {
    return this._availableOverrides;
  }

  isEnabled() {
    return this._overridesEnabled;
  }

  enableOverride(override) {
    if (override.active) {
      return;
    }

    const { blocks, matches, uaTransformer } = override.config;
    const listener = details => {
      // Don't actually override the UA for an experiment if the user is not
      // part of the experiment (unless they force-enabed the override).
      if (
        !override.config.experiment ||
        override.permanentPrefEnabled === true
      ) {
        for (const header of details.requestHeaders) {
          if (header.name.toLowerCase() === "user-agent") {
            // Don't override the UA if we're on a mobile device that has the
            // "Request Desktop Site" mode enabled. The UA for the desktop mode
            // is set inside Gecko with a simple string replace, so we can use
            // that as a check, see https://searchfox.org/mozilla-central/rev/89d33e1c3b0a57a9377b4815c2f4b58d933b7c32/mobile/android/chrome/geckoview/GeckoViewSettingsChild.js#23-28
            let isMobileWithDesktopMode =
              override.currentPlatform == "android" &&
              header.value.includes("X11; Linux x86_64");

            if (!isMobileWithDesktopMode) {
              header.value = uaTransformer(header.value);
            }
          }
        }
      }
      return { requestHeaders: details.requestHeaders };
    };

    browser.webRequest.onBeforeSendHeaders.addListener(
      listener,
      { urls: matches },
      ["blocking", "requestHeaders"]
    );

    const listeners = { onBeforeSendHeaders: listener };
    if (blocks) {
      const blistener = () => {
        return { cancel: true };
      };

      browser.webRequest.onBeforeRequest.addListener(
        blistener,
        { urls: blocks },
        ["blocking"]
      );

      listeners.onBeforeRequest = blistener;
    }
    this._activeListeners.set(override, listeners);
    override.active = true;
  }

  onOverrideConfigChanged(override) {
    // Check whether the override should be hidden from about:compat.
    override.hidden = override.config.hidden;

    // Setting the override's permanent pref overrules whether it is hidden.
    if (override.permanentPrefEnabled !== undefined) {
      override.hidden = !override.permanentPrefEnabled;
    }

    // Also check whether the override should be active.
    let shouldBeActive = true;

    // Overrides can be force-deactivated by their permanent preference.
    if (override.permanentPrefEnabled === false) {
      shouldBeActive = false;
    }

    // Overrides gated behind an experiment the user is not part of do not
    // have to be activated, unless they are gathering telemetry, or the
    // user has force-enabled them with their permanent pref.
    if (override.config.experiment && override.permanentPrefEnabled !== true) {
      shouldBeActive = false;
    }

    if (shouldBeActive) {
      this.enableOverride(override);
    } else {
      this.disableOverride(override);
    }

    if (this._overridesEnabled) {
      this._aboutCompatBroker.portsToAboutCompatTabs.broadcast({
        overridesChanged: this._aboutCompatBroker.filterOverrides(
          this._availableOverrides
        ),
      });
    }
  }

  async registerUAOverrides() {
    const platformMatches = ["all"];
    const { os } = await browser.runtime.getPlatformInfo();
    platformMatches.push(os);
    platformMatches.push(os == "android" ? "android" : "desktop");

    for (const override of this._availableOverrides) {
      if (platformMatches.includes(override.platform)) {
        override.availableOnPlatform = true;
        override.currentPlatform = os;

        // If there is a specific about:config preference governing
        // this override, monitor its state.
        const pref = override.config.permanentPref;
        override.permanentPrefEnabled =
          pref && (await browser.aboutConfigPrefs.getPref(pref));
        if (pref) {
          const checkOverridePref = () => {
            browser.aboutConfigPrefs.getPref(pref).then(value => {
              override.permanentPrefEnabled = value;
              this.onOverrideConfigChanged(override);
            });
          };
          browser.aboutConfigPrefs.onPrefChange.addListener(
            checkOverridePref,
            pref
          );
        }

        this.onOverrideConfigChanged(override);
      }
    }

    this._overridesEnabled = true;
    this._aboutCompatBroker.portsToAboutCompatTabs.broadcast({
      overridesChanged: this._aboutCompatBroker.filterOverrides(
        this._availableOverrides
      ),
    });
  }

  unregisterUAOverrides() {
    for (const override of this._availableOverrides) {
      this.disableOverride(override);
    }

    this._overridesEnabled = false;
    this._aboutCompatBroker.portsToAboutCompatTabs.broadcast({
      overridesChanged: false,
    });
  }

  disableOverride(override) {
    if (!override.active) {
      return;
    }

    const listeners = this._activeListeners.get(override);
    for (const [name, listener] of Object.entries(listeners)) {
      browser.webRequest[name].removeListener(listener);
    }
    override.active = false;
    this._activeListeners.delete(override);
  }
}

module.exports = UAOverrides;
