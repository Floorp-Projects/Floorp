/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["Region"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  LocationHelper: "resource://gre/modules/LocationHelper.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "wifiScanningEnabled",
  "browser.region.network.scan",
  true
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "networkTimeout",
  "browser.region.timeout",
  5000
);

// Retry the region lookup every hour on failure, a failure
// is likely to be a service failure so this gives the
// service some time to restore. Setting to 0 disabled retries.
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "retryTimeout",
  "browser.region.retry-timeout",
  60 * 60 * 1000
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "loggingEnabled",
  "browser.region.log",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "cacheBustEnabled",
  "browser.region.update.enabled",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "updateDebounce",
  "browser.region.update.debounce",
  60 * 60 * 24
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "lastUpdated",
  "browser.region.update.updated",
  0
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "localGeocodingEnabled",
  "browser.region.local-geocoding",
  false
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "timerManager",
  "@mozilla.org/updates/timer-manager;1",
  "nsIUpdateTimerManager"
);

const log = console.createInstance({
  prefix: "Region.jsm",
  maxLogLevel: lazy.loggingEnabled ? "All" : "Warn",
});

const REGION_PREF = "browser.search.region";
const COLLECTION_ID = "regions";
const GEOLOCATION_TOPIC = "geolocation-position-events";

// Prefix for all the region updating related preferences.
const UPDATE_PREFIX = "browser.region.update";

// The amount of time (in seconds) we need to be in a new
// location before we update the home region.
// Currently set to 2 weeks.
const UPDATE_INTERVAL = 60 * 60 * 24 * 14;

const MAX_RETRIES = 3;

// If the user never uses geolocation, schedule a periodic
// update to check the current location (in seconds).
const UPDATE_CHECK_NAME = "region-update-timer";
const UPDATE_CHECK_INTERVAL = 60 * 60 * 24 * 7;

// Let child processes read the current home value
// but dont trigger redundant updates in them.
let inChildProcess =
  Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

/**
 * This module keeps track of the users current region (country).
 * so the SearchService and other consumers can apply region
 * specific customisations.
 */
