/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["Region"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  LocationHelper: "resource://gre/modules/LocationHelper.jsm",
  Services: "resource://gre/modules/Services.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
});

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "wifiScanningEnabled",
  "browser.region.network.scan",
  true
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "regionFetchTimeout",
  "browser.region.timeout",
  5000
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "loggingEnabled",
  "browser.region.log",
  false
);

const log = console.createInstance({
  prefix: "Region.jsm",
  maxLogLevel: loggingEnabled ? "All" : "Warn",
});

function timeout(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

/**
 * This module keeps track of the users current region (country).
 * so the SearchService and other consumers can apply region
 * specific customisations.
 */
class RegionDetector {
  // Keep track of the wifi data across listener events.
  wifiDataPromise = null;
  // Store the AbortController for the geolocation requests so we
  // can abort the request on timeout.
  fetchController = null;

  /**
   * Fetch the region that we consider to be the users home region.
   *
   * @returns {object}
   *   JSON with country_code field defining users current region.
   */
  async getHomeRegion() {
    return Promise.race([this._getRegion(), this._timeout()]);
  }

  /**
   * Implementation of the above region fetching.
   */
  async _getRegion() {
    log.info("getRegion called");
    this.fetchController = new AbortController();
    let fetchOpts = {
      headers: { "Content-Type": "application/json" },
      credentials: "omit",
      signal: this.fetchController.signal,
    };
    if (wifiScanningEnabled) {
      let wifiData = await this._fetchWifiData();
      if (wifiData) {
        let postData = JSON.stringify({ wifiAccessPoints: wifiData });
        log.info("Sending wifi details: ", wifiData);
        fetchOpts.method = "POST";
        fetchOpts.body = postData;
      }
    }
    let url = Services.urlFormatter.formatURLPref("browser.region.network.url");
    let req = await fetch(url, fetchOpts);

    try {
      let res = await req.json();
      this.fetchController = null;
      log.info("getRegion returning " + res.country_code);
      return { country_code: res.country_code };
    } catch (err) {
      log.error("Error fetching region", err);
      throw new Error("region-fetch-no-result");
    }
  }

  /**
   * Implement the timeout for region requests. This will be run for
   * all region requests, but the error will only be returned if it
   * completes first.
   */
  async _timeout() {
    await timeout(regionFetchTimeout);
    if (this.fetchController) {
      this.fetchController.abort();
    }
    throw new Error("region-fetch-timeout");
  }

  async _fetchWifiData() {
    log.info("fetchWifiData called");
    this.wifiService = Cc["@mozilla.org/wifi/monitor;1"].getService(
      Ci.nsIWifiMonitor
    );
    this.wifiService.startWatching(this);

    return new Promise(resolve => {
      this.wifiDataPromise = resolve;
    });
  }

  onChange(accessPoints) {
    log.info("onChange called");
    if (!accessPoints || !this.wifiDataPromise) {
      return;
    }

    if (this.wifiService) {
      this.wifiService.stopWatching(this);
      this.wifiService = null;
    }

    if (this.wifiDataPromise) {
      let data = LocationHelper.formatWifiAccessPoints(accessPoints);
      this.wifiDataPromise(data);
      this.wifiDataPromise = null;
    }
  }
}

let Region = new RegionDetector();
