/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Seconds between maintenance runs.
const MAINTENANCE_INTERVAL_SECONDS = 7 * 86400;

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  PlacesDBUtils: "resource://gre/modules/PlacesDBUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

/**
 * This component can be used as a starter for modules that have to run when
 * certain categories are invoked.
 */
function PlacesCategoriesStarter() {
  Services.obs.addObserver(this, PlacesUtils.TOPIC_SHUTDOWN);
}

PlacesCategoriesStarter.prototype = {
  observe: function PCS_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case PlacesUtils.TOPIC_SHUTDOWN:
        Services.obs.removeObserver(this, PlacesUtils.TOPIC_SHUTDOWN);
        if (Cu.isModuleLoaded("resource://gre/modules/PlacesDBUtils.jsm")) {
          PlacesDBUtils.shutdown();
        }
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
