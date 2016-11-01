/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// //////////////////////////////////////////////////////////////////////////////
// // Constants

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

// Fired by TelemetryController when async telemetry data should be collected.
const TOPIC_GATHER_TELEMETRY = "gather-telemetry";

// Seconds between maintenance runs.
const MAINTENANCE_INTERVAL_SECONDS = 7 * 86400;

// //////////////////////////////////////////////////////////////////////////////
// // Imports

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesDBUtils",
                                  "resource://gre/modules/PlacesDBUtils.jsm");

/**
 * This component can be used as a starter for modules that have to run when
 * certain categories are invoked.
 */
function PlacesCategoriesStarter()
{
  Services.obs.addObserver(this, TOPIC_GATHER_TELEMETRY, false);
  Services.obs.addObserver(this, PlacesUtils.TOPIC_SHUTDOWN, false);

  // nsINavBookmarkObserver implementation.
  let notify = () => {
    if (!this._notifiedBookmarksSvcReady) {
      // TODO (bug 1145424): for whatever reason, even if we remove this
      // component from the category (and thus from the category cache we use
      // to notify), we keep being notified.
      this._notifiedBookmarksSvcReady = true;
      // For perf reasons unregister from the category, since no further
      // notifications are needed.
      Cc["@mozilla.org/categorymanager;1"]
        .getService(Ci.nsICategoryManager)
        .deleteCategoryEntry("bookmark-observers", "PlacesCategoriesStarter", false);
      // Directly notify PlacesUtils, to ensure it catches the notification.
      PlacesUtils.observe(null, "bookmarks-service-ready", null);
    }
  };

  [ "onItemAdded", "onItemRemoved", "onItemChanged", "onBeginUpdateBatch",
    "onEndUpdateBatch", "onItemVisited", "onItemMoved"
  ].forEach(aMethod => this[aMethod] = notify);
}

PlacesCategoriesStarter.prototype = {
  // ////////////////////////////////////////////////////////////////////////////
  // // nsIObserver

  observe: function PCS_observe(aSubject, aTopic, aData)
  {
    switch (aTopic) {
      case PlacesUtils.TOPIC_SHUTDOWN:
        Services.obs.removeObserver(this, PlacesUtils.TOPIC_SHUTDOWN);
        Services.obs.removeObserver(this, TOPIC_GATHER_TELEMETRY);
        let globalObj =
          Cu.getGlobalForObject(PlacesCategoriesStarter.prototype);
        let descriptor =
          Object.getOwnPropertyDescriptor(globalObj, "PlacesDBUtils");
        if (descriptor.value !== undefined) {
          PlacesDBUtils.shutdown();
        }
        break;
      case TOPIC_GATHER_TELEMETRY:
        PlacesDBUtils.telemetry();
        break;
      case "idle-daily":
        // Once a week run places.sqlite maintenance tasks.
        let lastMaintenance = 0;
        try {
          lastMaintenance =
            Services.prefs.getIntPref("places.database.lastMaintenance");
        } catch (ex) {}
        let nowSeconds = parseInt(Date.now() / 1000);
        if (lastMaintenance < nowSeconds - MAINTENANCE_INTERVAL_SECONDS) {
          PlacesDBUtils.maintenanceOnIdle();
        }
        break;
      default:
        throw new Error("Trying to handle an unknown category.");
    }
  },

  // ////////////////////////////////////////////////////////////////////////////
  // // nsISupports

  classID: Components.ID("803938d5-e26d-4453-bf46-ad4b26e41114"),

  _xpcom_factory: XPCOMUtils.generateSingletonFactory(PlacesCategoriesStarter),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIObserver
  , Ci.nsINavBookmarkObserver
  ])
};

// //////////////////////////////////////////////////////////////////////////////
// // Module Registration

var components = [PlacesCategoriesStarter];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
