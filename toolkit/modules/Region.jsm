/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["Region"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
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

const REGION_PREF = "browser.search.region";

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
  // Topic for Observer events fired by Region.jsm.
  REGION_TOPIC = "browser-region";
  // Verb for event fired when we update the region.
  REGION_UPDATED = "region-updated";
  // Values for telemetry.
  TELEMETRY = {
    SUCCESS: 0,
    NO_RESULT: 1,
    TIMEOUT: 2,
    ERROR: 3,
  };

  /**
   * Read currently stored region data and if needed trigger background
   * region detection.
   */
  init() {
    let region = Services.prefs.getCharPref(REGION_PREF, null);
    if (!region) {
      Services.tm.idleDispatchToMainThread(this._fetchRegion.bind(this));
    }
    this._home = region;
  }

  /**
   * Get the region we currently consider the users home.
   *
   * @returns {string}
   *   The users current home region.
   */
  get home() {
    return this._home;
  }

  /**
   * Fetch the users current region.
   *
   * @returns {string}
   *   The country_code defining users current region.
   */
  async _fetchRegion() {
    let startTime = Date.now();
    let telemetryResult = this.TELEMETRY.SUCCESS;
    let result = null;

    try {
      result = await Promise.race([
        this._getRegion(),
        timeout(regionFetchTimeout),
      ]);
    } catch (err) {
      telemetryResult = this.TELEMETRY[err.message] || this.TELEMETRY.ERROR;
      log.error("Failed to fetch region", err);
    }

    let took = Date.now() - startTime;
    if (result) {
      await this._storeRegion(result);
    }
    Services.telemetry
      .getHistogramById("SEARCH_SERVICE_COUNTRY_FETCH_TIME_MS")
      .add(took);

    Services.telemetry
      .getHistogramById("SEARCH_SERVICE_COUNTRY_FETCH_RESULT")
      .add(telemetryResult);

    return result;
  }

  /**
   * Validate then store the region and report telemetry.
   *
   * @param region
   *   The region to store.
   */
  async _storeRegion(region) {
    let prefix = "SEARCH_SERVICE";
    let isTimezoneUS = isUSTimezone();
    // If it's a US region, but not a US timezone, we don't store
    // the value. This works because no region defaults to
    // ZZ (unknown) in nsURLFormatter
    if (region != "US" || isTimezoneUS) {
      log.info("Saving home region:", region);
      this._setRegion(region, true);
    }

    // and telemetry...
    if (region == "US" && !isTimezoneUS) {
      log.info("storeRegion mismatch - US Region, non-US timezone");
      Services.telemetry
        .getHistogramById(`${prefix}_US_COUNTRY_MISMATCHED_TIMEZONE`)
        .add(1);
    }
    if (region != "US" && isTimezoneUS) {
      log.info("storeRegion mismatch - non-US Region, US timezone");
      Services.telemetry
        .getHistogramById(`${prefix}_US_TIMEZONE_MISMATCHED_COUNTRY`)
        .add(1);
    }
    // telemetry to compare our geoip response with
    // platform-specific country data.
    // On Mac and Windows, we can get a country code via sysinfo
    let platformCC = await Services.sysinfo.countryCode;
    if (platformCC) {
      let probeUSMismatched, probeNonUSMismatched;
      switch (AppConstants.platform) {
        case "macosx":
          probeUSMismatched = `${prefix}_US_COUNTRY_MISMATCHED_PLATFORM_OSX`;
          probeNonUSMismatched = `${prefix}_NONUS_COUNTRY_MISMATCHED_PLATFORM_OSX`;
          break;
        case "win":
          probeUSMismatched = `${prefix}_US_COUNTRY_MISMATCHED_PLATFORM_WIN`;
          probeNonUSMismatched = `${prefix}_NONUS_COUNTRY_MISMATCHED_PLATFORM_WIN`;
          break;
        default:
          log.error(
            "Platform " +
              Services.appinfo.OS +
              " has system country code but no search service telemetry probes"
          );
          break;
      }
      if (probeUSMismatched && probeNonUSMismatched) {
        if (region == "US" || platformCC == "US") {
          // one of the 2 said US, so record if they are the same.
          Services.telemetry
            .getHistogramById(probeUSMismatched)
            .add(region != platformCC);
        } else {
          // non-US - record if they are the same
          Services.telemetry
            .getHistogramById(probeNonUSMismatched)
            .add(region != platformCC);
        }
      }
    }
  }

  /**
   * Save the updated region and notify observers.
   *
   * @param region
   *   The region to store.
   */
  _setRegion(region = "", notify = false) {
    this._home = region;
    Services.prefs.setCharPref("browser.search.region", region);
    if (notify) {
      Services.obs.notifyObservers(
        null,
        this.REGION_TOPIC,
        this.REGION_UPDATED,
        region
      );
    }
  }

  /**
   * Make the request to fetch the region from the configured service.
   */
  async _getRegion() {
    log.info("_getRegion called");
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
    log.info("_getRegion url is: ", url);

    if (!url) {
      return null;
    }

    try {
      let req = await fetch(url, fetchOpts);
      let res = await req.json();
      this.fetchController = null;
      log.info("_getRegion returning ", res.country_code);
      return res.country_code;
    } catch (err) {
      log.error("Error fetching region", err);
      throw new Error("NO_RESULT");
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
    throw new Error("TIMEOUT");
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
Region.init();

function timeout(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

// A method that tries to determine if this user is in a US geography.
function isUSTimezone() {
  // Timezone assumptions! We assume that if the system clock's timezone is
  // between Newfoundland and Hawaii, that the user is in North America.

  // This includes all of South America as well, but we have relatively few
  // en-US users there, so that's OK.

  // 150 minutes = 2.5 hours (UTC-2.5), which is
  // Newfoundland Daylight Time (http://www.timeanddate.com/time/zones/ndt)

  // 600 minutes = 10 hours (UTC-10), which is
  // Hawaii-Aleutian Standard Time (http://www.timeanddate.com/time/zones/hast)

  let UTCOffset = new Date().getTimezoneOffset();
  return UTCOffset >= 150 && UTCOffset <= 600;
}
