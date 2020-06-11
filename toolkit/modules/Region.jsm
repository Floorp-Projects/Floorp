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
  "networkTimeout",
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
      result = await this._getRegion();
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
    let fetchOpts = {
      headers: { "Content-Type": "application/json" },
      credentials: "omit",
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
      let req = await this._fetchTimeout(url, fetchOpts, networkTimeout);
      let res = await req.json();
      log.info("_getRegion returning ", res.country_code);
      return res.country_code;
    } catch (err) {
      log.error("Error fetching region", err);
      throw new Error("NO_RESULT");
    }
  }

  /**
   * Gets the users current location using the same reverse IP
   * request that is used for GeoLocation requests.
   *
   * @returns {Object} location
   *   Object representing the user location, with a location key
   *   that contains the lat / lng coordinates.
   */
  async _getLocation() {
    log.info("_getLocation called");
    let fetchOpts = { headers: { "Content-Type": "application/json" } };
    let url = Services.urlFormatter.formatURLPref("geo.provider.network.url");
    let req = await this._fetchTimeout(url, fetchOpts, networkTimeout);
    let result = await req.json();
    log.info("_getLocation returning", result);
    return result;
  }

  /**
   * Return the users current region using
   * request that is used for GeoLocation requests.
   *
   * @returns {String}
   *   A 2 character string representing a region.
   */
  async _getRegionLocally() {
    let { location } = await this._getLocation();
    return this._geoCode(location);
  }

  // TODO: Stubs for testing
  async _getPlainMap() {
    return null;
  }
  async _getBufferedMap() {
    return null;
  }

  /**
   * Take a location and return the region code for that location
   * by looking up the coordinates in geojson map files.
   * Inspired by https://github.com/mozilla/ichnaea/blob/874e8284f0dfa1868e79aae64e14707eed660efe/ichnaea/geocode.py#L114
   *
   * @param {Object} location
   *   A location object containing lat + lng coordinates.
   *
   * @returns {String}
   *   A 2 character string representing a region.
   */
  async _geoCode(location) {
    let plainMap = await this._getPlainMap();
    let polygons = this._getPolygonsContainingPoint(location, plainMap);
    // The plain map doesnt have overlapping regions so return
    // region straight away.
    if (polygons.length) {
      return polygons[0].region;
    }
    let bufferedMap = await this._getBufferedMap();
    polygons = this._getPolygonsContainingPoint(location, bufferedMap);
    // Only found one matching region, return.
    if (polygons.length === 1) {
      return polygons[0].region;
    }
    // Matched more than one region, find the longest distance
    // from a border and return that region.
    if (polygons.length > 1) {
      return this._findLargestDistance(location, polygons);
    }
    return null;
  }

  /**
   * Find all the polygons that contain a single point, return
   * an array of those polygons along with the region that
   * they define
   *
   * @param {Object} point
   *   A lat + lng coordinate.
   * @param {Object} map
   *   Geojson object that defined seperate regions with a list
   *   of polygons.
   *
   * @returns {Array}
   *   An array of polygons that contain the point, along with the
   *   region they define.
   */
  _getPolygonsContainingPoint(point, map) {
    let polygons = [];
    for (const feature of map.features) {
      let coords = feature.geometry.coordinates;
      if (feature.geometry.type === "Polygon") {
        if (this._polygonInPoint(point, coords[0])) {
          polygons.push({
            coords: coords[0],
            region: feature.properties.alpha2,
          });
        }
      } else if (feature.geometry.type === "MultiPolygon") {
        for (const innerCoords of coords) {
          if (this._polygonInPoint(point, innerCoords[0])) {
            polygons.push({
              coords: innerCoords[0],
              region: feature.properties.alpha2,
            });
          }
        }
      }
    }
    return polygons;
  }

  /**
   * Find the largest distance between a point and and a border
   * that contains it.
   *
   * @param {Object} location
   *   A lat + lng coordinate.
   * @param {Object} polygons
   *   Array of polygons that define a border.
   *
   * @returns {String}
   *   A 2 character string representing a region.
   */
  _findLargestDistance(location, polygons) {
    let maxDistance = { distance: 0, region: null };
    for (const polygon of polygons) {
      for (const [lng, lat] of polygon.coords) {
        let distance = this._distanceBetween(location, { lng, lat });
        if (distance > maxDistance.distance) {
          maxDistance = { distance, region: polygon.region };
        }
      }
    }
    return maxDistance.region;
  }

  /**
   * Check whether a point is contained within a polygon using the
   * point in polygon algorithm:
   * https://en.wikipedia.org/wiki/Point_in_polygon
   * This casts a ray from the point and counts how many times
   * that ray intersects with the polygons borders, if it is
   * an odd number of times the point is inside the polygon.
   *
   * @param {Object} location
   *   A lat + lng coordinate.
   * @param {Object} polygon
   *   Array of coordinates that define the boundaries of a polygon.
   *
   * @returns {boolean}
   *   Whether the point is within the polygon.
   */
  _polygonInPoint({ lng, lat }, poly) {
    let inside = false;
    // For each edge of the polygon.
    for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
      let xi = poly[i][0];
      let yi = poly[i][1];
      let xj = poly[j][0];
      let yj = poly[j][1];
      // Does a ray cast from the point intersect with this polygon edge.
      let intersect =
        yi > lat != yj > lat && lng < ((xj - xi) * (lat - yi)) / (yj - yi) + xi;
      // If so toggle result, an odd number of intersections
      // means the point is inside.
      if (intersect) {
        inside = !inside;
      }
    }
    return inside;
  }

  /**
   * Find the distance between 2 points.
   *
   * @param {Object} p1
   *   A lat + lng coordinate.
   * @param {Object} p2
   *   A lat + lng coordinate.
   *
   * @returns {int}
   *   The distance between the 2 points.
   */
  _distanceBetween(p1, p2) {
    return Math.hypot(p2.lng - p1.lng, p2.lat - p1.lat);
  }

  /**
   * A wrapper around fetch that implements a timeout, will throw
   * a TIMEOUT error if the request is not completed in time.
   *
   * @param {String} url
   *   The time url to fetch.
   * @param {Object} opts
   *   The options object passed to the call to fetch.
   * @param {int} timeout
   *   The time in ms to wait for the request to complete.
   */
  async _fetchTimeout(url, opts, timeout) {
    let controller = new AbortController();
    opts.signal = controller.signal;
    return Promise.race([fetch(url, opts), this._timeout(timeout, controller)]);
  }

  /**
   * Implement the timeout for network requests. This will be run for
   * all network requests, but the error will only be returned if it
   * completes first.
   *
   * @param {int} timeout
   *   The time in ms to wait for the request to complete.
   * @param {Object} controller
   *   The AbortController passed to the fetch request that
   *   allows us to abort the request.
   */
  async _timeout(timeout, controller) {
    await new Promise(resolve => setTimeout(resolve, timeout));
    if (controller) {
      controller.abort();
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
