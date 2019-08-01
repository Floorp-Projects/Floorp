/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser, module */

class Injections {
  constructor(availableInjections) {
    this.INJECTION_PREF = "perform_injections";

    this._injectionsEnabled = true;

    this._availableInjections = availableInjections;
    this._activeInjections = new Map();
  }

  bindAboutCompatBroker(broker) {
    this._aboutCompatBroker = broker;
  }

  bootup() {
    browser.aboutConfigPrefs.onPrefChange.addListener(() => {
      this.checkInjectionPref();
    }, this.INJECTION_PREF);
    this.checkInjectionPref();
  }

  checkInjectionPref() {
    browser.aboutConfigPrefs.getPref(this.INJECTION_PREF).then(value => {
      if (value === undefined) {
        browser.aboutConfigPrefs.setPref(this.INJECTION_PREF, true);
      } else if (value === false) {
        this.unregisterContentScripts();
      } else {
        this.registerContentScripts();
      }
    });
  }

  getAvailableInjections() {
    return this._availableInjections;
  }

  isEnabled() {
    return this._injectionsEnabled;
  }

  async registerContentScripts() {
    const platformMatches = ["all"];
    let platformInfo = await browser.runtime.getPlatformInfo();
    platformMatches.push(platformInfo.os == "android" ? "android" : "desktop");

    for (const injection of this._availableInjections) {
      if (platformMatches.includes(injection.platform)) {
        injection.availableOnPlatform = true;
        await this.enableInjection(injection);
      }
    }

    this._injectionsEnabled = true;
    this._aboutCompatBroker.portsToAboutCompatTabs.broadcast({
      interventionsChanged: this._aboutCompatBroker.filterOverrides(
        this._availableInjections
      ),
    });
  }

  replaceStringInRequest(requestId, inString, outString, inEncoding = "utf-8") {
    const filter = browser.webRequest.filterResponseData(requestId);
    const decoder = new TextDecoder(inEncoding);
    const encoder = new TextEncoder();
    const RE = new RegExp(inString, "g");
    const carryoverLength = inString.length;
    let carryover = "";

    filter.ondata = event => {
      const replaced = (
        carryover + decoder.decode(event.data, { stream: true })
      ).replace(RE, outString);
      filter.write(encoder.encode(replaced.slice(0, -carryoverLength)));
      carryover = replaced.slice(-carryoverLength);
    };

    filter.onstop = event => {
      if (carryover.length) {
        filter.write(encoder.encode(carryover));
      }
      filter.close();
    };
  }

  async enableInjection(injection) {
    if (injection.active) {
      return;
    }

    if ("pdk5fix" in injection) {
      const { urls, types } = injection.pdk5fix;
      const listener = (injection.pdk5fix.listener = ({ requestId }) => {
        this.replaceStringInRequest(
          requestId,
          "VideoContextChromeAndroid",
          "VideoContextAndroid"
        );
        return {};
      });
      browser.webRequest.onBeforeRequest.addListener(
        listener,
        { urls, types },
        ["blocking"]
      );
      injection.active = true;
      return;
    }

    try {
      const handle = await browser.contentScripts.register(
        injection.contentScripts
      );
      this._activeInjections.set(injection, handle);
      injection.active = true;
    } catch (ex) {
      console.error(
        "Registering WebCompat GoFaster content scripts failed: ",
        ex
      );
    }
  }

  unregisterContentScripts() {
    for (const injection of this._availableInjections) {
      this.disableInjection(injection);
    }

    this._injectionsEnabled = false;
    this._aboutCompatBroker.portsToAboutCompatTabs.broadcast({
      interventionsChanged: false,
    });
  }

  async disableInjection(injection) {
    if (!injection.active) {
      return;
    }

    if (injection.pdk5fix) {
      const { listener } = injection.pdk5fix;
      browser.webRequest.onBeforeRequest.removeListener(listener);
      injection.active = false;
      delete injection.pdk5fix.listener;
      return;
    }

    const contentScript = this._activeInjections.get(injection);
    await contentScript.unregister();
    this._activeInjections.delete(injection);
    injection.active = false;
  }
}

module.exports = Injections;
