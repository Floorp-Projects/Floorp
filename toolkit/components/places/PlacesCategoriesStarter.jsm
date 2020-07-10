/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Fired by TelemetryController when async telemetry data should be collected.
const TOPIC_GATHER_TELEMETRY = "gather-telemetry";

// Seconds between maintenance runs.
const MAINTENANCE_INTERVAL_SECONDS = 7 * 86400;

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PlacesDBUtils",
  "resource://gre/modules/PlacesDBUtils.jsm"
);

/**
 * This component can be used as a starter for modules that have to run when
 * certain categories are invoked.
 */
function PlacesCategoriesStarter() {
  Services.obs.addObserver(this, TOPIC_GATHER_TELEMETRY);
  Services.obs.addObserver(this, PlacesUtils.TOPIC_SHUTDOWN);
}

PlacesCategoriesStarter.prototype = {
  observe: function PCS_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case PlacesUtils.TOPIC_SHUTDOWN:
        Services.obs.removeObserver(this, PlacesUtils.TOPIC_SHUTDOWN);
        Services.obs.removeObserver(this, TOPIC_GATHER_TELEMETRY);
        if (Cu.isModuleLoaded("resource://gre/modules/PlacesDBUtils.jsm")) {
          PlacesDBUtils.shutdown();
        }
        break;
      case TOPIC_GATHER_TELEMETRY:
        PlacesDBUtils.telemetry();
        break;
      case "idle-daily":
        // Once a week run places.sqlite maintenance tasks.
        let lastMaintenance = Services.prefs.getIntPref(
          "places.database.lastMaintenance",
          0
        );
        let nowSeconds = parseInt(Date.now() / 1000);
        if (lastMaintenance < nowSeconds - MAINTENANCE_INTERVAL_SECONDS) {
          PlacesDBUtils.maintenanceOnIdle();
        }
        break;
      default:
        throw new Error("Trying to handle an unknown category.");
    }
  },

  classID: Components.ID("803938d5-e26d-4453-bf46-ad4b26e41114"),
  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
};

var EXPORTED_SYMBOLS = ["PlacesCategoriesStarter"];