class RegionDetector {
  // The users home location.
  _home = null;
  // The most recent location the user was detected.
  _current = null;
  // The RemoteSettings client used to sync region files.
  _rsClient = null;
  // Keep track of the wifi data across listener events.
  _wifiDataPromise = null;
  // Keep track of how many times we have tried to fetch
  // the users region during failure.
  _retryCount = 0;
  // Let tests wait for init to complete.
  _initPromise = null;
  // Topic for Observer events fired by Region.jsm.
  REGION_TOPIC = "browser-region-updated";
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
  async init() {
    if (this._initPromise) {
      return this._initPromise;
    }
    if (lazy.cacheBustEnabled && !inChildProcess) {
      Services.tm.idleDispatchToMainThread(() => {
        lazy.timerManager.registerTimer(
          UPDATE_CHECK_NAME,
          () => this._updateTimer(),
          UPDATE_CHECK_INTERVAL
        );
      });
    }
    let promises = [];
    this._home = Services.prefs.getCharPref(REGION_PREF, null);
    if (!this._home && !inChildProcess) {
      promises.push(this._idleDispatch(() => this._fetchRegion()));
    }
    if (lazy.localGeocodingEnabled && !inChildProcess) {
      promises.push(this._idleDispatch(() => this._setupRemoteSettings()));
    }
    return (this._initPromise = Promise.all(promises));
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
   * Get the last region we detected the user to be in.
   *
   * @returns {string}
   *   The users current region.
   */
  get current() {
    return this._current;
  }

  /**
   * Fetch the users current region.
   *
   * @returns {string}
   *   The country_code defining users current region.
   */
  async _fetchRegion() {
    if (this._retryCount >= MAX_RETRIES) {
      return null;
    }
    let startTime = Date.now();
    let telemetryResult = this.TELEMETRY.SUCCESS;
    let result = null;

    try {
      result = await this._getRegion();
    } catch (err) {
      telemetryResult = this.TELEMETRY[err.message] || this.TELEMETRY.ERROR;
      log.error("Failed to fetch region", err);
      if (lazy.retryTimeout) {
        this._retryCount++;
        lazy.setTimeout(() => {
          Services.tm.idleDispatchToMainThread(this._fetchRegion.bind(this));
        }, lazy.retryTimeout);
      }
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
      this._setCurrentRegion(region, true);
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
   * Save the update current region and check if the home region
   * also needs an update.
   *
   * @param {string} region
   *   The region to store.
   */
  _setCurrentRegion(region = "") {
    log.info("Setting current region:", region);
    this._current = region;

    let now = Math.round(Date.now() / 1000);
    let prefs = Services.prefs;
    prefs.setIntPref(`${UPDATE_PREFIX}.updated`, now);

    // Interval is in seconds.
    let interval = prefs.getIntPref(
      `${UPDATE_PREFIX}.interval`,
      UPDATE_INTERVAL
    );
    let seenRegion = prefs.getCharPref(`${UPDATE_PREFIX}.region`, null);
    let firstSeen = prefs.getIntPref(`${UPDATE_PREFIX}.first-seen`, 0);

    // If we don't have a value for .home we can set it immediately.
    if (!this._home) {
      this._setHomeRegion(region);
    } else if (region != this._home && region != seenRegion) {
      // If we are in a different region than what is currently
      // considered home, then keep track of when we first
      // seen the new location.
      prefs.setCharPref(`${UPDATE_PREFIX}.region`, region);
      prefs.setIntPref(`${UPDATE_PREFIX}.first-seen`, now);
    } else if (region != this._home && region == seenRegion) {
      // If we have been in the new region for longer than
      // a specified time period, then set that as the new home.
      if (now >= firstSeen + interval) {
        this._setHomeRegion(region);
      }
    } else {
      // If we are at home again, stop tracking the seen region.
      prefs.clearUserPref(`${UPDATE_PREFIX}.region`);
      prefs.clearUserPref(`${UPDATE_PREFIX}.first-seen`);
    }
  }

  // Wrap a string as a nsISupports.
  _createSupportsString(data) {
    let string = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    string.data = data;
    return string;
  }

  /**
   * Save the updated home region and notify observers.
   *
   * @param {string} region
   *   The region to store.
   * @param {boolean} [notify]
   *   Tests can disable the notification for convenience as it
   *   may trigger an engines reload.
   */
  _setHomeRegion(region, notify = true) {
    if (region == this._home) {
      return;
    }
    log.info("Updating home region:", region);
    this._home = region;
    Services.prefs.setCharPref("browser.search.region", region);
    if (notify) {
      Services.obs.notifyObservers(
        this._createSupportsString(region),
        this.REGION_TOPIC
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
    if (lazy.wifiScanningEnabled) {
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
      let req = await this._fetchTimeout(url, fetchOpts, lazy.networkTimeout);
      let res = await req.json();
      log.info("_getRegion returning ", res.country_code);
      return res.country_code;
    } catch (err) {
      log.error("Error fetching region", err);
      let errCode = err.message in this.TELEMETRY ? err.message : "NO_RESULT";
      throw new Error(errCode);
    }
  }

  /**
   * Setup the RemoteSetting client + sync listener and ensure
   * the map files are downloaded.
   */
  async _setupRemoteSettings() {
    log.info("_setupRemoteSettings");
    this._rsClient = RemoteSettings(COLLECTION_ID);
    this._rsClient.on("sync", this._onRegionFilesSync.bind(this));
    await this._ensureRegionFilesDownloaded();
    // Start listening to geolocation events only after
    // we know the maps are downloded.
    Services.obs.addObserver(this, GEOLOCATION_TOPIC);
  }

  /**
   * Called when RemoteSettings syncs new data, clean up any
   * stale attachments and download any new ones.
   *
   * @param {Object} syncData
   *   Object describing the data that has just been synced.
   */
  async _onRegionFilesSync({ data: { deleted } }) {
    log.info("_onRegionFilesSync");
    const toDelete = deleted.filter(d => d.attachment);
    // Remove local files of deleted records
    await Promise.all(
      toDelete.map(entry => this._rsClient.attachments.deleteDownloaded(entry))
    );
    await this._ensureRegionFilesDownloaded();
  }

  /**
   * Download the RemoteSetting record attachments, when they are
   * successfully downloaded set a flag so we can start using them
   * for geocoding.
   */
  async _ensureRegionFilesDownloaded() {
    log.info("_ensureRegionFilesDownloaded");
    let records = (await this._rsClient.get()).filter(d => d.attachment);
    log.info("_ensureRegionFilesDownloaded", records);
    if (!records.length) {
      log.info("_ensureRegionFilesDownloaded: Nothing to download");
      return;
    }
    await Promise.all(records.map(r => this._rsClient.attachments.download(r)));
    log.info("_ensureRegionFilesDownloaded complete");
    this._regionFilesReady = true;
  }

  /**
   * Fetch an attachment from RemoteSettings.
   *
   * @param {String} id
   *   The id of the record to fetch the attachment from.
   */
  async _fetchAttachment(id) {
    let record = (await this._rsClient.get({ filters: { id } })).pop();
    let { buffer } = await this._rsClient.attachments.download(record);
    let text = new TextDecoder("utf-8").decode(buffer);
    return JSON.parse(text);
  }

  /**
   * Get a map of the world with region definitions.
   */
  async _getPlainMap() {
    return this._fetchAttachment("world");
  }

  /**
   * Get a map with the regions expanded by a few km to help
   * fallback lookups when a location is not within a region.
   */
  async _getBufferedMap() {
    return this._fetchAttachment("world-buffered");
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
    let req = await this._fetchTimeout(url, fetchOpts, lazy.networkTimeout);
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
    if (polygons.length == 1) {
      log.info("Found in single exact region");
      return polygons[0].properties.alpha2;
    }
    if (polygons.length) {
      log.info("Found in ", polygons.length, "overlapping exact regions");
      return this._findFurthest(location, polygons);
    }

    // We haven't found a match in the exact map, use the buffered map
    // to see if the point is close to a region.
    let bufferedMap = await this._getBufferedMap();
    polygons = this._getPolygonsContainingPoint(location, bufferedMap);

    if (polygons.length === 1) {
      log.info("Found in single buffered region");
      return polygons[0].properties.alpha2;
    }

    // Matched more than one region, which one of those regions
    // is it closest to without the buffer.
    if (polygons.length) {
      log.info("Found in ", polygons.length, "overlapping buffered regions");
      let regions = polygons.map(polygon => polygon.properties.alpha2);
      let unBufferedRegions = plainMap.features.filter(feature =>
        regions.includes(feature.properties.alpha2)
      );
      return this._findClosest(location, unBufferedRegions);
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
          polygons.push(feature);
        }
      } else if (feature.geometry.type === "MultiPolygon") {
        for (const innerCoords of coords) {
          if (this._polygonInPoint(point, innerCoords[0])) {
            polygons.push(feature);
          }
        }
      }
    }
    return polygons;
  }

  /**
   * Find the largest distance between a point and any of the points that
   * make up an array of regions.
   *
   * @param {Object} location
   *   A lat + lng coordinate.
   * @param {Array} regions
   *   An array of GeoJSON region definitions.
   *
   * @returns {String}
   *   A 2 character string representing a region.
   */
  _findFurthest(location, regions) {
    let max = { distance: 0, region: null };
    this._traverse(regions, ({ lat, lng, region }) => {
      let distance = this._distanceBetween(location, { lng, lat });
      if (distance > max.distance) {
        max = { distance, region };
      }
    });
    return max.region;
  }

  /**
   * Find the smallest distance between a point and any of the points that
   * make up an array of regions.
   *
   * @param {Object} location
   *   A lat + lng coordinate.
   * @param {Array} regions
   *   An array of GeoJSON region definitions.
   *
   * @returns {String}
   *   A 2 character string representing a region.
   */
  _findClosest(location, regions) {
    let min = { distance: Infinity, region: null };
    this._traverse(regions, ({ lat, lng, region }) => {
      let distance = this._distanceBetween(location, { lng, lat });
      if (distance < min.distance) {
        min = { distance, region };
      }
    });
    return min.region;
  }

  /**
   * Utility function to loop over all the coordinate points in an
   * array of polygons and call a function on them.
   *
   * @param {Array} regions
   *   An array of GeoJSON region definitions.
   * @param {Function} fun
   *   Function to call on individual coordinates.
   */
  _traverse(regions, fun) {
    for (const region of regions) {
      if (region.geometry.type === "Polygon") {
        for (const [lng, lat] of region.geometry.coordinates[0]) {
          fun({ lat, lng, region: region.properties.alpha2 });
        }
      } else if (region.geometry.type === "MultiPolygon") {
        for (const innerCoords of region.geometry.coordinates) {
          for (const [lng, lat] of innerCoords[0]) {
            fun({ lat, lng, region: region.properties.alpha2 });
          }
        }
      }
    }
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
    await new Promise(resolve => lazy.setTimeout(resolve, timeout));
    if (controller) {
      // Yield so it is the TIMEOUT that is returned and not
      // the result of the abort().
      lazy.setTimeout(() => controller.abort(), 0);
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
      this._wifiDataPromise = resolve;
    });
  }

  /**
   * If the user is using geolocation then we will see frequent updates
   * debounce those so we aren't processing them constantly.
   *
   * @returns {bool}
   *   Whether we should continue the update check.
   */
  _needsUpdateCheck() {
    let sinceUpdate = Math.round(Date.now() / 1000) - lazy.lastUpdated;
    let needsUpdate = sinceUpdate >= lazy.updateDebounce;
    if (!needsUpdate) {
      log.info(`Ignoring update check, last seen ${sinceUpdate} seconds ago`);
    }
    return needsUpdate;
  }

  /**
   * Dispatch a promise returning function to the main thread and
   * resolve when it is completed.
   */
  _idleDispatch(fun) {
    return new Promise(resolve => {
      Services.tm.idleDispatchToMainThread(fun().then(resolve));
    });
  }

  /**
   * timerManager will call this periodically to update the region
   * in case the user never users geolocation.
   */
  async _updateTimer() {
    if (this._needsUpdateCheck()) {
      await this._fetchRegion();
    }
  }

  /**
   * Called when we see geolocation updates.
   * in case the user never users geolocation.
   *
   * @param {Object} location
   *   A location object containing lat + lng coordinates.
   *
   */
  async _seenLocation(location) {
    log.info(`Got location update: ${location.lat}:${location.lng}`);
    if (this._needsUpdateCheck()) {
      let region = await this._geoCode(location);
      if (region) {
        this._setCurrentRegion(region);
      }
    }
  }

  onChange(accessPoints) {
    log.info("onChange called");
    if (!accessPoints || !this._wifiDataPromise) {
      return;
    }

    if (this.wifiService) {
      this.wifiService.stopWatching(this);
      this.wifiService = null;
    }

    if (this._wifiDataPromise) {
      let data = lazy.LocationHelper.formatWifiAccessPoints(accessPoints);
      this._wifiDataPromise(data);
      this._wifiDataPromise = null;
    }
  }

  observe(aSubject, aTopic, aData) {
    log.info(`Observed ${aTopic}`);
    switch (aTopic) {
      case GEOLOCATION_TOPIC:
        // aSubject from GeoLocation.cpp will be a GeoPosition
        // DOM Object, but from tests we will receive a
        // wrappedJSObject so handle both here.
        let coords = aSubject.coords || aSubject.wrappedJSObject.coords;
        this._seenLocation({
          lat: coords.latitude,
          lng: coords.longitude,
        });
        break;
    }
  }

  // For tests to create blank new instances.
  newInstance() {
    return new RegionDetector();
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
